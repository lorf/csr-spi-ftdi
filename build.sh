#!/bin/sh
#
# Build using docker container image with all dependencies installed
#

BUILD_IMAGE=lorf/csr-spi-ftdi-build:ubuntu-16.04-i386

echo "Downloading build image can take some time, please be patient..."
docker pull "$BUILD_IMAGE"

echo "Starting build in a Docker contaner..."
docker run --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) -w $(pwd) -i \
    "$BUILD_IMAGE" ./build-in-docker.sh
