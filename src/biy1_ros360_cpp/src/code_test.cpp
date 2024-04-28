#include "rclcpp/rclcpp.hpp"
#include "rcl_interfaces/msg/log.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp" // Include for PoseStamped
#include <regex>
#include <string>
#include <vector>

using PoseStamped = geometry_msgs::msg::PoseStamped;
using PoseStampedList = std::vector<PoseStamped>;

class LogListener : public rclcpp::Node
{
public:
    LogListener() : Node("log_listener")
    {
        log_subscription_ = this->create_subscription<rcl_interfaces::msg::Log>(
            "/rosout", 10, std::bind(&LogListener::log_callback, this, std::placeholders::_1));
    }

private:
    void log_callback(const rcl_interfaces::msg::Log::SharedPtr msg)
    {
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

    PoseStampedList remainingDestinationPoseList(const PoseStampedList& poseList, const std::pair<double, double>& destinationCoords) {
        PoseStampedList trimmedList;

        for (const auto& pose : poseList) {
            double x = pose.pose.position.x;
            double y = pose.pose.position.y;

            // Check if current pose coordinates match the destination coordinates
            if (std::abs(x - destinationCoords.first) < 0.01 && std::abs(y - destinationCoords.second) < 0.01) {
                // If match found, stop trimming and return the trimmed list
                trimmedList.push_back(pose);
                break;
            } else {
                // If no match found, continue trimming
                trimmedList.push_back(pose);
            }
        }

        return trimmedList;
    }

    rclcpp::Subscription<rcl_interfaces::msg::Log>::SharedPtr log_subscription_;
    std::pair<double,double> destination_coords_; 
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<LogListener>());
    rclcpp::shutdown();
    return 0;
}
