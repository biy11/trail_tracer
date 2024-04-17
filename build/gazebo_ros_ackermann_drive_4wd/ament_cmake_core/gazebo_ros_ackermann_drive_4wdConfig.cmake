# generated from ament/cmake/core/templates/nameConfig.cmake.in

# prevent multiple inclusion
if(_gazebo_ros_ackermann_drive_4wd_CONFIG_INCLUDED)
  # ensure to keep the found flag the same
  if(NOT DEFINED gazebo_ros_ackermann_drive_4wd_FOUND)
    # explicitly set it to FALSE, otherwise CMake will set it to TRUE
    set(gazebo_ros_ackermann_drive_4wd_FOUND FALSE)
  elseif(NOT gazebo_ros_ackermann_drive_4wd_FOUND)
    # use separate condition to avoid uninitialized variable warning
    set(gazebo_ros_ackermann_drive_4wd_FOUND FALSE)
  endif()
  return()
endif()
set(_gazebo_ros_ackermann_drive_4wd_CONFIG_INCLUDED TRUE)

# output package information
if(NOT gazebo_ros_ackermann_drive_4wd_FIND_QUIETLY)
  message(STATUS "Found gazebo_ros_ackermann_drive_4wd: 0.5.4 (${gazebo_ros_ackermann_drive_4wd_DIR})")
endif()

# warn when using a deprecated package
if(NOT "" STREQUAL "")
  set(_msg "Package 'gazebo_ros_ackermann_drive_4wd' is deprecated")
  # append custom deprecation text if available
  if(NOT "" STREQUAL "TRUE")
    set(_msg "${_msg} ()")
  endif()
  # optionally quiet the deprecation message
  if(NOT ${gazebo_ros_ackermann_drive_4wd_DEPRECATED_QUIET})
    message(DEPRECATION "${_msg}")
  endif()
endif()

# flag package as ament-based to distinguish it after being find_package()-ed
set(gazebo_ros_ackermann_drive_4wd_FOUND_AMENT_PACKAGE TRUE)

# include all config extra files
set(_extras "ament_cmake_export_libraries-extras.cmake;ament_cmake_export_dependencies-extras.cmake;ament_cmake_export_include_directories-extras.cmake")
foreach(_extra ${_extras})
  include("${gazebo_ros_ackermann_drive_4wd_DIR}/${_extra}")
endforeach()
