FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    make \
    curl \
    git \
    cmake \
    pkg-config \
    libssl-dev \
    zlib1g-dev \
    libuv1-dev \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /sockets && chmod 777 /sockets

WORKDIR /app

COPY . .

RUN mkdir -p /app/include && \
    curl -L https://raw.githubusercontent.com/simdjson/simdjson/master/singleheader/simdjson.h -o /app/include/simdjson.h && \
    curl -L https://raw.githubusercontent.com/simdjson/simdjson/master/singleheader/simdjson.cpp -o /app/include/simdjson.cpp

RUN rm -rf /tmp/uWebSockets && \
    git clone --recursive https://github.com/uNetworking/uWebSockets.git /tmp/uWebSockets && \
    cd /tmp/uWebSockets/uSockets && \
    sed -i 's/-flto//g' Makefile && \
    make clean && make && \
    mkdir -p /app/lib /app/include/uWebSockets && \
    cp uSockets.a /app/lib/libuSockets.a && \
    cp src/libusockets.h /app/include/ && \
    cp -r /tmp/uWebSockets/src/* /app/include/uWebSockets

RUN make -f Makefile.docker clean && \
    make -f Makefile.docker build

CMD ["./main"]