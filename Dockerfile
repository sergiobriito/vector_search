FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    make \
    libgcc-s1 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN make build
RUN chmod +x main

ENV PORT=8080
EXPOSE 8080

CMD ["./main"]