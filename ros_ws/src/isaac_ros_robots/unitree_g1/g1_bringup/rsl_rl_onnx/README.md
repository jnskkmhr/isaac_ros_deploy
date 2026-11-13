# RSL-RL actor ONNX exports

Generated with `OnPolicyRunner.export_policy_to_onnx`. Each ONNX file embeds its weights; the matching JSON records checkpoint provenance and validation.

These graphs accept preprocessed observations (`obs`) and return raw actions (`actions`). They do not construct observation history, transform IMU measurements, scale actions, add default joint positions, or supply PD gains. Use the LEAPP bundles under `../policies/` with the ROS launcher.

The six MPM/NeRD/ROM actors concatenate `policy` (930 values) then `command` (7 values), and output 29 actions. The rigid actor uses its `policy` observation group, including velocity and pelvis-height commands, and outputs 15 leg/waist actions; arm targets are added only in the LEAPP bundle.

| Actor graph | Checkpoint | W&B status at download | Observations | Actions |
| --- | --- | --- | --- | --- |
| [g1_29dof_mpm_locomotion_5f1xyc21.onnx](g1_29dof_mpm_locomotion_5f1xyc21.onnx) | `model_39998.pt` | finished | 937 | 29 |
| [g1_29dof_rom_locomotion_tyiby5k6.onnx](g1_29dof_rom_locomotion_tyiby5k6.onnx) | `model_19999.pt` | finished | 937 | 29 |
| [g1_mpm_locomotion_8y9fhovl.onnx](g1_mpm_locomotion_8y9fhovl.onnx) | `model_44499.pt` | finished | 937 | 29 |
| [g1_mpm_locomotion_jqur5jhm.onnx](g1_mpm_locomotion_jqur5jhm.onnx) | `model_19999.pt` | finished | 937 | 29 |
| [g1_mpm_locomotion_kevrlv51.onnx](g1_mpm_locomotion_kevrlv51.onnx) | `model_39998.pt` | finished | 937 | 29 |
| [g1_nerd_locomotion_x36a2qh8.onnx](g1_nerd_locomotion_x36a2qh8.onnx) | `model_19500.pt` | running | 937 | 29 |
| [g1_rigid_locomotion_2jiewizp.onnx](g1_rigid_locomotion_2jiewizp.onnx) | `model_19999.pt` | finished | 83 | 15 |
| [g1_rft_locomotion_hiwy2c7j.onnx](g1_rft_locomotion_hiwy2c7j.onnx) | `model_18500.pt` | local checkpoint; status not queried | 1000 | 29 |
| [g1_rft_locomotion_kyfj909z.onnx](g1_rft_locomotion_kyfj909z.onnx) | `model_19999.pt` | finished | 1000 | 29 |
| [g1_rft_student_0s01bqjj.onnx](g1_rft_student_0s01bqjj.onnx) | `model_18500.pt` | running | 2020 | 29 |
| [g1_rigid_locomotion_ioo32x36.onnx](g1_rigid_locomotion_ioo32x36.onnx) | `model_15500.pt` | running | 937 | 29 |
| [g1_rft_locomotion_lpwxe5i0.onnx](g1_rft_locomotion_lpwxe5i0.onnx) | `model_15000.pt` | running | 937 | 29 |

All models were checked over 100 inference steps. LEAPP raw action outputs matched native ONNX outputs; maximum difference against PyTorch was below 0.00001. `running` means the latest available checkpoint is not yet a completed training run.

The RFT actor (`hiwy2c7j`) uses one 1,000-value `policy` group. It concatenates ten samples per term in this order: angular velocity, projected gravity, seven-value velocity/heading command, relative joint positions, joint velocities, and previous actions. Each term runs oldest to newest; heading quaternions use wxyz within the actor observation.

`kyfj909z` uses the same 1,000-value observation layout as `hiwy2c7j`: ten samples per policy term, including the seven-value command, with wxyz heading quaternions.

`0s01bqjj` accepts `obs` with 2,020 values: current `policy` (100), followed by `student_history` flattened from `(30, 64)` in oldest-to-newest frame order. History channels are angular velocity, projected gravity, relative joint positions, and joint velocities. The graph includes the TCN encoder and actor; it requires no teacher or privileged inputs. The LEAPP bundle constructs this history from live measurements.

`ioo32x36` concatenates `policy` (930 values; ten samples per term) then `command` (seven current values). The heading quaternion within `command` uses wxyz order.

`lpwxe5i0` concatenates `policy` (930 values; ten samples per term) then `command` (seven current values). The heading quaternion within `command` uses wxyz order.
