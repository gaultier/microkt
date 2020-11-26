fun zero_stmt_no_args() {}

fun one_stmt_no_args() {
  println("one_stmt_no_args")
}

println("A") // expect: A

one_stmt_no_args() // expect: one_stmt_no_args
one_stmt_no_args() // expect: one_stmt_no_args

println("B") // expect: B

fun call_other_func() {
  println("call_other_func")
  one_stmt_no_args()
}

call_other_func() // expect: call_other_func
// expect: one_stmt_no_args


fun func_with_var() {
  var local: Long = 5
  println(local)
}

println("C") // expect: C
func_with_var() // expect: 5


fun return_long() {
  return 42
}

println(return_long()) // expect: 42

fun return_true() {
  return true
}
fun return_false() {
  return false
}

println(return_false()) // expect: false
println(return_true()) // expect: true
