fun main() {
  println("hello!") // expect: hello!
println("world") // expect: world

println("""The quick brown 
fox jumps 
     over the 
lazy 
  dog""")
// expect: The quick brown 
// expect: fox jumps 
// expect:      over the 
// expect: lazy 
// expect:   dog

println("Luke I am your fathe" + "r") // expect: Luke I am your father

val a: String = "May the "
val b: String = "Force be with you"
val ab: String = a + b
println(ab) // expect: May the Force be with you
}
