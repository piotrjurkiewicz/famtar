#include <click/config.h>

#include "addfft.hh"
#include <click/args.hh>
#include <click/error.hh>

#include "packet_info.hh"
CLICK_DECLS

AddFFT::AddFFT() :
    _table(NULL), _port(0), _verbose(false), _down_timer(this)
{
}

AddFFT::~AddFFT()
{
}

int
AddFFT::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
        .read_mp("TABLE", ElementCastArg("FFT"), _table)
        .read_mp("PORT", _port)
        .read("VERBOSE", _verbose)
        .complete() < 0)
        return -1;

    return 0;
}

int
AddFFT::initialize(ErrorHandler *)
{
    _down = false;
    _down_timer.initialize(this);

    return 0;
}

Packet*
AddFFT::simple_action(Packet *p)
{
    if (!_down)
    {
        _table->add_flow(p, _port);
        if (_verbose)
            click_chatter("AddFFT: %s port: %u", packet_info(p).c_str(), _port);
    }

    return p;
}

#if HAVE_BATCH
PacketBatch*
AddFFT::simple_action_batch(PacketBatch* batch)
{
    if (!_down)
    {
        FOR_EACH_PACKET(batch, p) {
            _table->add_flow(p, _port);
            if (_verbose)
                click_chatter("AddFFT: %s port: %u", packet_info(p).c_str(), _port);
        }
    }
    return batch;
}
#endif

void
AddFFT::set_down()
{
    if (_down_timer.initialized())
    {
        _down = true;
        _down_timer.schedule_after_sec(5);
    }

    _table->remove_flows(_port);
}

void
AddFFT::set_up()
{
    _down = false;
}

void
AddFFT::run_timer(Timer *timer)
{
    assert(timer == &_down_timer);
    _down = false;
}

enum { H_DOWN, H_UP };

int
AddFFT::write_handler(const String &, Element *e, void *thunk, ErrorHandler *)
{
    AddFFT *addfft = (AddFFT *) e;
    switch ((intptr_t) thunk)
    {
        case H_DOWN:
        {
            addfft->set_down();
            return 0;
        }
        case H_UP:
        {
            addfft->set_up();
            return 0;
        }
        default:
            return -1;
    }
}

void
AddFFT::add_handlers()
{
    add_write_handler("down", write_handler, H_DOWN, Handler::BUTTON);
    add_write_handler("up", write_handler, H_UP, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AddFFT)
