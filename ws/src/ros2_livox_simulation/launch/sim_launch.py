#!/usr/bin/env python3
import os

from launch import LaunchDescription
from launch.actions import SetEnvironmentVariable, LogInfo, ExecuteProcess, TimerAction
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory, get_package_prefix
from launch.substitutions import Command


def _resolve_livox_root(pkg_share: str) -> str:
    world_rel = os.path.join("world", "rm_2026_slam_world.world")
    candidates = [
        os.getenv("LIVOX_SIM_ROOT", ""),
        os.path.abspath(os.path.join(os.path.dirname(__file__), "..")),
        os.path.abspath(os.path.join(os.getcwd(), "livox_laser_simulation_RO2")),
        os.path.abspath(os.path.join(os.getcwd(), "..", "livox_laser_simulation_RO2")),
        os.path.abspath(os.path.join(pkg_share, "..", "..", "..", "..")),
    ]
    for c in candidates:
        if c and os.path.exists(os.path.join(c, world_rel)):
            return c
    raise RuntimeError("Cannot resolve livox_laser_simulation_RO2 root. Set LIVOX_SIM_ROOT env.")


def generate_launch_description():
    pkg_share = get_package_share_directory("ros2_livox_simulation")
    pkg_prefix = get_package_prefix("ros2_livox_simulation")
    livox_root = _resolve_livox_root(pkg_share)

    robot_xacro = os.path.abspath(os.path.join(pkg_share, "urdf", "robot_sim.xacro"))
    robot_description = Command(["xacro ", robot_xacro])
    robot_description_param = {"robot_description": robot_description}

    world_path = os.path.join(livox_root, "world", "rm_2026_slam_world.world")
    model_path = os.path.join(livox_root, "models")

    plugin_candidates = [
        os.path.join(pkg_prefix, "lib"),
        os.path.join(livox_root, "install", "ros2_livox_simulation", "lib"),
    ]
    plugin_path = ""
    for p in plugin_candidates:
        if os.path.exists(p):
            plugin_path = p
            break

    set_gazebo_model_path = SetEnvironmentVariable(
        name="GAZEBO_MODEL_PATH",
        value=":".join([
            "/usr/share/gazebo-11/models",
            model_path,
        ]),
    )

    set_gazebo_plugin_path = SetEnvironmentVariable(
        name="GAZEBO_PLUGIN_PATH",
        value=":".join([
            plugin_path,
            os.getenv("GAZEBO_PLUGIN_PATH", ""),
        ]),
    )

    gazebo_server = ExecuteProcess(
        cmd=[
            "gzserver",
            world_path,
            "--verbose",
            "-slibgazebo_ros_init.so",
            "-slibgazebo_ros_factory.so",
        ],
        output="screen",
    )

    # 默认启动 GUI；如需纯 headless，可在启动前设 MAP_SIM_GZCLIENT=0
    launch_gzclient = os.getenv("MAP_SIM_GZCLIENT", "1") == "1"
    gazebo_client = ExecuteProcess(cmd=["gzclient"], output="screen") if launch_gzclient else None

    robot_state_pub = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        name="robot_state_publisher",
        output="screen",
        parameters=[{"use_sim_time": True}, robot_description_param],
    )

    spawn_robot = Node(
        package="gazebo_ros",
        executable="spawn_entity.py",
        output="screen",
        arguments=[
            "-entity", "cube_robot",
            "-topic", "robot_description",
            "-x", "0.0", "-y", "0.0", "-z", "0.5",
        ],
    )

    spawn_joint_state = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_state_broadcaster", "--controller-manager", "/controller_manager"],
        output="screen",
    )

    spawn_rear_wheel_ctrl = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["rear_wheel_velocity_controller", "--controller-manager", "/controller_manager"],
        output="screen",
    )

    # 给 gazebo/world 加载与 factory service 留足时间
    delayed_robot_state_pub = TimerAction(period=5.0, actions=[robot_state_pub])
    delayed_spawn_robot = TimerAction(period=12.0, actions=[spawn_robot])
    delayed_joint_state = TimerAction(period=18.0, actions=[spawn_joint_state])
    delayed_rear_ctrl = TimerAction(period=20.0, actions=[spawn_rear_wheel_ctrl])

    fix_cloud_node = Node(
        package="ros2_livox_simulation",
        executable="fix_cloud_time",
        name="fix_cloud_time",
        output="screen",
        parameters=[{"use_sim_time": True}],
    )

    fix_imu_node = Node(
        package="ros2_livox_simulation",
        executable="fix_imu_time",
        name="fix_imu_time",
        output="screen",
        parameters=[{"use_sim_time": True}],
    )

    actions = [
        set_gazebo_model_path,
        set_gazebo_plugin_path,
        LogInfo(msg=f"[INFO] livox_root={livox_root}"),
        LogInfo(msg=f"[INFO] world={world_path}"),
        LogInfo(msg=f"[INFO] headless={'yes' if not launch_gzclient else 'no'}"),
        gazebo_server,
        delayed_robot_state_pub,
        delayed_spawn_robot,
        delayed_joint_state,
        delayed_rear_ctrl,
        fix_cloud_node,
        fix_imu_node,
    ]

    if gazebo_client is not None:
        actions.insert(5, gazebo_client)

    return LaunchDescription(actions)
