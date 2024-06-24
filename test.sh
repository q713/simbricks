#!/bin/bash

make -j20 docker-images

make -j20 docker-images-debug

docker compose up --build --detach