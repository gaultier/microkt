class Empty{}
class Person{
  var id: Long = 99
  var age: Byte = 0
}

fun main(){ 
  println(Empty()) // expect: Instance of size 0
  println(Empty()) // expect: Instance of size 0
  val e : Empty = Empty()
  println(e) // expect: Instance of size 0

  println(Person()) // expect: Instance of size 25


  var p : Person = Person()
  p.id = 100L
  //println(p.id) // expect: 100
  //p.id = p.id + 1L
  //println(p.id) // expect: 101

  //p.id = p.id
  
}
