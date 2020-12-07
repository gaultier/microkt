FROM alpine:3.12 as builder

RUN apk add --no-cache make clang musl-dev

WORKDIR /mktc

COPY . .

RUN make mktc test

FROM alpine:3.12
RUN apk add --no-cache binutils
COPY --from=builder /mktc /usr/local/bin/
RUN mkdir -p  /usr/local/share/mktc/ && echo 'println("Hello, world!")' > /usr/local/share/mktc/hello_world.kts

# Sanity check
RUN mktc /usr/local/share/mktc/hello_world.kts && /usr/local/share/mktc/hello_world.exe > /dev/null

CMD ["mktc"]
