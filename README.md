# map_sim_pure_sim no-docker

这是一个从现有 `map_sim_pure_sim` 工程里整理出来的纯源码版本，目标很直接：

- 不依赖 Docker
- 保留 Livox + Gazebo + ROS 2 Humble 这条主仿真链路
- 目录结构尽量简单，方便在普通 Ubuntu 主机上直接编译和运行

这份仓库不是把原目录原封不动地打包上来，而是把和仿真主流程直接相关的内容单独拎了出来，去掉了容器文件、预编译产物和一堆历史残留，留下一个能落地、能复现、也更方便继续改的版本。

## 仓库里保留了什么

- `ws/src/livox_ros_driver2`
- `ws/src/ros2_livox_simulation`
- Gazebo world / models / URDF / launch / scripts / config
- 用于宿主机部署的辅助脚本：
  - `check_host_deps.sh`
  - `install_host_deps_ubuntu22_humble.sh`
  - `build_local.sh`
  - `run_sim_local.sh`

## 删掉了什么

- Docker 相关目录和脚本
  - `docker/`
  - `docker-compose.yml`
  - `one_click_docker_init.sh`
- 所有构建产物
  - `ws/build/`
  - `ws/install/`
  - `ws/log/`
- 临时备份、运行日志和明显的历史残留文件

## 适用环境

这套东西目前按下面这个环境验证过：

- Ubuntu 22.04
- ROS 2 Humble
- Gazebo Classic 11
- Livox SDK2 安装到 `/usr/local`

## 已验证的内容

在远程 Linux 主机上做过一轮实际验证，结论是这套源码可以正常工作。

已验证项：

1. `bash ./build_local.sh` 可以完成编译
2. `bash ./run_sim_local.sh` 可以拉起仿真主流程
3. headless 模式下，`cube_robot` 能成功生成
4. `joint_state_broadcaster` 和 `rear_wheel_velocity_controller` 能成功激活

补一句实话：

- 带 GUI 启动时，远程桌面/X11/GLX 是否稳定，跟目标机器自己的图形环境有关系
- 纯 SSH 环境如果没有可用显示，会更适合走 headless
- 这不是这份源码特有的问题，Gazebo Classic 在远程图形环境下本来就比较挑会话条件

## 目录结构

```text
map-sim-pure-sim-no-docker/
├── README.md
├── README_NO_DOCKER.md
├── .gitignore
├── check_host_deps.sh
├── install_host_deps_ubuntu22_humble.sh
├── build_local.sh
├── run_sim_local.sh
└── ws/
    └── src/
        ├── livox_ros_driver2/
        └── ros2_livox_simulation/
```

## 一分钟上手

先把仓库放到一个**纯英文路径**里，然后按下面走：

```bash
git clone <this-repo>
cd map-sim-pure-sim-no-docker
bash ./check_host_deps.sh
bash ./build_local.sh
bash ./run_sim_local.sh
```

如果只是想先确认主流程能不能起来，最省心的办法是用 headless 模式：

```bash
MAP_SIM_GZCLIENT=0 bash ./run_sim_local.sh
```

## 依赖安装

### 1. 基础工具

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  git \
  curl \
  pkg-config \
  python3-colcon-common-extensions \
  python3-rosdep \
  python3-vcstool
```

### 2. ROS 2 与 Gazebo

如果机器上还没有 ROS 2 Humble，最省事的方式是先装完整桌面版：

```bash
sudo apt install -y ros-humble-desktop
```

然后补齐这套工程常用到的包：

```bash
sudo apt install -y \
  gazebo \
  libgazebo-dev \
  ros-humble-gazebo-ros-pkgs \
  ros-humble-gazebo-plugins \
  ros-humble-gazebo-msgs \
  ros-humble-robot-state-publisher \
  ros-humble-controller-manager \
  ros-humble-ros2-controllers \
  ros-humble-joint-state-broadcaster \
  ros-humble-velocity-controllers \
  ros-humble-xacro
```

### 3. C++ / PCL / protobuf / APR 依赖

```bash
sudo apt install -y \
  libeigen3-dev \
  libpcl-dev \
  libapr1-dev \
  libprotobuf-dev \
  protobuf-compiler \
  libboost-chrono-dev \
  libboost-system-dev \
  libboost-thread-dev
```

### 4. Livox SDK2

`livox_ros_driver2` 需要宿主机预先装好 Livox SDK2。

至少要满足下面两个文件存在：

- `/usr/local/lib/liblivox_lidar_sdk_shared.so`
- `/usr/local/include/livox_lidar_api.h`

参考安装方式：

```bash
git clone https://github.com/Livox-SDK/Livox-SDK2.git
cd Livox-SDK2
mkdir build
cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### 5. 可选 Python 依赖

下面这些不是主仿真启动的硬要求，但仓库里有些脚本会用到：

```bash
sudo apt install -y python3-pygame python3-opencv python3-numpy
```

说明：

- `python3-pygame`
  - 对应 `ws/src/ros2_livox_simulation/scripts/controller_sim.py`
- `python3-opencv`
  - 对应一些点云/图像辅助脚本
- `python3-numpy`
  - 多个 Python 脚本都会用到
- `torch`
  - 只有 `trans_f.py` 这类脚本才需要，不是默认主流程硬依赖

## 构建说明

```bash
cd <repo-root>
bash ./build_local.sh
```

这个脚本做了几件事：

1. 检查 ROS 2 Humble 和 Livox SDK2 是否在标准位置
2. 清理 conda / 用户 Python 对 ROS Python 环境的干扰
3. 进入 `ws/src/livox_ros_driver2`
4. 调用它自带的 `build.sh humble`
5. 构建结束后，再检查 `livox_ros_driver2` 和 `ros2_livox_simulation` 这两个包是否真的能被 ROS 发现

也就是说，它不是只看命令有没有跑完，而是会再补一层包可见性检查，避免“前面已经失败了，后面还误报成功”的情况。

## 运行说明

### 默认行为：启动 GUI

```bash
bash ./run_sim_local.sh
```

当前源码里默认会尝试启动 Gazebo GUI，也就是 `gzclient`。

如果你是在本机桌面会话里跑，或者通过能带图形的远程桌面登录，这通常是最直观的方式。

### 纯无界面模式

如果机器没有图形环境，或者只是想先验证主流程，建议显式关掉 GUI：

```bash
MAP_SIM_GZCLIENT=0 bash ./run_sim_local.sh
```

### 脚本实际做的事情

`run_sim_local.sh` 会：

1. 清理 conda / 用户 Python 环境变量
2. source `/opt/ros/humble/setup.bash`
3. source `ws/install/setup.bash`
4. 设置 `LIVOX_SIM_ROOT`
5. 检查 Gazebo 默认端口 `11345` 是否已被占用
6. 调用：

```bash
ros2 launch ros2_livox_simulation sim_launch.py
```

如果端口已经被旧的 `gzserver` 占着，脚本会直接报错退出，避免再开一轮把日志弄得更乱。

## 常用命令

### 检查依赖

```bash
bash ./check_host_deps.sh
```

### 编译

```bash
bash ./build_local.sh
```

### 启动（GUI）

```bash
bash ./run_sim_local.sh
```

### 启动（headless）

```bash
MAP_SIM_GZCLIENT=0 bash ./run_sim_local.sh
```

### 杀掉旧 Gazebo 进程

```bash
pkill -9 gzclient
pkill -9 gzserver
pkill -f 'ros2 launch ros2_livox_simulation sim_launch.py'
```

## 已知坑点

下面这些不是猜的，都是实际整理和验证过程中踩出来的。

### 1. 路径里不要带中文

这个坑是实打实会炸的。

如果把工程放在这种路径下面：

```bash
~/桌面/xxx
```

`livox_ros_driver2` 在消息生成阶段有概率因为 `rosidl` / CMake 的路径处理问题直接编译失败。

稳妥做法就是一句话：

**仓库放纯英文路径。**

例如：

```bash
~/map-sim-pure-sim-no-docker
```

### 2. conda 会抢 ROS 的 Python

如果终端里已经 `conda activate base`，有些 ROS Python 组件会被 conda 的 `python3` 抢走。

最典型的报错是：

```text
ModuleNotFoundError: No module named 'em'
```

这份仓库里的 `build_local.sh` 和 `run_sim_local.sh` 已经专门处理了这个问题，会优先回退到系统 Python。

但如果你手动跑其它 ROS/colcon 命令，还是建议先：

```bash
conda deactivate
```

至少要确认：

```bash
which python3
```

输出最好是：

```bash
/usr/bin/python3
```

### 3. `rosdep` 没初始化不一定是主阻塞

如果看到类似：

```text
sudo rosdep init
rosdep update
```

那说明这台机器的 `rosdep` 还没做初始化。

这件事最好补上，但在依赖已经手动装齐的情况下，它未必是当前主故障。仓库里的构建脚本遇到这个情况会给出警告并跳过，不会因此直接把整轮构建判死。

### 4. 远程 GUI 会受显示环境影响

如果是在纯 SSH 会话里直接拉 Gazebo GUI，可能碰到这些典型问题：

- `Can't open display`
- `BadDrawable`
- `GLX`

这种情况通常不是源码本身坏了，而是当前会话没有可用的桌面显示环境，或者 X11/GLX 条件不够完整。

最稳的处理方法是：

- 有桌面会话时再开 GUI
- 没桌面时直接 `MAP_SIM_GZCLIENT=0`

### 5. 启动阶段可能会出现一小段重复 controller 输出

如果日志里已经出现：

- `activate successful`
- `Configured and activated`

后面又跟了几条“already loaded / can not be configured from active state”，大概率只是启动阶段的重复输出，不一定代表系统不可用。

这种情况优先看功能本身：

- 机器人有没有生成
- 控制器是否已激活
- 指令能不能发出去

如果功能是正常的，这类日志更像启动时序噪声。

## 这份仓库和原目录的关系

这份仓库更像一个“拿来就能编、能跑、能继续改”的整理版，不是历史目录的逐字镜像。

整理时主要做了这些事情：

- 保留主仿真所需源码和资源
- 去掉 Docker 相关内容
- 去掉构建产物和运行日志
- 增加宿主机依赖检查脚本
- 增加适配 conda 环境的构建/运行脚本
- 把一些实际踩到的坑写到文档里，避免后面重复踩

## 许可证和来源

仓库里保留了上游包自带的许可证文件：

- `ws/src/livox_ros_driver2/LICENSE.txt`
- `ws/src/ros2_livox_simulation/LICENSE`

如果你准备进一步对外分发，建议先把这两个包各自的许可证范围确认一遍，再决定自己的发布方式。

## 最后一句建议

如果只是想先判断“这套源码到底能不能跑”，最有效的命令不是 GUI 版本，而是：

```bash
MAP_SIM_GZCLIENT=0 bash ./run_sim_local.sh
```

这样可以先把图形环境带来的变量排掉，先确认主流程本身是活的。等主流程稳了，再去看 GUI。
