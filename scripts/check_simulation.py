#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check the standalone controller chain in MuJoCo only, never on hardware."""
import json
import math
import os
from pathlib import Path
import signal
import subprocess
import time

import rclpy
from controller_manager_msgs.srv import ListControllers
from isaac_ros_deploy_interfaces.msg import JointCommand
from rcl_interfaces.msg import Parameter, ParameterValue
from rcl_interfaces.srv import GetParameters, SetParameters
from sensor_msgs.msg import JointState


def main():
    if os.environ.get('ROS_DOMAIN_ID') != '161':
        raise RuntimeError('Run this simulation-only check with ROS_DOMAIN_ID=161.')
    output = Path('/tmp/isaac_deploy_validation')
    output.mkdir(exist_ok=True)
    log = (output / 'simulation.log').open('w')
    process = subprocess.Popen([
        'ros2', 'launch', 'g1_bringup', 'g1.launch.py',
        'policy:=g1_mpm_locomotion_8y9fhovl', 'hardware_type:=mujoco',
        'enable_viewer:=false',
    ], stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    rclpy.init()
    node = rclpy.create_node('standalone_simulation_check')
    states, commands = [], []
    node.create_subscription(JointState, '/joint_states', states.append, 100)
    node.create_subscription(JointCommand, '/safety_controller/blended_command', commands.append, 100)

    def call(kind, name, request):
        client = node.create_client(kind, name)
        if not client.wait_for_service(timeout_sec=20):
            raise RuntimeError(f'Service unavailable: {name}')
        future = client.call_async(request)
        rclpy.spin_until_future_complete(node, future, timeout_sec=10)
        if not future.done():
            raise RuntimeError(f'Service timed out: {name}')
        response = future.result()
        node.destroy_client(client)
        return response

    try:
        deadline = time.monotonic() + 35
        while True:
            response = call(ListControllers, '/controller_manager/list_controllers', ListControllers.Request())
            active = {c.name for c in response.controller if c.state == 'active'}
            if {'inference_controller', 'safety_controller', 'joint_state_broadcaster'} <= active:
                break
            if time.monotonic() > deadline:
                raise RuntimeError(f'Controllers did not activate: {active}')
            rclpy.spin_once(node, timeout_sec=0.2)
        initial = call(GetParameters, '/safety_controller/get_parameters', GetParameters.Request(names=['blend_ratio']))
        assert initial.values[0].double_value == 0.0, 'Policy blending must start disabled'
        for ratio in (1.0, 0.0):
            response = call(SetParameters, '/safety_controller/set_parameters', SetParameters.Request(parameters=[
                Parameter(name='blend_ratio', value=ParameterValue(type=3, double_value=ratio)),
            ]))
            assert response.results[0].successful
            deadline = time.monotonic() + (10 if ratio else 1)
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError('MuJoCo launch exited early')
                rclpy.spin_once(node, timeout_sec=0.1)
        assert len(states) > 100 and len(commands) > 100
        assert all(len(msg.name) == 29 for msg in states)
        assert all(math.isfinite(v) for msg in states for v in (*msg.position, *msg.velocity, *msg.effort))
        assert all(len(msg.names) == 29 for msg in commands)
        assert all(math.isfinite(v) for msg in commands for v in (*msg.position, *msg.velocity, *msg.effort, *msg.kp, *msg.kd))
        result = dict(active_controllers=sorted(active), initial_blend_ratio=0.0,
                      activation_and_deactivation=True, joint_state_samples=len(states),
                      joint_command_samples=len(commands), all_finite=True)
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))
    finally:
        node.destroy_node()
        rclpy.shutdown()
        process.send_signal(signal.SIGINT)
        try:
            process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            process.wait(timeout=5)
        log.close()


if __name__ == '__main__':
    main()
