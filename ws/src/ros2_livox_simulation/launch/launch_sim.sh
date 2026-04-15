#!/usr/bin/env bash
# =====================================================
# 🚀 启动 Livox 仿真环境 + SLAM 运行环境（相对路径版）
# =====================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIVOX_SIM_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MAP_SIM_ROOT="$(cd "$LIVOX_SIM_ROOT/.." && pwd)"
SLAM_ROOT="$MAP_SIM_ROOT/slam_2026_charles"

export LIVOX_SIM_ROOT
export SLAM_CHARLES_ROOT="$SLAM_ROOT"
export MAP_SIM_ROOT

source /opt/ros/humble/setup.bash
source "$LIVOX_SIM_ROOT/install/setup.sh"
source "$SLAM_ROOT/install/setup.bash"

sleep 1
ros2 launch ros2_livox_simulation sim_launch.py
