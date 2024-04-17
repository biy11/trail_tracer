#include <chrono>
#include <iostream>
#include <thread>
#include <memory>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <cmath> // Include for std::sqrt and std::pow
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <std_msgs/msg/string.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/imu.hpp>

using namespace std::chrono_literals;
//Added abaility to move robot
//Added file storing capabailities
//Added IMU data
class TestCode : public rclcpp::Node {
public:
    TestCode()
    : Node("robot_movement_control"), is_moving_(false), is_turning_(false) {
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/argo_sim/cmd_vel", 10);
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
            "/argo_sim/gps/fix", 10,
            std::bind(&TestCode::gps_fix_callback, this, std::placeholders::_1));

        imu_data_subscription_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/argo_sim/imu/data", 10,
            std::bind(&TestCode::imu_data_callback, this, std::placeholders::_1)
        );

        timer_ = this->create_wall_timer(
            100ms, std::bind(&TestCode::update_movement, this));
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
            open_gps_file(); // Call a function to handle file opening
        } else if (msg->data == "manual-stop") {
            is_moving_ = false;
            if(gps_file_.is_open()){
                gps_file_.close();
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

    void gps_fix_callback(const sensor_msgs::msg::NavSatFix::SharedPtr msg) {
        const double distance_threshold = 5.0; // Minimum distance in meters to consider for recording
        if(is_moving_ && gps_file_.is_open()){
            double distance = calculate_distance(last_latitude_, last_longitude_, msg->latitude, msg->longitude);
            if(distance > distance_threshold || is_turning_){
                last_latitude_ = msg->latitude;
                last_longitude_ = msg->longitude;
                gps_file_ << std::fixed << std::setprecision(8);
                gps_file_ << msg->latitude << ", " << msg->longitude << std::endl;
            }
        }
    }


    void manual_control(const geometry_msgs::msg::Twist::SharedPtr msg) {
        last_received_cmd_ = msg;
    }

    void update_movement() {
        if (is_moving_ && last_received_cmd_) {
            publisher_->publish(*last_received_cmd_);
        }
    }

        // IMU data callback function
    void imu_data_callback(const sensor_msgs::msg::Imu::SharedPtr msg) {
        const float angular_velocity_z_threshold = 0.02; // Threshold for significant rotation around the z-axis
        is_turning_ = std::abs(msg->angular_velocity.z) > angular_velocity_z_threshold;
        if (is_turning_) {
        RCLCPP_INFO(this->get_logger(), "Non-linear (turning). Angular Velocity Z: '%f'", msg->angular_velocity.z);
        }else{ 
         RCLCPP_INFO(this->get_logger(), "Angular Velocity Z: '%f'", msg->angular_velocity.z);
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


    double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
        double earth_radius = 6371000.0; // Earth's radius in meters
        double dLat = (lat2 - lat1) * M_PI / 180.0;
        double dLon = (lon2 - lon1) * M_PI / 180.0;
        lat1 = lat1 * M_PI / 180.0;
        lat2 = lat2 * M_PI / 180.0;

        double a = std::sin(dLat/2) * std::sin(dLat/2) +
                   std::sin(dLon/2) * std::sin(dLon/2) * std::cos(lat1) * std::cos(lat2); 
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a)); 
        double distance = earth_radius * c;

        return distance;
    }

    bool is_moving_;
    bool is_turning_;
    std::string trail_name_;
    geometry_msgs::msg::Twist::SharedPtr last_received_cmd_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr web_message_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr trail_name_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr joystick_subscription_;
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_fix_subscription_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_data_subscription_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::ofstream gps_file_;
    double last_latitude_, last_longitude_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TestCode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}

