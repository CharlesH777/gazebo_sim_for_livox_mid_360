#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WS_DIR="$ROOT_DIR/ws"

sanitize_python_env() {
  unset PYTHONHOME PYTHONPATH CONDA_PREFIX CONDA_DEFAULT_ENV CONDA_PROMPT_MODIFIER CONDA_SHLVL CONDA_EXE CONDA_PYTHON_EXE _CE_CONDA _CE_M || true
  export PATH="/usr/bin:/bin:/usr/sbin:/sbin:${PATH}"
  hash -r || true
}

sanitize_python_env
set +u
source /opt/ros/humble/setup.bash
source "$WS_DIR/install/setup.bash"
set -u

export LIVOX_SIM_ROOT="$WS_DIR/src/ros2_livox_simulation"
cd "$WS_DIR"

if command -v ss >/dev/null 2>&1 && ss -lnt 2>/dev/null | grep -q ':11345 '; then
  echo "[ERR] Gazebo master port 11345 is already in use."
  echo "[HINT] Stop the old gzserver first, then rerun."
  exit 1
fi

if [ "${MAP_SIM_GZCLIENT:-1}" = "1" ]; then
  echo "[INFO] Launch mode: GUI"
else
  echo "[INFO] Launch mode: headless"
fi

ros2 launch ros2_livox_simulation sim_launch.py
