#!/usr/bin/awk -f 

BEGIN {
    tests[0] = "e2e/print_integers"
    expected_outputs[0] = "1\n9990\n"

    tests[1] = "e2e/math_integers"
    expected_outputs[1] = "6\n1\n"

#    tests[1] = "e2e/print_bool"
#    expected_outputs[1] = "truefalsefalse"
#
#    tests[2] = "e2e/print_string"
#    expected_outputs[2] = "hello world"
#
#    tests[3] = "e2e/hello_world"
#    expected_outputs[3] = "hello, world!"
#
#    tests[4] = "e2e/print_char"
#    expected_outputs[4] = "A$"

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
        while(1) {
           res = getline < output_file
           if (res == -1) { exit 1 }
           if (res == 0) { break }

           actual_output = actual_output $0 "\n"
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
