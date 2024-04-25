
# @(#) ros2_communication.py 1.3 2024/02/26.
# Copyright (c) 2023 Aberystwyth University.
# All rights reserved.

#Imports.
import rclpy # ROS2 client lib for Python.
from rclpy.node import Node # Base class for ROS2 nodes.
from std_msgs.msg import String # Standard message type for strings.
from sensor_msgs.msg import NavSatFix, Imu  # Import the GPS and Imu message type.
from geometry_msgs.msg  import Twist # Message type for velocities.
import threading # For running thr ROS2 nodes in sperate thread.
import os
from pathlib import Path
import time
from geometry_msgs.msg import Point #Import tthe POint message type
import threading

##############################################################################
########## A .py file for the creation of publishers an subscribers.##########
##############################################################################

# @author Bilal [biy1]
# @version 0.1 - Initial development.
# @version 0.2 - Created web_message and ros_messages publisher and functionality.
# @version 0.3 - Created cmd_vel subscription and functionality.
# @version 0.4 - Comments added.
# @version 0.5 - Created subscription for gps/fix as well as functionality.
# @version 0.6 - Added off-sets for GPS values (for sensible location),
# @version 0.7 - Fixed bugs.
# @version 0.8 - Adpated subscriptions for argo_sim publishers. 
# @version 0.9 - Removed GPS off-sets as argo_sim has valid coordinates in xacro/urdf files defined.
# @version 1.0 - Created publisher and functionality for joystick_value to read velocitied for argo_sim/cmd_vel.
# @version 1.1 - Created publisher for trail_name storage.
# @version 1.2 - Fixed bugs.
# @version 1.3 - Added comments.
# @version 1.4 - Adding subscriber for way-point coordinates.
# @version 1.5 - Added publisher to tell which file to load from save trails
# @version 1.6 - Added susbriber for Imu topic for velocity data.
# @version 1.7 - Changed waypoint topic to subscription rather than a publisher.
 
ros2_web_connector = None  # This will be initialized once to connect to web interface.
latest_image = None


class ROS2WebConnector(Node):
    def __init__(self, message_callback):
        super().__init__('ros2_web_connector')
        #Creation of publishers for: web_messages, trail_name_topic and joystick_value.
        self.web_message_publisher = self.create_publisher(String, 'web_messages', 10)
        self.trail_name_publisher = self.create_publisher(String, 'trail_name_topic', 10)
        self.plotted_trail_name_publisher = self.create_publisher(String, 'plotted_trail_name_topic', 10)
        self.joystick_value_publisher = self.create_publisher(Twist, 'joystick_value', 10)
        self.load_file_publisher = self.create_publisher(String, 'load_file_topic', 10)

        #Creation of subscibers for: ros_messages, /argo_sim/gps/fix and /argo_sim/cmd_vel.
        self.ros_messages_subscription = self.create_subscription(String, 'ros_messages', self.listener_callback, 10)
        self.gps_subscription = self.create_subscription(NavSatFix, '/gps/fix', self.gps_callback, 10)
        self.cmd_vel_subscription = self.create_subscription(Twist, '/cmd_vel', self.cmd_callback,10)
        self.trail_files = self.create_subscription(String, '/trail_tracer/trail_files', self.trail_file_callback, 10)
        self.imu_subscription = self.create_subscription(Imu, '/imu/data', self.imu_callback, 10)
        self.waypoint_subscriber = self.create_subscription(Point, '/trail_tracer/waypoints',self.waypoint_callback ,10)
        

        self.message_callback = message_callback # Callback function for recived message processing.

    # Method to publish a string message to the 'web_messages' topic.
    def publish_message(self, message):
        msg = String()
        msg.data = message
        self.web_message_publisher.publish(msg)
        self.get_logger().info('Publishing: "%s"' % message)

    # Method to publish a trail name to the 'trail_name_topic'.
    def publish_trail_name(self, trail_name):
        msg = String()
        msg.data = trail_name
        self.trail_name_publisher.publish(msg)
        self.get_logger().info(f'Publishing trail name: {trail_name}') 

    # Method to publish joystick values (linear and argular vel) to the 'joystick_value' topic.
    def publish_joystick_value(self, linear_vel, angular_vel):
        msg = Twist()

        msg.linear.x = linear_vel
        msg.linear.y = 0.0
        msg.linear.z = 0.0

        msg.angular.x = 0.0
        msg.angular.y = 0.0
        msg.angular.z = angular_vel

        self.joystick_value_publisher.publish(msg)
        self.get_logger().info(f'Publishing joystick value: linear_vel={linear_vel}, angular_vel={angular_vel}')

    # Callback function for 'ros_messages' subscription.
    def listener_callback(self, msg):
        self.get_logger().info('I heard: "%s"' % msg.data)

    # Callback function for 'argo_sim/gps/fix' subscription.
    def gps_callback(self, msg):
        # Extract GPS data and forward it using the message_callback
        gps_data = {'latitude': msg.latitude, 'longitude': msg.longitude}
        self.message_callback(gps_data)  # Forward GPS data

    # Callback for '/trail_files' subscription
    def trail_file_callback(self, msg):
        # Log the received trail files data
        #self.get_logger().info(f'Received trail files data: {msg.data}')
        self.message_callback({'trail_files': msg.data})

    # Callback function for 'argo_sim/cmd_vel' subscription.
    def cmd_callback(self, msg):
        cmd_vel_data = {
            'linear_x': msg.linear.x,
            'linear_y': msg.linear.y,
            'linear_z': msg.linear.z,
            'angular_x': msg.angular.x,
            'angular_y': msg.angular.y,
            'angular_z': msg.angular.z,
        }
        self.message_callback(cmd_vel_data)
    
    # Callback for IMU data for front-end use.
    def imu_callback(self, msg):
        velocity_data = {
            'orientation_x': msg.orientation.x,
            'orientation_x': msg.orientation.y,
            'orientation_x': msg.orientation.z,
            'orientation_x': msg.orientation.w,
            'angular_velocity_x':msg.angular_velocity.x,
            'angular_velocity_y':msg.angular_velocity.y,
            'angular_velocity_z':msg.angular_velocity.z,
            'linear_acceleration_x': msg.linear_acceleration.x,
            'linear_acceleration_y': msg.linear_acceleration.y,
            'linear_acceleration_z':msg.linear_acceleration.z,
        }
        self.message_callback(velocity_data)

    # Callback for waypoints to be plotted on map when re-tracing a trail.
    def waypoint_callback(self, msg):
        waypoint_data = {'waypoint':{'x': msg.x, 'y':msg.y}}
        self.get_logger().info(f'Recieved waypoints: {waypoint_data}')
        self.message_callback(waypoint_data)


def publish_load_file(file_name):
    global ros2_web_connector
    if ros2_web_connector is not None:
        msg = String()
        msg.data = file_name
        ros2_web_connector.load_file_publisher.publish(msg)
        ros2_web_connector.get_logger().info(f'Publishing load file name: {file_name}')
    else:
        print("ROS2 node is not initialized.")

# Initilises ROS2 node if not already initiliased and starts it in a seperate thread
def start_ros_node(message_callback):
    global ros2_web_connector
    if ros2_web_connector is None:  # Initialize only if it's not already initialized
        rclpy.init(args=None)
        ros2_web_connector = ROS2WebConnector(message_callback)
        thread = threading.Thread(target=rclpy.spin, args=(ros2_web_connector,), daemon=True)
        thread.start()
    return ros2_web_connector  # Return the instance

# Method publishes a message if the ROS2 node is initilised  
def publish_message(message):
    if ros2_web_connector is not None:
        ros2_web_connector.publish_message(message)
    else:
        print("ROS2 node is not initialized.")

# Method publishes a trail name if ROS2 node is initilised  
def publish_trail_name(trail_name):
    if ros2_web_connector is not None:
        ros2_web_connector.publish_trail_name(trail_name)
    else:
        print("ROS2 node is not initialized.")

###########################################################################
######### Main function for initilising and running the ROS2 node #########
###########################################################################
def main(args=None):
    rclpy.init(args=args)
    try:
        global ros2_web_connector
        ros2_web_connector = ROS2WebConnector(lambda msg: print(f"Callback message: {msg}"))
        rclpy.spin(ros2_web_connector)
    except KeyboardInterrupt:
        pass
    finally:
        ros2_web_connector.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
