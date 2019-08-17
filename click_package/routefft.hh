#ifndef ROUTEFFT_HH
#define ROUTEFFT_HH
#include <click/batchelement.hh>
#include "fft.hh"
CLICK_DECLS

class RouteFFT : public BatchElement
{
    public:

        RouteFFT();
        ~RouteFFT();

        const char *class_name() const { return "RouteFFT"; }
        const char *port_count() const { return "1/-"; }
        const char *processing() const { return PUSH; }

        int configure(Vector<String> &conf, ErrorHandler *);
        int initialize(ErrorHandler *);

        void push(int, Packet *);
    #if HAVE_BATCH
        void push_batch (int, PacketBatch *);
    #endif

    private:

        FFT *_table;
        bool _verbose;
        bool _no_route_printed;
        inline int process(Packet *);
};

CLICK_ENDDECLS
#endif
