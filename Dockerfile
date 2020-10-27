FROM alpine as builder

RUN apk add --no-cache make gcc musl-dev

WORKDIR /microktc

COPY *.c .
COPY *.h .
COPY Makefile .

RUN make

FROM alpine
RUN apk add --no-cache gcc
COPY --from=builder /microktc /usr/local/bin/
