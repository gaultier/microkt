fun fibonacci(n: Long) : Long {
  if (n == 0) return 0
  if (n == 1) return 1

  return fibonacci(n-1) + fibonacci(n-2)
}

println(fibonacci(35)) // expect: 9227465
