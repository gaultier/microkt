fun zero_stmt_no_args() {}

fun one_stmt_no_args() {
  println("one_stmt_no_args")
}

println("A") // expect: A

one_stmt_no_args() // expect: one_stmt_no_args
one_stmt_no_args() // expect: one_stmt_no_args

println("B") // expect: B

fun one_stmt_no_args1() {
  println("one_stmt_no_args1")
  one_stmt_no_args()
}

one_stmt_no_args1() // expect: one_stmt_no_args1
// expect: one_stmt_no_args

