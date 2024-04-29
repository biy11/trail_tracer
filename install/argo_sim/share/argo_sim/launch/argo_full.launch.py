import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, SetEnvironmentVariable, IncludeLaunchDescription
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node, PushRosNamespace
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.conditions import IfCondition
from nav2_common.launch import RewrittenYaml

def generate_launch_description():
    # Argo simulation package directory
    pkg_argo_sim = get_package_share_directory('argo_sim')

    # General configurations
    namespace_ = LaunchConfiguration('namespace')
    use_sim_time_ = LaunchConfiguration('use_sim_time', default='true')
    output_dest = "log"  # Adjusted to log to match Argo's setup or screen based on requirement

    # Configure environment variables for Gazebo
    world_file_name = 'roadWorld.xml'  # Assuming default, adjust as necessary
    world = os.path.join(pkg_argo_sim, 'gazebo/worlds', world_file_name)
    pkg_install_path = pkg_argo_sim
    model_path = os.environ.get('GAZEBO_MODEL_PATH', '') + ':' + pkg_install_path
    resource_path = os.environ.get('GAZEBO_RESOURCE_PATH', '') + ':' + pkg_install_path

    # Navigation setup
    argo_nav_dir = get_package_share_directory('argo_nav')
    params_dir = os.path.join(argo_nav_dir, "config")
    nav_params = os.path.join(params_dir, "nav2_no_map_params.yaml")
    configured_params = RewrittenYaml(
        source_file=nav_params, root_key="", param_rewrites="", convert_types=True
    )
    bringup_dir = get_package_share_directory('nav2_bringup')

    # Static transform configuration if needed
    static_transform_ = LaunchConfiguration('static_transform', default="false")
    static_tf_node = Node(
        package="tf2_ros",
        condition=IfCondition(static_transform_),
        executable="static_transform_publisher",
        parameters=[{'use_sim_time': use_sim_time_}],
        arguments=["0", "0", "0", "0", "0", "0", "map", "odom"]
    )

    # Include the Navigation 2 command
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

    # Launch description to integrate all components
    ld = LaunchDescription([
        DeclareLaunchArgument(
            'namespace',
            default_value='',
            description='Name space of the simulated robot'
        ),
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='true',
            description='Use simulation (Gazebo) clock if true'
        ),
        SetEnvironmentVariable(name='GAZEBO_MODEL_PATH', value=model_path),
        SetEnvironmentVariable(name='GAZEBO_RESOURCE_PATH', value=resource_path),
        ExecuteProcess(
            cmd=['gazebo', '--verbose', world,
                 '-s', 'libgazebo_ros_init.so',
                 '-s', 'libgazebo_ros_factory.so'],
            output='screen'
        ),
        # Navigation related additions
        PushRosNamespace(namespace=namespace_),
        static_tf_node,
        navigation2_cmd,
        # Your other existing nodes and processes
    ])

    return ld

