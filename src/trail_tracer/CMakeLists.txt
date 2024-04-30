cmake_minimum_required(VERSION 3.8)
project(trail_tracer)

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
find_package(nav2_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

add_executable(nav2 src/nav2.cpp)
add_executable(trail_tracer src/trail_tracer.cpp)
add_executable(polishedCode src/polishedCode.cpp)



target_include_directories(trail_tracer PUBLIC

  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(trail_tracer PUBLIC c_std_99 cxx_std_17)  # Require C99 and C++17

ament_target_dependencies(nav2 rclcpp std_msgs geometry_msgs sensor_msgs GeographicLib tf2_geometry_msgs)
ament_target_dependencies(trail_tracer rclcpp std_msgs geometry_msgs sensor_msgs GeographicLib tf2_geometry_msgs)
ament_target_dependencies(polishedCode rclcpp std_msgs geometry_msgs sensor_msgs GeographicLib tf2_geometry_msgs nav2_msgs tf2 tf2_ros)


install(TARGETS trail_tracer nav2 polishedCode
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
