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


using namespace std::chrono_literals;
//Added abaility to move robot
//Added file storing capabailities
//Added IMU data
//Added files publishe
//timer for file publisher
//load file sub
class TestCode : public rclcpp::Node {
public:
    TestCode()
    : Node("robot_movement_control"), is_moving_(false), is_turning_(false) {
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/argo_sim/cmd_vel", 10);
        file_names_publisher_ = this->create_publisher<std_msgs::msg::String>("/trail_files", 10);

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
            std::bind(&TestCode::imu_data_callback, this, std::placeholders::_1));
        
        load_file_subscription_ = this->create_subscription<std_msgs::msg::String>(
            "/load_file_topic", 10,
            std::bind(&TestCode::file_loader_callback, this, std::placeholders::_1));

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
            open_gps_file(); // Call to a function to handle file opening
        } else if (msg->data == "manual-move"){
            is_moving_ = true;
        }else if (msg->data == "manual-stop") {
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
        if(is_moving_ && gps_file_.is_open() && is_turning_){
            gps_file_ << std::fixed << std::setprecision(8);
            gps_file_ << msg->latitude << ", " << msg->longitude << std::endl;
        }
    }


    void manual_control(const geometry_msgs::msg::Twist::SharedPtr msg) {
        last_received_cmd_ = msg;
    }

    void gps_to_utm_converter(){
        utm_coords.clear();
        for(const auto& [lat, lon] : waypoints_){
            int zone;
            bool northp;
            double easting, northing;
            GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, easting, northing);
            utm_coords.push_back(std::make_tuple(zone,northp, easting, northing));

            RCLCPP_INFO(this->get_logger(), "GPS (%f, %f) -> UTM Zone: %d%s, Easting: %f, Northing: %f",
                lat, lon, zone, northp ? "N" : "S", easting, northing);
        }
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
        // if (is_turning_) {
        // RCLCPP_INFO(this->get_logger(), "Non-linear (turning). Angular Velocity Z: '%f'", msg->angular_velocity.z);
        // }else{ 
        //  RCLCPP_INFO(this->get_logger(), "Angular Velocity Z: '%f'", msg->angular_velocity.z);
        // }
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


    bool is_moving_;
    bool is_turning_;
    std::string trail_name_;
    std::vector<std::pair<float,float>> waypoints_;
    std::vector<std::tuple<int,bool,double,double>> utm_coords;
    geometry_msgs::msg::Twist::SharedPtr last_received_cmd_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr file_names_publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr web_message_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr trail_name_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr joystick_subscription_;
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr gps_fix_subscription_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_data_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr load_file_subscription_;
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
