#!/usr/bin/env awk -f

BEGIN{ printf("#pragma once\nconst char stdlib[] = ")}

!/^\s*\/\// {printf("\"%s\\n\"\n", $0) }

END{print(";")}