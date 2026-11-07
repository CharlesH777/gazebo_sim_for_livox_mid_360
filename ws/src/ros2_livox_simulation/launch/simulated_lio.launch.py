#!/usr/bin/env python3
"""Launch the ground-truth-assisted simulated LIO wiring exercise.

The default inputs are intentionally generic and disconnected from the base
simulation drivers. Set cloud_topic and pose_topic to wire this node.
This interface uses Gazebo poses and is not a real LIO state estimator.
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            "cloud_topic",
            default_value="points_raw",
            description="PointCloud2 input topic to connect to the LiDAR driver",
        ),
        DeclareLaunchArgument(
            "pose_topic",
            default_value="ground_truth_pose",
            description="PoseStamped input topic providing Gazebo ground-truth poses",
        ),
        DeclareLaunchArgument(
            "use_sim_time",
            default_value="true",
            description="Use the Gazebo /clock time source",
        ),
        Node(
            package="ros2_livox_simulation",
            executable="simulated_lio",
            name="simulated_lio",
            output="screen",
            parameters=[{
                "use_sim_time": ParameterValue(
                    LaunchConfiguration("use_sim_time"), value_type=bool
                ),
            }],
            remappings=[
                ("points_raw", LaunchConfiguration("cloud_topic")),
                ("ground_truth_pose", LaunchConfiguration("pose_topic")),
            ],
        ),
    ])
