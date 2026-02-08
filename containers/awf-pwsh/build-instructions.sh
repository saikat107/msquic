#!/bin/bash
set -e

mkdir -p awf-build && cd awf-build

BASE_URL="https://raw.githubusercontent.com/github/gh-aw-firewall/main/containers/agent"
curl -fsSLO "$BASE_URL/Dockerfile"
curl -fsSLO "$BASE_URL/setup-iptables.sh"
curl -fsSLO "$BASE_URL/entrypoint.sh"
curl -fsSLO "$BASE_URL/pid-logger.sh"
curl -fsSLO "$BASE_URL/docker-stub.sh"

docker build \
  --build-arg BASE_IMAGE=ghcr.io/catthehacker/ubuntu:runner-22.04 \
  -t ghcr.io/saikat107/awf-pwsh:v0.13.12 \
  .

echo $GITHUB_TOKEN | docker login ghcr.io -u saikat107 --password-stdin
docker push ghcr.io/saikat107/awf-pwsh:v0.13.12
