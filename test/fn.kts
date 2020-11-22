fun zero_stmt_no_args() {}

fun one_stmt_no_args() {
  println("one_stmt_no_args")
}

println("main") // expect: main

one_stmt_no_args() // expect: one_stmt_no_args
