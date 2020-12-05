// 1 var
val a : Long = 10L

println(a) // expect: 10

println(a * a ) // expect: 100

// 2 vars of the same type

val long: Long = 500L

println(long) // expect: 500

println(long+a) // expect: 510


// Sub scope referring to a var in the parent scope
if (true)
{
  println(a+1L) // expect: 11
}

// Sub scope referring to a var in the parent scope and its current variables
if (true)
{
  val c: Long = 999L
  if (false) {
    val d: Long = 2L // This variable should not interfere
  } else {
    val d: Long = 99L 
    println(a+long+c+d) // expect: 1608
  }
}

// Initialize a variable with another variable
val x : Long = 2L * a
println(x) // expect: 20

// Conditionally initialize
val p: Long = if(x==20L) a - 5L else long
println(p) // expect: 5

// Other types
val short: Short = 12345
println(short) // expect: 12345

val i : Int = 2000000000
println(i) // expect: 2000000000

val byte: Byte = 8
println(byte) // expect: 8

val ch :Char = '%'
println(ch) // expect: %

val string : String = "hello"
println(string) // expect: hello

val bool : Boolean = true
println(bool) // expect: true

var myFirstVar: Long = 123
println(myFirstVar) // expect: 123
