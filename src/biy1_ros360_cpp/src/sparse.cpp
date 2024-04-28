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


using namespace std::chrono_literals;
//Added abaility to move robot
//Added file storing capabailities
//Added IMU data
//Added files publishe
//timer for file publisher
//load file sub
// Added goal publisher 
// added sparse coordinate storing
class TestCode : public rclcpp::Node {
public:
    TestCode()
    : Node("robot_movement_control"), is_moving_(false), is_turning_(false), first_coord_stored_(false), current_easting_(0.0), current_northing_(0.0),last_angular_vel_(0.0){
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
        file_names_publisher_ = this->create_publisher<std_msgs::msg::String>("/trail_files", 10);
        goal_pose_publisher_= this->create_publisher<geometry_msgs::msg::PoseStamped>("/goal_pose", 10);

        web_message_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/web_messages", 10,
            std::bind(&TestCode::web_messages_callback, this, std::placeholders::_1));

        trail_name_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/trail_name_topic", 10,
            std::bind(&TestCode::trail_name_callback, this, std::placeholders::_1));

        joystick_subscription_ = this->create_subscription<geometry_msgs::msg::Twist>(
            "/joystick_value", 10,
            std::bind(&TestCode::manual_control, this, std::placeholders::_1));

        gps_fix_subscription_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
            "/gps/fix", 10,
            std::bind(&TestCode::gps_fix_callback, this, std::placeholders::_1));
        
        load_file_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/load_file_topic", 10,
            std::bind(&TestCode::file_loader_callback, this, std::placeholders::_1));

        gps_to_utm_subscription_ = this->create_subscription<geometry_msgs::msg::PoseStamped>(
            "/utm_data", 10,
            std::bind(&TestCode::utm_data_callback, this, std::placeholders::_1));

        imu_data_subscription_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/imu/data", 10,
            std::bind(&TestCode::imu_data_callback, this, std::placeholders::_1));

        timer_update_movement_ = this->create_wall_timer(
            100ms, std::bind(&TestCode::update_movement, this));
        timer_publish_file_names_ = this->create_wall_timer(
            2s, std::bind(&TestCode::publish_file_name, this));

        waypoints_ = std::vector<std::pair<float,float>>();
        utm_coords = std::vector<std::tuple<int,bool, double, double>>();
        
    }

    ~TestCode(){
        if(gps_file_.is_open()){
            gps_file_.close();
        }
    }

private:
    void web_messages_callback(const std_msgs::msg::String::SharedPtr msg) {
        if (msg->data == "manual-start") {
            is_moving_ = true;
            //open_gps_file(); // Call to a function to handle file opening
        } else if (msg->data == "manual-move"){
            is_moving_ = true;
        }else if (msg->data == "manual-stop") {
            RCLCPP_INFO(this->get_logger(), "Original number of waypoints: %zu", recorded_waypoints_.size());
            is_moving_ = false;
            if(!recorded_waypoints_.empty()){
                auto simplified_points = rdp(recorded_waypoints_, 0.0001);
                RCLCPP_INFO(this->get_logger(), "Simplified number of waypoints: %zu", simplified_points.size());
                save_to_file(simplified_points);
            }
            recorded_waypoints_.clear();
            if(gps_file_.is_open()){
                gps_file_.close(); //close 
            }
        }
        RCLCPP_INFO(this->get_logger(), "Received command: '%s'", msg->data.c_str());
    }

    void trail_name_callback(const std_msgs::msg::String::SharedPtr msg){
        trail_name_ = msg->data;
        RCLCPP_INFO(this->get_logger(), "Trail name set to: '%s'", trail_name_.c_str());
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

    void gps_fix_callback(const sensor_msgs::msg::NavSatFix::SharedPtr msg) {
        last_gps_msg_ = msg; //Update last known location
        if(is_moving_){
            recorded_waypoints_.emplace_back(msg->latitude,msg->longitude, orientation_x_, orientation_y_, orientation_z_, orientation_w_);
        }
    }

    float perpendicularDistance(const std::tuple<float, float>& point, const std::tuple<float, float>& lineStart, const std::tuple<float, float>& lineEnd) {
        float x = std::get<0>(point);
        float y = std::get<1>(point);
        float x1 = std::get<0>(lineStart);
        float y1 = std::get<1>(lineStart);
        float x2 = std::get<0>(lineEnd);
        float y2 = std::get<1>(lineEnd);

        float numerator = std::abs((y2 - y1) * x - (x2 - x1) * y + x2 * y1 - y2 * x1);
        float denominator = std::sqrt((y2 - y1) * (y2 - y1) + (x2 - x1) * (x2 - x1));
        return numerator / denominator;
    }

    std::vector<std::tuple<float, float, float, float, float, float>> rdp(const std::vector<std::tuple<float, float, float, float, float, float>>& points, float epsilon) {
        if (points.size() < 3) {
            return points;
        }

        int maxIndex = 0;
        float maxDistance = 0;
        for (int i = 1; i < points.size() - 1; i++) {
            float distance = perpendicularDistance({std::get<0>(points[i]), std::get<1>(points[i])},
                                                {std::get<0>(points[0]), std::get<1>(points[0])},
                                                {std::get<0>(points.back()), std::get<1>(points.back())});
            if (distance > maxDistance) {
                maxDistance = distance;
                maxIndex = i;
            }
        }

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

    void save_to_file(const std::vector<std::tuple<float, float, float, float, float, float>>& points) {
        if (gps_file_.is_open()) {
            gps_file_ << std::fixed << std::setprecision(8);
            for (const auto& [lat, lon, x, y, z, w] : points) {
                gps_file_ << lat << ", " << lon << ", " << x << ", " << y << ", " << z << ", " << w << std::endl;
            }
            gps_file_.close(); // Close and store last position
        }
    }

    void manual_control(const geometry_msgs::msg::Twist::SharedPtr msg) {
        last_received_cmd_ = msg;
        double current_angualr_vel = msg->angular.z;

        //Threshold for detecting turns
        constexpr double ANGULAR_VEL_THRESHOLD = 0.001;

        if(std::abs(current_angualr_vel - last_angular_vel_) > ANGULAR_VEL_THRESHOLD){
            //RCLCPP_INFO(this->get_logger(), "Recording turn: %f, %f", current_angualr_vel, last_angular_vel_);  
            is_turning_ = true;
        } else{
            //RCLCPP_INFO(this->get_logger(), "STRIGHT: %f, %f", current_angualr_vel, last_angular_vel_);
            is_turning_ = false;
        }
        last_angular_vel_ = current_angualr_vel;
    }

    void utm_data_callback(const geometry_msgs::msg::PoseStamped::SharedPtr msg){
        current_easting_ = msg->pose.position.x;
        current_northing_ = msg->pose.position.y;
    }

    void gps_to_utm_converter() {
        utm_coords.clear();
        for (const auto& [lat, lon] : waypoints_) {
            int zone;
            bool northp;
            double easting, northing;
            GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, easting, northing);

            // Calculate relative positions
            double rel_easting =  current_easting_ - easting;
            double rel_northing = current_northing_ - northing;

            // Create and publish PoseStamped message with relative coordinates
            geometry_msgs::msg::PoseStamped goal_pose;
            goal_pose.header.stamp = this->get_clock()->now();
            goal_pose.header.frame_id = "map";  // Use the correct frame_id for your application

            goal_pose.pose.position.x = rel_easting;
            goal_pose.pose.position.y = rel_northing;
            goal_pose.pose.position.z = 0.0;

            // No rotation - assuming all poses face forward; modify as necessary
            goal_pose.pose.orientation.x = 0.0;
            goal_pose.pose.orientation.y = 0.0;
            goal_pose.pose.orientation.z = 0.0;
            goal_pose.pose.orientation.w = 1.0;

            goal_pose_publisher_->publish(goal_pose);

            RCLCPP_INFO(this->get_logger(), "Published relative UTM goal pose: Easting %f, Northing %f", rel_easting, rel_northing);
        }
    }

    void update_movement() {
        if (is_moving_ && last_received_cmd_) {
            publisher_->publish(*last_received_cmd_);
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
        } else {
        RCLCPP_WARN(this->get_logger(), "Directory '%s' does not exist.", directory_path.c_str());
        }
    }


    void file_loader_callback(const std_msgs::msg::String::SharedPtr msg){
        std::string file_path = "trails/" + msg->data; 
        std::ifstream file(file_path);

        if (!file.is_open()) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open file: %s", file_path.c_str());
            return;
        }

        waypoints_.clear(); // Clear any existing waypoints before loading new ones
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            float lat, lon;
            char comma;

            if (!(iss >> lat >> comma >> lon)) {
                // Error parsing the line
                RCLCPP_ERROR(this->get_logger(), "Error parsing line in file: %s", line.c_str());
                continue; // Skip this line
            }

            waypoints_.push_back(std::make_pair(lat, lon));
        }

        file.close();
        RCLCPP_INFO(this->get_logger(), "Loaded %zu waypoints from %s", waypoints_.size(), file_path.c_str());
        gps_to_utm_converter();    
    }


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////  VARIABLES  //////////////////////////////////////////////// 
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool is_moving_; // Flag for detecting weather robot is in manual drive or not
    bool is_turning_; // Flag for detecting weather robot is travvelling linear or not
    bool first_coord_stored_; // Variable to store first known gps location of robot
    double current_easting_; // Variable to store the current easting UTM coordinate
    double current_northing_; // Variable to store the current northing UTM coordinate
    double last_angular_vel_; // Variable to store the last known angualr.z value from cmd_vel
    double orientation_x_;
    double orientation_y_;
    double orientation_z_;
    double orientation_w_;
    std::string trail_name_; // Variable to store trail name 
    std::vector<std::pair<float,float>> waypoints_;
    std::vector<std::tuple<float,float,float,float,float,float>> recorded_waypoints_;
    std::vector<std::tuple<int,bool,double,double>> utm_coords;
    geometry_msgs::msg::Twist::SharedPtr last_received_cmd_;  // Last cmd_vel value recived
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr file_names_publisher_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr goal_pose_publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr web_message_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr trail_name_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr joystick_subscription_;
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_fix_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr load_file_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr gps_to_utm_subscription_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_data_subscription_;
    sensor_msgs::msg::NavSatFix::SharedPtr last_gps_msg_; 
    rclcpp::TimerBase::SharedPtr timer_update_movement_;
    rclcpp::TimerBase::SharedPtr timer_publish_file_names_;
    std::ofstream gps_file_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TestCode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
