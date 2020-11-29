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


fun return_long(): Long {
  return 42
}

println(return_long()) // expect: 42

fun return_true(): Boolean {
  return true
}
fun return_false(): Boolean {
  return false
}

println(return_false()) // expect: false
println(return_true()) // expect: true

fun return_other_func(): Long {
  return return_long()
}

println(return_other_func()) // expect: 42



fun empty_return() {
  1
  return
}
empty_return()


fun declared_unit_and_last_stmt_is_expr(): Unit { 1 }

fun return_string(): String {
  var a : String =  "return_string"

  return a
}
println(return_string()) // expect: return_string


// Parameters
fun one_param_unused(a: Long) {}

one_param_unused(100)


fun one_param(a: Long) { println(a) }
