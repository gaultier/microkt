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


