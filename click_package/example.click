// Import 'famtar' package
require(famtar);

// Flow Forwarding Table element
fft :: FFT(TIMEOUT 2s, GC_ON_ADD 1);

// Shared IP input path and routing table
ip :: Strip(14)
-> CheckIPHeader(INTERFACES 1.0.0.1/255.0.0.0 2.0.0.1/255.0.0.0)
-> fft_check :: CheckFFT(fft);

fft_check[0] -> rt :: LookupIPRoute(...);
fft_check[1] -> fft_rt :: RouteFFT(fft);

// ARP responses are copied to each ARPQuerier and the host
arpt :: Tee(3);

// Input and output paths for eth0
c0 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
FromDevice(eth0) -> c0;
out0 :: Queue(200)
-> ToDevice(eth0, DOWN_CALL fft_add0.down, UP_CALL fft_add0.up);

c0[0] -> ar0 :: ARPResponder(1.0.0.1 00:00:C0:CA:68:EF) -> out0;
arpq0 :: ARPQuerier(1.0.0.1, 00:00:C0:CA:68:EF) -> out0;
c0[1] -> arpt;
arpt[0] -> [1]arpq0;
c0[2] -> Paint(1) -> ip;
c0[3] -> Discard;

// Input and output paths for eth1
c1 :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
FromDevice(eth1) -> c1;
out1 :: Queue(200)
-> ToDevice(eth1, DOWN_CALL fft_add1.down, UP_CALL fft_add1.up);

c1[0] -> ar1 :: ARPResponder(2.0.0.1 00:00:C0:8A:67:EF) -> out1;
arpq1 :: ARPQuerier(2.0.0.1, 00:00:C0:8A:67:EF) -> out1;
c1[1] -> arpt;
arpt[1] -> [1]arpq1;
c1[2] -> Paint(2) -> ip;
c1[3] -> Discard;

// Local delivery
toh :: ToHost;
arpt[2] -> toh;
rt[0] -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> toh;

// Forwarding path for eth0
rt[1] -> fft_add0 :: AddFFT(fft, 0)
-> db0 :: DropBroadcasts
-> cp0 :: PaintTee(1)
-> go0 :: IPGWOptions(1.0.0.1)
-> FixIPSrc(1.0.0.1)
-> dt0 :: DecIPTTL
-> fr0 :: IPFragmenter(1500)
-> [0]arpq0;

fft_rt[0] -> db0;

dt0[1] -> ICMPError(1.0.0.1, timeexceeded) -> rt;
fr0[1] -> ICMPError(1.0.0.1, unreachable, needfrag) -> rt;
go0[1] -> ICMPError(1.0.0.1, parameterproblem) -> rt;
cp0[1] -> Discard;

// Forwarding path for eth1
rt[2] -> fft_add1 :: AddFFT(fft, 1)
-> db1 :: DropBroadcasts
-> cp1 :: PaintTee(2)
-> go1 :: IPGWOptions(2.0.0.1)
-> FixIPSrc(2.0.0.1)
-> dt1 :: DecIPTTL
-> fr1 :: IPFragmenter(1500)
-> [0]arpq1;

fft_rt[1] -> db1;

dt1[1] -> ICMPError(2.0.0.1, timeexceeded) -> rt;
fr1[1] -> ICMPError(2.0.0.1, unreachable, needfrag) -> rt;
go1[1] -> ICMPError(2.0.0.1, parameterproblem) -> rt;
cp1[1] -> Discard;
