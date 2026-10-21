# ---- Build stage ----
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY src/       ./src/
COPY Makefile   ./

RUN make lb

# ---- Runtime stage ----
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/bin/load_balancer ./load_balancer
COPY config.json ./

EXPOSE 8080

CMD ["./load_balancer", "--config", "config.json"]
