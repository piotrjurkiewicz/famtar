# Installation:

1.  Extract to any directory in a system with installed Click (there should not be any spaces in a directory path).
2.  Execute the following commands:
    -  `autoconf`
    -  `./configure --prefix=DIR`
        If Click is installed in the standard location ('/usr/local/click'), parameter `--prefix` can be omitted, otherwise it should point to the Click installation directory.
    -  `make`
3.  Compiled package is located in files 'famtar.uo' (userlevel shared library) and 'famtar.ko' (Linux kernel module).

# Usage:

## FFT element:

    FFT([TIMEOUT 2s, LOOP_AVOIDANCE 1, GC_ON_ADD 0, GC_ON_CHECK 0]);

    Type: - (element does not process packets directly)

Element implements Flow Forwarding Table as an associative array, using `HashTable<>` container from Click's standard library. `HashTable<>` container is a chained hash table. This ensures full distinguishability between flows. Keys in this table are tuples of flow identifying information, like network addresses and ports.

``` c++
    struct FlowKey
    {
        uint32_t srcaddr;
        uint32_t dstaddr;
        uint16_t srcport;
        uint16_t dstport;
    }
```

Values stored in the table are timestamp of last packet for the particular flow (in milliseconds), routing information and TTL value of the first packet of the particular flow.

``` c++
    struct FlowValue
    {
        uint32_t ts;
        uint8_t port;
        IPAddress gateway;
        uint8_t ttl;
    }
```

Click automatically expands and rebuilds table, when its load factor rises beyond 2. The size of hash, which is used for the bucket list addressing, depends on the current table size. Click takes care of it automatically

Argument **TIMEOUT** defines the time of inactivity, after which flows in FFT are being considered as expired. This argument is not compulsory. If not set, flows living time is infinite. When **TIMEOUT** is set to 0, flows stored in FFT are always being considered as expired.

Its value should be specified using time units, for example:

- 2 or 2s or 2sec (2 seconds)
- 500ms or 500msec (500 milliseconds)
- 1m or 1min (1 minute)

Argument **LOOP_AVOIDANCE** defines, whether loop resolution mechanism based on comparison of TTL values is active. Default value is 1, what means that the mechanism is active.

Arguments **GC_ON_ADD** and **GC_ON_CHECK** controls garbage collection performed during operations on the table. If **GC_ON_ADD** is 1, during a new flow addition, all entries in the same bucket to which the new flow is added, are scanned and expired entries are removed. This can prevent the hash table from overgrowing. If **GC_ON_CHECK** is 1, the same operation happens during flow checking, for all entries in the same bucket in which the checked flow resides.

## CheckFFT element:

    CheckFFT(TABLE fft[, VERBOSE 0])

    Type: PUSH 1/2

Checks whether FFT contains active entry for the flow of the processed packet and TTL values in FFT entry and packet match (only if **LOOP_AVOIDANCE** is set to 1). If these conditions are met, CheckFFT element pushes packet to the output port [1] and updates timestamp in the FFT entry. If not, is pushes packet to the output port [0].

The first argument **TABLE** is the name of FFT element instance, on which this element operates. This argument is compulsory.

With the argument **VERBOSE** it can be defined whether element should print to click_chatter result of FFT operation and information about every processed packet. This argument is optional, default is 0.

## AddFFT element:

    AddFFT(TABLE fft, PORT 0[, VERBOSE 0])

    Type: AGNOSTIC 1/1

Adds a new flow entry for the processed packet to the FFT, including routing information and TTL value. This element should be placed after the element, which performs routing and sets `dst_ip_anno` annotation.

The first argument **TABLE** is the name of FFT element instance, on which this element operates. This argument is compulsory.

The second argument, named **PORT**, defines the number of output port which will be remembered in the `port` FFT entry. Element RouteFFT will push packets of this flow to this output port. This argument is compulsory.

With the argument **VERBOSE** it can be defined whether element should print to click_chatter result of FFT operation and information about every processed packet. This argument is optional, default is 0.

## RouteFFT element:

    RouteFFT(TABLE fft[, VERBOSE 0])

    Type: PUSH 1/-

This element performs routing according to the routing information stored in FFT entry. In details, it sets packet's `dst_ip_anno` annotation to the one stored in the `gateway` field of FFT entry and pushes packet to the appropriate output port, using port number stored in the `port` field of FFT entry.

The first argument **TABLE** is the name of FFT element instance, on which this element operates. This argument is compulsory.

With the argument **VERBOSE** it can be defined whether element should print to click_chatter result of FFT operation and information about every processed packet. This argument is optional, default is 0.
