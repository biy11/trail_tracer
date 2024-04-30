/*
 * @(#) converter_node.cpp 0.2 29/04/2021 
 * 
 * Copyright (c) 2024 Aberystwth University
 * All rights reserved.
*/

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <GeographicLib/UTMUPS.hpp>
#include <GeographicLib/Geodesic.hpp>
/*
 * @author Bilal [biy1]
 * @version 0.1 - Initial creation
 * @version 0.2 - Added comments.
*/

// Class to convert GPS coordinates to UTM coordinates
class GPS2UTMConverter : public rclcpp::Node {
public:
    // Constructor for the class
    GPS2UTMConverter()
    : Node("gps_to_utm_conerter"){
        //Subscriber to GPS data
        subscription_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
            "/gps/fix", 10, std::bind(&GPS2UTMConverter::gps_callback, this, std::placeholders::_1)
        );
        //Publisher for UTM data
        publisher_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("utm_data", 10);
    }

private:
    /*
     * Callback function for the GPS data.
     * Converts the GPS data to UTM data and publishes it.
     * 
     * @param msg: GPS data message
     */
    void gps_callback(const sensor_msgs::msg::NavSatFix::SharedPtr msg){
        // Convert GPS to UTM
        double lat = msg->latitude;
        double lng = msg->longitude;
        int zone;
        bool northp;
        double x, y;
        // Convert GPS to UTM using GeographicLib
        GeographicLib::UTMUPS::Forward(lat,lng,zone,northp, x, y);
        // Publish the UTM data
        auto utm_msg = geometry_msgs::msg::PoseStamped();
        utm_msg.header.stamp = this->now();
        utm_msg.header.frame_id = "utm";
        utm_msg.pose.position.x = x;
        utm_msg.pose.position.y = y;

        publisher_->publish(utm_msg);
        //RCLCPP_INFO(this->get_logger(), "Published UTM coordinates: [%f, %f]", x, y); // For debugging
    }

    // Subscriber and publisher for GPS and UTM data
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr subscription_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr publisher_;
};
// Main function to create the node
int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<GPS2UTMConverter>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
