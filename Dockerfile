FROM alpine as builder

RUN apk add --no-cache make gcc musl-dev

WORKDIR /microktc

COPY *.c .
COPY *.h .
COPY *.asm .
COPY *.awk .
COPY Makefile .

RUN make microktc

FROM alpine
RUN apk add --no-cache gcc
COPY --from=builder /microktc /usr/local/bin/
RUN mkdir -p  /usr/local/share/microktc/ && echo 'print("hello, world!")' > /usr/local/share/microktc/hello_world.kts

CMD ["microktc"]
