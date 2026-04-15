#include <rclcpp/rclcpp.hpp>

#include <sensor_msgs/msg/point_cloud2.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>

using sensor_msgs::msg::PointCloud2;
using geometry_msgs::msg::PoseStamped;

class LidarToWorld : public rclcpp::Node
{
public:
  LidarToWorld()
  : Node("lidar_to_world")
  {
    auto qos = rclcpp::SensorDataQoS().reliable();

    cloud_sub_ = this->create_subscription<PointCloud2>(
      "/livox/lidar_PointCloud2",
      qos,
      std::bind(&LidarToWorld::cloudCb, this, std::placeholders::_1));

    pose_sub_ = this->create_subscription<PoseStamped>(
      "/cube_robot/world_pose",
      50,
      std::bind(&LidarToWorld::poseCb, this, std::placeholders::_1));

    cloud_pub_ = this->create_publisher<PointCloud2>("/lidar_world", qos);
    cloud_body_pub_ = this->create_publisher<PointCloud2>("/lidar_body", qos);

    // ===== lidar -> base_link extrinsic (from robot_sim.xacro) =====
    // livox_mount_joint: xyz=(0,0,0.4), rpy=(0,-30deg,0)
    const double theta = -30.0 * M_PI / 180.0;
    R_bl_ = Eigen::AngleAxisd(theta, Eigen::Vector3d::UnitY()).toRotationMatrix();
    t_bl_ = Eigen::Vector3d(0.0, 0.0, 0.4);

    RCLCPP_INFO(this->get_logger(),
      "LidarToWorld started (strict transform: pb=R_bl*pl+t_bl, pw=R_wb*pb+t_wb)");
  }

private:
  struct TimedPose
  {
    double t_sec;
    Eigen::Quaterniond q_wb;
    Eigen::Vector3d t_wb;
  };

  static double toSec(const builtin_interfaces::msg::Time &t)
  {
    return static_cast<double>(t.sec) + 1e-9 * static_cast<double>(t.nanosec);
  }

  void poseCb(const PoseStamped::SharedPtr msg)
  {
    if (!msg) return;

    TimedPose tp;
    tp.t_sec = toSec(msg->header.stamp);
    tp.q_wb = Eigen::Quaterniond(
      msg->pose.orientation.w,
      msg->pose.orientation.x,
      msg->pose.orientation.y,
      msg->pose.orientation.z);

    if (tp.q_wb.norm() < 1e-9) {
      return;
    }
    tp.q_wb.normalize();

    tp.t_wb = Eigen::Vector3d(
      msg->pose.position.x,
      msg->pose.position.y,
      msg->pose.position.z);

    std::lock_guard<std::mutex> lock(pose_mutex_);
    pose_buf_.push_back(tp);

    constexpr size_t kMaxPoseBuf = 4000;
    while (pose_buf_.size() > kMaxPoseBuf) {
      pose_buf_.pop_front();
    }
  }

  bool queryPoseAt(double t_query, Eigen::Quaterniond &q_wb, Eigen::Vector3d &t_wb)
  {
    std::lock_guard<std::mutex> lock(pose_mutex_);
    if (pose_buf_.empty()) return false;

    if (pose_buf_.size() == 1) {
      q_wb = pose_buf_.front().q_wb;
      t_wb = pose_buf_.front().t_wb;
      return true;
    }

    // Find first element with t >= t_query
    auto it = std::lower_bound(
      pose_buf_.begin(), pose_buf_.end(), t_query,
      [](const TimedPose &p, double t) { return p.t_sec < t; });

    // Query before first or after last: use nearest pose
    if (it == pose_buf_.begin()) {
      q_wb = it->q_wb;
      t_wb = it->t_wb;
      return true;
    }
    if (it == pose_buf_.end()) {
      q_wb = pose_buf_.back().q_wb;
      t_wb = pose_buf_.back().t_wb;
      return true;
    }

    const TimedPose &right = *it;
    const TimedPose &left = *(it - 1);

    const double dt = right.t_sec - left.t_sec;
    if (dt <= 1e-9) {
      q_wb = right.q_wb;
      t_wb = right.t_wb;
      return true;
    }

    const double alpha = std::clamp((t_query - left.t_sec) / dt, 0.0, 1.0);

    q_wb = left.q_wb.slerp(alpha, right.q_wb);
    q_wb.normalize();
    t_wb = (1.0 - alpha) * left.t_wb + alpha * right.t_wb;
    return true;
  }

  void cloudCb(const PointCloud2::SharedPtr msg)
  {
    if (!msg || msg->data.empty()) return;

    Eigen::Quaterniond q_wb;
    Eigen::Vector3d t_wb;
    if (!queryPoseAt(toSec(msg->header.stamp), q_wb, t_wb)) {
      return;
    }

    const Eigen::Matrix3d R_wb = q_wb.toRotationMatrix();

    int off_x = -1, off_y = -1, off_z = -1;
    for (const auto &f : msg->fields) {
      if (f.name == "x") off_x = f.offset;
      else if (f.name == "y") off_y = f.offset;
      else if (f.name == "z") off_z = f.offset;
    }
    if (off_x < 0 || off_y < 0 || off_z < 0) return;

    const size_t num_points = static_cast<size_t>(msg->width) * msg->height;
    const size_t step = msg->point_step;

    // 1) lidar -> base_link
    // pb = R_bl * pl + t_bl
    PointCloud2 body_cloud = *msg;
    body_cloud.header.frame_id = "base_link";

    uint8_t *body_data = body_cloud.data.data();
    for (size_t i = 0; i < num_points; ++i) {
      uint8_t *p = body_data + i * step;
      Eigen::Vector3d pl(
        *reinterpret_cast<float *>(p + off_x),
        *reinterpret_cast<float *>(p + off_y),
        *reinterpret_cast<float *>(p + off_z));

      const Eigen::Vector3d pb = R_bl_ * pl + t_bl_;

      const float x = static_cast<float>(pb.x());
      const float y = static_cast<float>(pb.y());
      const float z = static_cast<float>(pb.z());
      std::memcpy(p + off_x, &x, sizeof(float));
      std::memcpy(p + off_y, &y, sizeof(float));
      std::memcpy(p + off_z, &z, sizeof(float));
    }
    cloud_body_pub_->publish(body_cloud);

    // 2) base_link -> world
    // pw = R_wb * pb + t_wb
    PointCloud2 world_cloud = body_cloud;
    world_cloud.header.frame_id = "world";

    uint8_t *world_data = world_cloud.data.data();
    for (size_t i = 0; i < num_points; ++i) {
      uint8_t *p = world_data + i * step;
      Eigen::Vector3d pb(
        *reinterpret_cast<float *>(p + off_x),
        *reinterpret_cast<float *>(p + off_y),
        *reinterpret_cast<float *>(p + off_z));

      const Eigen::Vector3d pw = R_wb * pb + t_wb;

      const float x = static_cast<float>(pw.x());
      const float y = static_cast<float>(pw.y());
      const float z = static_cast<float>(pw.z());
      std::memcpy(p + off_x, &x, sizeof(float));
      std::memcpy(p + off_y, &y, sizeof(float));
      std::memcpy(p + off_z, &z, sizeof(float));
    }

    // Keep 3x downsample for /lidar_world
    const size_t stride = 3;
    if (num_points < stride) {
      cloud_pub_->publish(world_cloud);
      return;
    }

    PointCloud2 out_ds = world_cloud;
    const size_t new_points = num_points / stride;

    out_ds.width = static_cast<uint32_t>(new_points);
    out_ds.height = 1;
    out_ds.row_step = out_ds.width * step;
    out_ds.data.resize(new_points * step);

    for (size_t i = 0; i < new_points; ++i) {
      const uint8_t *src = world_cloud.data.data() + (i * stride) * step;
      uint8_t *dst = out_ds.data.data() + i * step;
      std::memcpy(dst, src, step);
    }

    cloud_pub_->publish(out_ds);
  }

private:
  rclcpp::Subscription<PointCloud2>::SharedPtr cloud_sub_;
  rclcpp::Subscription<PoseStamped>::SharedPtr pose_sub_;
  rclcpp::Publisher<PointCloud2>::SharedPtr cloud_pub_;
  rclcpp::Publisher<PointCloud2>::SharedPtr cloud_body_pub_;

  std::deque<TimedPose> pose_buf_;
  std::mutex pose_mutex_;

  Eigen::Matrix3d R_bl_;
  Eigen::Vector3d t_bl_;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LidarToWorld>());
  rclcpp::shutdown();
  return 0;
}
