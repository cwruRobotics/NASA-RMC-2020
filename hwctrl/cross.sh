#!/bin/bash

WORKSPACE=$1
DEFAULTS=${WORKSPACE}/src/NASA-RMC-2020/hwctrl/defaults.yaml

echo $WORKSPACE
echo $DEFAULTS

ros_cross_compile ${WORKSPACE} --arch armhf --os ubuntu --rosdistro melodic --colcon-defaults $DEFAULTS
