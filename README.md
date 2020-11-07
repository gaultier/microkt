# microktc

A WIP tiny compiler for a subset of the Kotlin language, with zero dependencies. It compiles a (very small) subset of the Kotlin language to a native executable. 

For now it only supports macos-x86_64 although it would not be hard to add more platforms.

```sh
# Only requires a C99 compiler and a POSIX environment (make, awk).
make

# Run the tests:
make test

# Compile a source file:
./microktc e2e/math_integers.kts

# And then run the executable:
./e2e/math_integers
```
