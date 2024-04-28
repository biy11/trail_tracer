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

//Added abaility to move robot
//Added file storing capabailities
//Added IMU data
//Added files publishe
//timer for file publisher
//load file sub
// Added goal publisher 
// added sparse coordinate storing via rbd algo


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
    bool is_paused_;

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

class TrailTracer : public rclcpp::Node {
public:
    TrailTracer()
    : Node("robot_movement_control"), is_moving_(false), current_easting_(0.0), current_northing_(0.0), 
    current_pose_x(0.0), current_pose_y(0.0),waypoint_sender_(std::make_shared<WaypointSender>("waypoint_sender"))
    {
        ////////////////// PUBLISHERS FOR CMD_VEL, FILE NAMES AND WAYPOINTS //////////////////
        cmd_vel_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
        file_names_publisher_ = this->create_publisher<std_msgs::msg::String>("trail_tracer/trail_files", 10);
        file_waypoint_publisher_ = this->create_publisher<geometry_msgs::msg::Point>("/trail_tracer/waypoints", 10);

        //////////////////////////////// SUBSCRIPTIONS ////////////////////////////////
        web_message_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/web_gui/web_messages", 10,
            std::bind(&TrailTracer::web_messages_callback, this, std::placeholders::_1));

        trail_name_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/web_gui/trail_name_topic", 10,
            std::bind(&TrailTracer::trail_name_callback, this, std::placeholders::_1));

        joystick_subscription_ = this->create_subscription<geometry_msgs::msg::Twist>(
            "/web_gui/joystick_value", 10,
            std::bind(&TrailTracer::manual_control, this, std::placeholders::_1));

        load_file_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/web_gui/load_file_topic", 10,
            std::bind(&TrailTracer::file_loader_callback, this, std::placeholders::_1));

        gps_fix_subscription_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
            "/gps/fix", 10,
            std::bind(&TrailTracer::gps_fix_callback, this, std::placeholders::_1));

        gps_to_utm_subscription_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
            "/utm_data", 10,
            std::bind(&TrailTracer::utm_data_callback, this, std::placeholders::_1));

        imu_data_subscription_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/imu/data", 10,
            std::bind(&TrailTracer::imu_data_callback, this, std::placeholders::_1));

        odom_subscription_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/odom", 10, std::bind(&TrailTracer::odom_callback, this, std::placeholders::_1));
        
        log_reader_subscription_ = this->create_subscription<rcl_interfaces::msg::Log>(
            "/rosout", 10, std::bind(&TrailTracer::log_reader_callback, this, std::placeholders::_1));

        timer_update_movement_ = this->create_wall_timer(
            100ms, std::bind(&TrailTracer::update_movement, this));

        timer_publish_file_names_ = this->create_wall_timer(
            1s, std::bind(&TrailTracer::publish_file_name, this));
        
    }

    ~TrailTracer(){
        if(gps_file_.is_open()){
            gps_file_.close();
        }
    }

private:

    /////////////////////////////////////////////////////////////////////////////////////// 
    ///////////////////////////////// CALLBACK FUNCTIONS //////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////

    void web_messages_callback(const std_msgs::msg::String::SharedPtr msg) {
        if (msg->data == "manual-start") {
            is_moving_ = true;
            //open_gps_file(); // Call to a function to handle file opening
        } else if (msg->data == "manual-move"){
            is_moving_ = true;
        }else if (msg->data == "manual-stop") {
            //RCLCPP_INFO(this->get_logger(), "Original number of waypoints: %zu", recorded_waypoints_.size()); //Logging for debugging purposes
            is_moving_ = false;
            if(!recorded_waypoints_.empty()){
                auto simplified_points = rdp(recorded_waypoints_, 0.000036);
                RCLCPP_INFO(this->get_logger(), "Simplified number of waypoints: %zu", simplified_points.size());
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
        RCLCPP_INFO(this->get_logger(), "Received command: '%s'", msg->data.c_str()); // Logging for debugging purposes
    }

    void trail_name_callback(const std_msgs::msg::String::SharedPtr msg){
        trail_name_ = msg->data;
        RCLCPP_INFO(this->get_logger(), "Trail name set to: '%s'", trail_name_.c_str()); // Logging for debugging purposes
        if (is_moving_) {
            open_gps_file(); // Re-open the file with the new trail name if already moving
        }
    }

    // IMU data callback function
    void imu_data_callback(const sensor_msgs::msg::Imu::SharedPtr msg) {
        orientation_x_ = msg->orientation.x;
        orientation_y_ = msg->orientation.y;
        orientation_z_ = msg->orientation.z;
        orientation_w_ = msg->orientation.w;
    }

    // Odom Callback
    void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg){
        current_pose_x = msg->pose.pose.position.x;
        current_pose_y = msg->pose.pose.position.y;
    }
    
    // Gps callback
    void gps_fix_callback(const sensor_msgs::msg::NavSatFix::SharedPtr msg) {
        last_gps_msg_ = msg; //Update last known location

        if(is_moving_){
            recorded_waypoints_.emplace_back(msg->latitude,msg->longitude, orientation_x_, orientation_y_, orientation_z_, orientation_w_);
        }
    }
    // Callback for ros log reader
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

    // Cllabck for UTM coordinate publsiher
    void utm_data_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg){
        current_easting_ = msg->pose.position.x;
        current_northing_ = msg->pose.position.y;
    }

    // Callback for loading files
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
            file_waypoint_publisher_->publish(point_msg);

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
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////// FILE OPERATION FUNCTIONS ////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void save_to_file(const std::vector<std::tuple<float, float, float, float, float, float>>& points) {
        if (gps_file_.is_open()) {
            gps_file_ << std::fixed << std::setprecision(8);
            for (const auto& [lat, lon, x, y, z, w] : points) {
                gps_file_ << lat << ", " << lon << ", " << x << ", " << y << ", " << z << ", " << w << std::endl;
            }
            gps_file_.close(); // Close and store last position
        }
    }

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
                RCLCPP_ERROR(this->get_logger(), "Failed to open file: %s", file_path.c_str());
            }
        } else {
            RCLCPP_WARN(this->get_logger(), "Trail name is empty. Cannot open GPS file.");
        }
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
        } else{
            RCLCPP_WARN(this->get_logger(), "Directory '%s' does not exist.", directory_path.c_str());
            }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////// RAMER-DOUGLAS-PEAUCKER ALGORITHM ///////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    float perpendicularDistance(const std::tuple<float, float>& point, const std::tuple<float, float>& lineStart, const std::tuple<float, float>& lineEnd) {
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

    std::vector<std::tuple<float, float, float, float, float, float>> rdp(const std::vector<std::tuple<float, float, float, float, float, float>>& points, float epsilon) {
        // RCLCPP_INFO(this->get_logger(), "RDP called with %zu points, epsilon: %f", points.size(), epsilon);      // Logging for debugging purposes  
        if (points.size() < 3) {
            return points;
        }

        int maxIndex = 0;
        float maxDistance = 0;
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

        if (maxDistance > epsilon) {
            std::vector<std::tuple<float, float, float, float, float, float>> recResults1 = rdp(std::vector<std::tuple<float, float, float, float, float, float>>(points.begin(), points.begin() + maxIndex + 1), epsilon);
            std::vector<std::tuple<float, float, float, float, float, float>> recResults2 = rdp(std::vector<std::tuple<float, float, float, float, float, float>>(points.begin() + maxIndex, points.end()), epsilon);

            recResults1.pop_back(); // Avoid duplicate point
            recResults1.insert(recResults1.end(), recResults2.begin(), recResults2.end());
            return recResults1;
        } else {
            return {points.front(), points.back()};
        }
    }



    void manual_control(const geometry_msgs::msg::Twist::SharedPtr msg) {
        last_received_cmd_ = msg;
    }
    

    void update_movement() {
        if (is_moving_ && last_received_cmd_) {
            cmd_vel_publisher_->publish(*last_received_cmd_);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////// WAYPOINT CLACULATION AND MANIPLUTAION ///////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

    void process_all_waypoints() {
        poses_.clear();

        bool first_point = true;

        for (const auto& [lat, lon] : waypoints_) {
            int zone;
            bool northp;
            double easting, northing;
            GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, easting, northing);

            if (first_point) {
                // Initialize last coordinates with the first waypoint's UTM coordinates
                first_point = false;
                createAndStorePose(easting, northing);
                continue;
            }
            createAndStorePose(easting, northing);
            // Update the last known UTM coordinates to current for the next iteration
        }

        // Logging the number of poses saved for debugging purposes
        // RCLCPP_INFO(this->get_logger(), "Processed and stored %zu poses.", poses_.size());
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
        // Logging coordinates for debugging purpuses
        // RCLCPP_INFO(this->get_logger(),
        //     "Pose: Position (x: %f, y: %f, z: %f), Orientation (x: %f, y: %f, z: %f, w: %f)",
        //     pose.pose.position.x, pose.pose.position.y, pose.pose.position.z,
        //     pose.pose.orientation.x, pose.pose.orientation.y, pose.pose.orientation.z, pose.pose.orientation.w);
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
        // Logging remianig cooridnates for debugging purposes
        // RCLCPP_INFO(this->get_logger(),
        //     "Pose with orientation: Position (x: %f, y: %f, z: %f), Orientation (x: %f, y: %f, z: %f, w: %f)",
        //     pose.pose.position.x, pose.pose.position.y, pose.pose.position.z,
        //     pose.pose.orientation.x, pose.pose.orientation.y, pose.pose.orientation.z, pose.pose.orientation.w);
    }




    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////  VARIABLES  //////////////////////////////////////////////// 
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool is_moving_; // Flag for detecting weather robot is in manual drive or not
    double current_easting_; // Variable to store the current easting UTM coordinate
    double current_northing_; // Variable to store the current northing UTM coordinate
    double current_pose_x;
    double current_pose_y;
    std::shared_ptr<WaypointSender> waypoint_sender_;
    double orientation_x_;
    double orientation_y_;
    double orientation_z_;
    double orientation_w_;
    std::string trail_name_; // Variable to store trail name 
    std::pair<double,double> destination_coords_; 
    std::vector<std::pair<float,float>> waypoints_;
    geometry_msgs::msg::Twist::SharedPtr last_received_cmd_;  // Last cmd_vel value recived
    std::vector<geometry_msgs::msg::PoseStamped> poses_;
    std::vector<std::tuple<float,float,float,float,float,float>> recorded_waypoints_;


    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr file_names_publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr web_message_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr trail_name_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr joystick_subscription_;
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_fix_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr load_file_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr gps_to_utm_subscription_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_data_subscription_;
    rclcpp::Publisher<geometry_msgs::msg::Point>::SharedPtr file_waypoint_publisher_;
    rclcpp::Subscription<rcl_interfaces::msg::Log>::SharedPtr log_reader_subscription_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_subscription_; 
    sensor_msgs::msg::NavSatFix::SharedPtr last_gps_msg_; 
    rclcpp::TimerBase::SharedPtr timer_update_movement_;
    rclcpp::TimerBase::SharedPtr timer_publish_file_names_;
    std::ofstream gps_file_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TrailTracer>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
