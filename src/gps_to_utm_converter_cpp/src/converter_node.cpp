#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <GeographicLib/UTMUPS.hpp>
#include <GeographicLib/Geodesic.hpp>

class GPS2UTMConverter : public rclcpp::Node {
public:
    GPS2UTMConverter()
    : Node("gps_to_utm_conerter"){
        //Subscriber to GPS data
        subscription_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
            "/gps/fix", 10, std::bind(&GPS2UTMConverter::gps_callback, this, std::placeholders::_1)
        );

        publisher_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("utm_data", 10);
    }

private:
    void gps_callback(const sensor_msgs::msg::NavSatFix::SharedPtr msg){
        double lat = msg->latitude;
        double lng = msg->longitude;
        int zone;
        bool northp;
        double x, y;
        GeographicLib::UTMUPS::Forward(lat,lng,zone,northp, x, y);

        auto utm_msg = geometry_msgs::msg::PoseStamped();
        utm_msg.header.stamp = this->now();
        utm_msg.header.frame_id = "utm";
        utm_msg.pose.position.x = x;
        utm_msg.pose.position.y = y;

        publisher_->publish(utm_msg);
        //RCLCPP_INFO(this->get_logger(), "Published UTM coordinates: [%f, %f]", x, y);
    }


    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr subscription_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr publisher_;
};

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<GPS2UTMConverter>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
