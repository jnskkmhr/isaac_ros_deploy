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

All models were checked over 100 inference steps. LEAPP raw action outputs matched native ONNX outputs; maximum difference against PyTorch was below 0.000005. `running` means the latest available checkpoint is not yet a completed training run.
