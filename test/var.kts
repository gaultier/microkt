// 1 var
val a : Long = 10

println(a)

println(a * a )

// 2 vars of the same type

val b: Long = 500

println(b)
println(b+a)


// Sub scope referring to a var in the parent scope
if (true)
{
  println(a+1)
}

// Sub scope referring to a var in the parent scope and its current variables
if (true)
{
  val c: Long = 999
  if (false) {
    val d: Long = 2 // This variable should not interfere
  } else {
    val d: Long = 99
    println(a+b+c+d)
  }
}

// Initialize a variable with another variable
val x : Long = 2 * a
println(x)

// Conditionally initialize
val p: Long = if(x==20) a - 5 else b
println(p)


// Other types

val s: Short = 12345
println(s)

val i : Int = 2000000000
println(i)

val b: Byte = 8
println(b)

val ch :Char = '%'
println(ch)


val s : String = "hello"
println(s)

val bool : Boolean = true
println(bool)


var myFirstVar: Long = 123
println(myFirstVar)
