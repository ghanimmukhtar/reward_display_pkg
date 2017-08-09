cmake_minimum_required(VERSION 2.8.3)
project(reward_display_pkg)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  std_msgs
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread filesystem date_time)

catkin_package(
#  INCLUDE_DIRS include
#  LIBRARIES reward_display_pkg
#  CATKIN_DEPENDS roscpp rospy std_msgs
#  DEPENDS system_lib
)

include_directories(
  ${catkin_INCLUDE_DIRS}
)
include_directories(.)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

add_executable(philTest src/philTest.cpp)
target_link_libraries(philTest ${catkin_LIBRARIES} ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
