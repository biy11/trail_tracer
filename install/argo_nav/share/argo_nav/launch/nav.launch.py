import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription, conditions
from launch.substitutions import Command, LaunchConfiguration, PythonExpression
from launch.actions import DeclareLaunchArgument, ExecuteProcess, SetEnvironmentVariable, IncludeLaunchDescription
from launch_ros.actions import Node, PushRosNamespace
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.conditions import IfCondition
from nav2_common.launch import RewrittenYaml

output_dest = "screen"

def generate_launch_description():
  argo_nav_dir = get_package_share_directory('argo_nav')
  params_dir = os.path.join(argo_nav_dir, "config")
  nav_params = os.path.join(params_dir, "nav2_no_map_params.yaml")
  configured_params = RewrittenYaml(
        source_file=nav_params, root_key="", param_rewrites="", convert_types=True
  )
  bringup_dir = get_package_share_directory('nav2_bringup')


  namespace_ = LaunchConfiguration('namespace')
  namespace_launch_arg = DeclareLaunchArgument(
    'namespace',
    default_value='',
    description='Name space of the simulated robot'
  )

  use_sim_time_ = LaunchConfiguration('use_sim_time', default='true')
  use_sim_time_arg = DeclareLaunchArgument(
    'use_sim_time',
    default_value='true',
    description='Use simulation (Gazebo) clock if true'
  )

  static_transform_ = LaunchConfiguration('static_transform', default="false")
  static_transform_arg = DeclareLaunchArgument(
    'static_transform',
    default_value='false',
    description='Set to true to get a static transform from map to odom'
  )

  static_tf_node = Node(
    package = "tf2_ros",
    condition=IfCondition(static_transform_),
    executable = "static_transform_publisher",
    parameters=[{
      'use_sim_time': use_sim_time_
    }],
    arguments = ["0", "0", "0", "0", "0", "0", "map", "odom"]
  )

  navigation2_cmd = IncludeLaunchDescription(
    PythonLaunchDescriptionSource(
      os.path.join(bringup_dir, "launch", "navigation_launch.py")
    ),
    launch_arguments={
      "namespace": namespace_,
      "use_sim_time": use_sim_time_,
      "params_file": configured_params,
      "autostart": "True",
    }.items(),
  )


  ld = LaunchDescription()


  ld.add_action(namespace_launch_arg)
  ld.add_action(use_sim_time_arg)
  ld.add_action(static_transform_arg)

  ld.add_action(PushRosNamespace(namespace=namespace_))

  ld.add_action(static_tf_node)

  ld.add_action(navigation2_cmd)

  return ld
