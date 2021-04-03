# Cross Compile Instructions

1. Download docker desktop from here (https://www.docker.com/products/docker-desktop) for windows (WSL 2) and OSX.  If you're using linux, I'm sure theres a way to run docker images too.  (following the directions here should work fine: https://docs.docker.com/engine/install/ubuntu/)

2. Run `chmod +x {filename}` on both scripts in this folder

3. Run the setup script from this directory (make sure you run this as root (`sudo`))

4. change directory to your catkin workspace (such as `catkin_ws` or similar). 

5. Run `catkin clean`

6. Run the cross compile command: `{workspace_name}/src/NASA-RMC-2020/hwctrl/cross_compile.sh .` (The dot at the end is important). IT WILL NOT WORK UNLESS YOU RUN IT FROM THE WORKSPACE ROOT! This will take a while on the first run and will take a few GB of space to download the docker image. After that, the build takes around 60 seconds on my computer with 7 cores allocated to docker.

7. Use `scp` to transfer binary to the beaglebone. Eventually I will make a script for this. For now, do it manually. 

#### Note:
Sometimes theres issues with shared libraries and such on the beaglebone. Usually you just have to simlink some binaries due to differing versions.

### Links:
https://github.com/ros-tooling/cross_compile
