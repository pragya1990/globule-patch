"test comb-pms-A" don't work because I can't get Apache test framework
to send proxy requests to the proxy (i.e. GET http://master/ HTTP/1.1).

"test floodcomb" should work but I currently have strange problems where
t/TEST thinks startup of servers fails whereas it is actually started.
Problem occurs intermittently, but frequent.

Arno, 12-1-2005
