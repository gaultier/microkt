provider mkt {
    probe gc_sweep__start(long long gc_round, long long gc_allocated_bytes);
    probe gc_sweep__free(long long gc_round, long long gc_allocated_bytes, struct alloc_atom* to_free);
    probe gc_sweep__done(long long gc_round, long long gc_allocated_bytes);

    probe gc_obj__mark(long long gc_round, long long gc_allocated_bytes);
};
