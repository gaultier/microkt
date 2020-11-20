
var a: Long = 0
var b: Long = 1

var i: Long = 1
while (i < 35) {
    val tmp: Long = b
    b = b + a
    a = tmp
    i = i + 1
}

println(b) // expect: 9227465
