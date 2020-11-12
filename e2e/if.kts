println(if (true) { 42 } else { 99 })
println(if   (false) { 42 } else { 99 })

println(if   (true) { false } else { true })
println(if   (false) { false } else { true })

println(if   (true) { 'A' } else { 'B' })
println(if   (false) { 'A' } else { 'B' })

println(if   (true) { "hello" } else { "world" })
println(if   (false) { "hello" } else { "world" })

// Nested ifs + if..else-if..else with optional curlies
println( 
  if (1 <= 2) {
    if ( 99 == 98) 1000 
    else if (42 == 42) 999 // This
    else 500
  } else     0
)
