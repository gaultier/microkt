fun main() {
  var long : Long = 5L
long = 6L
println(long) // expect: 6

var int: Int = 99
val int_mult: Int = 2
int = int * int_mult
println(int) // expect: 198

var short : Short = 200
short = short* short
println(short) // expect: 40000

var byte : Byte = 7
val byte_mult: Byte = 2
byte = byte + byte - byte_mult * byte + byte + byte
println(byte) // expect: 14

var char : Char = 'B'
char = 'C'
println(char) // expect: C


var string : String= "hello"
string = "world"
println(string) // expect: world

}
