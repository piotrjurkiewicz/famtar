#ifndef CHECKFFT_HH
#define CHECKFFT_HH
#include <click/batchelement.hh>
#include "fft.hh"
CLICK_DECLS

class CheckFFT : public BatchElement
{
    public:

        CheckFFT();
        ~CheckFFT();

        const char *class_name() const { return "CheckFFT"; }
        const char *port_count() const { return "1/2"; }
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
        inline int process(Packet *);
};

CLICK_ENDDECLS
#endif
