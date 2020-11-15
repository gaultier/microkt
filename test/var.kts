// 1 var
val a : Long = 10

println(a)

println(a * a )

// 2 vars of the same type

val b: Long = 500

println(b)
println(b+a)


// Sub scope referring to a var in the parent scope
{
  println(a+1)
}
