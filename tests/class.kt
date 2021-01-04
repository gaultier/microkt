class Empty{}

class Person {
  var id: Long = 0
  var name: String = ""
}

fun main(){ 
  println(Empty()) // Expected: Instance of size 0
  println(Empty()) // Expected: Instance of size 0
}
