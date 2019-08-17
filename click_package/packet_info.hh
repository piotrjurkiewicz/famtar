#ifndef PACKET_INFO_HH
#define PACKET_INFO_HH
#include <click/string.hh>
#include <click/packet.hh>
CLICK_DECLS

static String
packet_info(Packet *p)
{
    char buffer[64];

    String sourceadd = "NaA";
    String destinadd = "NaA";
    uint16_t srcport = 0;
    uint16_t dstport = 0;
    String protocol = "---";

    if (p->has_network_header())
    {
        const click_ip *iph = p->ip_header();

        sourceadd = IPAddress(iph->ip_src).unparse();
        destinadd = IPAddress(iph->ip_dst).unparse();

        if (IP_FIRSTFRAG(iph)
        && (iph->ip_p == IP_PROTO_TCP || iph->ip_p == IP_PROTO_UDP))
        {
            srcport = *((const uint16_t *) (p->transport_header()));
            srcport = ntohs(srcport);
            dstport = *((const uint16_t *) (p->transport_header() + 2));
            dstport = ntohs(dstport);
            if (iph->ip_p == IP_PROTO_TCP)
                protocol = "TCP";
            if (iph->ip_p == IP_PROTO_UDP)
                protocol = "UDP";
        }
    }

    snprintf(buffer, 64, "%3s %s:%u -> %s:%u", protocol.c_str(),
             sourceadd.c_str(), srcport, destinadd.c_str(), dstport);

    return String(buffer);
}

CLICK_ENDDECLS
#endif
