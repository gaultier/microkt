# microkt

A work-in-progress compiler for a subset of the Kotlin language, with zero dependencies. It compiles a (very small) subset of the Kotlin language to a native executable.

For now it only supports x86_64 (macOS & Linux) although it would not be hard to add more platforms.

Have a look at the `tests` directory to get a feeling of what's supported. Here's a sample (see the `Quickstart` section to see how to run it):

```kotlin
// Compute the 35th fibonacci number iteratively
var a: Long = 0
var b: Long = 1
var i: Long = 1

while (i < 35) {
    val tmp: Long = b
    b = b + a
    a = tmp
    i = i + 1
}
println(b) // expect: 9227465

// Compute the 35th fibonacci number recursively
fun fibonacci(n: Long) : Long {
  if (n == 0) return 0
  if (n == 1) return 1

  return fibonacci(n-1) + fibonacci(n-2)
}
println(fibonacci(35)) // expect: 9227465
```

## Quickstart

```sh
# Only requires a C99 compiler toolchain and GNU make
make

# Run the tests
make check

# Compile a source file (requires `as` and `ld` in the PATH)
./mktc tests/hello_world.kts

# And then run the executable
./tests/hello_world.exe
Hello, world!

# Also works in Docker
docker build -t microkt .
docker run --rm -it microkt sh -c 'mktc /usr/local/share/mktc/hello_world.kts \
    && /usr/local/share/mktc/hello_world.exe'

Hello, world!
```

## Features

- Small, fast, and portable: the code is written in C99 with zero dependencies. It compiles under a second to an executable no bigger than 100 Kib
- Produces small native executables under 10 Kib in a few milliseconds
- Friendly error messages
- Tiny memory usage
- Simple mark and sweep garbage collector

## On the roadmap
*Future features that are not yet implemented but are planned. In no particular order:*

- Cross-compilation
- Classes
- Floats and doubles
- Type inference
- Out-of-order declarations (e.g. use before declare)

## Non features

*Things that we do not have today. Maybe we will have them in the future, and maybe not. PRs welcome, I guess. In no particular order:*

- Support for the full Kotlin language
- Production grade quality. There will be bugs; use at your own risk
- The Kotlin standard library (we only support `println` at the moment)
- Language Server Protocol (LSP)
- Source code formatter
- Support for other architectures e.g. ARM or RISC
- 32 bits support
- Windows/FreeBSD/NetBSD/OpenBSD/Illumos support (although Linux executables should work just fine on FreeBSD, NetBSD and Illumos; Windows has WSL)
- Linter
- Compile to JVM bytecode
- Compile to JS
- Compile to WebAssembly
- Highly optimized generated assembler instructions a la LLVM/GCC


## Develop

Available Makefile flags:
- WITH_LOGS=0|1 : disable/enable logs. Defaults to 0.
- WITH_ASAN=0|1 : disable/enable AddressSanitizer (`clang` only). Defaults to 0.
- WITH_OPTIMIZE=0|1 : optimization level. Corresponds to respectively -O0 and -O2
- WITH_DTRACE=0|1 : disable/enable dtrace in generated executables. Defaults to 0 on Linux and 1 on other platforms.
- CC, AS, LD: standard make variables

```sh
# Debug build with logs and asan, using clang
make WITH_OPTIMIZE=0 WITH_LOGS=1 WITH_ASAN=1 CC=clang

# Release build without logs with asan using gcc and tell `mktc` to use `lld` as linker
make WITH_ASAN=1 CC=gcc LD=ld64.lld
```

## License

BSD-3
