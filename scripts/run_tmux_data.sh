#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Prepare three telemetry command lines inside the container; Enter runs each one.
set -euo pipefail

workspace=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
session=g1_data
platform_args=()
if [[ ${1:-} == --jetson ]]; then
    platform_args+=(--jetson)
    session=g1_data_jetson
    shift
fi
if [[ $# -gt 1 || ${1:-} == --help ]]; then
    echo "Usage: $0 [--jetson] [OUTPUT_DIRECTORY]" >&2
    exit 1
fi
output=${1:-recordings/$(date +%Y%m%d_%H%M%S)}
command -v tmux >/dev/null || { echo 'Install tmux in the development container.' >&2; exit 1; }

printf -v topics_command '%q ' ./scripts/ros.sh topic list
printf -v record_command '%q ' ./scripts/record_data.sh "${platform_args[@]}" "$output"
printf -v foxglove_command '%q ' ./scripts/run_foxglove.sh "${platform_args[@]}"

if ! tmux has-session -t "=$session" 2>/dev/null; then
    new_session_args=(new-session -d -P -F '#{pane_id}' -s "$session" -n data -c "$workspace")
    # Override the tmux server's cached ROS environment for all three panes.
    new_session_args+=(-e "ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}")
    new_session_args+=(-e "RMW_IMPLEMENTATION=${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}")
    topics_pane=$(tmux "${new_session_args[@]}" 'bash --noprofile -i')
    record_pane=$(tmux split-window -h -P -F '#{pane_id}' -t "$topics_pane" -c "$workspace" 'bash --noprofile -i')
    foxglove_pane=$(tmux split-window -v -P -F '#{pane_id}' -t "$record_pane" -c "$workspace" 'bash --noprofile -i')

    # Literal text only: deliberately do not send Enter to any pane.
    tmux send-keys -t "$topics_pane" -l "$topics_command"
    tmux send-keys -t "$record_pane" -l "$record_command"
    tmux send-keys -t "$foxglove_pane" -l "$foxglove_command"
    tmux select-pane -t "$topics_pane"
else
    echo "Reusing $session; commands, output directory, and pane environments are unchanged."
fi

tmux set-option -t "$session" mouse on
if [[ -n ${TMUX:-} ]]; then
    exec tmux switch-client -t "$session"
elif [[ -t 0 && -t 1 ]]; then
    exec tmux attach-session -t "$session"
else
    echo "Session ready. Attach with: tmux attach -t $session"
fi
