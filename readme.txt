This project is concerned with the implementation of a flexible cache and memory hierarchy simulator and its usage in comparing the performance, area, and energy of different memory hierarchy configurations, using a subset of the SPEC-2000 benchmark suite. Since this is just a simulator, no actual data is stored. Instead, the addresses are mapped to specific locations in the caches and coherence is maintained across the two levels (L1 and L2).

The writeback policy used here is Write back - Write Allocate, meaning dirty blocks, on replacement are written back to the previous level of memory (either L2 cache or the memory). The replacement policy for the cache block is LRU (Least Recently Used). The user is provided with an option to include a Victim Cache. Even the inclusion of an L2 cache is a choice left to the user.

The simulator reads a trace file in the following format:
r|w <hex address> 
r|w <hex address> ...
“r” (read) indicates a load and “w” (write) indicates a store from the processor.
Example:
r ffe04540 
r ffe04544 
w 0eff2340 
r ffe04548


The inputs to this simulator have the following format:

sim_cache <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <VC_NUM_BLOCKS> <L2_SIZE> <L2_ASSOC> <trace_file>

where the trace-file is a text file that contains the traces.

For the detailed specification of the project, please see the pdf titled 'proj1-f15-v1.0'.
