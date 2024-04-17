/*
 * Copyright (c) 2010, Daniel Hewlett, Antons Rebguns
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *      * Neither the name of the <organization> nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY Antons Rebguns <email> ''AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL Antons Rebguns <email> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **/

/*
 * \file  gazebo_ros_skid_steer_drive_6w.h
 *
 * \brief A skid steering drive plugin for 6w robots. gazebo_ros_skid_steer_drive, Inspired by gazebo_ros_diff_drive and SkidSteerDrivePlugin
 *
 * \author  Zdenek Materna (imaterna@fit.vutbr.cz)
 *
 * Converted to ROS2 by Fred Labrosse <ffl@aber.ac.uk>
 */

#ifndef GAZEBO_ROS_SKID_STEER_DRIVE_H_6W_
#define GAZEBO_ROS_SKID_STEER_DRIVE_H_6W_

#define BOOST_BIND_NO_PLACEHOLDERS // Needed to solve "reference to _1 is ambiguous"

#include <map>

#include <gazebo/common/common.hh>
#include <gazebo/physics/physics.hh>

// ROS
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>


namespace gazebo {

  class Joint;
  class Entity;

  class GazeboRosSkidSteerDrive6W : public ModelPlugin
  {

    public:
	  GazeboRosSkidSteerDrive6W();
      ~GazeboRosSkidSteerDrive6W();
      void Load(physics::ModelPtr _parent, sdf::ElementPtr _sdf);

    protected:
      virtual void UpdateChild();
      virtual void FiniChild();

    private:
      void publishOdometry(common::Time current_time);
      void getWheelVelocities();

      physics::WorldPtr world;
      physics::ModelPtr parent;
      event::ConnectionPtr update_connection_;

      std::string left_front_joint_name_;
      std::string right_front_joint_name_;
      std::string left_middle_joint_name_;
      std::string right_middle_joint_name_;
      std::string left_rear_joint_name_;
      std::string right_rear_joint_name_;

      double wheel_separation_;
      double wheel_diameter_;
      double torque;
      double wheel_speed_[6];

      physics::JointPtr joints[6];

      // ROS STUFF
      std::shared_ptr<rclcpp::Node> rosnode_;
      rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odomPub_;
      rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmdVelSub_;
      tf2_ros::TransformBroadcaster* transform_broadcaster_;
      nav_msgs::msg::Odometry odom_;
//       std::string tf_prefix_;
      bool broadcast_tf_;

//       boost::mutex lock;

      std::string robot_namespace_;
      std::string command_topic_;
      std::string odometry_topic_;
      std::string odometry_frame_;
      std::string robot_base_frame_;

      // Custom Callback Queue
//      ros::CallbackQueue queue_;
//      boost::thread callback_queue_thread_;
//      void QueueThread();

      // DiffDrive stuff
      void cmdVelCallback(const geometry_msgs::msg::Twist& cmd_msg);

      double x_;
      double rot_;
      bool alive_;

      // Update Rate
      double update_rate_;
      double update_period_;
      common::Time last_update_time_;

      double covariance_x_;
      double covariance_y_;
      double covariance_yaw_;

  };

}


#endif /* GAZEBO_ROS_SKID_STEER_DRIVE_H_6W_ */