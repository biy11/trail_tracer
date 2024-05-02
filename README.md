# ROS2 Trail Tracer Workspace

## Description
This is a ROS2 workspace for the trail_tracer project.

## Prerequisites
- ROS2: Make sure you have ROS2 installed on your system. You can find installation instructions [here](https://docs.ros.org/en/humble/Installation.html).
- python3: Make sure you have Python 3 installed on your system. You can find installation instructions [here](https://www.python.org/downloads/).
- Flask: Make sure you have Flask installed on your system. You can install it using pip:

   - pip install flask

- GeographicLib: Make sure you have GeographicLib installed on your system. You can install it using pip:
    
    - pip install GeographicLib

1. Clone this repository:
   - git clone https://github.com/biy11/trail_tracer.git
    

2. Build the workspace:
    
    - cd trail_tracer

    - colcon build
    

## Usage
1. Source the ROS2 environment:

    - source /opt/ros/humble/setup.bash
    

2. Launch required files and gazebo:
    
    - ros2 launch argo_sim gazebo.launch.py
    
    - ros2 launch argo_nav nav.launch.py use_sime_time:=true static_transform:=true

3. Run required nodes:
    
    - ros2 run trail_tracer trail_tracer
    
    - ros2 run gps_to_utm_converter_cpp converter_node (optionally add node to launch file, see File modifications 2.)

4. Run Rviz for data visualisation (optional):

    - rviz2 -d rviz/argo_sim.rviz use_sim_time:=true 

## Launch Web GUI:
1. Navigate to web_gui package:

    - cd src/web_gui

2. Launch web_gui application:

    - python3 app.py


## File modifications:

1. nav2_no_map_params.yaml file: Please see "local_nav2_no_map_param.yaml" for the configuration file used in this project. Author of file is Fred Labrosse [ffl], changes made to paramateres by Bilal [biy1].

    NOTE: THIS FILE MAY NOT BE UP TO DATE, BUT HAS BEEN USED AND ADAPTED FOR THIS PROJECT. BELOW ARE SOME CHANGES MADE TO THIS FILE FROM ITS ORIGINAL FORM.

    - Uncomment/add the following to conifg file:

        bt_navigator:

            ros__parameters:

                global_frame: map
                robot_base_frame: base_link
                odom_topic: /odom
                bt_loop_duration: 1 # Changed to 1 from 10
                default_server_timeout: 5 # Changed from 20 to 5
                #default_nav_to_pose_bt_xml: "no_spin_behaviour.xml"
            #    navigators: ["navigate_to_pose", "navigate_through_poses"]
                navigators: ["navigate_through_poses"] # Change from "navigate_to_pose" to "navigate_through_pose"
            #    navigators: ["navigate_to_pose"]
            #    navigate_to_pose:
            #      plugin: "nav2_bt_navigator/NavigateToPoseNavigator"
                navigate_through_poses:
                 plugin: "nav2_bt_navigator/NavigateThroughPosesNavigator"

        # Uncomment and change parameteres for waypoint_follower.
        waypoint_follower:

            ros__parameters:

                loop_rate: 20
                stop_on_failure: false
                waypoint_task_executor_plugin: "wait_at_waypoint"
                wait_at_waypoint:
                plugin: "nav2_waypoint_follower::WaitAtWaypoint"
                enabled: True
                waypoint_pause_duration: 0

        controller_server:

            ros__parameters:
                min_y_velocity_threshold: 0.5 # From 0.001 to 0.5

                progress_checker:
                    required_movement_radius: 0.3 # From 0.5 to 0.3
                    movement_time_allowance: 15.0 # From 10 to 15
                
                general_goal_checker:
                    xy_goal_tolerance: 1.0 
                    yaw_goal_tolerance: 3.14 #180 degrees
                    
        global_costmap:

            global_costmap:
                    width: 350 # from 100 to 350
                    height: 350 # from 100 to 350

2. Optionally add gps_to_utm_converter_node_cpp to launch file for ease of launch:

    # Add to launch file:

    gps_to_utm_converter_node = Node(

        package='gps_to_utm_converter_cpp',
        executable='converter_node',
        name='gps_to_utm_converter',
        namespace=namespace_,
        output='screen',
        parameters=[{'use_sim_time': use_sim_time_}],
    )

    # Add to the end of Node(), 
    Node(

        package='gazebo_ros',
        executable='spawn_entity.py',
        name='urdf_spawner',
        namespace=namespace_,
        output='screen',
        arguments=["-robot_namespace", namespace_,
                    "-topic", [namespace_, "/robot_description"],
                    "-entity", "argo_sim"]
    ),

    gps_to_utm_converter_node, # Addition of node.
    




## License
This project is licensed under the Appache License 2.0. See the LICENSE file for more details.


## Contact
For any questions or issues, please contact me (project maintainer):

Bilal Yousufzai (biy1@aber.ac.uk or bilalyousufzai28@gmail.com)