fun main() {
  var a: Long = 0L
while (a<5L) {
  println(a)
  a = a + 1L
}
println(2L*a)
// expect: 0
// expect: 1
// expect: 2
// expect: 3
// expect: 4
// expect: 10
}
