cmake_minimum_required(VERSION 3.8)
project(biy1_ros360_cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
list(APPEND CMAKE_MODULE_PATH "/usr/share/cmake/geographiclib")
find_package(GeographicLib REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

add_executable(helloWorld src/helloWorld.cpp)
add_executable(examplePublisher src/examplePublisher.cpp)
add_executable(exampleSubscriber src/exampleSubscriber.cpp)
add_executable(testCode src/testCode.cpp)
add_executable(nav2 src/nav2.cpp)
add_executable(trail_tracer src/trail_tracer.cpp)
add_executable(waypoint_navigator src/waypoint_navigator.cpp)

target_include_directories(helloWorld PUBLIC

  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(helloWorld PUBLIC c_std_99 cxx_std_17)  # Require C99 and C++17

ament_target_dependencies(examplePublisher rclcpp std_msgs)
ament_target_dependencies(exampleSubscriber rclcpp std_msgs)
ament_target_dependencies(testCode rclcpp std_msgs geometry_msgs sensor_msgs) 
ament_target_dependencies(helloWorld rclcpp std_msgs geometry_msgs sensor_msgs GeographicLib tf2_geometry_msgs)
ament_target_dependencies(nav2 rclcpp std_msgs geometry_msgs sensor_msgs GeographicLib tf2_geometry_msgs)
ament_target_dependencies(trail_tracer rclcpp std_msgs geometry_msgs sensor_msgs GeographicLib tf2_geometry_msgs)
ament_target_dependencies(waypoint_navigator rclcpp std_msgs geometry_msgs sensor_msgs GeographicLib tf2_geometry_msgs)

install(TARGETS helloWorld examplePublisher exampleSubscriber testCode nav2 trail_tracer waypoint_navigator
  DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
