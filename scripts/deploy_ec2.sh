#!/usr/bin/env bash
# -------------------------------------------------------
# AWS EC2 Deployment Script for the C++ HTTP Load Balancer
# Run this on a fresh Ubuntu 22.04 EC2 t2.micro instance
# -------------------------------------------------------
set -euo pipefail

REPO_URL=https://github.com/newgencoder1/http-load-balancer # <-- change this
APP_DIR="$HOME/load-balancer"

echo "=== [1/5] Updating system packages ==="
sudo apt-get update -y
sudo apt-get upgrade -y

echo "=== [2/5] Installing Docker ==="
sudo apt-get install -y ca-certificates curl gnupg lsb-release
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
    | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
  https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" \
  | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt-get update -y
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin

# Add current user to docker group so we don't need sudo
sudo usermod -aG docker "$USER"
echo "NOTE: You may need to log out and back in for docker group to take effect."

echo "=== [3/5] Installing Docker Compose standalone ==="
COMPOSE_VERSION="v2.24.0"
sudo curl -SL \
    "https://github.com/docker/compose/releases/download/${COMPOSE_VERSION}/docker-compose-linux-x86_64" \
    -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
docker-compose --version

echo "=== [4/5] Cloning repository ==="
if [ -d "$APP_DIR" ]; then
    echo "Repo already exists, pulling latest..."
    cd "$APP_DIR" && git pull
else
    git clone "$REPO_URL" "$APP_DIR"
    cd "$APP_DIR"
fi

echo "=== [5/5] Building and starting with Docker Compose ==="
cd "$APP_DIR"
docker-compose down 2>/dev/null || true
docker-compose up --build -d

echo ""
echo "==================================================="
PUBLIC_IP=$(curl -s http://169.254.169.254/latest/meta-data/public-ipv4 || echo "<your-ec2-public-ip>")
echo "  Load Balancer is running!"
echo "  Test: curl http://${PUBLIC_IP}:8080/"
echo "  Stats: curl http://${PUBLIC_IP}:8080/stats"
echo "==================================================="
