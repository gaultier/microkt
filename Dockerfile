FROM alpine:3.12 as builder

RUN apk add --no-cache make gcc musl-dev

WORKDIR /mktc

COPY . .

RUN make check

FROM alpine:3.12
RUN apk add --no-cache binutils
COPY --from=builder /mktc/mktc /usr/local/bin/
COPY --from=builder /mktc/mkt_stdlib.o /usr/local/lib/
RUN mkdir -p  /usr/local/share/mktc/ && echo 'fun main() {println("Hello, world!")}' > /usr/local/share/mktc/hello_world.kt

# Sanity check
RUN mktc /usr/local/share/mktc/hello_world.kt && /usr/local/share/mktc/hello_world.exe | grep "Hello, world!"
