# G1 locomotion demo

Use `g1_bringup:g1` for the included policies; it supplies their custom gains and
velocity/heading command adapter.

## Setup

Follow the published [apt repository setup](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/configaptrepo.html)
and [Isaac ROS CLI setup](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/initroscli.html)
in Docker mode if not already done. On the host:

```bash
cd /path/to/this/repo
export ISAAC_ROS_WS=$PWD
isaac-ros activate --build-local --no-push
```

For later sessions, use `isaac-ros activate`. Inside the container, run commands
from `cd "$ISAAC_ROS_WS"`. `bazel run` builds what it needs automatically.
Internal dependencies require NVIDIA network access and repository credentials.

### x86

<details>
<summary><strong>x86 setup: install Isaac ROS CLI and build the G1 package</strong></summary>

Use this guide for an Ubuntu x86_64 workstation or server. Steps 1–5 run on the
**host**; build and policy commands run **inside the container**.

| Step | Stage | Where |
| --- | --- | --- |
| 1 | Initialize Compute | x86 host |
| 2 | Configure Isaac ROS Apt Repository | x86 host |
| 3 | Initialize Isaac ROS CLI | x86 host |
| 4 | Create a Workspace | x86 host |
| 5 | Activate the Development Environment | x86 host → container |
| 6 | Build the G1 Package | Container |
| 7 | Run a Policy | Container; hardware network setup on host |

#### 1. Initialize Compute

Use Ubuntu 24.04 and an NVIDIA GPU with a driver compatible with this checkout's
CUDA 13.2 configuration. Install the GPU driver using the
[official compute setup guide](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/initializecompute.html).
Check the host before proceeding:

```bash
uname -m                       # x86_64
cat /etc/os-release             # Ubuntu 24.04 / noble
nvidia-smi                     # Verify GPU visibility and driver version
```

The container supplies its CUDA and ROS dependencies; the GPU driver runs on
the host. NVIDIA network access is required for internal GitLab, apt, and
Artifactory downloads. Use SSD storage for the checkout, images, and Bazel cache.

#### 2. Configure Isaac ROS Apt Repository

The following is the **internal development** apt configuration for x86 in
this checkout. See the [apt guide](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/configaptrepo.html)
when using a different release.

##### 2.1. Set the locale and install prerequisites

```bash
sudo apt-get update
sudo apt-get install -y curl gnupg locales software-properties-common git git-lfs
sudo locale-gen en_US.UTF-8
sudo update-locale LANG=en_US.UTF-8
export LANG=en_US.UTF-8
sudo add-apt-repository -y universe
```

##### 2.2. Add the internal x86 apt repository

```bash
curl -fsSL https://isaac.download.nvidia.com/isaac-ros/repos.key \
  | sudo gpg --dearmor --yes -o /usr/share/keyrings/nvidia-isaac-ros.gpg
printf '%s\n' \
  'deb [signed-by=/usr/share/keyrings/nvidia-isaac-ros.gpg] http://isaac-debian-repo.nvidia.com:8080 noble main external-main' \
  | sudo tee /etc/apt/sources.list.d/nvidia-isaac-ros.list
printf '%s\n' 'Package: *' 'Pin: origin isaac.download.nvidia.com' 'Pin-Priority: 400' \
  | sudo tee /etc/apt/preferences.d/isaac-ros
sudo apt-get update
```

#### 3. Initialize Isaac ROS CLI

##### 3.1. Install the CLI

```bash
sudo apt-get install -y isaac-ros-cli
```

##### 3.2. Set up Docker and NVIDIA Container Toolkit

Install Docker Engine using the [Ubuntu Docker installation guide](https://docs.docker.com/engine/install/ubuntu/#install-using-the-repository).
Then [install NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html#with-apt-ubuntu-debian)
and [configure the Docker runtime](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html#configuring-docker).
Follow the [Isaac ROS CLI initialization guide](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/initroscli.html)
and add your user to the Docker group:

```bash
sudo usermod -aG docker "$USER"
```

Log out and log back in so the Docker group membership takes effect. Then:

```bash
sudo systemctl daemon-reload
sudo systemctl restart docker

docker info | grep -E 'Runtimes|Default Runtime'
docker run --rm hello-world
docker run --rm --gpus all ubuntu:24.04 bash -lc 'echo "NVIDIA runtime OK"'
```

##### 3.3. Select Docker isolation mode

```bash
sudo isaac-ros init docker
```

#### 4. Create a Workspace

##### 4.1. Clone the repository and policy branch

Clone the `main` branch of the `jkamohara/isaac` fork, which contains this
`g1_bringup` package and its policy bundles.

```bash
git lfs install
mkdir -p "$HOME/workspaces"
git clone --recurse-submodules --branch main \
  ssh://git@gitlab-master.nvidia.com:12051/jkamohara/isaac.git "$HOME/workspaces/isaac"
cd "$HOME/workspaces/isaac"
git lfs pull
```

For an existing checkout, start at `cd "$HOME/workspaces/isaac"`. Ensure its
submodules and LFS files are present before building. Follow the root
[repository setup guide](../../../../../README.md) for GitLab/NGC access.

##### 4.2. Configure repository credentials

The container configuration mounts the host's `~/.ssh` and `~/.netrc` read-only.
Configure GitLab SSH access for cloning, and store the GitLab token in
`~/.netrc` for Bazel's HTTPS dependency downloads. Its entry has this format
(edit the file locally; do not commit it):

```text
machine gitlab-master.nvidia.com
  login YOUR_GITLAB_USERNAME
  password YOUR_GITLAB_TOKEN
```

#### 5. Activate the Development Environment

##### 5.1. Prepare the host paths

Prepare the paths used by this repository's Docker configuration:

```bash
mkdir -p "$HOME/.ssh" "$HOME/.aws" "$HOME/.cache" "$HOME/.config"
touch "$HOME/.gitconfig" "$HOME/.bash_history" "$HOME/.netrc"
chmod 600 "$HOME/.netrc"
```

##### 5.2. Build and enter the development container

```bash
export ISAAC_ROS_WS="$HOME/workspaces/isaac"
cd "$ISAAC_ROS_WS"
isaac-ros activate --build-local --no-push
```

The repository's `.isaac-ros-cli/config.yaml` selects the `ros_eng` image layer,
which installs Bazelisk and the development tools. Build this image once; use
`isaac-ros activate` for later sessions. Rebuild if the image configuration
changes or the matching local image is missing. Do not reinstall the G1 apt
package on every container entry: the commands below build the custom package
from source with Bazel.

#### 6. Build the G1 Package

The host checkout is mounted at `/workspaces/isaac_ros-dev`. Run:

```bash
cd /workspaces/isaac_ros-dev
bazel build \
  //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:g1 \
  //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:shell
```

This checkout defaults to x86_64, so these commands do not need Jetson or ARM64
build flags. The first build fetches dependencies and compiles the runtime;
later runs reuse the cache. Bazel supplies the ROS environment, so this workflow
does not require a `colcon build` or `source install/setup.bash` step.

#### 7. Run a Policy

Continue with the [Run section](#run): choose **Simulation (MuJoCo)** or
**Hardware (real G1)**. Both present the tmux launcher first and individual
commands as an optional alternative.

For a second terminal, open a new host terminal and enter the same container:

```bash
export ISAAC_ROS_WS="$HOME/workspaces/isaac"
cd "$ISAAC_ROS_WS"
isaac-ros activate
```

Inside that container, use the `g1_bringup:shell` Bazel target shown in the Run
section before issuing ROS commands. For SSH without a graphical desktop,
use `enable_viewer:=false` with the individual MuJoCo launch command.

</details>

### Jetson

<details>
<summary><strong>Jetson setup: install Isaac ROS CLI, build, and run a G1 policy</strong></summary>

Follow these stages in order. Steps 1–5 run on the **Jetson host**; steps 6–7
run **inside the container**. Step 8 labels the host and container commands
separately.

| Step | Stage | Where |
| --- | --- | --- |
| 1 | Initialize Compute | Jetson host |
| 2 | Configure Isaac ROS Apt Repository | Jetson host |
| 3 | Initialize Isaac ROS CLI | Jetson host |
| 4 | Create a Workspace | Jetson host |
| 5 | Activate the Development Environment | Jetson host → container |
| 6 | Build the G1 Package | Container |
| 7 | Deploy in MuJoCo | Container |
| 8 | Deploy on the Real G1 | Host networking, then container |
| 9 | Troubleshooting | As indicated |

#### 1. Initialize Compute

These instructions build this internal monorepo directly on the Jetson. They
use its `g1_bringup` package and included policy bundles.

This checkout's `config/isaac_config_lock.yaml` selects **JetPack 7.2 / L4T
r39.2**, and `.bazelrc` provides the corresponding `jetpack72` build configuration.
Do not use these build flags as a compatibility workaround for JetPack 6.2 or
7.1; use a repository release and container matching that JetPack version.
Jetson and real G1 execution have not yet been validated for this custom package.

On the **Jetson host**, check the installed platform:

```bash
uname -m                       # aarch64
cat /etc/nv_tegra_release       # R39 (release), REVISION: 2.0 for this checkout
cat /etc/os-release             # Ubuntu 24.04 / noble
```

Complete the [Jetson compute setup](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/initializecompute.html),
including JetPack components, the recommended power mode, and clock settings.
Use SSD storage for the checkout, Docker images, and Bazel cache.
The Jetson needs NVIDIA network access to internal GitLab, apt, and Artifactory
servers during setup and builds.

#### 2. Configure Isaac ROS Apt Repository

The following is the **internal development** apt configuration for Jetson in
this checkout. See the [apt guide](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/configaptrepo.html)
when using a different release.

##### 2.1. Set the locale and install prerequisites

```bash
sudo apt-get update
sudo apt-get install -y curl gnupg locales software-properties-common git git-lfs
sudo locale-gen en_US.UTF-8
sudo update-locale LANG=en_US.UTF-8
export LANG=en_US.UTF-8
sudo add-apt-repository -y universe
```

##### 2.2. Add the internal Jetson apt repository

```bash
curl -fsSL https://isaac.download.nvidia.com/isaac-ros/repos.key \
  | sudo gpg --dearmor --yes -o /usr/share/keyrings/nvidia-isaac-ros.gpg
printf '%s\n' \
  'deb [signed-by=/usr/share/keyrings/nvidia-isaac-ros.gpg] http://isaac-debian-repo.nvidia.com:8080 noble-jetpack main external-main' \
  | sudo tee /etc/apt/sources.list.d/nvidia-isaac-ros.list
printf '%s\n' 'Package: *' 'Pin: origin isaac.download.nvidia.com' 'Pin-Priority: 400' \
  | sudo tee /etc/apt/preferences.d/isaac-ros
sudo apt-get update
```

#### 3. Initialize Isaac ROS CLI

##### 3.1. Install the CLI

```bash
sudo apt-get install -y isaac-ros-cli
```

##### 3.2. Set up Docker and NVIDIA Container Toolkit

Complete the [Jetson Docker setup](https://docs.nvidia.com/jetson/agx-thor-devkit/user-guide/latest/setup_docker.html)
for Docker Engine and NVIDIA Container Toolkit. Then follow the
[CLI initialization guide](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/initroscli.html):

```bash
sudo usermod -aG docker "$USER"
```

Log out and log back in so the Docker group membership takes effect. Then:

```bash
sudo systemctl daemon-reload
sudo systemctl restart docker

docker info | grep -E 'Runtimes|Default Runtime'
docker run --rm hello-world
docker run --rm --gpus all ubuntu:24.04 bash -lc 'echo "NVIDIA runtime OK"'
```

##### 3.3. Select Docker isolation mode

```bash
sudo isaac-ros init docker
```

#### 4. Create a Workspace

##### 4.1. Clone the repository and policy branch

Clone the `main` branch of the `jkamohara/isaac` fork, which contains this
`g1_bringup` package and its policy bundles.

```bash
git lfs install
mkdir -p "$HOME/workspaces"
git clone --recurse-submodules --branch main \
  ssh://git@gitlab-master.nvidia.com:12051/jkamohara/isaac.git "$HOME/workspaces/isaac"
cd "$HOME/workspaces/isaac"
git lfs pull
```

For an existing checkout, start at `cd "$HOME/workspaces/isaac"`. Ensure its
submodules and LFS files are present before building. Follow the root
[repository setup guide](../../../../../README.md) for GitLab/NGC access.

##### 4.2. Configure repository credentials

The container configuration mounts the host's `~/.ssh` and `~/.netrc` read-only.
Configure GitLab SSH access for cloning, and store the GitLab token in
`~/.netrc` for Bazel's HTTPS dependency downloads. Its entry has this format
(edit the file locally; do not commit it):

```text
machine gitlab-master.nvidia.com
  login YOUR_GITLAB_USERNAME
  password YOUR_GITLAB_TOKEN
```

#### 5. Activate the Development Environment

##### 5.1. Prepare the host paths

Prepare the paths used by this repository's Docker configuration:

```bash
mkdir -p "$HOME/.ssh" "$HOME/.aws" "$HOME/.cache" "$HOME/.config"
touch "$HOME/.gitconfig" "$HOME/.bash_history" "$HOME/.netrc"
chmod 600 "$HOME/.netrc"
```

##### 5.2. Build and enter the development container

```bash
export ISAAC_ROS_WS="$HOME/workspaces/isaac"
cd "$ISAAC_ROS_WS"
isaac-ros activate --build-local --no-push
```

The repository's `.isaac-ros-cli/config.yaml` selects the `ros_eng` image layer,
which installs Bazelisk and the development tools. Build this image once; use
`isaac-ros activate` for later sessions. Rebuild if the image configuration
changes or the matching local image is missing. Do not reinstall the G1 apt
package on every container entry: the commands below build the custom package
from source with Bazel.

#### 6. Build the G1 Package

The host checkout is mounted at `/workspaces/isaac_ros-dev`. Run:

```bash
cd /workspaces/isaac_ros-dev
bazel build --config=jetpack72 --config=arm64_native \
  //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:g1 \
  //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:shell
```

`jetpack72` selects the Jetson target libraries; `arm64_native` selects ARM64
build-time ROS tools because the build itself runs on the Jetson. Use both
flags for every `bazel build` and `bazel run` command below. The first build
fetches dependencies and compiles the runtime; later runs reuse the cache.
Bazel supplies the ROS environment, so this workflow does not require a
`colcon build` or `source install/setup.bash` step.

#### 7. Deploy in MuJoCo

##### 7.1. Launch the robot and selected policy

In **container terminal 1**:

```bash
cd /workspaces/isaac_ros-dev
export ROS_DOMAIN_ID=45
bazel run --config=jetpack72 --config=arm64_native \
  //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:g1 -- \
  hardware_type:=mujoco policy:=g1_29dof_mpm_locomotion_5f1xyc21
```

For SSH without a graphical desktop, add `enable_viewer:=false`. Select another
included bundle using its directory name under `g1_bringup/policies/`. An
external bundle uses `inference_controller_config_path:=/path/to/bundle.yaml`;
keep the YAML, ONNX, and referenced safetensors files together inside the mounted
workspace. External bundles must use the ROS-adapted format produced by
`g1_bringup/scripts/prepare_bundle.py`.

Startup gates policy joint targets with `blend_ratio=0.0`. Wait for
`Core InferenceController activated` before enabling policy control.

##### 7.2. Open a second container terminal

Open **another terminal on the Jetson host** and enter the same container:

```bash
export ISAAC_ROS_WS="$HOME/workspaces/isaac"
cd "$ISAAC_ROS_WS"
isaac-ros activate
```

In that **container terminal 2**, start the ROS command shell:

```bash
cd /workspaces/isaac_ros-dev
export ROS_DOMAIN_ID=45
bazel run --config=jetpack72 --config=arm64_native \
  //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:shell
```

##### 7.3. Enable the policy and release the virtual gantry

At the resulting shell prompt:

```bash
ros2 param set --no-daemon /safety_controller blend_ratio 1.0
# MuJoCo only: release the virtual gantry after activating the policy.
ros2 service call /virtual_gantry/set_gantry_enabled std_srvs/srv/SetBool '{data: false}'
```

##### 7.4. Send velocity commands from the keyboard

In the same command shell:

```bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -p speed:=0.3 -p turn:=0.3 -p repeat_rate:=20.0
```

The keyboard publishes `/cmd_vel`. This package's heading adapter converts it
to `/policy_cmd_vel` and `/policy_heading`. `repeat_rate` keeps commands arriving
before the adapter's 0.5-second velocity-command timeout. Use `k` for zero
velocity.

##### 7.5. Return to the startup pose

After exiting teleop with Ctrl-C, gate policy targets again with:

```bash
ros2 param set --no-daemon /safety_controller blend_ratio 0.0
```

This returns to the startup pose; it is **not** a zero-torque or damping stop.

#### 8. Deploy on the Real G1

First validate the selected bundle in simulation. Support the robot for the
initial hardware test and keep the robot's emergency-stop procedure available.
The Jetson must have an Ethernet connection to the G1's low-level control board.
This also applies when the Jetson is mounted on the robot.

##### 8.1. Configure the robot Ethernet connection on the host

On the **Jetson host**, identify the robot-facing Ethernet interface:

```bash
ip -br link
ip -br address
cd "$HOME/workspaces/isaac"
python3 ros_ws/src/isaac_ros_robots/isaac_ros_robots_tools/scripts/setup_network.py
```

Select **Unitree G1** and that Ethernet interface. The script changes the
selected interface's IP address and multicast route; do not select the Wi-Fi
or Ethernet connection carrying your SSH session. The G1 preset assigns
`192.168.123.99/24` to the host interface and checks `192.168.123.161` for the
robot. If the lab uses different addressing, configure the matching addresses
instead. Confirm the interface again with `ip -br address` after a host reboot.

##### 8.2. Launch the hardware driver and selected policy

In **container terminal 1**, replace `eno1` with the interface just configured:

```bash
cd /workspaces/isaac_ros-dev
export ROS_DOMAIN_ID=45
bazel run --config=jetpack72 --config=arm64_native \
  //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:g1 -- \
  hardware_type:=real network_interface:=eno1 \
  policy:=g1_29dof_mpm_locomotion_5f1xyc21
```

##### 8.3. Enable the policy and send velocity commands

After controller activation, use the same second-terminal command shell,
`blend_ratio` commands, and teleop command as in simulation. Omit the virtual
gantry service call on hardware.

##### 8.4. Understand the two network configurations

The CLI container uses host networking, so it sees the Jetson's Ethernet
interfaces. `network_interface` selects the interface for the Unitree SDK2
hardware driver. The `g1` Bazel target selects Fast DDS for ROS; do not replace
it with a ROS Cyclone DDS configuration just to select the robot interface.
The repository defaults ROS discovery to `LOCALHOST`, which supports ROS
terminals on this Jetson while the Unitree driver communicates over Ethernet.
Use the same `ROS_DOMAIN_ID` in all terminals, and choose a different domain
from other robots. Remote ROS nodes on another computer require a separate
ROS discovery configuration; SSH into the Jetson to follow this guide.

#### 9. Troubleshooting

| Symptom | Check |
| --- | --- |
| `401` or `403` during dependency downloads | GitLab token in the mounted `~/.netrc`, repository permissions, and NVIDIA network access. SSH authentication alone does not authenticate Bazel HTTPS downloads. |
| Container image not found | From this checkout on the host, run `isaac-ros activate --build-local --no-push`. |
| Wrong executable architecture or CUDA library errors | Match the host JetPack version to the checkout and container; use both native Jetson Bazel flags. |
| ROS package missing in a second terminal | Enter the same container and run the `g1_bringup:shell` target before using ROS commands. |
| `Node not found` or waiting for `robot_description` | Check earlier launch errors, use the same ROS domain and middleware in each terminal, and avoid a separate `unshare --net` session. |
| Keyboard command immediately returns to zero | Keep `repeat_rate:=20.0`; the heading adapter times out after 0.5 seconds without a command. |
| Hardware has no joint/IMU data | Check `network_interface`, the host IP/multicast configuration, Ethernet link, and G1 low-level control availability. |

References: [Getting started](https://isaac.gitlab-master-pages.nvidia.com/isaac/getting_started/index.html),
[Isaac ROS Deploy](https://isaac.gitlab-master-pages.nvidia.com/isaac/repositories_and_packages/isaac_ros_deploy/index.html),
and [G1 deployment tutorial](https://isaac.gitlab-master-pages.nvidia.com/isaac/repositories_and_packages/isaac_ros_deploy/tutorials/deploy_agile.html).

</details>

## Run

Run the commands below from the repository root inside the development
container, except for the hardware network setup explicitly marked **host**.
For native JetPack 7.2 on Jetson, use `scripts/run_tmux_jetson.sh` in place of
`scripts/run_tmux.sh` in the examples below. It adds
`--config=jetpack72 --config=arm64_native` to all four panes, including the
blend-ratio and teleop commands, and uses the separate `g1_monorepo_jetson`
session. Export the same nonzero `ROS_DOMAIN_ID` before starting it; the
launcher passes that value to every new pane. Reattaching preserves existing
pane environments. Commands run individually still need the Bazel flags
shown in the Jetson guide above.

The tmux launcher creates four panes: launch, activation, teleop, and policy
disable. Commands are prefilled but do not run until you press Enter. Start the
launch pane first, wait for `Core InferenceController activated`, then run the
activation and teleop commands. Click a pane to select it.

The fourth pane sets `blend_ratio=0.0` to return to the startup pose; this is
not an emergency stop or zero-torque state. Re-running the script reattaches
to the existing session without changing its policy or panes. Start a new
session when changing the policy or switching between simulation and hardware.

<details>
<summary><strong>Simulation (MuJoCo)</strong></summary>

#### Run with tmux

```bash
./scripts/run_tmux.sh g1_29dof_mpm_locomotion_5f1xyc21 mujoco
```

MuJoCo holds the robot with a virtual gantry during startup. After activating
the policy, press `G` in the MuJoCo viewer to release the gantry. Alternatively,
use the gantry service command below from the ROS command shell.

#### Optional: run commands individually

In the first container terminal:

```bash
bazel run //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:g1 -- \
  hardware_type:=mujoco policy:=g1_29dof_mpm_locomotion_5f1xyc21
```

For SSH without a graphical desktop, add `enable_viewer:=false` to the launch
command. Wait for `Core InferenceController activated`. In a second container
terminal, open the ROS command shell, enable the policy, and release the gantry:

```bash
bazel run //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:shell
ros2 param set --no-daemon /safety_controller blend_ratio 1.0
ros2 service call /virtual_gantry/set_gantry_enabled std_srvs/srv/SetBool '{data: false}'
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p speed:=1.0 -p turn:=1.0
```

After exiting teleop with Ctrl-C, return to the startup pose:

```bash
ros2 param set --no-daemon /safety_controller blend_ratio 0.0
```

</details>

<details>
<summary><strong>Hardware (real G1)</strong></summary>

#### Configure the robot network

Before the first real-robot run, configure networking **on the host**:

```bash
./ros_ws/src/isaac_ros_robots/isaac_ros_robots_tools/scripts/setup_network.py
```

Select the Ethernet interface connected to the G1's low-level control board.
Replace `eno1` below with that interface. Real-hardware operation has not been
validated for this port.

#### Run with tmux

Inside the container:

```bash
# x86
./scripts/run_tmux.sh g1_29dof_mpm_locomotion_5f1xyc21 real eno1

# jetson
./scripts/run_tmux_jetson.sh g1_29dof_mpm_locomotion_5f1xyc21 real eno1
```

Wait for controller activation before enabling the policy. Hardware mode does
not start MuJoCo and has no virtual gantry to release.

#### Optional: run commands individually

In the first container terminal:

```bash
bazel run //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:g1 -- \
  hardware_type:=real network_interface:=eno1 \
  policy:=g1_29dof_mpm_locomotion_5f1xyc21
```

Wait for `Core InferenceController activated`. In a second container terminal:

```bash
bazel run //ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup:shell
ros2 param set --no-daemon /safety_controller blend_ratio 1.0
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p speed:=1.0 -p turn:=1.0
```

After exiting teleop with Ctrl-C, return to the startup pose:

```bash
ros2 param set --no-daemon /safety_controller blend_ratio 0.0
```

</details>

For both modes, keyboard controls are `i` / `,` for forward/backward, `j` / `l`
for turning, Shift+`J` / Shift+`L` for sideways motion, and `k` for zero velocity.

## Record torque and monitor temperatures

Run these commands inside the container from the repository root. Use the same
`ROS_DOMAIN_ID` as the robot launch in every terminal.

The G1 launch always publishes the safety controller's final joint targets,
feedforward torque and PD gains on `/safety_controller/blended_command`.
No extra launch argument is needed.

Open one tmux window with three panes: topic list on the left, recording at the
upper right, and Foxglove at the lower right. Click a pane and press Enter to run
its prefilled command:

```bash
./scripts/run_tmux_data.sh
# Native JetPack 7.2:
./scripts/run_tmux_data.sh --jetson
```

The default recording directory is `recordings/YYYYMMDD_HHMMSS`. Optionally pass
an output directory as the last argument. Re-running the launcher reattaches to
the existing session without replacing its commands or recording directory.

Alternatively, run the commands individually. Start recording **before** walking
and keep recording during the subsequent standing period:

```bash
./scripts/record_data.sh recordings/876gqr97_trial01
```

Choose a new output directory for every trial. Ctrl-C closes the MCAP bag.
Record the selected policy, checkpoint and experiment conditions alongside it.
On native JetPack 7.2, use `./scripts/record_data.sh --jetson recordings/876gqr97_trial01`.

For live monitoring, use another terminal:

```bash
./scripts/run_foxglove.sh
```

On native JetPack 7.2, use `./scripts/run_foxglove.sh --jetson`.
An optional final argument changes the default port, for example `8766`.
In Foxglove, open a Foxglove WebSocket connection to `ws://localhost:8765`
(or `ws://JETSON_IP:8765` when connecting from another computer).

| Signal | Topic / field | Meaning |
| --- | --- | --- |
| Joint torque | `/joint_states`, `effort` | MuJoCo applied joint torque; on hardware, Unitree SDK `tau_est` |
| Joint motion | `/joint_states`, `position`, `velocity` | Match each array element to the same message's `name` array |
| Controller targets and gains | `/safety_controller/blended_command` | Final position/velocity targets, feedforward effort, `kp`, `kd` |
| Motor temperature | `/diagnostics` | Joint-named status entries with `surface_temperature_C` and `winding_temperature_C` |

In Foxglove's Plot panel, select left/right knee effort and temperature fields.
The command message's `effort` is **feedforward only**; total PD torque also
includes the position and velocity feedback terms. MuJoCo does not publish real
motor temperatures. Hardware temperature recording has not yet been validated.

The recorder also captures velocity/heading commands, IMU, transforms, simulation
clock, robot description and ROS logs. Watch recorder warnings for dropped
messages; a bag directory alone does not guarantee a complete recording.

## Policy bundles

All bundles are under `ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup/policies/`.
Select a bundle directory name with `policy:=NAME`.

| W&B run | Bundle path | Checkpoint |
| --- | --- | --- |
| [8y9fhovl](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/8y9fhovl) | [policies/g1_mpm_locomotion_8y9fhovl/](policies/g1_mpm_locomotion_8y9fhovl/) | `model_38000.pt` |
| [k9mk7koh](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/k9mk7koh) | [policies/g1_mpm_locomotion_k9mk7koh/](policies/g1_mpm_locomotion_k9mk7koh/) | `model_26500.pt` |
| [0hg1l3g4](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/0hg1l3g4) | [policies/g1_mpm_locomotion_0hg1l3g4/](policies/g1_mpm_locomotion_0hg1l3g4/) | `model_21500.pt` |
| [wqqevars](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/wqqevars) | [policies/g1_mpm_locomotion_wqqevars/](policies/g1_mpm_locomotion_wqqevars/) | `model_24500.pt` |
| [mz3xm6nh](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/mz3xm6nh) | [policies/g1_mpm_locomotion_mz3xm6nh/](policies/g1_mpm_locomotion_mz3xm6nh/) | `model_25000.pt` |
| [oxy2txmd](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/oxy2txmd) | [policies/g1_mpm_locomotion_oxy2txmd/](policies/g1_mpm_locomotion_oxy2txmd/) | `model_21500.pt` |
| [3dzhq0v2](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/3dzhq0v2) | [policies/g1_mpm_locomotion_3dzhq0v2/](policies/g1_mpm_locomotion_3dzhq0v2/) | `model_13500.pt` |
| [z84rvj6i](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/z84rvj6i) | [policies/g1_mpm_locomotion_z84rvj6i/](policies/g1_mpm_locomotion_z84rvj6i/) | `model_25000.pt` |
| [17c4dih9](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/17c4dih9) | [policies/g1_mpm_locomotion_17c4dih9/](policies/g1_mpm_locomotion_17c4dih9/) | `model_25000.pt` |
| [7cawp916](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/7cawp916) | [policies/g1_mpm_locomotion_7cawp916/](policies/g1_mpm_locomotion_7cawp916/) | `model_16000.pt` |
| [gzi8enaw](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/gzi8enaw) | [policies/g1_mpm_locomotion_gzi8enaw/](policies/g1_mpm_locomotion_gzi8enaw/) | `model_11000.pt` |
| [876gqr97](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/876gqr97) | [policies/g1_mpm_locomotion_876gqr97/](policies/g1_mpm_locomotion_876gqr97/) | `model_18000.pt` |
| [5f1xyc21](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/5f1xyc21) | [policies/g1_29dof_mpm_locomotion_5f1xyc21/](policies/g1_29dof_mpm_locomotion_5f1xyc21/) | `model_39998.pt` |
| [evh180n4](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_mpm_mixed_terrain/runs/evh180n4) | [policies/g1_29dof_mpm_locomotion_evh180n4/](policies/g1_29dof_mpm_locomotion_evh180n4/) | `model_39998.pt` |
| [m2g8sl07](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_soft_rough_ppo/runs/m2g8sl07) | [policies/g1_29dof_mpm_locomotion_m2g8sl07/](policies/g1_29dof_mpm_locomotion_m2g8sl07/) | `model_19999.pt` |
| [ufmtmwr9](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_soft_flat_ppo/runs/ufmtmwr9) | [policies/g1_29dof_mpm_locomotion_ufmtmwr9/](policies/g1_29dof_mpm_locomotion_ufmtmwr9/) | `model_19999.pt` |
| [ftuwther](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_rigid_rough_ppo/runs/ftuwther) | [policies/g1_29dof_rigid_locomotion_ftuwther/](policies/g1_29dof_rigid_locomotion_ftuwther/) | `model_23000.pt` |
| [tyiby5k6](https://forge.coreweave.com/wandb/jnskkmr/g1_29dof_soft_flat_ppo/runs/tyiby5k6) | [policies/g1_29dof_rom_locomotion_tyiby5k6/](policies/g1_29dof_rom_locomotion_tyiby5k6/) | `model_19999.pt` |

`g1_mpm_locomotion_876gqr97` was exported with the tilting-sand balance task,
which uses zero velocity commands.

Run IDs and checkpoint filenames come from each bundle's `provenance.json`.

For an external bundle, use `inference_controller_config_path:=/path/to/bundle.yaml`
instead, keeping its referenced ONNX and safetensors files beside it.

## Troubleshooting

### Missing packages (`tmux`, `ping`, XCB headers)

An existing container or older image may lack packages needed by the policy and
data-recording tmux launchers. Updating the repository or running a Bazel build
does not install these system packages.

To use the current container **without rebuilding the image**, run inside it:

```bash
sudo apt-get update
sudo apt-get install -y tmux iputils-ping libxcb1-dev
```

Then rerun the command that failed. These manual installs remain in this container,
but are lost when it is removed and recreated from the older image.

| Error | Package | Purpose |
| --- | --- | --- |
| `xcb/xcb.h: No such file or directory` | `libxcb1-dev` | XCB headers needed by the X11 build |
| `No such file or directory: 'ping'` | `iputils-ping` | Robot connectivity checks |
| `Install tmux in the development container.` | `tmux` | Multi-pane launcher |

For a persistent installation, the updated `docker/Dockerfile.ros_eng` includes
these packages for both x86 and Jetson. Rebuild the development image once.
Stop any running robot controller first, then run on the **host**, outside the
container:

```bash
cd ~/workspaces/isaac
docker stop isaac_monorepo_dev_container  # If the old container is still running.
isaac-ros activate --build-local --no-push
```

The CLI attaches to an existing running container before checking the image,
even with `--build-local`; stop the old container to use the rebuilt image.
Inside the new container, verify the tools:

```bash
command -v ping tmux
tmux -V
```

The packages now persist across container recreation. Later sessions only need
`isaac-ros activate`. For the missing XCB header, rerun the Bazel build without
clearing its cache.

### ROS domain configuration

Use the same nonzero `ROS_DOMAIN_ID` in every terminal that launches the ROS
stack or runs ROS CLI commands. For example, set this inside the container
before starting the launcher:

```bash
export ROS_DOMAIN_ID=45
```

`45` is an example; choose a domain distinct from other ROS stacks sharing
your network. The Unitree SDK2 hardware driver separately uses Cyclone DDS
domain `0` to communicate with the robot. A nonzero ROS domain keeps the
ROS Fast DDS traffic separate from that connection.

`run_tmux_jetson.sh` forwards the exported value to all newly created panes.
If reusing an existing session, check `echo "$ROS_DOMAIN_ID"` in each pane
and set it there if needed. Restart any ROS processes launched with a
different domain; changing a shell variable does not update running processes.
