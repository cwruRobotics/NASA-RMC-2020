cmake_minimum_required(VERSION 3.0.2)
project(randomize_arena)

find_package(catkin REQUIRED COMPONENTS
  rospy
)

catkin_package(
  CATKIN_DEPENDS 
    rospy
)

catkin_python_setup()

include_directories(
  ${catkin_INCLUDE_DIRS}
)
