# ---- Build stage ----
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y g++ make && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY server.cpp ./

RUN g++ -std=c++17 -O2 -pthread -o backend_server server.cpp -pthread

# ---- Runtime stage ----
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/backend_server ./backend_server

# Default port; override with ENV or CMD
ARG BACKEND_PORT=9001
ENV BACKEND_PORT=${BACKEND_PORT}
EXPOSE ${BACKEND_PORT}

CMD ["sh", "-c", "./backend_server ${BACKEND_PORT}"]
