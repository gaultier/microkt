#!/usr/sbin/dtrace -s

struct runtime_val_header {
    size_t rv_size : 54;
    unsigned int rv_color : 2;
    unsigned int rv_tag : 8;
};

struct alloc_atom {
    struct alloc_atom* aa_next;
    struct runtime_val_header aa_header;
};

pid$target::mkt_instance_make:entry {
    printf("size=%lld", arg0)
}

pid$target::mkt_instance_make:return {
    printf("ptr alloced=%p returned=%p\n",arg1, arg1-16)
}

pid$target::mkt_instance_println:entry {
    printf("ptr=%p\n", arg0)
}

mkt*:::gc_sweep-free {
    this->atom = (struct alloc_atom*) copyin(arg2, sizeof(struct alloc_atom));
    this->header = (struct runtime_val_header) this->atom->aa_header;
    this->data = stringof(copyin(arg2+16, this->header.rv_size));

    printf("round=%lld ptr=%p allocated_bytes=%lld size=%lu data=`%.*s`", arg0, arg2, arg1,(unsigned long ) this->header.rv_size, (int) this->header.rv_size, this->data);
}
