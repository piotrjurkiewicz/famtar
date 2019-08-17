#ifndef FFT_HH
#define FFT_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <click/straccum.hh>
CLICK_DECLS

#define FFT_DETAILED_STATS 0

class FFT : public Element
{
    public:

        FFT();
        ~FFT();

        const char *class_name() const { return "FFT"; }

        int configure(Vector<String> &conf, ErrorHandler *);
        int initialize(ErrorHandler *);
        void cleanup(CleanupStage);
        void add_handlers();

        int add_flow(Packet *, uint8_t port);
        int add_flow(IPAddress src_addr, IPAddress dst_addr, uint16_t src_port, uint16_t dst_port,
                     Timestamp ts, IPAddress gateway, uint8_t port, uint8_t ttl, bool overwrite_existing);
        int check_flow(Packet *);
        int route_flow(Packet *);

        void remove_flows(uint8_t port);

    private:

        struct FlowKey
        {
            IPAddress sa;
            IPAddress da;
            uint16_t sp;
            uint16_t dp;

            FlowKey() : sa(), da(), sp(0), dp(0) {}

            FlowKey(IPAddress src_addr, IPAddress dst_addr, uint16_t src_port, uint16_t dst_port)
            {
                sa = src_addr;
                da = dst_addr;
                sp = src_port;
                dp = dst_port;
            }

            FlowKey(Packet *p)
            {
                const click_ip *iph = p->ip_header();

                sa = IPAddress(iph->ip_src);
                da = IPAddress(iph->ip_dst);

                if (IP_FIRSTFRAG(iph) && (iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP))
                {
                    sp = *((const uint16_t *) (p->transport_header()));
                    dp = *((const uint16_t *) (p->transport_header() + 2));
                }
                else
                {
                    sp = 0;
                    dp = 0;
                }
            }

            inline hashcode_t
            hashcode() const
            {
                uint32_t a = ((uint32_t) sa * 59) ^ (uint32_t) da;
                a = a ^ sp ^ (dp << 16);
                // Bob Jenkins http://burtleburtle.net/bob/hash/integer.html
                a = (a + 0x7ed55d16) + (a << 12);
                a = (a ^ 0xc761c23c) ^ (a >> 19);
                a = (a + 0x165667b1) + (a << 5);
                a = (a + 0xd3a2646c) ^ (a << 9);
                a = (a + 0xfd7046c5) + (a << 3);
                a = (a ^ 0xb55a4f09) ^ (a >> 16);
                return a;
            }

            inline bool
            operator==(const FlowKey &b) const
            {
                return sa == b.sa && da == b.da
                    && sp == b.sp && dp == b.dp;
            }
        };

        struct FlowValue
        {
            Timestamp ts;
            IPAddress gateway;
            uint8_t port;
            uint8_t ttl;
#if FFT_DETAILED_STATS
            Timestamp first;
            Timestamp last;
            uint32_t packets;
            uint64_t bytes;
#endif
        };

        uint32_t _timeout;
        bool _loop_avoidance;
        bool _gc_on_add;
        bool _gc_on_check;

        HashTable<FlowKey, FlowValue> _table;

#if FFT_DETAILED_STATS
        StringAccum _overwritten_flows;
#endif

        void global_garbage_collection();
        void bucket_garbage_collection(const FlowKey, const Timestamp);

        enum dumptype
        {
            ACTIVE, ALL
        };

        String dump_table(enum dumptype);

        static String read_handler(Element *, void *);
        static int write_handler(const String &, Element *, void *, ErrorHandler *);

        bool is_expired(const Timestamp, const Timestamp);
        void print_flow_info(StringAccum *, const FlowKey &, const FlowValue &,
                             const Timestamp);
};

CLICK_ENDDECLS
#endif
