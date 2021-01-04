class Empty{}

class Person {
  var id: Long = 99
  var name: String = "joe"
}

fun main(){ 
  println(Empty()) // expect: Instance of size 0
  println(Empty()) // expect: Instance of size 0

  println(Person()) // expect: Instance of size 16

  // WIP
  // println(Person().id)
}
