// 1 var
val a : Long = 10

println(a) // expect: 10

println(a * a ) // expect: 100

// 2 vars of the same type

val b: Long = 500

println(b) // expect: 500

println(b+a) // expect: 510


// Sub scope referring to a var in the parent scope
if (true)
{
  println(a+1) // expect: 11
}

// Sub scope referring to a var in the parent scope and its current variables
if (true)
{
  val c: Long = 999
  if (false) {
    val d: Long = 2 // This variable should not interfere
  } else {
    val d: Long = 99 
    println(a+b+c+d) // expect: 1608
  }
}

// Initialize a variable with another variable
val x : Long = 2 * a
println(x) // expect: 20

// Conditionally initialize
val p: Long = if(x==20) a - 5 else b
println(p) // expect: 5

// Other types
val s: Short = 12345
println(s) // expect: 12345

val i : Int = 2000000000
println(i) // expect: 2000000000

val b: Byte = 8
println(b) // expect: 8

val ch :Char = '%'
println(ch) // expect: %

val s : String = "hello"
println(s) // expect: hello

val bool : Boolean = true
println(bool) // expect: true

var myFirstVar: Long = 123
println(myFirstVar) // expect: 123
