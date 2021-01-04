class Empty{}

class Person {
  var id: Long = 0
  var name: String = ""
}

fun main(){ 
  println(Empty()) // expect: Instance of size 0
  println(Empty()) // expect: Instance of size 0
  println(Person()) // expect: Instance of size 16
}
