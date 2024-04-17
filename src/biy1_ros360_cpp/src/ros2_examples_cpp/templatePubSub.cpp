// PHS: Template combining together publisher and subscriber examples

//General C++ includes as needed
#include <chrono>
#include <functional>
#include <memory>
#include <string>

//ROS 2 Specific includes
//Important: these includes should be reflected in the package.xml and CMakeLists.txt
#include "rclcpp/rclcpp.hpp"

// TODO: Add or replace with message type needed
#include "std_msgs/msg/string.hpp" 

using namespace std::chrono_literals;
using std::placeholders::_1;

/* Template giving outline for creating a program
 * that contains both a publisher and subscriber.
 */

class PubSubTemplate : public rclcpp::Node
{
public:
  //TODO: Update node name to reflect purpose of node
  PubSubTemplate() : Node("pub_sub_template")
  {
    // TODO: Update message type and topic name as appropriate
    publisher_ = this->create_publisher<std_msgs::msg::String>("/hello/world", 10);

    // TODO: Update message type and topic name as appropriate
    subscription_ = this->create_subscription<std_msgs::msg::String>(
      "/hello/world", 10, std::bind(&PubSubTemplate::topic_callback, this, _1));
    
    // Create a timer that will trigger calls to the method timer_callback
    // TODO: Update timer duration as required
    timer_ = this->create_wall_timer(
      500ms, std::bind(&PubSubTemplate::timer_callback, this));
  }

private:

  void timer_callback()
  {
    //TODO add behaviour here to be repeated at regular intervals

    //Create an instance of the appropriate message type
    auto message = std_msgs::msg::String();

    //Set the values for necessary fields in message type
    message.data = "Hello, world!";

    //Log output for message to be published
    RCLCPP_INFO(this->get_logger(), "Publishing: '%s'", message.data.c_str());

    //publish the message to topic
    publisher_->publish(message);
  }

  //TODO rename method to better reflect topic it is subscribed to
  //TODO update message type as appropriate
  void topic_callback(const std_msgs::msg::String & msg)
  {
    //Log output of message received
    //TODO update fields used from pre-defined message type
    RCLCPP_INFO(this->get_logger(), "I heard: '%s'", msg.data.c_str());
  }

  //Declaration of private fields used for timer, publisher and subscriber
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
};

//Main method defining entry point for program
int main(int argc, char * argv[])
{
  //Initialise ROS 2 for this node
  rclcpp::init(argc, argv);

  //Create the instance of the Node subclass and 
  // start the spinner with a pointer to the instance
  rclcpp::spin(std::make_shared<PubSubTemplate>());

  //When the node is terminated, shut down ROS 2 for this node
  rclcpp::shutdown();
  return 0;
}
