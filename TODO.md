* GC:
  - Detect OOM & trigger GC/stop/compact/realloc?
  - Maintain hash table of allocated addresses to scan the stack in O(1)
  - Trigger alloc when reaching 80% of occupation
  - Dtrace probes/scripts
  - Custom allocator based on mmap

* Respect the System V ABI in regards to caller/callee preserved registers
* Generate synthetic ast nodes for calls to mkt runtime functions
* Floats
* Hex numbers
* Binary numbers
* Octal numbers
* Class padding
* Class methods
* Class constructor
