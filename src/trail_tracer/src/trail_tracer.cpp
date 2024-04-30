/*
 * @(#) trail_tracer.cpp 1.7 2024/04/29 
 * 
 * Copyright (c) 2024 Aberystwth University
 * All rights reserved.
*/

// Include necessary libraries for the node
#include <chrono>
#include <iostream>
#include <thread>
#include <memory>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/point.hpp>
#include <std_msgs/msg/string.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <vector>
#include <string>
#include <utility>
#include <sstream>
#include <GeographicLib/UTMUPS.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <nav2_msgs/action/follow_waypoints.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <rcl_interfaces/msg/log.hpp>
#include <regex>

/**
 * This node is responsible for recording trails by storing GPS data to a file, loading trails from files, 
 * and sending waypoints to the WaypointFollower node. It also handles manual control of the robot's movement and
 * allows for pausing and resuming trail following. Processes log messages from the system and communicates with 
 * the front-end.
 * 
 * @author Bilal [biy1]
 * @version 0.1 - Initial creation of the file.
 * @version 0.2 - Added ability to move robot.
 * @version 0.3 - Added file storing capabilities.
 * @version 0.4 - Added IMU and GPS data handlers.
 * @version 0.5 - Added files publisher.
 * @version 0.6 - Added timer for file publisher.
 * @version 0.7 - Added load file sub.
 * @version 0.8 - Added utm data for conversion.
 * @version 0.9 - Added file loading handeling for creating waypoints.
 * @version 1.0 - Added goal publisher for waypoints (WaypointSender).
 * @version 1.1 - Added orientation values from IMU.
 * @version 1.2 - Added odom data for tracking position.
 * @version 1.3 - Added remainingDestinationPoseList.
 * @version 1.4 - Added sparse coordinate storing via rbd algorithm.
 * @version 1.5 - Added log reader callback for log messages.
 * @version 1.6 - Added logger function for log messages.
 * @version 1.7 - Added comments and cleaned up code.
 * 
*/

// using namespace for easier access to message types assigning Alises for message types.
using namespace std::chrono_literals;
using PoseStamped = geometry_msgs::msg::PoseStamped;
using FullPoseVector = std::vector<std::tuple<float, float, float, float, float, float>>;
using Tuple = std::tuple<float, float>;



// Class for sending waypoints to the action server
class WaypointSender : public rclcpp::Node
{
public:
    // Setting variables for message types for easier acess and use
    using FollowWaypoints = nav2_msgs::action::FollowWaypoints;
    using GoalHandleFollowWaypoints = rclcpp_action::ClientGoalHandle<FollowWaypoints>;

    // Constructor for node and creating an action client
    explicit WaypointSender(const std::string & name) : Node(name) {
        this->client_ptr_ = rclcpp_action::create_client<FollowWaypoints>(
            this, "/follow_waypoints");
    }

    // Method to set and snd waypoints to the action server
    void set_waypoints(const std::vector<PoseStamped>& waypoints) {
        auto goal_msg = FollowWaypoints::Goal();
        goal_msg.poses = waypoints;
        // Check to see if action server is availaible before sending waypoints.
        if (!client_ptr_->wait_for_action_server()) {
            RCLCPP_ERROR(this->get_logger(), "Action server not available after waiting");
            return;
        }
        // Configure goal options.
        auto send_goal_options = rclcpp_action::Client<FollowWaypoints>::SendGoalOptions();
        send_goal_options.result_callback = std::bind(&WaypointSender::goal_result_callback, this, std::placeholders::_1);
        client_ptr_->async_send_goal(goal_msg, send_goal_options);
    }
    // Method to pause waypoint following, by canceling current goals.
    void pause() {
        client_ptr_->async_cancel_all_goals();
        //RCLCPP_INFO(this->get_logger(), "Waypoint follower paused."); // for debugging purposes.
    }

    // Method to resume waypoint followign be re-sending remainig waypoints.
    void resume(const std::vector<PoseStamped>& waypoints) {
        //RCLCPP_INFO(this->get_logger(), "Resuming waypoint follower."); // For debugging purposes.
        // Resend the waypoints to resume following.
        if (!waypoints.empty()) {
            set_waypoints(waypoints);
        }
    }

private:
    // Pointer to action client handeling FollowWaypoints actions.
    rclcpp_action::Client<FollowWaypoints>::SharedPtr client_ptr_; 

    // Callback function for waypoint following toprocess results
    void goal_result_callback(const GoalHandleFollowWaypoints::WrappedResult & result) {
        switch (result.code) {
            case rclcpp_action::ResultCode::SUCCEEDED:
                RCLCPP_INFO(this->get_logger(), "Goal was achieved.");
                break;
            case rclcpp_action::ResultCode::ABORTED:
                RCLCPP_ERROR(this->get_logger(), "Goal was aborted.");
                break;
            default:
                RCLCPP_ERROR(this->get_logger(), "Eror unkown result.");
                break;
        }
    }
};

// Class for trail tracing and manual control
class TrailTracer : public rclcpp::Node {
public:
    TrailTracer()
    : Node("robot_movement_control"), is_moving_(false), current_easting_(0.0), current_northing_(0.0), 
    current_pose_x(0.0), current_pose_y(0.0), waypoint_sender_(std::make_shared<WaypointSender>("waypoint_sender"))
    {
        ////////////////// PUBLISHERS FOR CMD_VEL, FILE NAMES AND WAYPOINTS //////////////////
        cmd_vel_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
        file_names_publisher_ = this->create_publisher<std_msgs::msg::String>("trail_tracer/trail_files", 10);
        file_waypoint_publisher_ = this->create_publisher<geometry_msgs::msg::Point>("/trail_tracer/waypoints", 10);
        log_publisher_ = this->create_publisher<std_msgs::msg::String>("/trail_tracer/log_messages", 10);

        //////////////////////////////// SUBSCRIPTIONS ////////////////////////////////
        // Subscription for web_messages to recive commands from fornt-end.
        web_message_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/web_gui/web_messages", 10,                                             
            std::bind(&TrailTracer::web_messages_callback, this, std::placeholders::_1));
        // Subscription for trail name  input in front-end for file making.
        trail_name_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/web_gui/trail_name_topic", 10,
            std::bind(&TrailTracer::trail_name_callback, this, std::placeholders::_1));
        // Subscription for joystick values from front-end joystick.
        joystick_subscription_ = this->create_subscription<geometry_msgs::msg::Twist>(
            "/web_gui/joystick_value", 10,
            std::bind(&TrailTracer::manual_control_callback, this, std::placeholders::_1));
        // Subscription for which file to be loaded fro trail tracing.
        load_file_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/web_gui/load_file_topic", 10,
            std::bind(&TrailTracer::file_loader_callback, this, std::placeholders::_1));
        // Subscription for gps data for recording trails.
        gps_fix_subscription_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
            "/gps/fix", 10,
            std::bind(&TrailTracer::gps_fix_callback, this, std::placeholders::_1));
        // Subscription for utm data to convert from gps to UTM data to pass to WaypointFollower.
        gps_to_utm_subscription_ = this->create_subscription<PoseStamped>(
            "/utm_data", 10,
            std::bind(&TrailTracer::utm_data_callback, this, std::placeholders::_1));
        // Subscription for obtaining orientation values from IMU.
        imu_data_subscription_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/imu/data", 10,
            std::bind(&TrailTracer::imu_data_callback, this, std::placeholders::_1));
        // Subscription for odometry data for tacking positioning in local/global
        odom_subscription_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/odom", 10, std::bind(&TrailTracer::odom_callback, this, std::placeholders::_1));
        // Subscription for reading log data for display in front-end system log.
        log_reader_subscription_ = this->create_subscription<rcl_interfaces::msg::Log>(
            "/rosout", 10, std::bind(&TrailTracer::log_reader_callback, this, std::placeholders::_1));
        // Timer update for publishing joystick values to cmd_vel topic.
        timer_update_movement_ = this->create_wall_timer(
            100ms, std::bind(&TrailTracer::update_movement, this));
        // Timer for publsihing avaliable trail files for front-end.
        timer_publish_file_names_ = this->create_wall_timer(
            500ms, std::bind(&TrailTracer::publish_file_name, this));

        
        
    }
    // Destructor for closing file if open.
    ~TrailTracer(){
        if(gps_file_.is_open()){
            gps_file_.close();
        }
    }

private:

    /////////////////////////////////////////////////////////////////////////////////////// 
    ///////////////////////////////// CALLBACK FUNCTIONS //////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Callback function web_messages.
     * 
     * This function is called when a new web_message is received. It performs an operation depending
     * on the type of message recives. messages can be manual-start, manual-move, manual-stop, pause-trail, resume-trail.
     * 
     * @param msg The trail name message.
     */
    void web_messages_callback(const std_msgs::msg::String::SharedPtr msg) {
        if (msg->data == "manual-start") {
            is_moving_ = true;
        } else if (msg->data == "manual-move"){
            is_moving_ = true;
        }else if (msg->data == "manual-stop") {
            //RCLCPP_INFO(this->get_logger(), "Original number of waypoints: %zu", recorded_waypoints_.size()); //Logging for debugging purposes
            is_moving_ = false;
            if(!recorded_waypoints_.empty()){
                auto simplified_points = rdp(recorded_waypoints_, 0.000036);
                //RCLCPP_INFO(this->get_logger(), "Simplified number of waypoints: %zu", simplified_points.size()); //Logging for debugging purposes
                save_to_file(simplified_points);
            }
            recorded_waypoints_.clear();
            if(gps_file_.is_open()){
                gps_file_.close(); //close 
            }
        }else if(msg->data == "pause-trail"){
            waypoint_sender_->pause();
        }else if(msg->data == "resume-trail"){
            remainingDestinationPoseList(poses_,destination_coords_);
        }
        //RCLCPP_INFO(this->get_logger(), "Received command: '%s'", msg->data.c_str()); // Logging for debugging purposes
    }

    /**
     * @brief Callback function for the trail name message.
     * 
     * This function is called when a new trail name message is received. It updates the trail name
     * variable and re-opens the GPS file with the new trail name if the robot is already moving.
     * 
     * @param msg The trail name message.
     */
    void trail_name_callback(const std_msgs::msg::String::SharedPtr msg){
        trail_name_ = msg->data;
        //RCLCPP_INFO(this->get_logger(), "Trail name set to: '%s'", trail_name_.c_str()); // Logging for debugging purposes
        if (is_moving_) {
            open_gps_file(); // Re-open the file with the new trail name if already moving
        }
    }
    
    /**
     * @brief Callback function for IMU data.
     * 
     * This function is called when new IMU data is received. It extracts the orientation data
     * from the message and stores it in the corresponding variables.
     * 
     * @param msg The IMU data message.
     */
    void imu_data_callback(const sensor_msgs::msg::Imu::SharedPtr msg) {
        // Extract orientation data from the message
        orientation_x_ = msg->orientation.x;
        orientation_y_ = msg->orientation.y;
        orientation_z_ = msg->orientation.z;
        orientation_w_ = msg->orientation.w;
    }

    /**
     * @brief Callback function for the odometry message.
     * 
     * This function is called whenever a new odometry message is received.
     * It extracts the current position from the message and updates the
     * 'current_pose_x' and 'current_pose_y' variables accordingly.
     * 
     * @param msg The odometry message containing the current position.
     */
    void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg){
        current_pose_x = msg->pose.pose.position.x;
        current_pose_y = msg->pose.pose.position.y;
    }
    
    /**
     * @brief Callback function for GPS fix messages.
     * 
     * This function is called whenever a new GPS fix message is received. It updates the last known location and, if the system is moving, records the current waypoint.
     * 
     * @param msg A shared pointer to the received NavSatFix message.
     */
    void gps_fix_callback(const sensor_msgs::msg::NavSatFix::SharedPtr msg) {
        last_gps_msg_ = msg; //Update last known location

        if(is_moving_){
            recorded_waypoints_.emplace_back(msg->latitude,msg->longitude, orientation_x_, orientation_y_, orientation_z_, orientation_w_);
        }
    }
    
    /**
     * @brief callback function for joystick values.
     * 
     * This function is called when a new joystick value message is received. It updates the last received command.
    */
    void manual_control_callback(const geometry_msgs::msg::Twist::SharedPtr msg) {
        last_received_cmd_ = msg;
    }
    
    /**
     * @brief Callback function for reading log messages.
     * 
     * This function is called when a log message is received. It checks the message source and content
     * to perform specific actions based on the log message. Allowing for fornt-end log information display from nav2 operations.
     * 
     * @param msg The log message received.
     */
    void log_reader_callback(const rcl_interfaces::msg::Log::SharedPtr msg){
        // Check if the message is from bt_navigator and contains the navigation text
        if (msg->name == "bt_navigator" && msg->msg.find("Begin navigating") != std::string::npos) {
            // Extract the destination coordinates from the message
            std::regex coord_regex("\\((-?\\d+\\.\\d+),\\s*(-?\\d+\\.\\d+)\\)"); 
            std::smatch matches;
            std::string::const_iterator searchStart(msg->msg.cbegin());
            std::vector<std::pair<float, float>> coordinates;

            // Find all matches of message type
            for (std::sregex_iterator it(msg->msg.begin(), msg->msg.end(), coord_regex), end_it; it != end_it; ++it) {
                matches = *it;
                float x = std::stof(matches[1].str());
                float y = std::stof(matches[2].str());
                coordinates.push_back(std::make_pair(x, y));
            }

            // Check for at least two coordinate pairs
            if (coordinates.size() >= 2) {
                destination_coords_ = coordinates[1]; // Select the second pair
                //RCLCPP_INFO(this->get_logger(), "Destination coordinates: (%f, %f)", destination_coords_.first, destination_coords_.second); // Logging for debugging purposes.
            }
        }
        // check for messages for trail completion, trail cancel/pause, trail start, and log result
        if(msg->name == "waypoint_follower" && msg->msg.find("Completed all") != std::string::npos){
            logger("[waypoint_follower] [INFO]: SUCCESS ALL WAYPOINTS REACHED");
        } else if(msg->name == "waypoint_follower" && msg->msg.find("Received follow waypoint request with") != std::string::npos){
            logger("[waypoint_follower] [INFO]: Received follow waypoint request");
        } 
        // Check for messages from behavior_server and controller_server.
        if(msg->name == "behavior_server" && msg->msg.find("Running backup") != std::string::npos){
            logger("[behavior_server] [INFO]: Running backup");
        } else if(msg->name == "behavior_server" && msg->msg.find("backup completed successfully") != std::string::npos){
            logger("[behavior_server] [INFO]: backup completed successfully");
        }else if(msg->name == "behavior_server" && msg->msg.find("Running spin") != std::string::npos){
            logger("[behavior_server] [INFO]: Running spin");
        }else if(msg->name == "behavior_server" && msg->msg.find("spin completed successfully") != std::string::npos){
            logger("[behavior_server] [INFO]: spin completed successfully");
        }
        // Check for messages from controller_server and local_costmap.local_costmap
        else if(msg->name == "controller_server" && msg->msg.find("Failed to make progress") != std::string::npos){
            logger("[Controll_server] [ERROR] Failed to make progress");
        } else if(msg->name == "controller_server" && msg->msg.find("Aborting handle") != std::string::npos){
            logger("[controller_server] [WARN]: [follow_path] [ActionServer] Aborting handle.");
        } else if(msg->name == "controller_server" && msg->msg.find("Received a goal, begin computing control effort.") != std::string::npos){
            logger("[controller_server] [INFO]: Received a goal, begin computing control effort.");
        } else if(msg->name == "controller_server" && msg->msg.find("[controller_server]: Passing new path to controller.") != std::string::npos){
            logger("[controller_server] [INFO]: Passing new path to controller.");
        } else if(msg->name == "controller_server" && msg->msg.find("Client requested to cancel the goal") != std::string::npos){
            logger("[Controll_server] [WARN] Client requested to stop goal");
        }else if(msg->name == "controller_server" && msg->msg.find("Client requested to cancel the goal") != std::string::npos){
            logger("[Controll_server] [WARN] Client requested to stop goal");
        }else if(msg->name == "local_costmap.local_costmap" && msg->msg.find("Received request to clear entirely the local_costmap") != std::string::npos){
            logger("[local_costmap.local_costmap] [INFO]: Received request to clear entirely the local_costmap");
        }
        // check for message from planner server regarding eather goal pose is computable.
        else if(msg->name == "planner_server" && msg->msg.find("The goal sent to the planner is off the global costmap.") != std::string::npos){
            logger("[planner_server [WARN]: The goal sent to the planner is off the global costmap. Planning will always fail to this goal.");
            logger("[trail_tracer] [INFO]: ENDING CURRENT OPERATION!");
            logger("Please choose another file or increase the map height/width in ~/argo_nav/config/nav2_no_map_params.yaml.");
        }
    }

    /**
     * @brief Callback function for UTM data.
     * 
     * This function is called when new UTM data is received. It updates the current easting and northing values.
     * 
     * @param msg The UTM data message.
     */
    void utm_data_callback(const PoseStamped::SharedPtr msg){
        current_easting_ = msg->pose.position.x;
        current_northing_ = msg->pose.position.y;
    }

    /**
     * @brief Callback function for file loader messages.
     * 
     * This function is called when a new file loader message is received. It opens the file with the provided name
     * and processes the waypoints stored in the file. The waypoints are then sent to the WaypointSender object.
     * Processing is done based on the file format, with the new format (with orientation) containing full pose information.
     * 
     * @param msg The file loader message containing the file name.
     */
    void file_loader_callback(const std_msgs::msg::String::SharedPtr msg) {
        std::string file_path = "trails/" + msg->data;
        std::ifstream file(file_path);
        if (!file.is_open()) {
            //RCLCPP_ERROR(this->get_logger(), "Failed to open file: %s", file_path.c_str()); // For debugging purposes
            logger("[trail_tracer] [ERROR] Failed to open file");
            return;
        }
        // Clear the current waypoints and poses before processing new data.
        waypoints_.clear();
        poses_.clear();
        bool full_pose_format = false;
        std::string line;
        // Read each line in the file and process the data
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            float lat, lon, qx, qy, qz, qw;
            std::vector<float> values;
            std::string value;
            while (std::getline(iss, value, ',')){
                values.push_back(std::stof(value));
            }
            // Check the number of values in the line to determine the format.
            if(values.size() ==2){
                lat = values[0];
                lon = values[1];
            } else if (values.size() == 6){
                lat = values[0];
                lon = values[1];
                qx = values[2];
                qy=values[3];
                qz = values[4];
                qw = values[5];
                full_pose_format = true;
            } else{
                //RCLCPP_ERROR(this->get_logger(), "Error parsing line: %s", line.c_str()); // For debugging purposes
                logger("[trail_tracer] [ERROR] Failed to parse line in file");
                continue;
            }
            waypoints_.emplace_back(lat, lon);
            geometry_msgs::msg::Point point_msg;
            point_msg.x = lat; // Assign latitude to x
            point_msg.y = lon; // Assign longitude to y
            point_msg.z = 0.0; // Set z to 0.0 if not used
            file_waypoint_publisher_->publish(point_msg);
            // Check if the full pose format is used and store the full pose.
            if(values.size() == 6){
                createAndStoreFullPose(lat,lon,qx,qy,qz,qw);
            }
        }
        file.close();
        if(full_pose_format){
            waypoint_sender_->set_waypoints(poses_);
            // RCLCPP_INFO(this->get_logger(), "Processed and stored %zu poses from new format data.", poses_.size()); // Logging for debugging purposes
        } else{
            process_all_waypoints(); 
            }
    }

    
    /**
     * @brief Function for logging information.
     * 
     * This function is used to log information to the front-end. It publishes the message to the log_messages topic.
     * 
     */
    void logger(const std::string &message){
        auto msg = std_msgs::msg::String();
        msg.data = message;
        log_publisher_->publish(msg);
    } 
    
    
    /**
     * @brief Update the movement of the robot.
     * 
     * This function updates the movement of the robot based on the last received command.
     * If the robot is moving, the last received command is published to the cmd_vel topic.
     */
    void update_movement() {
        if (is_moving_ && last_received_cmd_) {
            cmd_vel_publisher_->publish(*last_received_cmd_);
        }
    }

    
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////// FILE OPERATION FUNCTIONS ////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Save the recorded waypoints to a file.
     * 
     * This function saves the recorded waypoints to a file. The file is stored in the 'trails' directory with the trail name as the file name.
     * If the file is already open, it is closed before opening a new file. If the trail name is empty, a warning message is logged.
     * The waypoints are simplified using the Ramer-Douglas-Peucker algorithm before saving.
     */
    void save_to_file(const FullPoseVector& points) {
        if (gps_file_.is_open()) {
            // Records gps data to file to 8 decimal places
            gps_file_ << std::fixed << std::setprecision(8);
            for (const auto& [lat, lon, x, y, z, w] : points) {
                gps_file_ << lat << ", " << lon << ", " << x << ", " << y << ", " << z << ", " << w << std::endl;
            }
            gps_file_.close(); // Close and store last position
        }
    }

    /**
     * @brief Open the GPS file for writing.
     * 
     * This function opens the GPS file for writing. The file is stored in the 'trails' directory with the trail name as the file name.
     * If the file is already open, it is closed before opening a new file. If the trail name is empty, a warning message is logged.
     */
    void open_gps_file() {
        std::string directory_path = "trails";
        std::filesystem::create_directory(directory_path); // Ensure directory exists
        if (!trail_name_.empty()) {
            std::string file_path = directory_path + "/" + trail_name_ + ".txt";
            if (gps_file_.is_open()) {
                gps_file_.close(); // Close any existing file
            }
            gps_file_.open(file_path, std::ios::out); // Open new file
            if (!gps_file_.is_open()) {
                //RCLCPP_ERROR(this->get_logger(), "Failed to open file: %s", file_path.c_str()); // Logging for debugging purposes
                logger("[trail_tracer] [ERROR] Failed to parse line in file (trail_tracer function: void open_gps_file())");
            }
        } else {
            //RCLCPP_WARN(this->get_logger(), "Trail name is empty. Cannot open GPS file.");  // Logging for debugging purposes
            logger("[trail_tracer] [WARN] Failed to open file, trail name is empty");
        }
    }

    /**
     * @brief Publish the names of all files in the 'trails' directory.
     * 
     * This function publishes the names of all files in the 'trails' directory as a string message.
     * If no files are found, a warning message is logged. Used for front-end file selection.
     */
   void publish_file_name(){
       std::string directory_path = "trails";
       if(std::filesystem::exists(directory_path)) {
            std::string file_list; // To accumulate all file names
            for(const auto& entry : std::filesystem::directory_iterator(directory_path)){
                if(!file_list.empty()){
                    file_list += ", "; // Separator between file names
                }
                file_list += entry.path().filename().string();
            }
            if(!file_list.empty()) {
                std_msgs::msg::String msg;
                msg.data = file_list;
                file_names_publisher_->publish(msg);
                } else {
                    //RCLCPP_WARN(this->get_logger(), "No files found in directory '%s'.", directory_path.c_str());// Logging for debugging purposes
                    logger("[trail_tracer] [WARN] No files found in directory");
                }
        } else{
            //RCLCPP_WARN(this->get_logger(), "Directory '%s' does not exist.", directory_path.c_str()); // Logging for debugging purposes
            logger("[trail_tracer] [WARN] Directory does not exist");
            }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////// RAMER-DOUGLAS-PEAUCKER ALGORITHM ///////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Calculate the perpendicular distance between a point and a line.
     * 
     * This function calculates the perpendicular distance between a point and a line defined by two points.
     * It takes the point coordinates and the line start and end coordinates as input and returns the perpendicular distance.
     * Code inspired by: https://rosettacode.org/wiki/Ramer-Douglas-Peucker_line_simplification#C++, adapted for this project.
     * 
     * @param point The coordinates of the point.
     * @param lineStart The coordinates of the line start point.
     * @param lineEnd The coordinates of the line end point.
     * @return The perpendicular distance between the point and the line.
     */
    float perpendicularDistance(const Tuple& point, const Tuple& lineStart, const Tuple& lineEnd) {
        float x = std::get<0>(point);
        float y = std::get<1>(point);
        float x1 = std::get<0>(lineStart);
        float y1 = std::get<1>(lineStart);
        float x2 = std::get<0>(lineEnd);
        float y2 = std::get<1>(lineEnd);

        // Compute the differences
        float dx = x2 - x1;
        float dy = y2 - y1;

        // Avoid division by zero
        if (dx == 0 && dy == 0) {
            // The line points are the same
            dx = x - x1;
            dy = y - y1;
            return std::sqrt(dx * dx + dy * dy);
        }

        // Calculation of the projection and the distance
        float t = ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy);
        float nearestX = x1 + t * dx;
        float nearestY = y1 + t * dy;
        float distX = x - nearestX;
        float distY = y - nearestY;

        float distance = std::sqrt(distX * distX + distY * distY);

        // Logging the calculation details, used for debugging purposes
        // RCLCPP_INFO(this->get_logger(), "Perpendicular distance calculation: Point (%f, %f), Line Start (%f, %f), Line End (%f, %f), Distance %.10f",
        //             x, y, x1, y1, x2, y2, distance);

        return distance;
    }

    /**
     * @brief Ramer-Douglas-Peucker algorithm.
     * 
     * This function implements the Ramer-Douglas-Peucker algorithm for simplifying a polyline.
     * It takes a list of points and an epsilon value as input and returns a simplified list of points.
     * code inspired by: https://rosettacode.org/wiki/Ramer-Douglas-Peucker_line_simplification#C++, adapted for this project.
     * 
     * @param points The list of points to simplify.
     * @param epsilon The epsilon value for simplification.
     * @return The simplified list of points.
     */
    FullPoseVector rdp(const FullPoseVector& points, float epsilon) {
        // RCLCPP_INFO(this->get_logger(), "RDP called with %zu points, epsilon: %f", points.size(), epsilon);      // Logging for debugging purposes  
        if (points.size() < 3) {
            return points;
        }
        int maxIndex = 0;
        float maxDistance = 0;
        // Find the point with the maximum distance
        for (std::size_t i = 1; i < points.size() - 1; i++) {
            float distance = perpendicularDistance({std::get<0>(points[i]), std::get<1>(points[i])},
                                                {std::get<0>(points[0]), std::get<1>(points[0])},
                                                {std::get<0>(points.back()), std::get<1>(points.back())});
            if (distance > maxDistance) {
                maxDistance = distance;
                maxIndex = i;
            }
        }
        //RCLCPP_INFO(this->get_logger(), "Max distance: %f, Max index: %d", maxDistance, maxIndex); //  Logging for debuging purposes
        // Check if the maximum distance is greater than epsilon
        if (maxDistance > epsilon) {
            // Recursively simplify the two subparts
            FullPoseVector recResults1 = rdp(FullPoseVector(points.begin(), points.begin() + maxIndex + 1), epsilon);
            FullPoseVector recResults2 = rdp(FullPoseVector(points.begin() + maxIndex, points.end()), epsilon);

            recResults1.pop_back(); // Avoid duplicate point
            recResults1.insert(recResults1.end(), recResults2.begin(), recResults2.end());
            return recResults1;
        } else {
            return {points.front(), points.back()};
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////// WAYPOINT CLACULATION AND MANIPLUTAION ///////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Function to remove remaining destination poses.
     * 
     * This function removes all remaining destination poses from the provided pose list. It starts from the first pose that matches the destination coordinates and removes all subsequent poses.
     * These will be the remaining coordinates to pass after a trail is canceled or paused.
     * 
     * @param poseList The list of poses to remove the remaining destination poses from.
     * @param destinationCoords The destination coordinates to match and start removing from.
     */
    void remainingDestinationPoseList(const std::vector<PoseStamped> poseList, const std::pair<double, double>& destinationCoords) {
        std::vector<PoseStamped> trimmedList;
         bool addToNewList;
         // Iterate through all poses and check if the current pose matches the destination coordinates.
        for (const auto& pose : poseList) {
            double x = pose.pose.position.x;
            double y = pose.pose.position.y;
            // Check if the current pose matches the destination coordinates.
            if (addToNewList || (std::abs(x - destinationCoords.first) < 0.01 && std::abs(y - destinationCoords.second) < 0.01)) {
                addToNewList = true;
                trimmedList.push_back(pose);
            }
        }
        waypoint_sender_->set_waypoints(trimmedList);
    }

    /**
     * @brief Function to process all waypoints.
     * 
     * This function processes all waypoints stored in the 'waypoints_' vector. It calculates the UTM coordinates for each waypoint
     * and stores the poses in the 'poses_' vector. The poses are then sent to the WaypointSender object.
     */
    void process_all_waypoints() {
        poses_.clear();
        bool first_point = true;
        // Process all waypoints by converting them to UTM coordinates and storing the poses.
        for (const auto& [lat, lon] : waypoints_) {
            int zone;
            bool northp;
            double easting, northing;
            GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, easting, northing);
            // Check if this is the first point, if so, there is no previous point to calculate heading from.
            if (first_point) {
                // Initialize last coordinates with the first waypoint's UTM coordinates
                first_point = false;
                createAndStorePose(easting, northing);
                continue;
            }
            createAndStorePose(easting, northing);
        }

        // Logging the number of poses saved for debugging purposes
        // RCLCPP_INFO(this->get_logger(), "Processed and stored %zu poses.", poses_.size());
        waypoint_sender_->set_waypoints(poses_);
    }

    /**
     * @brief Create and store a full pose.
     * 
     * This function creates a full pose using the provided latitude, longitude, quaternion values, and current position.
     * Used when a trail is plotted on map in front-end and orientation is not provided.
     * 
     * @param easting The easting value.
     * @param northing The northing value.
     */
    void createAndStorePose(double easting, double northing){
        // Calculate deltas relative to the last known position
        double delta_x = current_easting_ - easting;
        double delta_y = current_northing_ - northing;

        // Calculate heading from the last waypoint to the current waypoint
        double heading = std::atan2(delta_y, delta_x);

        tf2::Quaternion quaternion;
        quaternion.setRPY(0, 0, heading); // Roll, pitch, yaw

        // Calculate positions relative to the robot's current position
        double relative_x = current_pose_x + (current_easting_ - easting);
        double relative_y = current_pose_y + (current_northing_- northing);

        // Create a new pose
        PoseStamped pose;
        pose.header.stamp = this->get_clock()->now();
        pose.header.frame_id = "map";
        pose.pose.position.x = relative_x;
        pose.pose.position.y = relative_y;
        pose.pose.position.z = 0.0; // Assuming flat terrain
        pose.pose.orientation.x = 0.0;
        pose.pose.orientation.y = 0.0;
        pose.pose.orientation.z = quaternion.z();
        pose.pose.orientation.w = quaternion.w();
        // Store the pose in the vector
        poses_.push_back(pose);
    }

    /**
     * @brief Create and store a full pose.
     * 
     * This function creates a full pose using the provided latitude, longitude, quaternion values, and current position.
     * It then stores the pose in the 'poses_' vector. This is used when a trail is recorded and orientation is provided.
     * 
     * @param lat The latitude value.
     * @param lon The longitude value.
     * @param qx The x component of the quaternion.
     * @param qy The y component of the quaternion.
     * @param qz The z component of the quaternion.
     * @param qw The w component of the quaternion.
     */
    void createAndStoreFullPose(double lat, double lon, float qx, float qy, float qz, float qw){
        int zone;
        bool northp;
        double easting, northing;
        GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, easting, northing);
        // Calculate deltas relative to the last known position
        double relative_x = current_pose_x + (current_easting_ - easting);
        double relative_y = current_pose_y + (current_northing_- northing);
        // Create and store the pose
        PoseStamped pose;   
        pose.header.stamp = this->get_clock()->now();
        pose.header.frame_id = "map";
        pose.pose.position.x = relative_x;
        pose.pose.position.y = relative_y;
        pose.pose.position.z = 0.0; // Assuming flat terrain
        pose.pose.orientation.x = qx;
        pose.pose.orientation.y = qy;
        pose.pose.orientation.z = qz;
        pose.pose.orientation.w = qw;
        // Store the pose in the vector.
        poses_.push_back(pose);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////  VARIABLES  //////////////////////////////////////////////// 
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool is_moving_; // Flag for detecting weather robot is in manual drive or not
    double current_easting_; // Variable to store the current easting UTM coordinate
    double current_northing_; // Variable to store the current northing UTM coordinate
    // Current pose variables
    double current_pose_x;
    double current_pose_y;
    // Orientation variables
    double orientation_x_;
    double orientation_y_;
    double orientation_z_;
    double orientation_w_;
    std::shared_ptr<WaypointSender> waypoint_sender_; // Waypoint sender object
    std::string trail_name_; // Trail name variable
    std::pair<double,double> destination_coords_; // Destination coordinates for the robot to reach after cnaceling a trail.
    std::vector<std::pair<float,float>> waypoints_; // Vector to store waypoints for trail tracing.
    geometry_msgs::msg::Twist::SharedPtr last_received_cmd_;  // Last cmd_vel value recived.
    std::vector<PoseStamped> poses_; // Vector to store poses for trail tracing.
    std::vector<std::tuple<float,float,float,float,float,float>> recorded_waypoints_; // Vector to store recorded waypoints for trail tracing.

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_publisher_; // Publisher for cmd_vel topic.
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr file_names_publisher_; // Publisher for file names.
    rclcpp::Publisher<geometry_msgs::msg::Point>::SharedPtr file_waypoint_publisher_; // Publisher for waypoints.
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr log_publisher_; // Publisher for log messages.

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr web_message_subscription_; // Subscription for web messages.
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr trail_name_subscription_; // Subscription for trail name.
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr joystick_subscription_; // Subscription for joystick values.
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_fix_subscription_; // Subscription for GPS fix messages.
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr load_file_subscription_; // Subscription for loading files.
    rclcpp::Subscription<PoseStamped>::SharedPtr gps_to_utm_subscription_; // Subscription for UTM data.
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_data_subscription_; // Subscription for IMU data.
    rclcpp::Subscription<rcl_interfaces::msg::Log>::SharedPtr log_reader_subscription_; // Subscription for log messages.
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_subscription_; // Subscription for odometry data.
    
    sensor_msgs::msg::NavSatFix::SharedPtr last_gps_msg_; // Last GPS message received.
    rclcpp::TimerBase::SharedPtr timer_update_movement_; // Timer for updating movement.
    rclcpp::TimerBase::SharedPtr timer_publish_file_names_; // Timer for publishing file names.
    std::ofstream gps_file_; // File stream for storing GPS data.
};

// Main function to run the node
int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TrailTracer>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
