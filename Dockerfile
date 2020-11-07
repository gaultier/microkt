FROM alpine as builder

RUN apk add --no-cache make gcc musl-dev

WORKDIR /microktc

COPY . .

RUN make microktc test BIN_TEST=./microktc

FROM alpine
RUN apk add --no-cache gcc
COPY --from=builder /microktc /usr/local/bin/
RUN mkdir -p  /usr/local/share/microktc/ && echo 'println(123 + 456 - 789)' > /usr/local/share/microktc/math.kts

CMD ["microktc"]
