#!/usr/bin/awk -f 

BEGIN {
    tests[0] = "e2e/print_bool"
    expected_outputs[0] = "truefalsefalse"

    tests[1] = "e2e/print_string"
    expected_outputs[1] = "hello world"

    tests[2] = "e2e/print_integers"
    expected_outputs[2] = "19990"

    tests[3] = "e2e/hello_world"
    expected_outputs[3] = "hello, world!"

    tests[4] = "e2e/print_char"
    expected_outputs[4] = "A"

    is_tty = system("test -t 1") == 0

    if (is_tty) {
        RED = "\x1b[31m"
        GREEN = "\x1b[32m"
        GREY = "\x1b[90m"
        WHITE = "\x1b[37m"
        RESET = "\x1b[0m"
    } else {
        RED = ""
        GREEN = ""
        GREY = ""
        WHITE = ""
        RESET = ""
    }

    ret = 0

    for (i=0; i<length(tests); i++) {
        test = tests[i]
        expected_output = expected_outputs[i]

        output_file = "/tmp/actual_output"
        exit_code = system("./" test " > " output_file)
        
        actual_output = ""
        o = ""
        while (1) {
            res == getline o < output_file
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
        close(output_file)
    }    
    exit(ret)
}
