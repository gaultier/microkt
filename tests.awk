#!/usr/bin/awk -f 

BEGIN {
    tests[0] = "e2e/print_bool"
    tests[1] = "e2e/print_string"
    tests[2] = "e2e/print_integers"
    tests[3] = "e2e/hello_world"

    // TODO: extract expected outputs from the source file comments?
    outputs[0] = "truefalsefalse"
    outputs[1] = "hello world"
    outputs[2] = "1999"
    outputs[3] = "hello, world!"

    RED = "\x1b[31m"
    GREEN = "\x1b[32m"
    GREY = "\x1b[90m"
    WHITE = "\x1b[37m"
    RESET = "\x1b[0m"

    ret = 0

    for (i=0; i<length(tests); i++) {
        test = tests[i]
        expected_output = outputs[i]

        infile = "/tmp/actual_output"
        exit_code = system("./" test " > " infile)
        
        actual_output = ""
        o = ""
        while (1) {
            res == getline o < infile
            actual_output = actual_output o
            if (res != 1) { break }
        }

        if (exit_code != 0 || actual_output != expected_output) {
            print RED "✘ " test RESET
            print WHITE "Actual output below:\n" RESET GREY actual_output RESET

            ret = 1
        } else {
            print GREEN "✔ " test RESET
        }
        close(infile)
    }    
    exit(ret)
}
