#!/usr/bin/awk
BEGIN{print("#pragma once"); printf("const char stdlib[] = ")}
{printf("\"%s\\n\"\n", $0) }
END{print(";")}'
