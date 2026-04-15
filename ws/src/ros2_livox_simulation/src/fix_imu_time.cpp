#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <geometry_msgs/msg/quaternion.hpp>

#include <cmath>

using sensor_msgs::msg::Imu;

class FixImuTime : public rclcpp::Node
{
public:
  FixImuTime()
  : Node("fix_imu_time")
  {
    // ===== 绕 Y 轴 -30° =====
    double deg = -30.0;
    double theta = deg * M_PI / 180.0;
    cos_t_ = std::cos(theta);
    sin_t_ = std::sin(theta);

    // ===== QoS：IMU 必须 RELIABLE（FAST-LIO 要求）=====
    auto imu_qos = rclcpp::QoS(rclcpp::KeepLast(200)).reliable();

    sub_ = this->create_subscription<Imu>(
      "/livox/imu",
      imu_qos,
      std::bind(&FixImuTime::cb, this, std::placeholders::_1));

    pub_ = this->create_publisher<Imu>(
      "/imu_fixed",
      imu_qos);

    RCLCPP_INFO(
      this->get_logger(),
      "FixImuTime started: rotate Y -30°, RELIABLE QoS, IMU time shifted -0.08s");
  }

private:
  void cb(const Imu::SharedPtr msg)
  {
    Imu out = *msg;

    // =====================================================
    // ✅ 核心修正：IMU 时间向“过去”平移 0.08 秒
    // =====================================================
    rclcpp::Time t(msg->header.stamp);
    t = t - rclcpp::Duration::from_seconds(0.08);
    out.header.stamp = t;

    // ===== 旋转角速度 =====
    rotate_vec(
      out.angular_velocity.x,
      out.angular_velocity.y,
      out.angular_velocity.z
    );

    // ===== 旋转线加速度 =====
    rotate_vec(
      out.linear_acceleration.x,
      out.linear_acceleration.y,
      out.linear_acceleration.z
    );

    // ===== 旋转姿态四元数 =====
    rotate_quaternion(out.orientation);

    pub_->publish(out);
  }

  // ===== 绕 Y 轴旋转向量 =====
  inline void rotate_vec(double &x, double &y, double &z)
  {
    double x_new =  cos_t_ * x + sin_t_ * z;
    double z_new = -sin_t_ * x + cos_t_ * z;
    x = x_new;
    z = z_new;
    // y 不变
  }

  // ===== 绕 Y 轴旋转四元数 =====
  void rotate_quaternion(geometry_msgs::msg::Quaternion &q)
  {
    double half = (-30.0 * M_PI / 180.0) * 0.5;
    double sr = std::sin(half);
    double cr = std::cos(half);

    geometry_msgs::msg::Quaternion r;
    r.x = 0.0;
    r.y = sr;
    r.z = 0.0;
    r.w = cr;

    geometry_msgs::msg::Quaternion res;
    res.w = r.w*q.w - r.x*q.x - r.y*q.y - r.z*q.z;
    res.x = r.w*q.x + r.x*q.w + r.y*q.z - r.z*q.y;
    res.y = r.w*q.y - r.x*q.z + r.y*q.w + r.z*q.x;
    res.z = r.w*q.z + r.x*q.y - r.y*q.x + r.z*q.w;

    q = res;
  }

  rclcpp::Subscription<Imu>::SharedPtr sub_;
  rclcpp::Publisher<Imu>::SharedPtr pub_;

  double cos_t_;
  double sin_t_;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FixImuTime>());
  rclcpp::shutdown();
  return 0;
}
