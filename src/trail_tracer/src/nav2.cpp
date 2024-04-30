#include <rclcpp/rclcpp.hpp>
#include <GeographicLib/UTMUPS.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <vector>
#include <tuple>

using namespace GeographicLib;
using std::vector;
using std::tuple;
using std::make_tuple;

class NavTwo : public rclcpp::Node {
public:
    NavTwo() : Node("Nav2") {
        // List of GPS coordinates (latitude, longitude)
        vector<tuple<double, double>> gps_coordinates = {
            {52.41641815, -4.06503560},
            {52.41645905, -4.06508388},
            {52.41650322, -4.06511606},
            {52.41653103, -4.06514020},
            {52.41656375, -4.06515630},
            {52.41659810, -4.06517775}
        };

        // Vector to store converted UTM coordinates (zone, northp, easting, northing)
        vector<tuple<int, bool, double, double>> utm_coordinates;

        for (const auto& [lat, lon] : gps_coordinates) {
            int zone;
            bool northp;
            double easting, northing;
            UTMUPS::Forward(lat, lon, zone, northp, easting, northing);
            utm_coordinates.push_back(make_tuple(zone, northp, easting, northing));

            RCLCPP_INFO(this->get_logger(), "GPS (%f, %f) -> UTM Zone: %d%s, Easting: %f, Northing: %f",
                        lat, lon, zone, northp ? "N" : "S", easting, northing);
        }

        // Initialize the goal publisher
        goal_publisher_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("/goal_pose", 10);

        // Convert and publish each UTM coordinate as a goal
        for (const auto& [zone, northp, easting, northing] : utm_coordinates) {
            publishGoal(easting, northing);
        }
    }

private:
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr goal_publisher_;

    void publishGoal(double easting, double northing) {
        // Assume these UTM coordinates are directly applicable or have been translated to the map frame
        geometry_msgs::msg::PoseStamped goal_msg;
        goal_msg.header.stamp = this->get_clock()->now();
        goal_msg.header.frame_id = "map"; // Ensure this matches your operational context
        goal_msg.pose.position.x = easting;
        goal_msg.pose.position.y = northing;
        goal_msg.pose.orientation.w = 1.0; // Assuming no orientation, facing straight ahead

        goal_publisher_->publish(goal_msg);
        RCLCPP_INFO(this->get_logger(), "Publishing goal at Easting: %f, Northing: %f", easting, northing);

        // Small delay to ensure goals are published sequentially
        rclcpp::sleep_for(std::chrono::seconds(1));
    }
};

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<NavTwo>());
    rclcpp::shutdown();
    return 0;
}