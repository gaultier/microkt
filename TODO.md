* GC:
  - Detect OOM & trigger GC/stop/compact/realloc?
  - Maintain hash table of allocated addresses to scan the stack in O(1)
  - Trigger alloc when reaching 80% of occupation
  - Dtrace probes/scripts
  - Custom allocator based on mmap pages

* Floats
* Hex numbers
* Binary numbers
* Octal numbers
* Class
