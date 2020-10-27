FROM alpine as builder

RUN apk add --no-cache make clang

WORKDIR /microktc

COPY *.{h,c} .
COPY Makefile .

RUN make

FROM alpine
COPY --from=builder /microktc /usr/local/bin/
