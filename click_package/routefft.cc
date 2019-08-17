#include <click/config.h>

#include "routefft.hh"
#include <click/args.hh>
#include <click/error.hh>

#include "packet_info.hh"
CLICK_DECLS

RouteFFT::RouteFFT() :
    _table(NULL), _verbose(false), _no_route_printed(false)
{
}

RouteFFT::~RouteFFT()
{
}

int
RouteFFT::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
        .read_mp("TABLE", ElementCastArg("FFT"), _table)
        .read("VERBOSE", _verbose)
        .complete() < 0)
        return -1;

    return 0;
}

int
RouteFFT::initialize(ErrorHandler *)
{
    return 0;
}

inline int
RouteFFT::process(Packet *p)
{
    int port = _table->route_flow(p);

    if (_verbose)
        click_chatter("RouteFFT: %s port: %d", packet_info(p).c_str(),
                      port);

    if (port >= 0)
        return port;
    else
    {
        if (_verbose || !_no_route_printed)
        {
            click_chatter("RouteFFT: no route for packet: %s", packet_info(p).c_str());
            _no_route_printed = true;
        }
        return noutputs();
    }
}

void
RouteFFT::push(int, Packet *p)
{
    checked_output_push(process(p), p);
}

#if HAVE_BATCH
void
RouteFFT::push_batch(int, PacketBatch *batch)
{
    CLASSIFY_EACH_PACKET(noutputs() + 1, process, batch, checked_output_push_batch);
}
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(RouteFFT)
