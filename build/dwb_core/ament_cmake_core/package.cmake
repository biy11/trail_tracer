set(_AMENT_PACKAGE_NAME "dwb_core")
set(dwb_core_VERSION "1.1.13")
set(dwb_core_MAINTAINER "Carl Delsey <carl.r.delsey@intel.com>")
set(dwb_core_BUILD_DEPENDS "nav2_common" "rclcpp" "std_msgs" "geometry_msgs" "nav_2d_msgs" "dwb_msgs" "nav2_costmap_2d" "pluginlib" "sensor_msgs" "visualization_msgs" "nav_2d_utils" "nav_msgs" "tf2_ros" "nav2_util" "nav2_core")
set(dwb_core_BUILDTOOL_DEPENDS "ament_cmake")
set(dwb_core_BUILD_EXPORT_DEPENDS )
set(dwb_core_BUILDTOOL_EXPORT_DEPENDS )
set(dwb_core_EXEC_DEPENDS "rclcpp" "std_msgs" "rclcpp" "std_msgs" "geometry_msgs" "dwb_msgs" "nav2_costmap_2d" "nav_2d_utils" "pluginlib" "nav_msgs" "tf2_ros" "nav2_util" "nav2_core")
set(dwb_core_TEST_DEPENDS "ament_lint_common" "ament_lint_auto" "ament_cmake_gtest")
set(dwb_core_GROUP_DEPENDS )
set(dwb_core_MEMBER_OF_GROUPS )
set(dwb_core_DEPRECATED "")
set(dwb_core_EXPORT_TAGS)
list(APPEND dwb_core_EXPORT_TAGS "<build_type>ament_cmake</build_type>")
list(APPEND dwb_core_EXPORT_TAGS "<nav2_core plugin=\"local_planner_plugin.xml\"/>")
