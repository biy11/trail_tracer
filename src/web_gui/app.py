# @(#) app.py 1.0 2024/04/27.
# Copyright (c) 2023 Aberystwyth University.
# All rights reserved.

# This is a Flask application which serves as a web interface for ROS2-based robitic systems.
# It integrates with ROS2 trrough a custom cummunication module to accomodate real-time interactions
# and data exchange between the GUI and ROS2 application.

from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO
import json
import os
from web_gui.ros2_communication import start_ros_node, publish_message, publish_trail_name, publish_load_file

##############################################################################
###### A Flask .py application for ROS2 trail following robot web GUI. ########
##############################################################################

# @author Bilal [biy1]
# @version 0.1 - Initial development.
# @version 0.2 - Created web_message and ros_messages_handler functionality.
# @version 0.3 - Added cmd_vel comunication functionality.
# @version 0.4 - Added gps/fix functionality.
# @version 0.5 - Created handel_joystick_value communicate and set cmd-val via joystick value.
# @version 0.6 - Fixed bugs.
# @version 0.7 - Created comunication handler for trail_name_topic.
# @version 0.8 - Fixed bugs.
# @version 0.9 - Added comments.
# @version 1.0 - Grammar and spelling error fix.
# @version 1.1 - Adjusted values for angular velocity from joystick values.
# @version 1.1 - Added cumunicaion handler for plotted trails.
# @version 1.2 - Removed communication handler for plotted trails and chnaged it with a function to write the file stright from fornt-end rathe rthan back-end
# @version 1.3 - Added route for user_guide
# @version 1.4 - Added emiter for trail waypoints to be plotted on map. 


app = Flask(__name__)
socketio = SocketIO(app) # Initilises SocketIO for real-time communication.

# Initialize the ROS2 node once and store it in a global variable for access across the Flask app.
global_ros_node = None

@app.route('/')
def index():
    # Serve the main page if the web GUI.
    return render_template('index.html')
# Route to the about page.
@app.route('/user_guide')
def user_guide():
    return render_template('user_guide.html')
# Route to the 'original' traethlin page.
@app.route('/traethlin')
def traethlin():
    return render_template('traethlin.html')

# Route to save the trail to a file.
@app.route('/save-trail', methods=['POST'])
# Method to save the trail to a file.
def save_trail():
    # Get the plot name and coordinates from the request.
    data = request.json
    plot_name = data['plotName']
    coordinates = data['coordinates']
    
    # Updated file path to save in the trails folder
    home_dir = os.path.expanduser('~')
    trails_folder = os.path.join(home_dir,'trail_tracer','trails')
    file_path = os.path.join(trails_folder, f'{plot_name}.txt')

    # Ensure the trails folder exists, create if it doesn't
    if not os.path.exists(trails_folder):
        os.makedirs(trails_folder)

    # Writing the coordinates to the file to 8 decimal points.
    with open(file_path, 'w') as file:
        for coord in coordinates:
            lat = "{:.8f}".format(coord[0])
            lng = "{:.8f}".format(coord[1])
            file.write(f'{lat},{lng}\n')    
    return jsonify({'message': f'Trail {plot_name} saved successfully!'})

# Route to check if a trail file exists.
@app.route('/check-trail-exists', methods=['POST'])
def check_trail_exists():
    # Get the plot name from the request.
    data = request.json
    plot_name = data['plotName']
    home_dir = os.path.expanduser('~')
    # Updated file path to save in the trails folder
    trails_folder = os.path.join(home_dir,'trail_tracer','trails')
    file_path = os.path.join(trails_folder, f'{plot_name}.txt')
    # Check if the file exists.
    file_exists = os.path.exists(file_path)
    return jsonify({'exists': file_exists}) # Return the result as a JSON object.

# Method to handel messages recived from ROS2 and emit them to teh web GUI through WebSocket.
def handle_ros_messages(message):
    # Emit messages to the frontend based on their type.
    if 'latitude' in message and 'longitude' in message:
        socketio.emit('gps_data', message)  # Emit GPS data specifically.
    elif 'linear_x' in message or 'angular_z' in message:
        socketio.emit('cmd_vel_data', message)  # Emit cmd_vel data. #No longer in use, but in case needed in future
    elif 'trail_files' in message:
        socketio.emit('trail_files_data', {'data': message['trail_files']}) # Emit trail files data.
    elif 'linear_velocity' in message or 'angular_velocity_z' in message:
        socketio.emit('odom_data', message) # Emit odom data.
    elif 'waypoint' in message: 
        socketio.emit('waypoint_data', {'data': message['waypoint']}) # Emit waypoint data.
    elif 'log_message' in message:
        log_info = message['log_message']
        socketio.emit('log_info',{'data': log_info}) # Emit log messages.
    else:
        socketio.emit('ros_message', {'data': message})  # Emit general ROS messages.

# Method for handeling messages sent from the web GUI to ROS2.
@socketio.on('web_message')
def handle_web_message(json):
    app.logger.info(f'Received message: {json}') # Log the recived message
    publish_message(json['data'])  # Publish the message received from the web interface.

# Method for handeling the trail name message sent from the web GUI.
@socketio.on('trail_name_topic')
def handel_trail_name(json):
    app.logger.info(f'Received trail name: {json}')
    trail_name = json['data']
    publish_trail_name(trail_name)

# Method for handeling the trail name message sent from the web GUI.
@socketio.on('load_file')
def handle_load_file(json):
    app.logger.info(f'Received load file name: {json}') 
    load_file_name = json['data']
    publish_load_file(load_file_name)

# Method for handeling joystick values sent from the web GUI for the robot.
@socketio.on('joystick_value')
def handle_joystick_value(data):
    app.logger.info(f'Received joystick value: {data}')
    forward_backward_value = data['forwardBackward']
    left_right_value = data['leftRight']

    #scaling the values by 3x and -0.5 (for inverted movement controll).
    linear_vel = float(forward_backward_value) * 3
    angular_vel = float(left_right_value) * -0.5

    # ROS node to publish joystick values.
    if global_ros_node is not None:
        global_ros_node.publish_joystick_value(linear_vel, angular_vel)

    else:
        app.logger.error("ROS2 node is not initialized.")

# Initilise the global ROS2 node for communication with the ROS2 system.
def initialize_ros_node():
    global global_ros_node
    if global_ros_node is None:
        global_ros_node = start_ros_node(handle_ros_messages)

if __name__ == '__main__':
    initialize_ros_node()  # Initialize the ROS2 node.
    socketio.run(app, debug=True) # Start Flask application with SocketIO support.
