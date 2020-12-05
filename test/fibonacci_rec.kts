fun fibonacci(n: Long) : Long {
  if (n == 0L) return 0L
  if (n == 1L) return 1L

  return fibonacci(n-1L) + fibonacci(n-2L)
}

println(fibonacci(35L)) // expect: 9227465
