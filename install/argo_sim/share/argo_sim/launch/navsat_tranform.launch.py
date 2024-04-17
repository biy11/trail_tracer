import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.substitutions import Command, LaunchConfiguration
from launch.actions import DeclareLaunchArgument, ExecuteProcess, SetEnvironmentVariable, IncludeLaunchDescription
from launch_ros.actions import Node
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():
    # Get the package directory
    pkg_argo_sim = get_package_share_directory('argo_sim')

    # Launch configuration variables
    namespace_ = LaunchConfiguration('namespace')
    use_sim_time_ = LaunchConfiguration('use_sim_time', default='true')

    # Declare the namespace launch argument
    namespace_launch_arg = DeclareLaunchArgument(
        'namespace',
        default_value='argo_sim',
        description='Name space of the simulated robot'
    )

    # Declare the use simulation time launch argument
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation (Gazebo) clock if true'
    )

    # Robot description configuration
    argo_urdf = Command(['xacro', ' ', os.path.join(pkg_argo_sim, 'description/urdf', 'argo.urdf.xacro')])

    # Gazebo world file configuration
    world_file_name = 'roadWorld.xml'
    world = os.path.join(pkg_argo_sim, 'gazebo/worlds', world_file_name)

    # Environment variable configurations for Gazebo
    model_path = os.environ.get('GAZEBO_MODEL_PATH', '') + ':' + pkg_argo_sim
    resource_path = os.environ.get('GAZEBO_RESOURCE_PATH', '') + ':' + pkg_argo_sim

    # Node configurations
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        parameters=[{'use_sim_time': use_sim_time_, 'robot_description': argo_urdf}],
        namespace=namespace_,
        output='screen',
        respawn=True
    )

    joint_state_publisher = Node(
        package='joint_state_publisher',
        executable='joint_state_publisher',
        namespace=namespace_,
        parameters=[{'use_sim_time': use_sim_time_}],
        output='screen',
        respawn=True
    )

    # navsat_transform_node configuration
    navsat_transform_config = os.path.join(pkg_argo_sim, 'config', 'navsat_config.yaml')
    navsat_transform_node = Node(
        package='robot_localization',
        executable='navsat_transform_node',
        name='navsat_transform_node',
        namespace=namespace_,
        output='screen',
        parameters=[navsat_transform_config],
        remappings=[('/imu/data', '/argo_sim/imu/data'), ('/gps/fix', '/argo_sim/gps/fix'), ('/odometry/filtered', '/argo_sim/odom')]
    )

      # Path to the Nav2 bringup launch file
    nav2_launch_file = os.path.join(
      get_package_share_directory('nav2_bringup'), 
      'launch', 
      'bringup_launch.py'
    )

    # Path to nav2_params.yaml file
    nav2_params_file = os.path.join(pkg_argo_sim, 'config', 'nav2', 'nav2_params.yaml')

      # Include the Nav2 bringup launch description
    nav2_bringup_launch = IncludeLaunchDescription(
      PythonLaunchDescriptionSource(nav2_launch_file),
      launch_arguments={
          'namespace': namespace_,
          'use_sim_time': use_sim_time_,
          'autostart': 'true',
          'params_file': nav2_params_file,
          'map': os.path.join(pkg_argo_sim, 'maps', 'dummy_map.yaml'),
      }.items(),
    )
    # Launch description
    return LaunchDescription([
        namespace_launch_arg,
        use_sim_time_arg,
        SetEnvironmentVariable(name='GAZEBO_MODEL_PATH', value=model_path),
        SetEnvironmentVariable(name='GAZEBO_RESOURCE_PATH', value=resource_path),
        ExecuteProcess(
            cmd=['gazebo', '--verbose', world, '-s', 'libgazebo_ros_init.so', '-s', 'libgazebo_ros_factory.so', '-s', 'libgazebo_ros_force_system.so'],
            output='screen'
        ),
        robot_state_publisher,
        joint_state_publisher,
        Node(
            package='gazebo_ros',
            executable='spawn_entity.py',
            name='urdf_spawner',
            namespace=namespace_,
            output='screen',
            arguments=["-robot_namespace", namespace_, "-topic", ["robot_description"], "-entity", "argo_sim"]
        ),
        navsat_transform_node,
        nav2_bringup_launch,
    ])