provider mkt {
    probe gc_sweep__start(long long gc_allocated_bytes);
    probe gc_sweep__free(long long gc_allocated_bytes, void* to_free_ptr, long long to_free_size, void* data);
    probe gc_sweep__done(long long gc_allocated_bytes);
};
