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
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN rm -rf /tmp/uWebSockets && \
    git clone --recursive https://github.com/uNetworking/uWebSockets.git /tmp/uWebSockets && \
    cd /tmp/uWebSockets/uSockets && \
    sed -i 's/-flto//g' Makefile && \
    make clean && \
    make && \
    mkdir -p /app/lib && \
    cp uSockets.a /app/lib/libuSockets.a && \
    mkdir -p /app/include && \
    cp -r /tmp/uWebSockets/src /app/include/uWebSockets && \
    cp -r /tmp/uWebSockets/uSockets/src /app/include/uSockets

RUN make -f Makefile.docker clean && \
    make -f Makefile.docker build

CMD ["./main"]