# Dockerized Multi-Threaded HTTP Load Balancer

A production-inspired HTTP Load Balancer built from scratch in C++ using POSIX sockets and multithreading. The load balancer distributes incoming client requests across multiple backend servers, continuously monitors backend health, performs automatic failover, and can be deployed locally using Docker or on AWS EC2.

---

## Features

* Multi-threaded HTTP Load Balancer written in C++
* Reverse proxy architecture
* Round Robin load balancing
* Least Connections load balancing
* Automatic backend health monitoring
* Retry-based failure detection and recovery
* Automatic failover and backend restoration
* Backend latency tracking
* Real-time `/stats` endpoint
* Dockerized deployment
* Docker Compose orchestration
* AWS EC2 deployment ready

---

## Architecture

```text
             Clients
                |
                v
      +------------------+
      |  Load Balancer   |
      |      :8080       |
      +------------------+
          /     |     \
         /      |      \
        v       v       v
    Backend1 Backend2 Backend3
      :9001    :9002    :9003

      Health Checker Thread
             |
             v
   Continuous Health Monitoring
```

---

## Tech Stack

### Systems Programming

* C++17
* POSIX Sockets
* Linux Networking APIs
* Multithreading (`std::thread`)
* Synchronization (`std::mutex`)

### Networking

* TCP/IP
* HTTP/1.1
* Reverse Proxy
* Load Balancing Algorithms

### DevOps & Cloud

* Docker
* Docker Compose
* AWS EC2
* Makefile

---

## Load Balancing Algorithms

### Round Robin

Requests are distributed cyclically across all healthy backend servers.

```text
Req1 → Backend1
Req2 → Backend2
Req3 → Backend3
Req4 → Backend1
```

### Least Connections

Requests are routed to the backend currently handling the fewest active connections.

```text
Backend1 : 4 connections
Backend2 : 2 connections
Backend3 : 6 connections

New Request → Backend2
```

---

## Health Checking & Failover

A dedicated background thread periodically checks the health of every backend server.

### Failure Detection

* Health checks run every 5 seconds
* Consecutive failures are tracked
* Backend removed from routing pool after repeated failures

### Recovery

* Recovered backend automatically rejoins routing pool
* No manual intervention required

### Metrics Collected

* Backend status
* Active connections
* Health state
* Response latency

---

## Project Structure

```text
load-balancer/
├── src/
│   ├── main.cpp
│   ├── load_balancer.cpp
│   ├── load_balancer.h
│   ├── backend_pool.cpp
│   ├── backend_pool.h
│   ├── health_checker.cpp
│   ├── health_checker.h
│   ├── connection_handler.cpp
│   ├── connection_handler.h
│   ├── http_parser.cpp
│   └── http_parser.h
│
├── backend/
│   ├── server.cpp
│   └── Dockerfile
│
├── Dockerfile
├── docker-compose.yml
├── config.json
├── Makefile
└── README.md
```

---

## Build

```bash
make all
```

---

## Run Locally

Start backend servers:

```bash
./bin/backend_server 9001 &
./bin/backend_server 9002 &
./bin/backend_server 9003 &
```

Start the load balancer:

```bash
./bin/load_balancer --config config.json
```

Test:

```bash
curl http://localhost:8080/
```

---

## Docker Deployment

Build and run:

```bash
docker compose up --build
```

Verify:

```bash
docker ps
```

Test:

```bash
curl http://localhost:8080/
```

---

## AWS EC2 Deployment (Deployment Guide)

The project is designed to be deployable on AWS EC2 using Docker and Docker Compose.

### 1. Launch an EC2 Instance

* Ubuntu 22.04 LTS
* t2.micro (Free Tier eligible)
* Configure a Security Group with:

```text
22    SSH
8080  TCP
```

### 2. Connect to the Instance

```bash
ssh -i <key.pem> ubuntu@<EC2_PUBLIC_IP>
```

### 3. Install Docker

```bash
sudo apt update

sudo apt install docker.io -y

sudo systemctl enable docker
sudo systemctl start docker
```

### 4. Install Docker Compose

```bash
sudo apt install docker-compose-plugin -y
```

Verify:

```bash
docker compose version
```

### 5. Clone the Repository

```bash
git clone <repository-url>

cd load-balancer
```

### 6. Start the Services

```bash
docker compose up -d --build
```

Verify containers:

```bash
docker ps
```

### 7. Access the Load Balancer

```text
http://<EC2_PUBLIC_IP>:8080
```

### Notes

These steps have been documented to demonstrate cloud deployment readiness. Actual deployment has not been performed as part of the project.

---

## Monitoring

View runtime statistics:

```bash
curl http://localhost:8080/stats
```

Example metrics:

* Backend health status
* Active connections
* Backend latency
* Request routing information

---

## Benchmarking

Example Apache Bench test:

```bash
ab -n 5000 -c 50 http://localhost:8080/
```

| Metric          | Result |
| --------------- | ------ |
| Requests        | 5000   |
| Concurrency     | 50     |
| Requests/sec    | TBD    |
| Average Latency | TBD    |

---

## Key Engineering Challenges Solved

* Concurrent request handling using multithreading
* Reverse proxy request forwarding
* Dynamic backend selection
* Health monitoring and failover
* Thread-safe backend state management
* Containerized multi-service deployment

---

## Future Improvements

* Thread Pool Architecture
* epoll-based Event Loop
* HTTPS/TLS Termination
* Sticky Sessions
* Rate Limiting
* Prometheus Metrics Export
* GitHub Actions CI/CD
* Backend Connection Pooling

---

## Screenshots

### Server Port 1 Listening
<img width="376" height="67" alt="image" src="https://github.com/user-attachments/assets/01a48fea-f86d-499d-90ea-daf67d2b8712" />

### Health Checker Tracking 3 server ports and observing Server 2 has failed
<img width="496" height="527" alt="image" src="https://github.com/user-attachments/assets/66827992-3eb6-44ba-86cf-752c5245f75f" />


### Load Balancer Logging Servers
<img width="450" height="425" alt="image" src="https://github.com/user-attachments/assets/06bb3899-c7dd-4314-8e94-f9c81756ef7b" />

