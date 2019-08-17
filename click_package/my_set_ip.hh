#if HAVE_INDIFFERENT_ALIGNMENT
#define unaligned_net_short(v) (ntohs(*reinterpret_cast<const uint16_t*>(v)))
#define UNALIGNED_NET_SHORT_EQ(x, y) ((x) == htons((y)))
#else
static inline uint16_t
unaligned_net_short(const void *v)
{
    const uint8_t *d = reinterpret_cast<const uint8_t *>(v);
    return (d[0] << 8) | d[1];
}
#define UNALIGNED_NET_SHORT_EQ(x, y) (unaligned_net_short(&(x)) == (y))
#endif

#define IP_ETHERTYPE(et) (UNALIGNED_NET_SHORT_EQ((et), ETHERTYPE_IP) || UNALIGNED_NET_SHORT_EQ((et), ETHERTYPE_IP6))

static inline bool
set_ip_headers(WritablePacket *&p, int dlt)
{
    const click_ip *iph = 0;
    const uint8_t *data = p->data();
    const uint8_t *end_data = p->end_data();

    if (dlt == 1)
        iph = reinterpret_cast<const click_ip*>(data);
    else if (dlt == 2)
    {
        const click_ether* ethh = reinterpret_cast<const click_ether*>(data);
        if (data + sizeof(click_ether) <= end_data)
        {
            if (IP_ETHERTYPE(ethh->ether_type))
                iph = reinterpret_cast<const click_ip*>(ethh + 1);
            else if (UNALIGNED_NET_SHORT_EQ(ethh->ether_type, ETHERTYPE_8021Q) && data + sizeof(click_ether_vlan) <= end_data)
            {
                // XXX don't handle 802.1Q-in-802.1Q
                const click_ether_vlan* ethvh = reinterpret_cast<const click_ether_vlan*>(ethh);
                if (IP_ETHERTYPE(ethvh->ether_vlan_encap_proto))
                    iph = reinterpret_cast<const click_ip*>(ethvh + 1);
            }
        }
    }

    if (!iph)
        return false;

    if (iph->ip_v == 4)
    {
        if (iph->ip_hl >= 5 && reinterpret_cast<const uint8_t*>(iph) + (iph->ip_hl << 2) <= end_data)
        {
            p->set_ip_header(iph, iph->ip_hl << 2);
            p->set_dst_ip_anno(iph->ip_dst);
            return true;
        }
    }
    else if (iph->ip_v == 6)
    {
        if (reinterpret_cast<const uint8_t*>(iph) + sizeof(click_ip6) <= end_data)
        {
            p->set_ip6_header(reinterpret_cast<const click_ip6*>(iph));
            return true;
        }
    }

    return false;
}
