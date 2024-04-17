import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, SetEnvironmentVariable, IncludeLaunchDescription
from launch.substitutions import Command, LaunchConfiguration
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

def generate_launch_description():
    pkg_argo_sim = get_package_share_directory('argo_sim')
    pkg_nav2 = get_package_share_directory('nav2_bringup')  

    namespace_ = LaunchConfiguration('namespace')
    namespace_launch_arg = DeclareLaunchArgument(
        'namespace',
        default_value='argo_sim',
        description='Name space of the simulated robot'
    )

    use_sim_time_ = LaunchConfiguration('use_sim_time', default='true')
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation (Gazebo) clock if true'
    )
    
    nav2_launch_file = os.path.join(pkg_nav2, 'launch', 'bringup_launch.py')
    nav2_parameters_file = os.path.join(pkg_argo_sim, 'config', 'nav2_params.yaml')
    map_yaml_file = os.path.join(pkg_argo_sim, 'config', 'lank_map.yaml')

    argo_urdf = Command(['xacro', ' ', os.path.join(pkg_argo_sim,
                                                    'description/urdf',
                                                    'argo.urdf.xacro')])

    world_file_name = 'roadWorld.xml'
    world = os.path.join(pkg_argo_sim, 'gazebo/worlds', world_file_name)

    pkg_install_path = get_package_share_directory('argo_sim')
    model_path = os.environ.get('GAZEBO_MODEL_PATH', '') + ':' + pkg_install_path
    resource_path = os.environ.get('GAZEBO_RESOURCE_PATH', '') + ':' + pkg_install_path

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        parameters=[{
            'use_sim_time': use_sim_time_,
            'robot_description': argo_urdf
        }],
        namespace=namespace_,
        output="both",
        arguments=['--ros-args', '--log-level', 'WARN'],
        respawn=True
    )

    joint_state_publisher = Node(
        package='joint_state_publisher',
        name='joint_state_publisher',
        executable='joint_state_publisher',
        namespace=namespace_,
        output="both",
        parameters=[{'use_sim_time': use_sim_time_}],
        arguments=['--ros-args', '--log-level', 'WARN'],
        respawn=True
    )

    gps_to_utm_converter_node = Node(
        package='gps_to_utm_converter_cpp',
        executable='converter_node',
        name='gps_to_utm_converter',
        namespace=namespace_,
        output='screen',
        parameters=[{'use_sim_time': use_sim_time_}],
    )

    return LaunchDescription([
        namespace_launch_arg,
        use_sim_time_arg,
        SetEnvironmentVariable(name='GAZEBO_MODEL_PATH', value=model_path),
        SetEnvironmentVariable(name='GAZEBO_RESOURCE_PATH', value=resource_path),
        ExecuteProcess(
                cmd=['gazebo', '--verbose', world,
                     '-s', 'libgazebo_ros_init.so',
                     '-s', 'libgazebo_ros_factory.so',
                     '-s', 'libgazebo_ros_force_system.so'
                     ],
                output='screen'),
        robot_state_publisher,
        joint_state_publisher,
        Node(
            package='gazebo_ros',
            executable='spawn_entity.py',
            name='urdf_spawner',
            namespace=namespace_,
            output='screen',
            arguments=["-robot_namespace", namespace_,
                       "-topic", ["/", namespace_, "/robot_description"],
                       "-entity", "argo_sim"]
        ),
        gps_to_utm_converter_node,
        
        # Include Navigation2 Launch
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([nav2_launch_file]),
            launch_arguments={
                'namespace': namespace_,
                'use_sim_time': use_sim_time_,
                'params_file': nav2_parameters_file,
                'map': map_yaml_file,
            }.items(),
        ),
    ])
