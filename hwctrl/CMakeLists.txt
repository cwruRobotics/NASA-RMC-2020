cmake_minimum_required(VERSION 3.7.2) # this version of cmake is the max that works on the beaglebone
project(hwctrl)

if(${CMAKE_VERSION} VERSION_GREATER "3.9.0") 
    message("Setting CMAKE policy CM0069 to OLD api in order to avoid lto errors.")
    cmake_policy(SET CMP0069 OLD)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message("Setting build type to debug, since no value was provided")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-std=c++14 -O2)
else()
    add_compile_options(-std=c++14 -Wall -Wextra -pedantic -g)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  sensor_msgs
  geometry_msgs
  rospy
  message_generation
  message_runtime
  rostest
)

catkin_python_setup()

add_message_files(
  DIRECTORY msg
)

add_service_files(
  FILES 
    LED.srv
)

generate_messages(
  DEPENDENCIES
    std_msgs
    sensor_msgs
    geometry_msgs
)

catkin_package(
  INCLUDE_DIRS include
#  LIBRARIES hwctrl
  CATKIN_DEPENDS roscpp std_msgs message_runtime sensor_msgs geometry_msgs
#  DEPENDS system_lib
  CATKIN_DEPENDS message_runtime
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

add_executable(hwctrl
  src/hwctrl_node.cpp
  src/threads/sensor_thread.cpp
  src/threads/canbus_thread.cpp
  src/threads/motor_thread.cpp
  src/hardware/sensor.cpp
  src/hardware/motor.cpp
  src/hardware/quad_encoder.cpp
  src/hardware/uwb.cpp
  src/hardware/lsm6ds3.cpp
  src/hardware/ads1120.cpp
  src/hardware/adt7310.cpp
  src/hardware/estop.cpp
  src/hardware/vesc.cpp
  src/hardware/bmc.cpp
  src/interface/gpio.cpp
  src/interface/spi.cpp
  src/util.cpp
)

add_executable(hwctrl_cal
  src/hwctrl_cal_node.cpp
  src/threads/sensor_thread.cpp
  src/threads/sensor_cal_thread.cpp
  src/threads/canbus_thread.cpp
  src/threads/motor_thread.cpp
  src/hardware/sensor.cpp
  src/hardware/motor.cpp
  src/hardware/quad_encoder.cpp
  src/hardware/uwb.cpp
  src/hardware/lsm6ds3.cpp
  src/hardware/ads1120.cpp
  src/hardware/estop.cpp
  src/hardware/adt7310.cpp
  src/hardware/vesc.cpp
  src/hardware/bmc.cpp
  src/interface/gpio.cpp
  src/interface/spi.cpp
  src/util.cpp
)

add_executable(led_node src/led_node.cpp)

# if(CATKIN_ENABLE_TESTING)
  # catkin_add_gtest(hwctrl_tests test/testing.cpp src/util.cpp)
  # target_link_libraries(hwctrl_tests ${catkin_LIBRARIES})
# endif()

# we only want lto if we are in release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message("Enabling LTO")
    set_property(TARGET hwctrl PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
    set_property(TARGET hwctrl_cal PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
else()
    message("Disabling LTO")
    set_property(TARGET hwctrl PROPERTY INTERPROCEDURAL_OPTIMIZATION False)
    set_property(TARGET hwctrl_cal PROPERTY INTERPROCEDURAL_OPTIMIZATION False)
endif()

target_link_libraries(hwctrl ${catkin_LIBRARIES})
target_link_libraries(hwctrl_cal ${catkin_LIBRARIES})
target_link_libraries(led_node ${catkin_LIBRARIES})

# for hwctrl executable
add_dependencies(hwctrl ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
add_dependencies(hwctrl hwctrl_generate_messages_cpp)

# for hwctrl_cal executable
add_dependencies(hwctrl_cal ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
add_dependencies(hwctrl_cal hwctrl_generate_messages_cpp)

add_dependencies(led_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
