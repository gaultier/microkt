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
RUN mkdir -p  /usr/local/share/microktc/ && echo 'print(123 + 456 - 789)' > /usr/local/share/microktc/math.kts

CMD ["microktc"]
