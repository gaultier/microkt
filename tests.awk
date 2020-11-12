#!/usr/bin/awk -f 

BEGIN {
    tests[0] = "e2e/integers"
    expected_outputs[0] = "1\n9990\n"

    tests[1] = "e2e/math_integers"
    expected_outputs[1] = "6\n-2\n12\n200\n4\n"

    tests[2] = "e2e/char"
    expected_outputs[2] = "A\n$\n"

    tests[3] = "e2e/string"
    expected_outputs[3] = "hello!\nworld\n"

    tests[4] = "e2e/hello_world"
    expected_outputs[4] = "hello, world!\n"

    tests[5] = "e2e/bool"
    expected_outputs[5] = "true\nfalse\nfalse\n"

    tests[6] = "e2e/comparison"
    expected_outputs[6] = "true\ntrue\nfalse\ntrue\nfalse\nfalse\ntrue\ntrue\nfalse\ntrue\n"

    tests[7] = "e2e/negation"
    expected_outputs[7] = "false\ntrue\n"

    tests[8] = "e2e/if"
    expected_outputs[8] = "42\n99\nfalse\ntrue\nA\nB\nhello\nworld\n"

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
            print WHITE "Expected output below:\n" RESET GREY expected_output RESET

            ret = 1
        } else {
            print GREEN "✔ " test RESET
        }
        close(output_file)
    }    
    exit(ret)
}
