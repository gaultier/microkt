# microkt

A work-in-progress tiny compiler for a subset of the Kotlin language, with zero dependencies. It compiles a (very small) subset of the Kotlin language to a native executable.

For now it only supports x86_64 (macOS & Linux) although it would not be hard to add more platforms.

Have a look at the `test` directory to get a feeling of what's supported.

## Features

- Small, fast, and portable: the code is written in C99 with zero dependencies. It compiles under a second to an executable no bigger than 100 Kib
- Produces small native executables under 10 Kib: while the generated machine code is not optimized, they are fast by virtue of being native executables
- Produces portable executables that don't even depend on the C standard library. Portable here means portable within the same OS and architecture but across OS versions. An executable produced today should work on a 15 year old Linux system just fine because of the stable ABI guarantees Linux offers. On macOS, we link with libSystem as recommended by Apple which guarantees some level compatibility with past and future macOS versions
- Friendly error messages
- Tiny memory usage

## Non features

Things that we do not have today. Maybe we will have them in the future, and maybe not. PRs welcome, I guess.

In no particular order:

- Support for the full Kotlin language
- Production grade quality. There will be bugs; use at your own risk
- The Kotlin standard library (we only support `println` at the moment)
- Language Server Protocol (LSP)
- Source code formatter
- Support for other architectures e.g. ARM or RISC
- 32 bits support
- Windows/FreeBSD/NetBSD/OpenBSD support
- Linter
- Compiling to JVM bytecode
- Compiling to JS
- Compiling to WebAssembly

## Quickstart


```sh
# Only requires a C99 compiler and a POSIX environment (make, awk).
make

# Run the tests
make test

# Compile a source file (requires `as` and `ld` in the PATH)
./microktc test/hello_world.kts

# And then run the executable
./test/hello_world.exe
Hello, world!

# Also works in Docker
docker build -t microkt .
docker run -it microkt sh -c 'microktc /usr/local/share/microktc/hello_world.kts \
    && /usr/local/share/microktc/hello_world.exe'

Hello, world!
```


## Develop

```
make DEBUG=1
```

## License

BSD-3
