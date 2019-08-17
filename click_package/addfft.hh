#ifndef ADDFFT_HH
#define ADDFFT_HH
#include <click/batchelement.hh>
#include <click/timer.hh>
#include "fft.hh"
CLICK_DECLS

class AddFFT : public BatchElement
{
    public:

        AddFFT();
        ~AddFFT();

        const char *class_name() const { return "AddFFT"; }
        const char *port_count() const { return "1/1"; }
        const char *processing() const { return AGNOSTIC; }

        int configure(Vector<String> &conf, ErrorHandler *);
        int initialize(ErrorHandler *);
        void add_handlers();

        Packet *simple_action(Packet *);
    #if HAVE_BATCH
        PacketBatch *simple_action_batch(PacketBatch*);
    #endif

        void run_timer(Timer *timer);

        void set_down();
        void set_up();

    private:

        FFT *_table;
        uint8_t _port;
        bool _verbose;
        Timer _down_timer;
        bool _down;

        static int write_handler(const String &, Element *, void *, ErrorHandler *);
};

CLICK_ENDDECLS
#endif
