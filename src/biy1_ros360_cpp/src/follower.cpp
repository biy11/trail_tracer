#include <chrono>
#include <iostream>
#include <thread>
#include <memory>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <string>
#include <utility>
#include <sstream>
#include <GeographicLib/UTMUPS.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/point.hpp>
#include <std_msgs/msg/string.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <nav2_msgs/action/follow_waypoints.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <rcl_interfaces/msg/log.hpp>
#include <regex>



using namespace std::chrono_literals;
using PoseStamped = geometry_msgs::msg::PoseStamped;



class WaypointSender : public rclcpp::Node
{
public:
    using FollowWaypoints = nav2_msgs::action::FollowWaypoints;
    using GoalHandleFollowWaypoints = rclcpp_action::ClientGoalHandle<FollowWaypoints>;

    explicit WaypointSender(const std::string & name) : Node(name) {
        this->client_ptr_ = rclcpp_action::create_client<FollowWaypoints>(
            this, "/follow_waypoints");
    }

    

    void set_waypoints(const std::vector<geometry_msgs::msg::PoseStamped>& waypoints) {
        auto goal_msg = FollowWaypoints::Goal();
        goal_msg.poses = waypoints;

        if (!client_ptr_->wait_for_action_server()) {
            RCLCPP_ERROR(this->get_logger(), "Action server not available after waiting");
            return;
        }

        auto send_goal_options = rclcpp_action::Client<FollowWaypoints>::SendGoalOptions();
        send_goal_options.result_callback = std::bind(&WaypointSender::goal_result_callback, this, std::placeholders::_1);
        client_ptr_->async_send_goal(goal_msg, send_goal_options);
    }

    void pause() {
        client_ptr_->async_cancel_all_goals();
        RCLCPP_INFO(this->get_logger(), "Waypoint follower paused.");
    }

void resume(const std::vector<geometry_msgs::msg::PoseStamped>& waypoints) {

    RCLCPP_INFO(this->get_logger(), "Resuming waypoint follower.");
    // Resend the waypoints to resume following
    if (!waypoints.empty()) {
        set_waypoints(waypoints);
    }
}

private:
    rclcpp_action::Client<FollowWaypoints>::SharedPtr client_ptr_;
    void goal_result_callback(const GoalHandleFollowWaypoints::WrappedResult & result) {
        switch (result.code) {
            case rclcpp_action::ResultCode::SUCCEEDED:
                RCLCPP_INFO(this->get_logger(), "Goal was achieved.");
                break;
            case rclcpp_action::ResultCode::ABORTED:
                RCLCPP_ERROR(this->get_logger(), "Goal was aborted.");
                break;
            default:
                RCLCPP_ERROR(this->get_logger(), "Unkown result code.");
                break;
        }
    }
};


class TestCode : public rclcpp::Node
{
public:
    TestCode() : Node("robot_movement_control"),current_northing_(0.0), current_easting_(0.0), current_pose_x(0.0), current_pose_y(0.0),waypoint_sender_(std::make_shared<WaypointSender>("waypoint_sender")) {
        this->declare_parameter<std::string>("trails_directory", "trails");
        this->get_parameter("trails_directory", trails_directory_);
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);

        std::filesystem::create_directory(trails_directory_); // Ensure directory exists

        file_names_publisher_ = this->create_publisher<std_msgs::msg::String>("/trail_tracer/trail_files", 5);
        file_waypoint_publisher = this->create_publisher<geometry_msgs::msg::Point>("/trail_tracer/waypoints", 10);

        load_file_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/load_file_topic", 10,
            std::bind(&TestCode::file_loader_callback, this, std::placeholders::_1)); 
        
        gps_to_utm_subscription_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
            "/utm_data", 10,
            std::bind(&TestCode::utm_data_callback, this, std::placeholders::_1));

        odom_subscription_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/odom", 10, std::bind(&TestCode::odom_callback, this, std::placeholders::_1));

        web_message_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/web_messages", 10,
            std::bind(&TestCode::web_messages_callback, this, std::placeholders::_1));

        waypoint_subscriber_ = this->create_subscription<std_msgs::msg::String>(
            "/waypoint_follower/status", 10,
            std::bind(&TestCode::waypoint_callback, this, std::placeholders::_1));

        log_reader_subscription_ = this->create_subscription<rcl_interfaces::msg::Log>(
            "/rosout", 10, std::bind(&TestCode::log_reader_callback, this, std::placeholders::_1));
    
        timer_publish_file_names_ = this->create_wall_timer(
            2s, std::bind(&TestCode::publish_file_name, this));

        waypoints_ = std::vector<std::pair<float,float>>();

    }


private:
    void web_messages_callback(const std_msgs::msg::String::SharedPtr msg) {
        if (msg->data == "pause-trail") {
            waypoint_sender_->pause();
            // RUn function to pause current waypoint following
        } else if (msg->data == "resume-trail"){
            RCLCPP_INFO(this->get_logger(), "Destination coordinates: (%f, %f)", destination_coords_.first, destination_coords_.second);
            remainingDestinationPoseList(poses_,destination_coords_);
            //waypoint_sender_->resume(poses_);
            // RUn function to end current waypoint following
        }
        RCLCPP_INFO(this->get_logger(), "Received command: '%s'", msg->data.c_str());
    }

    void log_reader_callback(const rcl_interfaces::msg::Log::SharedPtr msg){
        // Check if the message is from bt_navigator and contains the navigation text
        if (msg->name == "bt_navigator" && msg->msg.find("Begin navigating") != std::string::npos) {
            std::regex coord_regex("\\((-?\\d+\\.\\d+),\\s*(-?\\d+\\.\\d+)\\)");
            std::smatch matches;
            std::string::const_iterator searchStart(msg->msg.cbegin());
            std::vector<std::pair<float, float>> coordinates;

            // Find all matches
            while (std::regex_search(searchStart, msg->msg.cend(), matches, coord_regex)) {
                float x = std::stof(matches[1].str());
                float y = std::stof(matches[2].str());
                coordinates.push_back(std::make_pair(x, y));
                searchStart = matches.suffix().first;
            }

            // Check if we have at least two coordinate pairs
            if (coordinates.size() >= 2) {
                destination_coords_ = coordinates[1]; // Select the second pair
                RCLCPP_INFO(this->get_logger(), "Destination coordinates: (%f, %f)", destination_coords_.first, destination_coords_.second);
                // Here you can store or use the destination coordinates as needed
            }
        }
    }

    void waypoint_callback(const std_msgs::msg::String::SharedPtr msg)
    {
        // Parse the message to find the waypoint index
        RCLCPP_INFO(this->get_logger(), "Received waypoint status: '%s'", msg->data.c_str());
        // Implement logic to determine and store the last reached waypoint
    }

    void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
    {
        current_pose_x = msg->pose.pose.position.x;
        current_pose_y = msg->pose.pose.position.y;
    }

    void utm_data_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg){
        current_easting_ = msg->pose.position.x;
        current_northing_ = msg->pose.position.y;
    }

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
        RCLCPP_WARN(this->get_logger(), "No files found in directory '%s'.", directory_path.c_str());
    }
    } else {
        RCLCPP_WARN(this->get_logger(), "Directory '%s' does not exist.", directory_path.c_str());
        }
    }


    void remainingDestinationPoseList(const std::vector<geometry_msgs::msg::PoseStamped> poseList, const std::pair<double, double>& destinationCoords) {
        std::vector<geometry_msgs::msg::PoseStamped> trimmedList;
         bool addToNewList;
        for (const auto& pose : poseList) {
            double x = pose.pose.position.x;
            double y = pose.pose.position.y;

            if (addToNewList || (std::abs(x - destinationCoords.first) < 0.01 && std::abs(y - destinationCoords.second) < 0.01)) {
                // Start adding from this pose if it matches the destination coordinates
                addToNewList = true;
                trimmedList.push_back(pose);
            }
        }
        // Print the trimmed list
        std::cout << "Trimmed List:" << std::endl;
        for (const auto& pose : trimmedList) {
            std::cout << "Pose at (" << pose.pose.position.x << ", " << pose.pose.position.y << ")" << std::endl;
        }

        waypoint_sender_->set_waypoints(trimmedList);
    }

    void file_loader_callback(const std_msgs::msg::String::SharedPtr msg) {
        std::string file_path = "trails/" + msg->data;
        std::ifstream file(file_path);
        if (!file.is_open()) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open file: %s", file_path.c_str());
            return;
        }
        waypoints_.clear();
        poses_.clear();
        bool full_pose_format = false;
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            float lat, lon, qx, qy, qz, qw;
            char comma;
            std::vector<float> values;
            std::string value;
            while (std::getline(iss, value, ',')){
                values.push_back(std::stof(value));
            }
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
                RCLCPP_ERROR(this->get_logger(), "Error parsing line: %s", line.c_str());
                continue;
            }
            waypoints_.emplace_back(lat, lon);
            geometry_msgs::msg::Point point_msg;
            point_msg.x = lat; // Assign latitude to x
            point_msg.y = lon; // Assign longitude to y
            point_msg.z = 0.0; // Set z to 0.0 if not used
            file_waypoint_publisher->publish(point_msg);

            if(values.size() == 6){
                createAndStoreFullPose(lat,lon,qx,qy,qz,qw);
            }
        }
        file.close();
        if(full_pose_format){
            waypoint_sender_->set_waypoints(poses_);
            RCLCPP_INFO(this->get_logger(), "Processed and stored %zu poses from new format data.", poses_.size());
        } else{
            process_all_waypoints(); 
            }
    }

    void process_all_waypoints() {
        poses_.clear();

        double last_easting = 0.0, last_northing = 0.0;
        bool first_point = true;

        for (const auto& [lat, lon] : waypoints_) {
            int zone;
            bool northp;
            double easting, northing;
            GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, easting, northing);

            if (first_point) {
                // Initialize last coordinates with the first waypoint's UTM coordinates
                last_easting = easting;
                last_northing = northing;
                first_point = false;
                createAndStorePose(easting, northing);
                continue;
            }
            createAndStorePose(easting, northing);
            // Update the last known UTM coordinates to current for the next iteration
            last_easting = easting;
            last_northing = northing;
        }

        // log the number of poses saved
        RCLCPP_INFO(this->get_logger(), "Processed and stored %zu poses.", poses_.size());
        waypoint_sender_->set_waypoints(poses_);
    }

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

        geometry_msgs::msg::PoseStamped pose;
        pose.header.stamp = this->get_clock()->now();
        pose.header.frame_id = "map";
        pose.pose.position.x = relative_x;
        pose.pose.position.y = relative_y;
        pose.pose.position.z = 0.0; // Assuming flat terrain
        pose.pose.orientation.x = 0.0;
        pose.pose.orientation.y = 0.0;
        pose.pose.orientation.z = quaternion.z();
        pose.pose.orientation.w = quaternion.w();

        poses_.push_back(pose);
        RCLCPP_INFO(this->get_logger(),
            "Pose: Position (x: %f, y: %f, z: %f), Orientation (x: %f, y: %f, z: %f, w: %f)",
            pose.pose.position.x, pose.pose.position.y, pose.pose.position.z,
            pose.pose.orientation.x, pose.pose.orientation.y, pose.pose.orientation.z, pose.pose.orientation.w);
    }

    void createAndStoreFullPose(double lat, double lon, float qx, float qy, float qz, float qw){
        int zone;
        bool northp;
        double easting, northing;
        GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, easting, northing);

        double relative_x = current_pose_x + (current_easting_ - easting);
        double relative_y = current_pose_y + (current_northing_- northing);

        geometry_msgs::msg::PoseStamped pose;
        pose.header.stamp = this->get_clock()->now();
        pose.header.frame_id = "map";
        pose.pose.position.x = relative_x;
        pose.pose.position.y = relative_y;
        pose.pose.position.z = 0.0; // Assuming flat terrain
        pose.pose.orientation.x = qx;
        pose.pose.orientation.y = qy;
        pose.pose.orientation.z = qz;
        pose.pose.orientation.w = qw;

        poses_.push_back(pose);
        RCLCPP_INFO(this->get_logger(),
            "Pose with orientation: Position (x: %f, y: %f, z: %f), Orientation (x: %f, y: %f, z: %f, w: %f)",
            pose.pose.position.x, pose.pose.position.y, pose.pose.position.z,
            pose.pose.orientation.x, pose.pose.orientation.y, pose.pose.orientation.z, pose.pose.orientation.w);
    }

    double current_northing_;
    double current_easting_;
    double current_pose_x;
    double current_pose_y;
    std::vector<std::pair<float,float>> waypoints_;
    std::pair<double,double> destination_coords_; 
    std::shared_ptr<WaypointSender> waypoint_sender_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr load_file_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr gps_to_utm_subscription_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_subscription_; 
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr file_names_publisher_;
    rclcpp::Publisher<geometry_msgs::msg::Point>::SharedPtr file_waypoint_publisher;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr web_message_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr waypoint_subscriber_;
    rclcpp::Subscription<rcl_interfaces::msg::Log>::SharedPtr log_reader_subscription_;
    std::string trails_directory_;
    std::vector<geometry_msgs::msg::PoseStamped> poses_;
    rclcpp::TimerBase::SharedPtr timer_publish_file_names_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;


};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TestCode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}