BJ Wu <scu.bjwu@gmail.com>
------------------------------------
THe propose of this memory pool is:

*Use memory efficiently under resource limited system
*Reduce memory fragment
*Track memory leak and using condition

To build the memory pool, run 'make lib' to get the share lib or 'make test' to generate testing program. In addition, you could run 'make proto' to generate proto.h file contains all the function daclarations. I steal this from Samba project:) 

File test.c is a simple example to show how to play with this memory pool lib. Of course, it is also the testing program that the memory pool lib is tested through by Sanity test, Stress test, Multiple Threads test and Simulation test.

Link data structure is used for this memory pool. Two chains (high cache pool and normal pool) are created for data allocation. High cache pool is limited with 50 chunks and maintains only the most frequently used chunk. Normal pool maintains the chunks rejected by high cache pool and ranks chunks based on their size. To improve performance, the current pointer of each chain is always located around the latest allocated chunk. I tried other data structure like splay tree for the memory pool, but memory pool based on link data structure is the winner.

All memory space allocated from the mempool is aligned to reduce memory fragment. Moreover, chunks in normal pool may be merged into a bigger one after every free action. Anyway, allocation could sitll fails when there has enough but uncontinuous chunks in memory pool. Memory fragment is really a pain in my ass....

You could find more usage details from the file MEMPool.MANUAL.

Please send me emails if you have any fancy idea for this ugly memory pool program. Have fun:)

Cheers,
BJ Wu

