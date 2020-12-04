# microkt

A work-in-progress tiny compiler for a subset of the Kotlin language, with zero dependencies. It compiles a (very small) subset of the Kotlin language to a native executable.

For now it only supports x86_64 (macOS & Linux) although it would not be hard to add more platforms.

Have a look at the `test` directory to get a feeling of what's supported. Here's a sample (see the `Quickstart` section to see how to run it):

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
# Only requires a C99 compiler toolchain and a POSIX environment (make, awk).
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
docker run --rm -it microkt sh -c 'microktc /usr/local/share/microktc/hello_world.kts \
    && /usr/local/share/microktc/hello_world.exe'

Hello, world!
```

## Present features and goals

- Small, fast, and portable: the code is written in C99 with zero dependencies. It compiles under a second to an executable no bigger than 100 Kib
- Produces small native executables under 10 Kib in a few milliseconds
- Produces portable executables that don't even depend on the C standard library. Portable here means portable within the same OS and architecture but across OS versions. An executable produced today should work on a 15 year old Linux system just fine because of the stable ABI guarantees Linux offers. On macOS, we link with libSystem as recommended by Apple which guarantees some level compatibility with past and future macOS versions
- Friendly error messages
- Tiny memory usage

## On the roadmap
*Future features that are not yet implemented but are planned. In no particular order:*

- Cross-compilation
- Classes
- Floats and doubles
- Integer literal suffixes (e.g. `10L`)
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

```sh
# Debug build and run the tests
make DEBUG=1 test

# Release build and run the tests, this is the default
make DEBUG=0 test
```

## License

BSD-3
