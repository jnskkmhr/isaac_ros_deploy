# Standalone G1 policy deployment

A focused copy of the existing Isaac ROS G1 deployment packages. It keeps the
G1 launch file, controller configuration, policy bundles, observation processing,
joint command conversion, and safety behavior. It builds their dependencies with
colcon, without building the Isaac monorepo.

The default inference backend is **CPU ONNX Runtime**. CPU LibTorch remains for
the original controller's tensor operations. CUDA, TensorRT, and Triton are not
required by this default image.

## First-time setup

On a Linux host with Docker installed:

```bash
cd ~/workspace/isaac_ros_deploy
./scripts/activate.sh --build
```

Inside the container:

```bash
./scripts/build.sh
```

The source, build output, policies, and recordings are stored in the host workspace
mounted at `/workspaces/isaac_ros_deploy`. Subsequent container starts use:

```bash
./scripts/activate.sh
```

The container shell uses `admin` with your host UID/GID, so workspace files stay
owned by your host user. The standard colored Bash prompt also applies in tmux.

This workspace uses its own Docker image; it does not change `isaac-ros activate`
or the existing monorepo container. The image includes tmux, ping, keyboard teleop,
Foxglove Bridge, and the MCAP rosbag plugin.

## Run

<details>
<summary>Simulation</summary>

Inside the container:

```bash
./scripts/run_tmux.sh g1_mpm_locomotion_8y9fhovl
```

One window contains four panes: launch, policy activation, keyboard commands, and
policy deactivation. Mouse selection is enabled. Commands are filled in but
**not executed**; press Enter in the pane you want to run.

Optional individual commands:

```bash
./scripts/ros.sh launch g1_bringup g1.launch.py policy:=g1_mpm_locomotion_8y9fhovl hardware_type:=mujoco
./scripts/ros.sh param set --no-daemon /safety_controller blend_ratio 1.0
./scripts/ros.sh run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p speed:=1.0 -p turn:=1.0
./scripts/ros.sh param set --no-daemon /safety_controller blend_ratio 0.0
```

Use `enable_viewer:=false` for headless simulation. With a desktop display,
`activate.sh` passes the display socket and available Xauthority file. When the
NVIDIA Docker runtime is registered, it also exposes the host NVIDIA graphics
driver so MuJoCo uses GPU OpenGL instead of CPU software rendering. Policy
inference uses the backend selected by the image profile. Graphics rendering and
policy inference are separate GPU workloads.

</details>

<details>
<summary>Hardware</summary>

Use the Ethernet interface connected to the G1 low-level board, including when
running on a backpack Jetson. Replace `eth0` with that interface:

```bash
./scripts/run_tmux.sh g1_mpm_locomotion_8y9fhovl real eth0
```

Equivalent launch:

```bash
./scripts/ros.sh launch g1_bringup g1.launch.py policy:=g1_mpm_locomotion_8y9fhovl hardware_type:=real network_interface:=eth0
```

The original Unitree SDK2 hardware plugin communicates with the robot. Host
networking is retained. ROS uses Fast DDS by default; `network_interface` selects
the robot interface for the Unitree SDK independently of ROS discovery.

The launch keeps the existing default `blend_ratio=0.0`. This selects the safety
controller's existing behavior; it is not a hardware emergency stop. Policy
activation remains an explicit parameter change.

</details>

<details>
<summary>Recording and Foxglove</summary>

```bash
./scripts/run_tmux_data.sh
```

Three panes contain topic listing, recording, and Foxglove commands. Commands are
not submitted automatically. Optional individual commands:

```bash
./scripts/ros.sh topic list
./scripts/record_data.sh recordings/trial_01
./scripts/run_foxglove.sh
```

Recordings use MCAP and persist in the host workspace. Topics include joint
states, blended joint commands, diagnostics, IMU, velocity commands, simulation
state, TF, and robot description. Foxglove listens on port 8765. The old optional
`--jetson` argument is accepted by these data scripts but is no longer needed;
the image and build use the host's native architecture.

</details>

## Older JetPack support

The CPU image uses Ubuntu 24.04 / ROS Jazzy userspace and architecture-specific
CPU wheels. It does not link the policy runner to the host JetPack CUDA version.
This removes the CUDA/Triton compatibility barrier; it does not establish that
all older JetPack releases or boards are supported.

Native ARM64 compilation, control-loop timing, Unitree communication, and physical
robot behavior must still be validated on the target Jetson. No robot hardware
has been tested as part of this extraction. GUI rendering can have separate
host-driver requirements; hardware deployment does not require a GUI.

## GPU inference

Use the same G1 launch, tmux, and recording commands with either backend. Select
an image profile on the host, then build the ROS packages inside that container:

| Profile | Target | Inference runtime | Validation |
|---|---|---|---|
| `cpu` | x86 or ARM64 | ONNX Runtime CPU | x86 tested; Jetson pending |
| `x86-gpu` | x86 NVIDIA GPU | Triton 2.69 / CUDA 13.2 / cuDNN 9.20 | See `VALIDATION.md` |
| `jp72-gpu` | JetPack 7.2, ARM64 | Same pinned Triton release, CUDA 13.2, cuDNN 9.20 | Native build and device tests pending |
| `jp71-gpu` | JetPack 7.1, ARM64 | Triton 2.69 with CUDA 13.0 and cuDNN 9.12 | Experimental; binary compatibility and runtime verification required |
| `jp6-gpu` | JetPack 6.2, Orin | Source-built ONNX Runtime 1.23.2 CUDA / CUDA 12.6 / cuDNN 9.3 | Experimental; native build and runtime verification required |
| `jp5-gpu` | JetPack 5.0.2+, Orin/Xavier | Source-built ONNX Runtime 1.23.2 CUDA / CUDA 11.8 / cuDNN 8.9.7 | Experimental; native build, driver compatibility, and runtime verification required |

For x86:

```bash
./scripts/activate.sh --profile x86-gpu --build
# Inside the container:
./scripts/build.sh
./scripts/run_tmux.sh g1_mpm_locomotion_8y9fhovl
```

On a JetPack 7.2 device, use `--profile jp72-gpu` instead. For subsequent starts,
omit `--build`, but retain `--profile`. Select `--profile cpu` to return to CPU
inference. Build and install directories are separate for each profile and CPU
architecture. The first switch requires one package build for that profile.

The x86/JetPack 7 GPU image extends the shared development image. It installs only Triton's
ONNX backend and its runtime dependencies, rather than the full Isaac ROS
monorepo image. CUDA performs model inference; CPU LibTorch 2.9 retains the
controller's tensor processing. This is not the monorepo's CUDA LibTorch build.
TensorRT engine optimization is not enabled. The current Triton runner uses
ONNX Runtime's CUDA execution provider, with one explicit GPU instance on device
0. Container startup checks that the CUDA driver exposes a GPU before ROS starts.

`docker/gpu_dependencies.json` pins URLs and checksums. The Triton and cuDNN
releases for x86/JetPack 7.2 match the source monorepo, and the CUDA runtime
libraries come from NVIDIA's CUDA 13.2 redistribution manifest. Triton downloads
require access to the NVIDIA internal Artifactory used by the monorepo. A fresh
build does not require a local monorepo checkout or Bazel cache.

JetPack 7.1 uses a different CUDA release. Its profile is a candidate configuration,
not a compatibility claim: the monorepo's ARM64 Triton binary must still be tested
against CUDA 13.0/cuDNN 9.12 on the device. JetPack 6/5 use the separate source-build path below. CPU inference remains an
option to test there; its ARM64 build, ROS dependencies, and hardware timing are
also unverified.

The host needs NVIDIA Container Toolkit and a compatible NVIDIA driver. Images
are built natively: an x86 build is not a Jetson binary. GPU architecture changes
do not require different G1 controller source files.

To run focused checks inside a built container:

```bash
./scripts/test.sh
source ros_ws/"$DEPLOY_INSTALL_DIR"/setup.bash
ROS_DOMAIN_ID=161 python3 scripts/check_simulation.py
```

The simulation check only launches MuJoCo. It checks disabled policy blending at
startup, activation/deactivation, and finite joint state/command data. It never
connects to robot hardware.

## Experimental JetPack 6/5 GPU setup

These profiles are implemented, but **neither image has been built or run on a
Jetson**. Treat compiler errors, dependency compatibility, and device execution as
verification work, not as established support for every JetPack 6.x/5.x release.

On the Jetson host, select one:

```bash
# JetPack 6.2 / Orin:
./scripts/activate.sh --profile jp6-gpu --build

# JetPack 5.0.2+ / Orin or Xavier:
./scripts/activate.sh --profile jp5-gpu --build
```

Inside the container, the procedure is unchanged:

```bash
./scripts/build.sh
./scripts/run_tmux.sh <policy_bundle_name>
./scripts/run_tmux_data.sh
```

Omit `--build` on subsequent activation, but keep the chosen `--profile`.
For a low-memory Jetson, use `BUILD_JOBS=1 ./scripts/activate.sh --profile jp6-gpu --build`
(or `jp5-gpu`). Building ONNX Runtime's CUDA provider from source can take substantial
time and disk space; it happens during the first image build, not each activation.

`docker/Dockerfile.jetpack-legacy` uses two stages. The build stage installs a
checksum-pinned CUDA toolkit and compiles ONNX Runtime from commit
`a83fc4d58cb48eb68890dd689f94f28288cf2278` (1.23.2). The final stage copies the
inference libraries; it excludes nvcc and the ONNX Runtime build tree. GCC 11
is used for CUDA compilation. Kernel targets are Orin (87) for `jp6-gpu`, and
Xavier/Orin (72/87) for `jp5-gpu`.

Both profiles retain Ubuntu 24.04 / ROS Jazzy container userspace and the existing
G1 controllers. They select the direct ONNX Runtime CUDA adapter rather than
Triton. `ISAAC_DEPLOY_ONNX_DEVICE=cuda` is set by the image; no policy YAML edit is
needed. The adapter rejects CUDA selection if its runtime lacks the CUDA provider.
Model initialization errors propagate rather than triggering a CPU retry.

The JetPack 5 profile includes NVIDIA's **Jetson-specific CUDA 11.8 compatibility
package** inside the image and places those user-mode driver libraries first in
its library path. It does not install a driver or modify packages on the host.
NVIDIA documents this upgrade mechanism for JetPack 5; the combination with this
Ubuntu 24.04 container still requires verification on your device.
See [NVIDIA's Jetson CUDA upgrade documentation](https://docs.nvidia.com/cuda/archive/11.8.0/cuda-for-tegra-appnote/index.html).
The JetPack 6 profile uses the host driver with CUDA 12.6 userspace libraries.
JetPack 6.0/6.1 are not validated by this JetPack 6.2 candidate.

First verify the image and package build, numerical checks, and headless MuJoCo:

```bash
./scripts/test.sh
source ros_ws/"$DEPLOY_INSTALL_DIR"/setup.bash
ROS_DOMAIN_ID=161 python3 scripts/check_simulation.py
```

GPU execution, graphics, control timing, SDK networking, and physical robot
operation are all pending on Jetson. CPU/GPU numerical validation on x86 checks
the adapter code; it does not validate these ARM64 CUDA builds.

## Source and validation

`SOURCE_MANIFEST.json` records the original monorepo revision and file hashes.
`VALIDATION.md` records the actual build and runtime checks and remaining limits.
Copied packages retain their original licenses. The original monorepo is not
modified by this workspace.
