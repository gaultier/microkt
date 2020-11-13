FROM alpine as builder

RUN apk add --no-cache make gcc musl-dev

WORKDIR /microktc

COPY . .

RUN make microktc test BIN_TEST=./microktc

FROM alpine
RUN apk add --no-cache binutils
COPY --from=builder /microktc /usr/local/bin/
RUN mkdir -p  /usr/local/share/microktc/ && echo 'println("Hello, world!")' > /usr/local/share/microktc/hello_world.kts

# Sanity check
RUN microktc /usr/local/share/microktc/hello_world.kts && /usr/local/share/microktc/hello_world > /dev/null

CMD ["microktc"]
