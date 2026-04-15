#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <tf2_ros/transform_broadcaster.h>

#include <thread>
#include <atomic>

namespace gazebo
{

class CubeRobotWorldPosePlugin : public ModelPlugin
{
public:
  void Load(physics::ModelPtr model, sdf::ElementPtr /*_sdf*/) override
  {
    model_ = model;

    if (!model_)
    {
      gzerr << "[tf_pub] model is null" << std::endl;
      return;
    }

    if (model_->GetName() != "cube_robot")
    {
      gzerr << "[tf_pub] Wrong model: " << model_->GetName() << std::endl;
      return;
    }

    // -------- ROS 2 init --------
    if (!rclcpp::ok())
    {
      int argc = 0;
      char **argv = nullptr;
      rclcpp::init(argc, argv);
    }

    ros_node_ = std::make_shared<rclcpp::Node>("cube_robot_world_pose_plugin");

    // ===== use_sim_time：只在未声明时声明 =====
    if (!ros_node_->has_parameter("use_sim_time"))
      ros_node_->declare_parameter<bool>("use_sim_time", true);
    ros_node_->set_parameter(rclcpp::Parameter("use_sim_time", true));

    // ---------------- Publisher ----------------
    pose_pub_ = ros_node_->create_publisher<geometry_msgs::msg::PoseStamped>(
        "/cube_robot/world_pose", 10);

    odom_pub_ = ros_node_->create_publisher<nav_msgs::msg::Odometry>(
        "/cube_robot/odom", 10);

    tf_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(ros_node_);

    // ---------------- Executor thread ----------------
    executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    executor_->add_node(ros_node_);
    spin_thread_ = std::thread([this]() { executor_->spin(); });

    // ---------------- Gazebo update callback ----------------
    update_connection_ = event::Events::ConnectWorldUpdateBegin(
        std::bind(&CubeRobotWorldPosePlugin::OnUpdate, this));

    gzmsg << "[tf_pub] CubeRobotWorldPosePlugin loaded, waiting for /clock..." << std::endl;
  }

  void OnUpdate()
  {
    if (!ros_node_)
      return;

    // === 1) 获取 ROS 时间（sim time）===
    const rclcpp::Time stamp = ros_node_->get_clock()->now();

    // === 2) /clock 未 ready：stamp=0，直接返回 ===
    if (stamp.nanoseconds() == 0)
    {
      if (!warned_no_clock_)
      {
        RCLCPP_WARN(
            ros_node_->get_logger(),
            "[tf_pub] /clock not ready yet, suppressing odom/TF publish");
        warned_no_clock_ = true;
      }
      return;
    }

    // === 3) 核心修复：同一个 stamp 只发布一次（否则你会看到 odom hz 里 min=0.000s）===
    // 这会直接导致 message_filters 不同步 -> PGO buffer_size 永远 0
    if (has_last_stamp_ && stamp == last_stamp_)
    {
      return;
    }
    last_stamp_ = stamp;
    has_last_stamp_ = true;

    // 第一次进入：提示一下
    if (!clock_ready_)
    {
      RCLCPP_INFO(
          ros_node_->get_logger(),
          "[tf_pub] sim time ready, start publishing odom/TF");
      clock_ready_ = true;
    }

    if (!model_)
      return;

    ignition::math::Pose3d pose = model_->WorldPose();

    // ---------------- PoseStamped ----------------
    geometry_msgs::msg::PoseStamped pose_msg;
    pose_msg.header.stamp = stamp;
    pose_msg.header.frame_id = "world";

    pose_msg.pose.position.x = pose.Pos().X();
    pose_msg.pose.position.y = pose.Pos().Y();
    pose_msg.pose.position.z = pose.Pos().Z();

    pose_msg.pose.orientation.x = pose.Rot().X();
    pose_msg.pose.orientation.y = pose.Rot().Y();
    pose_msg.pose.orientation.z = pose.Rot().Z();
    pose_msg.pose.orientation.w = pose.Rot().W();

    pose_pub_->publish(pose_msg);

    // ---------------- Odometry ----------------
    nav_msgs::msg::Odometry odom_msg;
    odom_msg.header.stamp = stamp;
    odom_msg.header.frame_id = "world";
    odom_msg.child_frame_id = "base_link";
    odom_msg.pose.pose = pose_msg.pose;

    auto linear_vel = model_->WorldLinearVel();
    auto angular_vel = model_->WorldAngularVel();

    odom_msg.twist.twist.linear.x = linear_vel.X();
    odom_msg.twist.twist.linear.y = linear_vel.Y();
    odom_msg.twist.twist.linear.z = linear_vel.Z();
    odom_msg.twist.twist.angular.x = angular_vel.X();
    odom_msg.twist.twist.angular.y = angular_vel.Y();
    odom_msg.twist.twist.angular.z = angular_vel.Z();

    odom_pub_->publish(odom_msg);

    // ---------------- TF ----------------
    geometry_msgs::msg::TransformStamped tf_msg;
    tf_msg.header.stamp = stamp;
    tf_msg.header.frame_id = "world";
    tf_msg.child_frame_id = "base_link";

    tf_msg.transform.translation.x = pose.Pos().X();
    tf_msg.transform.translation.y = pose.Pos().Y();
    tf_msg.transform.translation.z = pose.Pos().Z();
    tf_msg.transform.rotation = pose_msg.pose.orientation;

    tf_broadcaster_->sendTransform(tf_msg);
  }

  ~CubeRobotWorldPosePlugin() override
  {
    if (executor_)
      executor_->cancel();

    if (spin_thread_.joinable())
      spin_thread_.join();
  }

private:
  physics::ModelPtr model_;
  event::ConnectionPtr update_connection_;

  rclcpp::Node::SharedPtr ros_node_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pose_pub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  std::shared_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

  std::shared_ptr<rclcpp::executors::SingleThreadedExecutor> executor_;
  std::thread spin_thread_;

  // ===== 状态 =====
  std::atomic<bool> clock_ready_{false};
  std::atomic<bool> warned_no_clock_{false};

  // ===== 关键：防止同一 stamp 重复发布 =====
  rclcpp::Time last_stamp_{0, 0, RCL_ROS_TIME};
  bool has_last_stamp_{false};
};

GZ_REGISTER_MODEL_PLUGIN(CubeRobotWorldPosePlugin)

} // namespace gazebo
