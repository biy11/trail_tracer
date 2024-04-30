# ROS2 Workspace

## Description
This is a ROS2 workspace for the trail_tracer project.

## Prerequisites
- ROS2: Make sure you have ROS2 installed on your system. You can find installation instructions [here](https://docs.ros.org/en/humble/Installation.html).
- python3: Make sure you have Python 3 installed on your system. You can find installation instructions [here](https://www.python.org/downloads/).

1. Clone this repository:

    git clone https://github.com/biy11/trail_tracer.git
    ```

2. Build the workspace:
    ```
    cd trail_tracer
    colcon build
    ```

## Usage
1. Source the ROS2 environment:
    ```
    source /opt/ros/humble/setup.bash
    ```

2. Launch required nodes and gazebo:
    ```
    ros2 launch argo_sim gazebo.launch.py
    ```
    ros2 launch argo_nav nav.launch.py use_sime_time:=true static_transform:=true
    ```
3. Run required nodes.
    ```
    ros2 run trail_tracer trail_tracer
    ```
    ros2 run gps_to_utm_converter_cpp converter_node

4. Run Rviz for data visualisation(optional):
    ```
    rviz2 -d rviz/argo_sim.rviz use_sim_time:=true 

## Launch Web GUI:
1. Navigate to web_gui package:
    ```
    cd src/web_gui

2. Launch web_gui application:
    ```
    python3 app.py

## License
This project is licensed under the Appache License 2.0. See the LICENSE file for more details.

## Contributions
Contributions are welcome! Please refer to the [Contribution Guidelines](CONTRIBUTING.md) for more details.

## Contact
For any questions or issues, please contact the project maintainers:
- John Doe (johndoe@example.com)
- Jane Smith (janesmith@example.com)