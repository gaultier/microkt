# microktc

A WIP tiny compiler for a subset of the Kotlin language, with zero dependencies. It compiles a (very small) subset of the Kotlin language to a native executable. 

For now it only supports x86_64 (macOs & Linux) although it would not be hard to add more platforms.

```sh
# Only requires a C99 compiler and a POSIX environment (make, awk).
make

# Run the tests:
make test

# Compile a source file:
./microktc e2e/hello_world.kts

# And then run the executable:
./e2e/hello_world
hello, world!

# Docker
docker build -t microktc .
docker run -it microktc sh -c 'microktc /usr/local/share/microktc/math.kts && /usr/local/share/microktc/math'
-210

```
