# microktc

A WIP tiny compiler for a subset of the Kotlin language, with zero dependencies. It compiles a (very small) subset of the Kotlin language to a native executable. For now it only supports macos-x86_64 although it would not be hard to add more platforms.

```sh
# Only requires a C99 compiler and a POSIX environment
make
./e2e/hello_world
hello, world!

# Run the tests
make test
```
