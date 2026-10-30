# Standalone deployment validation

Tested on x86-64, Ubuntu 24.04 userspace, ROS Jazzy, CPU-only Torch 2.9.0 and
ONNX Runtime 1.23.2. Tests ran on 2026-10-29.

| Check | Result |
| --- | --- |
| Standalone colcon build | 15 packages built, including the Unitree SDK2 hardware plugin |
| Original core and G1 bundle tests | 100 passed; none skipped |
| LEAPP backend mapping tests | 4 passed |
| CPU adapter numerical comparison | All 18 bundles matched Python ONNX Runtime for 30 stateful steps each; rtol 1e-5, atol 1e-6 |
| MuJoCo startup | Robot and four controllers activated; runner logged as `onnxruntime` |
| Safety blending | Started at 0.0; explicitly changed to 1.0, then back to 0.0 |
| Telemetry during the integration check | 2,229 joint-state messages and 2,228 joint-command messages; all 29 joints present and all values finite |
| Control/data tmux | Four/three panes; mouse enabled; commands prefilled without execution |
| Recording dependencies | Foxglove and MCAP ROS packages installed |
| Hardware robot description | 29 command joints generated; no hardware connection attempted |
| Original Triton runner | Initial compile-only check; subsequent GPU runtime results are recorded below |

The source tree is approximately 73 MB. The development image is approximately
4.37 GB, including ROS, compiler tools, CPU Torch, ONNX Runtime, recording, and
Foxglove. It has no CUDA/Triton installation. Build/install output stays in the
host-mounted workspace.

## Limits

- No ARM64/Jetson build or physical G1 test was performed.
- ARM64 CPU Torch wheels are available for the pinned version; this alone does
  not validate Jetson operation or control-loop deadlines.
- The initial CPU comparison checks the C++ adapter against Python ONNX Runtime.
  Subsequent Triton comparisons are recorded below; no physical robot test was run.
- This was a short zero-command MuJoCo check, not a locomotion-quality or thermal
  experiment. No claim is made about changing the policy's learned behavior.
- The CPU image is not a Triton runtime image. Use a GPU image profile for Triton.
- Startup logged one 6.65 ms controller-cycle overrun. Scheduling priority was
  not elevated in the test. This does not establish hard real-time performance.
- Existing URDF/KDL, PID anti-windup, and launch-remapping warnings remain.
  Shutdown logged ROS statistics publisher context errors; the simulation node
  exited. These existing behaviors were not changed during extraction.

## Reproduce

Inside the built container:

```bash
./scripts/test.sh
source ros_ws/"$DEPLOY_INSTALL_DIR"/setup.bash
ROS_DOMAIN_ID=161 python3 scripts/check_simulation.py
```

The simulation check always launches MuJoCo with its viewer disabled. It never
launches the real-hardware interface. Its results are written to
`/tmp/isaac_deploy_validation` inside the container. The initial test logs are
preserved locally under `validation/` (excluded from Git).

## Changes from the monorepo

| Area | Adaptation |
| --- | --- |
| Core inference | Added CPU ONNX Runtime runner and build-time backend selection; retained original Triton runner |
| LEAPP backend mapping | `backend: onnx` maps to the selected compiled runner; policy YAML is unchanged |
| Build manifests | Removed unconditional CUDA/Triton requirements from the CPU path |
| ROS converters | Build the message-to-tensor library used by G1; omit unused GPU tensor-transport nodes |
| Common utilities | Install the original headers without CUDA profiling dependencies |
| Joint-parameter controller | Select `.h` or `.hpp` URDF header according to availability; no logic change |
| Official G1 description package | Install the original hardware-interface Xacro; omit unused remote asset downloads |
| MuJoCo | Same 3.5.0 engine and pinned ROS integration, including the completion-wakeup fix; renamed a shadowed lock variable for strict compilation |
| Shell scripts | Replace Bazel invocation with colcon-installed ROS commands; retain pane layouts and recorded topics |
| tmux default | Match the existing launch default, `g1_29dof_mpm_locomotion_evh180n4`; the previously named `_it_2k` bundle was absent |

G1 launch/configuration files, policy artifacts, custom robot assets, observation
processing, gain conversion, and safety logic are unchanged. The original
monorepo has no new tracked modifications from this work.

## MuJoCo graphics correction

The initial standalone launcher exposed `/dev/dri` but did not request the
NVIDIA container runtime. The running container reported Mesa `llvmpipe`
(LLVM 20.1.2), so OpenGL rendering ran on the CPU.

`activate.sh` now uses the NVIDIA runtime when registered, with graphics,
utility, and display capabilities. A fresh container reported
`NVIDIA GeForce RTX 5090/PCIe/SSE2`, OpenGL 4.6, driver 595.99.02.
The actual G1 viewer and CPU inference controller started successfully in a
20-second MuJoCo smoke test on ROS domain 163. This verifies GPU selection and
viewer startup; no before/after frame-rate benchmark was performed.

Restart the container to apply this change. No image/package rebuild is needed.

The viewer smoke test ended via the timeout tool's SIGINT. During shutdown,
`ros2_control_node` exited with signal 11 and the heading node with signal 2.
The earlier ROS statistics shutdown messages also appeared. Startup and running
inference succeeded, but clean GUI shutdown is not established by this test.

## GPU support: x86 validation and Jetson status

Tested on the RTX 5090 with driver 595.99.02. The `x86-gpu` image uses the
monorepo's pinned Triton 2.69 server/ONNX backend, CUDA 13.2 runtime libraries,
and cuDNN 9.20. CPU Torch 2.9 still performs controller tensor processing.
No TensorRT engine optimization is enabled.

| Check | Result |
|---|---|
| GPU image | Built; approximately 7.71 GB including development tools |
| Missing GPU | Final container startup rejected missing CUDA driver access immediately; no CPU fallback |
| GPU package build | All 15 packages built from the standalone source |
| GPU core/G1 tests | Colcon reports 102 tests passed, none failed or skipped |
| GPU LEAPP mapping tests | 4 passed |
| GPU numerical comparison | All 18 bundles passed, 30 successive reference-input sets per bundle |
| Numerical tolerance | GPU: relative 1e-5, absolute 1e-5; CPU: relative 1e-5, absolute 1e-6 |
| Largest GPU output difference | 9.53674e-6 against Python CPU ONNX Runtime |
| GPU execution evidence | CUDAExecutionProvider loaded; NVIDIA reported the parity process using 712 MiB |
| GPU MuJoCo integration | Runner logged as `triton`; all four expected controllers active |
| Policy blending | Initial value 0.0; activation to 1.0 and return to 0.0 passed |
| GPU integration telemetry | 2,203 joint-state and 2,202 joint-command samples; all 29 joints present and finite |
| Model shutdown | All 18 model instances closed in at most 1.03 seconds |
| ROS process shutdown | MuJoCo controller node exited cleanly after the request cleanup fix |
| CPU regression | All 15 packages rebuilt; 100 core/G1 tests, 4 mapping tests, and all 18 bundle comparisons passed |
| JetPack 7.2 | ARM64 dependency URLs checked; native build, GPU execution, and hardware tests pending |
| JetPack 7.1 | Experimental CUDA 13.0/cuDNN 9.12 configuration; binary compatibility, build, and runtime checks pending |
| JetPack 6/5 | Experimental source-build profiles added; native image builds and device tests pending |

The numerical fixture feeds the same inputs, including history generated by the
CPU reference, into both runners. This isolates inference differences. It does
not establish identical closed-loop locomotion or hardware behavior.

The original 1e-6 absolute tolerance failed on a raw action difference of
1.84e-6. The GPU comparison uses an explicit 1e-5 absolute tolerance for FP32
cross-device accumulation differences. The CPU tolerance is unchanged.

The inherited Triton release callback did not delete completed inference requests.
Those requests retained model references and caused shutdown to wait for the
server's exit timeout. The callback now deletes requests when ownership returns.
Submission errors retain request ownership, and response-error paths wait for
the release callback before discarding its state. A model shutdown limit in the
numerical test catches recurrence of the retained-request problem. The same
shutdown check also passed with CPU inference.

Triton's ONNX backend reports CPU placement for some graph operations and host/GPU
copies. GPU inference is working, but not every ONNX operation runs on the GPU.
This implementation does not promise a latency improvement over CPU inference.

The short MuJoCo check logged a 5.92 ms controller-cycle overrun against the
200 Hz target. ROS statistics publishers still logged invalid-context messages
at shutdown, although all processes exited cleanly. Real-time deadlines, long
runs, Jetson operation, and physical robot behavior remain unverified.

GPU logs are preserved locally under `validation/`. Policy bundles, G1 launch
configuration, gains, action conversion, and safety behavior were not modified.
The original monorepo was not changed.

## Experimental JetPack 6/5 implementation

Added `jp6-gpu` and `jp5-gpu` profiles, a two-stage legacy GPU Dockerfile, and a
pinned ONNX Runtime CUDA source build. The runtime uses the existing ONNX adapter
with explicit CUDA provider selection. Controller and policy interfaces are unchanged.

No JetPack 6/5 image build or device test was performed here. Dependency URL and
script checks do not establish native build compatibility. The source build,
older driver compatibility, ROS Jazzy userspace on the Jetson host, GPU execution,
MuJoCo rendering, control deadlines, and robot communication remain unverified.

Legacy-adapter checks performed on x86:

| Check | Result |
|---|---|
| Script syntax | Passed |
| Profile dispatch | `jp6-gpu` and `jp5-gpu` select the legacy Dockerfile, image, and separate build paths in a mocked Docker check |
| Dependency URLs | All 23 pinned ARM64 download URLs returned successfully |
| CPU regression | All 18 bundles passed, 30 steps each; largest output difference 0 |
| Direct CUDA adapter | All 18 bundles passed on RTX 5090; largest output difference 9.53674e-6 |
| Missing CUDA provider | Rejected explicitly using the CPU-only runtime |
| Invalid device setting | Rejected explicitly |

The direct CUDA check linked a temporary test build against the available x86
ONNX Runtime 1.24.4 CUDA library. The Jetson Dockerfiles build 1.23.2 from source.
This verifies adapter/provider selection and numerical behavior on x86, not the
legacy CUDA 11.8/12.6 source builds. Logs are under `validation/` locally. The final CPU and negative checks are
in `validation/deploy-legacy-cpu-final.log`, after refreshing the installed adapter.
