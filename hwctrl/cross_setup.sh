#!/bin/bash

echo "Installing apt deps"
apt install docker python3 qemu-user-static -y

echo "Installing pip deps"
pip3 install ros_cross_compile

echo "Done, make sure you have docker desktop installed"
