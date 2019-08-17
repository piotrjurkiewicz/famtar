#include <click/config.h>

#include "fft.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

FFT::FFT() :
    _timeout(0xFFFFFFFF), _loop_avoidance(true),
    _gc_on_add(false), _gc_on_check(false)
{
}

FFT::~FFT()
{
}

int
FFT::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
        .read("TIMEOUT", SecondsArg(3), _timeout)
        .read("LOOP_AVOIDANCE", _loop_avoidance)
        .read("GC_ON_ADD", _gc_on_add)
        .read("GC_ON_CHECK", _gc_on_check)
        .complete() < 0)
        return -1;

    return 0;
}

int
FFT::initialize(ErrorHandler *)
{
    return 0;
}

void
FFT::cleanup(CleanupStage)
{

}

int
FFT::add_flow(IPAddress src_addr, IPAddress dst_addr, uint16_t src_port, uint16_t dst_port,
              Timestamp ts, IPAddress gateway, uint8_t port, uint8_t ttl, bool overwrite_existing)
{
    FlowKey fkey(src_addr, dst_addr, src_port, dst_port);
    FlowValue &fval = _table[fkey];

    if (!overwrite_existing)
        if (fval.ts != 0 && !is_expired(ts, fval.ts))
            if (!_loop_avoidance || fval.ttl == ttl)
                return -1;

#if FFT_DETAILED_STATS
    if (fval->ts != 0)
        print_flow_info(&_overwritten_flows, fkey, fval, ts);
#endif

    fval.ts = ts;
    fval.gateway = gateway;
    fval.port = port;
    fval.ttl = ttl;

#if FFT_DETAILED_STATS
    fval.first = 0;
    fval.last = 0;
    fval.packets = 0;
    fval.bytes = 0;
#endif

    if (_gc_on_add)
        bucket_garbage_collection(fkey, ts);

    return 0;
}

int
FFT::add_flow(Packet *p, uint8_t port)
{
    Timestamp p_ts = p->timestamp_anno();

    FlowKey fkey(p);
    FlowValue &fval = _table[fkey];

#if FFT_DETAILED_STATS
    if (fval->ts != 0)
        print_flow_info(&_overwritten_flows, fkey, fval, p_ts);
#endif

    fval.ts = p_ts;
    fval.gateway = p->dst_ip_anno();
    fval.port = port;

    if (p->has_network_header())
        fval.ttl = p->ip_header()->ip_ttl;
    else
        fval.ttl = 0;

#if FFT_DETAILED_STATS
    fval.first = p_ts;
    fval.last = p_ts;
    fval.packets = 1;
    fval.bytes = p->length();
#endif

    if (_gc_on_add)
        bucket_garbage_collection(fkey, p_ts);

    return 0;
}

int
FFT::check_flow(Packet *p)
{
    int ret;
    Timestamp p_ts = p->timestamp_anno();

    FlowKey fkey(p);
    FlowValue *fval = _table.get_pointer(fkey);

    ret = 0;

    if (fval)
    {
        ret = 1;

        // click_chatter("Table: %s, Packet: %s Now: %s Recent: %s", fval->ts.unparse().c_str(), p_ts.unparse().c_str(), Timestamp::now().unparse().c_str(), Timestamp::recent().unparse().c_str());

        if (is_expired(p_ts, fval->ts))
            ret = 0;

        if (_loop_avoidance && p->has_network_header())
            if (fval->ttl != p->ip_header()->ip_ttl)
                ret = 0;

        if (ret == 1)
        {
            fval->ts = p_ts;
#if FFT_DETAILED_STATS
            fval->last = p_ts;
            fval->packets += 1;
            fval->bytes += p->length();
#endif
        }

        if (_gc_on_check)
            bucket_garbage_collection(fkey, p_ts);
    }

    return ret;
}

int
FFT::route_flow(Packet *p)
{
    FlowKey fkey(p);
    FlowValue *fval = _table.get_pointer(fkey);

    if (fval)
    {
        uint8_t port = fval->port;
        IPAddress gateway = fval->gateway;
        if (gateway)
            p->set_dst_ip_anno(gateway);
        return port;
    }
    else
        return -1;
}

void
FFT::remove_flows(uint8_t port)
{
    auto it = _table.begin();

    while (it)
    {
        if (it.value().port == port)
            it = _table.erase(it);
        else
            it++;
    }
}

void
FFT::global_garbage_collection()
{
    Timestamp ts = Timestamp::now();

    auto it = _table.begin();

    while (it)
    {
        if (is_expired(ts, it.value().ts))
            it = _table.erase(it);
        else
            it++;
    }
}

void
FFT::bucket_garbage_collection(const FlowKey fkey, const Timestamp ts)
{
    auto it = _table.find_prefer(fkey);

    while (it)
    {
        if (it.key().hashcode() % _table.bucket_count()
            != fkey.hashcode() % _table.bucket_count())
            break;

        if (is_expired(ts, it.value().ts))
            it = _table.erase(it);
        else
            it++;
    }
}

String
FFT::dump_table(enum dumptype type)
{
    StringAccum sa;
    Timestamp ts = Timestamp::now();

#if FFT_DETAILED_STATS
    if (type == ALL)
        sa = StringAccum(_overwritten_flows);
#endif

    auto it = _table.begin();

    while (it)
    {
        if (type == ALL || !is_expired(ts, it.value().ts))
        {
            print_flow_info(&sa, it.key(), it.value(), ts);
        }
        it++;
    }

    return sa.take_string();
}

enum { H_SIZE, H_BUCKET_COUNT, H_MAX_BUCKET_SIZE, H_ACTIVE,
       H_ALL, H_CLEAR, H_REMOVE, H_MANUAL_GC };

String
FFT::read_handler(Element *e, void *thunk)
{
    FFT *cft = (FFT *) e;
    switch ((intptr_t) thunk)
    {
        case H_SIZE:
            return String(cft->_table.size());
        case H_BUCKET_COUNT:
            return String(cft->_table.bucket_count());
        case H_MAX_BUCKET_SIZE:
        {
            unsigned int max_bucket_size = 0;

            for (unsigned int b = 0; b < cft->_table.bucket_count(); b++)
                if (cft->_table.bucket_size(b) > max_bucket_size)
                    max_bucket_size = cft->_table.bucket_size(b);

            return String(max_bucket_size);
        }
        case H_ACTIVE:
            return cft->dump_table(ACTIVE);
        case H_ALL:
            return cft->dump_table(ALL);
        default:
            return "<error>";
    }
}

int
FFT::write_handler(const String &data, Element *e, void *thunk, ErrorHandler *)
{
    FFT *cft = (FFT *) e;
    switch ((intptr_t) thunk)
    {
        case H_CLEAR:
        {
#if FFT_DETAILED_STATS
            cft->_overwritten_flows.clear();
#endif
            cft->_table.clear();
            return 0;
        }
        case H_REMOVE:
        {
            unsigned int port;
            if (cp_integer(data, &port))
                cft->remove_flows(port);
            return 0;
        }
        case H_MANUAL_GC:
        {
            cft->global_garbage_collection();
            return 0;
        }
        default:
            return -1;
    }
}

void
FFT::add_handlers()
{
    add_read_handler("size", read_handler, H_SIZE);
    add_read_handler("bucket_count", read_handler, H_BUCKET_COUNT);
    add_read_handler("max_bucket_size", read_handler, H_MAX_BUCKET_SIZE);
    add_read_handler("active", read_handler, H_ACTIVE);
    add_read_handler("all", read_handler, H_ALL);
    add_write_handler("clear", write_handler, H_CLEAR, Handler::BUTTON);
    add_write_handler("remove", write_handler, H_REMOVE);
    add_write_handler("manual_gc", write_handler, H_MANUAL_GC, Handler::BUTTON);
    add_data_handlers("timeout", Handler::OP_READ | Handler::OP_WRITE, &_timeout);
    add_data_handlers("loop_avoidance", Handler::OP_READ | Handler::OP_WRITE
                      | Handler::CHECKBOX, &_loop_avoidance);
    add_data_handlers("gc_on_add", Handler::OP_READ | Handler::OP_WRITE
                      | Handler::CHECKBOX, &_gc_on_add);
    add_data_handlers("gc_on_check", Handler::OP_READ | Handler::OP_WRITE
                      | Handler::CHECKBOX, &_gc_on_check);
}

inline bool
FFT::is_expired(const Timestamp p_ts, const Timestamp fft_ts)
{
    Timestamp diff = p_ts - fft_ts;
    Timestamp::value_type diff_msec = diff.msecval();
    if (diff_msec < 0 || diff_msec > _timeout)
        return true;
    else
        return false;
}

void
FFT::print_flow_info(StringAccum *sa, const FlowKey &key, const FlowValue &val,
                     const Timestamp ts)
{
#if FFT_DETAILED_STATS
    sa->snprintf(256, "%08lx %s %u %s %u %u %ld %s %s %u %llu\n",
                 key.hashcode() % _table.bucket_count(),
                 key.sa.unparse().c_str(), ntohs(key.sp),
                 key.da.unparse().c_str(), ntohs(key.dp),
                 val.port,
                 (ts - val.ts).msecval(),
                 val.first.unparse().c_str(),
                 val.last.unparse().c_str(),
                 val.packets,
                 val.bytes);
#else
    sa->snprintf(256, "%08lx %s:%u -> %s:%u Port: %u Last pkt: %ld ms ago\n",
                 key.hashcode() % _table.bucket_count(),
                 key.sa.unparse().c_str(), ntohs(key.sp),
                 key.da.unparse().c_str(), ntohs(key.dp),
                 val.port,
                 (ts - val.ts).msecval());
#endif
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FFT)
