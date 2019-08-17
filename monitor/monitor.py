#!/usr/bin/python -B

import os
import sys
import time
import argparse
import logging
import signal
import subprocess
import socket

import sdnroute.linkinfo
import sdnroute.utils


class Xorpsh(object):

    BIN = "/usr/local/xorp/sbin/xorpsh"

    def __init__(self):
        self.cmds = []

    def connect(self):

        try:
            logger.info('Getting config for interfaces: %s' % ", ".join(ifs))
            out = subprocess.check_output([self.BIN, "-c", "configure", "-c", "show -all protocols ospf4"])
        except Exception:
            logger.error('Cannot get config config for interfaces: %s' % ", ".join(ifs))
            raise

        for name in ifs:
            try:
                logger.debug('[%s] getting address and starting cost' % name)
                vif_start = out.index('vif %s' % name)
                address_start = out.index("address", vif_start) + 8
                address_end = out.index(" ", address_start)
                ifs[name]['address'] = out[address_start:address_end]
                cost_start = out.index("interface-cost:", vif_start) + 16
                cost_end = out.index("\r", cost_start)
                ifs[name]['starting_cost'] = int(out[cost_start:cost_end])
                logger.info('[%s] has address: %s has starting cost: %d' % (name, ifs[name]['address'], ifs[name]['starting_cost']))
            except Exception:
                logger.error('Cannot get address and starting cost for %s: %s' % (name, out))
                raise

    def set_cost(self, name, cost):
        self.cmds.append("set protocols ospf4 area 0.0.0.0 interface %s vif %s address %s interface-cost %d" % (name, name, ifs[name]['address'], cost))
        logger.debug('[%s] setting cost to %d' % (name, cost))

    def flush(self):
        if self.cmds:
            try:
                out = subprocess.check_output([self.BIN, "-c", "configure"] + sum([['-c', cmd] for cmd in self.cmds], []) + ["-c", "commit"])
            except subprocess.CalledProcessError:
                out = ""
            if "OK" in out:
                logger.info('Executing commands OK')
            else:
                logger.info('Executing commands FAILED')
            self.cmds = []


class Quagga(object):

    PORT = 2604
    PASSWORD = "a"

    def __init__(self):
        self.cmds = []
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    def connect(self):

        try:
            logger.info("Connecting to localhost:%d" % self.PORT)
            self.s.connect(('localhost', self.PORT))
            logger.info("Connected to localhost:%d" % self.PORT)
        except Exception:
            logger.error("Connecting to localhost:%d" % self.PORT)
            raise

        reply = ""

        try:
            self.s.recv(4096)
            self.s.send("%s\n" % self.PASSWORD)
            reply = self.s.recv(4096)
            assert reply.endswith("> ")
            logger.info("Entered password")
        except Exception:
            logger.error("Error entering password: %s" % reply)
            raise

        for name in ifs:
            attempt = 0
            while True:
                attempt += 1
                reply = ""
                try:
                    logger.debug('[%s] getting starting cost' % name)
                    self.s.send("show ip ospf interface %s\n" % name)
                    reply = self.s.recv(4096)
                    cost_start = reply.index("Cost: ") + 6
                    cost_end = reply.index("\n", cost_start)
                    ifs[name]['starting_cost'] = int(reply[cost_start:cost_end])
                    logger.info('[%s] has starting cost: %d' % (name, ifs[name]['starting_cost']))
                except Exception:
                    if attempt > 10:
                        logger.error('Cannot get starting cost for %s: %s' % (name, reply))
                        raise
                    else:
                        time.sleep(1)
                else:
                    break

        assert reply.endswith("> ")
        reply = ""

        try:
            self.s.send("enable\nconf t\n")
            reply = self.s.recv(4096)
            assert reply.endswith("(config)# ")
            logger.info("Enabled mode")
        except Exception:
            logger.error("Error enabling mode: %s" % reply)
            raise

    def set_cost(self, name, cost):
        self.cmds.append("interface %s\nip ospf cost %d\nexit\n" % (name, cost))
        logger.debug('[%s] setting cost to %d' % (name, cost))

    def flush(self):
        if self.cmds:
            reply = ""
            try:
                self.s.send(''.join(self.cmds))
                reply = self.s.recv(4096)
                assert reply.endswith("(config)# ")
                logger.debug("Flushed")
            except (socket.error, AssertionError):
                logger.info("Error flushing: %s" % reply)
            self.cmds = []


CTRLS = {
    'xorp': Xorpsh,
    'quagga': Quagga
}

def signal_handler(sig, frame):
    logger.info('Quiting...')
    for name in ifs:
        if ifs[name]['over']:
            logger.info('[%s] setting cost to starting cost %d' % (name, ifs[name]['starting_cost']))
            ctrl.set_cost(name, ifs[name]['starting_cost'])
    ctrl.flush()
    logger.info('Done')
    sys.exit(0)

def iterate_all_interfaces():

    for name in ifs:

        speed = None

        try:
            with open(ifs[name]['file_speed_cfg']) as f:
                speed = int(f.readline())
        except (IOError, ValueError):
            pass

        if speed is None:
            with open(ifs[name]['file_speed_sys']) as f:
                speed = int(f.readline())

        if speed is None:
            logger.error('[%s] Cannot read speed' % name)
            continue

        byte_speed = speed * 1000000 / 8
        bw_min = int(opt.bottom * byte_speed)
        bw_max = int(opt.upper * byte_speed)

        now = time.time()

        try:
            name, counter = ifs[name]['stats_reader'].get_bytes(name, 'tx')
            logger.debug('[%s](%5d) counter: %s B' % (name, speed, "{:15,d}".format(counter)))
        except Exception:
            logger.error('Cannot read interface %s stats' % name)
            raise

        if ifs[name]['last_counter'] is None:
            ifs[name]['last_counter'] = counter

        if ifs[name]['last_counter'] <= counter:

            transfer = (counter - ifs[name]['last_counter']) / (now - ifs[name]['last_time'])
            transfer = min(max(0, transfer), byte_speed)
            smoothed = int((1.0 - opt.ewm_alpha) * ifs[name]['last_smoothed_transfer'] + opt.ewm_alpha * transfer)
            ifs[name]['last_smoothed_transfer'] = smoothed

            logger.debug('[%s](%5d) %s bit/s %s bit/s' % (name, speed, "{:12,d}".format(int(transfer*8)), "{:12,d}".format(int(smoothed*8))))

            common_log = '%s;%s;%d;%d;%d;%d;%d;%d;' % (time.time(), name, counter, transfer * 8, smoothed * 8, speed, bw_min*8, bw_max*8)

            if ifs[name]['over'] and smoothed < bw_min:

                ifs[name]['over'] = False

                logger.info('[%s](%5d) below: %s bit/s %s bit/s' % (name, speed, "{:12,d}".format(int(bw_min*8)), "{:12,d}".format(int(smoothed*8))))
                logger.info('[%s](%5d) lowering cost to %d' % (name, speed, ifs[name]['starting_cost']))

                if opt.log:
                    opt.log.write(common_log + 'up;%d\n' % ifs[name]['starting_cost'])

                ctrl.set_cost(name, ifs[name]['starting_cost'])

            elif not ifs[name]['over'] and smoothed > bw_max:

                ifs[name]['over'] = True

                logger.info('[%s](%5d) above: %s bit/s %s bit/s' % (name, speed, "{:12,d}".format(int(bw_max*8)), "{:12,d}".format(int(smoothed*8))))
                logger.info('[%s](%5d) raising cost to %d' % (name, speed, opt.cost))

                # with open("/tmp/famtar_monitors/" + name, "a") as file:
                    # file.write("+")

                if opt.log:
                    opt.log.write(common_log + 'down;%d\n' % opt.cost)

                ctrl.set_cost(name, opt.cost)

            else:
                if opt.log:
                    opt.log.write(common_log + 'none;\n')

        else:
            logger.warning('[%s](%5d) counter reset' % (name, speed))

        ifs[name]['last_counter'] = counter
        ifs[name]['last_time'] = now

    ctrl.flush()

if __name__ == '__main__':

    logger = logging.getLogger(__file__)
    logger.setLevel(logging.WARNING)

    # sdnroute.utils.makedirs("/tmp/famtar-monitors", exist_ok=True)

    parser = argparse.ArgumentParser()
    parser.add_argument("--name", "-n", help="node name (default = local)", default='local')
    parser.add_argument("--ctrl", "-C", help="routing cost controller (default = xorpsh)", default='xorpsh')
    parser.add_argument("--bottom", "-b", help="bottom threshold as a rate of maximum transfer (default = 0.7)", type=float, default=0.7)
    parser.add_argument("--upper", "-u", help="upper threshold as a rate of maximum transfer (default = 0.9)", type=float, default=0.9)
    parser.add_argument("--cost", "-c", help="cost in high state (default = 100)", type=int, default=100)
    parser.add_argument("--interval", "-i", help="counters read interval in ms (default = 200)", type=int, default=200)
    parser.add_argument("--ewm-alpha", "-s", help="exponential moving average (default = 0.2)", type=float, default=0.2)
    parser.add_argument("--cfgdir", "-d", help="cfg dir (default = None)", default='')
    parser.add_argument('--verbose', '-v', help="set verbose level (-v, -vv)", action='count')
    parser.add_argument('--log', type=argparse.FileType('w'))
    parser.add_argument('interfaces', metavar='iface', nargs='+', help='interfaces to monitor')
    opt = parser.parse_args()

    if opt.bottom > opt.upper:
        parser.error('bottom threshold must be smaller then upper threshold')

    if opt.verbose:
        if opt.verbose == 1:
            logger.setLevel(logging.INFO)
        elif opt.verbose > 1:
            logger.setLevel(logging.DEBUG)

    if opt.log:
        opt.log.write('ts;interface;bytes counter;current bit/s;current smoothed bit/s;speed;BW_MIN bit/s;BW_MAX bit/s;event;new cost\n')

    ch = logging.StreamHandler()
    formatter = logging.Formatter("%(asctime)s {:>10}: FAMTAR Monitor: %(message)s".format(opt.name))
    ch.setFormatter(formatter)
    logger.addHandler(ch)

    logger.info('Starting for interfaces: %s' % (", ".join(opt.interfaces)))

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    ifs = {}

    for if_name in opt.interfaces:

        ifs[if_name] = {
            'stats_reader': sdnroute.linkinfo.ClickCounterFileStats(path=opt.cfgdir),
            'ctrl': None,
            'file_speed_sys': "/sys/class/net/%s/speed" % if_name,
            'file_speed_cfg': opt.cfgdir + "/speed/" + if_name,
            'address': None,
            'starting_cost': 1,
            'last_counter': None,
            'last_time': 0.0,
            'last_smoothed_transfer': 0,
            'over': False
        }

    os.nice(-20)
    time.sleep(2)

    ctrl = CTRLS[opt.ctrl]()
    ctrl.connect()

    while True:

        iterate_all_interfaces()
        time.sleep(opt.interval / 1000.0)
