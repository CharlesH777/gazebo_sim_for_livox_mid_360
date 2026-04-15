#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WS_DIR="$ROOT_DIR/ws"
LIVOX_DIR="$WS_DIR/src/livox_ros_driver2"

sanitize_python_env() {
  # Avoid conda/user Python interfering with ROS Humble's Python packages.
  unset PYTHONHOME PYTHONPATH CONDA_PREFIX CONDA_DEFAULT_ENV CONDA_PROMPT_MODIFIER CONDA_SHLVL CONDA_EXE CONDA_PYTHON_EXE _CE_CONDA _CE_M || true
  export PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH}"
  hash -r || true
}

[ -f /opt/ros/humble/setup.bash ] || { echo "[ERR] Missing /opt/ros/humble/setup.bash" >&2; exit 1; }
[ -f /usr/local/lib/liblivox_lidar_sdk_shared.so ] || { echo "[ERR] Missing /usr/local/lib/liblivox_lidar_sdk_shared.so" >&2; exit 1; }
[ -f /usr/local/include/livox_lidar_api.h ] || { echo "[ERR] Missing /usr/local/include/livox_lidar_api.h" >&2; exit 1; }
[ -x "$LIVOX_DIR/build.sh" ] || { echo "[ERR] Missing $LIVOX_DIR/build.sh" >&2; exit 1; }

sanitize_python_env

echo "[INFO] python3=$(command -v python3)"
python3 -V || true

set +u
source /opt/ros/humble/setup.bash
set -u
cd "$LIVOX_DIR"

if command -v rosdep >/dev/null 2>&1; then
  if [ -d /etc/ros/rosdep/sources.list.d ]; then
    rosdep update || true
    cd "$WS_DIR"
    rosdep install --from-paths src --ignore-src -r -y --rosdistro humble || true
    cd "$LIVOX_DIR"
  else
    echo "[WARN] rosdep not initialized; skip rosdep install"
  fi
fi

bash ./build.sh humble

[ -f "$WS_DIR/install/setup.bash" ] || { echo "[ERR] Build did not produce $WS_DIR/install/setup.bash" >&2; exit 1; }

sanitize_python_env
set +u
source /opt/ros/humble/setup.bash
source "$WS_DIR/install/setup.bash"
set -u

ros2 pkg prefix livox_ros_driver2 >/dev/null
ros2 pkg prefix ros2_livox_simulation >/dev/null

echo "[OK] Build finished and both ROS packages are discoverable"
echo "[NEXT] Run: bash ./run_sim_local.sh"
