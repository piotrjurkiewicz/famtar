#include <click/config.h>

#include "checkfft.hh"
#include <click/args.hh>
#include <click/error.hh>

#include "packet_info.hh"
CLICK_DECLS

CheckFFT::CheckFFT() :
    _table(NULL), _verbose(false)
{
}

CheckFFT::~CheckFFT()
{
}

int
CheckFFT::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
        .read_mp("TABLE", ElementCastArg("FFT"), _table)
        .read("VERBOSE", _verbose)
        .complete() < 0)
        return -1;

    return 0;
}

int
CheckFFT::initialize(ErrorHandler *)
{
    return 0;
}

inline int
CheckFFT::process(Packet *p)
{
    int present_on_fft = _table->check_flow(p);

    if (_verbose)
        click_chatter("CheckFFT: %s result: %u", packet_info(p).c_str(),
                      present_on_fft);

    if (present_on_fft)
        return 1;
    else
        return 0;
}

void
CheckFFT::push(int, Packet *p)
{
    output(process(p)).push(p);
}

#if HAVE_BATCH
void
CheckFFT::push_batch(int, PacketBatch *batch)
{
    CLASSIFY_EACH_PACKET(2, process, batch, output_push_batch);
}
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckFFT)
