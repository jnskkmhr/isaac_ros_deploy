#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Prepare four command lines inside the Isaac ROS container; Enter runs each one.
set -euo pipefail

policy=${1:-g1_29dof_mpm_locomotion_evh180n4}
hardware_type=${2:-mujoco}
network_interface=${3:-}
session=g1_deploy
workspace=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)

if [[ $# -gt 3 || ! $policy =~ ^[a-zA-Z0-9_]+$ || ! $hardware_type =~ ^(mujoco|real)$ ]]; then
  echo "Usage: $0 [policy_bundle_name] [mujoco|real] [network_interface]" >&2
  exit 1
fi
if [[ $hardware_type == real && -z $network_interface ]]; then
  echo 'Real hardware requires the robot Ethernet interface as the third argument.' >&2
  exit 1
fi
if [[ ! -f "$workspace/ros_ws/src/isaac_ros_robots/unitree_g1/g1_bringup/policies/$policy/$policy.yaml" ]]; then
  echo "Unknown policy bundle: $policy" >&2
  exit 1
fi
printf -v launch_command '%q ' ./scripts/ros.sh launch g1_bringup g1.launch.py "policy:=$policy" "hardware_type:=$hardware_type"
if [[ -n $network_interface ]]; then
  printf -v interface_argument '%q' "network_interface:=$network_interface"
  launch_command+=" $interface_argument"
fi
command -v tmux >/dev/null || {
  echo 'Install tmux in the development container.' >&2
  exit 1
}

if ! tmux has-session -t "=$session" 2>/dev/null; then
  new_session_args=(new-session -d -P -F '#{pane_id}' -s "$session" -n control -c "$workspace")
  # Override the server's cached domain when creating a new session.
  if [[ -n ${ROS_DOMAIN_ID:-} ]]; then
    new_session_args+=(-e "ROS_DOMAIN_ID=$ROS_DOMAIN_ID")
  fi
  control_pane=$(tmux "${new_session_args[@]}" 'bash --noprofile -i')
  activation_pane=$(tmux split-window -h -P -F '#{pane_id}' -t "$control_pane" -c "$workspace" 'bash --noprofile -i')
  teleop_pane=$(tmux split-window -v -P -F '#{pane_id}' -t "$activation_pane" -c "$workspace" 'bash --noprofile -i')
  stop_pane=$(tmux split-window -v -P -F '#{pane_id}' -t "$teleop_pane" -c "$workspace" 'bash --noprofile -i')

  # Literal text only: deliberately do not send Enter to any pane.
  tmux send-keys -t "$control_pane" -l "$launch_command"
  tmux send-keys -t "$activation_pane" -l "./scripts/ros.sh param set --no-daemon /safety_controller blend_ratio 1.0"
  tmux send-keys -t "$teleop_pane" -l "./scripts/ros.sh run teleop_twist_keyboard teleop_twist_keyboard --ros-args -p speed:=1.0 -p turn:=1.0"
  tmux send-keys -t "$stop_pane" -l "./scripts/ros.sh param set --no-daemon /safety_controller blend_ratio 0.0"
  tmux select-pane -t "$control_pane"
else
  echo 'Reusing the existing g1_deploy session; commands and policy are unchanged.'
fi

tmux set-option -t "$session" mouse on

if [[ -n ${TMUX:-} ]]; then
  exec tmux switch-client -t "$session"
elif [[ -t 0 && -t 1 ]]; then
  exec tmux attach-session -t "$session"
else
  echo "Session ready. Attach with: tmux attach -t $session"
fi
