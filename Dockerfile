FROM alpine as builder

RUN apk add --no-cache make clang musl-dev

WORKDIR /microktc

COPY *.c .
COPY *.h .
COPY Makefile .

RUN make CC=clang LD=ldd

# FROM alpine
# COPY --from=builder /microktc /usr/local/bin/
