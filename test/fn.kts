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


var outer_bool: Boolean = false

fun print_outer_inner_bool(){
  var inner_bool: Boolean = true
  println(inner_bool) 
  println(outer_bool) 
}

print_outer_inner_bool()
// expect: true
// expect: false

//var outer_long: Long = 123
//
//fun print_outer_inner_long(){
//  var inner_long: Long = 456
//  println(inner_long) 
//  println(outer_long) 
//}
//
//print_outer_inner_long()
