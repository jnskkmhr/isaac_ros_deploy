# SPDX-License-Identifier: Apache-2.0
"""Run a 29-joint G1 LEAPP policy in MuJoCo or on real hardware."""

from pathlib import Path
import tempfile

from ament_index_python.packages import get_package_share_directory
from controller_manager_msgs.srv import SwitchController
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction, RegisterEventHandler, Shutdown
from launch.event_handlers import OnProcessExit
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
import rclpy
from rclpy.context import Context
from rclpy.executors import SingleThreadedExecutor
import xacro
import yaml


def _activate_controllers(_context, controllers):
    """Switch the chain together without depending on a sourced ROS CLI."""
    ros_context = Context()
    rclpy.init(context=ros_context)
    node = rclpy.create_node('g1_startup_activation', context=ros_context)
    executor = SingleThreadedExecutor(context=ros_context)
    executor.add_node(node)
    try:
        client = node.create_client(SwitchController, '/controller_manager/switch_controller')
        if not client.wait_for_service(timeout_sec=60.0):
            raise RuntimeError('Controller switch service is not available.')
        request = SwitchController.Request()
        request.activate_controllers = controllers
        request.strictness = SwitchController.Request.STRICT
        request.activate_asap = False
        request.timeout.sec = 10
        future = client.call_async(request)
        executor.spin_until_future_complete(future, timeout_sec=60.0)
        if not future.done():
            raise RuntimeError('Controller switch timed out.')
        response = future.result()
        if response is None or not response.ok:
            raise RuntimeError('Controller switch failed; see controller manager errors.')
    finally:
        executor.remove_node(node)
        executor.shutdown()
        node.destroy_node()
        rclpy.shutdown(context=ros_context)
    return []


def generate_launch_description():
    """Declare the policy, hardware, and viewer launch options."""
    policies = Path(get_package_share_directory('g1_bringup')) / 'policies'
    policy_names = sorted(
        bundle.parent.name for bundle in policies.glob('*/*.yaml')
        if bundle.is_file() and bundle.stem == bundle.parent.name
    )
    return LaunchDescription([
        DeclareLaunchArgument(
            'policy', default_value='g1_29dof_mpm_locomotion_evh180n4',
            choices=policy_names,
        ),
        DeclareLaunchArgument(
            'inference_controller_config_path', default_value='',
            description='Optional external bundle YAML; overrides policy',
        ),
        DeclareLaunchArgument('hardware_type', default_value='mujoco', choices=['mujoco', 'real']),
        DeclareLaunchArgument(
            'network_interface', default_value='',
            description='Robot Ethernet interface; required for real hardware',
        ),
        DeclareLaunchArgument('enable_viewer', default_value='true'),
        OpaqueFunction(function=_launch),
    ])


def _launch(context):
    """Configure the selected robot and activate its policy controller chain."""
    hardware_type = LaunchConfiguration('hardware_type').perform(context)
    if hardware_type not in ('mujoco', 'real'):
        raise ValueError(f'Unsupported hardware_type: {hardware_type}')
    use_sim_time = hardware_type == 'mujoco'
    network_interface = LaunchConfiguration('network_interface').perform(context).strip()
    if hardware_type == 'real' and not network_interface:
        raise ValueError(
            'hardware_type:=real requires network_interface:=<robot Ethernet interface>'
        )
    description = Path(get_package_share_directory('g1_description'))
    bringup = Path(get_package_share_directory('g1_bringup'))
    bundle_override = LaunchConfiguration('inference_controller_config_path').perform(context)
    policy = LaunchConfiguration('policy').perform(context)
    bundle = (Path(bundle_override).resolve() if bundle_override
              else bringup / 'policies' / policy / f'{policy}.yaml')
    if not bundle.is_file():
        raise FileNotFoundError(f'LEAPP bundle YAML is not accessible: {bundle}')
    bundle_config = yaml.safe_load(bundle.read_text())
    for model in bundle_config['models'].values():
        for term in model['inputs']:
            if term.get('kind') == 'command/body/velocity' and term.get('shape') == [1, 7]:
                raise ValueError(
                    'This bundle combines velocity and heading. '
                    'Use a ROS-adapted bundle produced by prepare_bundle.py.'
                )
    group = yaml.safe_load((bringup / 'config/controller_groups.yaml').read_text())['velocity']
    robot_description = xacro.process_file(
        str(description / 'urdf/g1_ros2_control.urdf.xacro'),
        mappings={
            'hardware_type': hardware_type,
            'network_interface': network_interface,
            'mujoco_model_path': str(description / 'mjcf/scene.xml'),
            'enable_viewer': LaunchConfiguration('enable_viewer').perform(context),
        },
    ).toxml()
    runtime = {
        'inference_controller': {'ros__parameters': {
            'config_path': str(bundle),
            'command_prefix': group['command_prefix'],
            'command_suffix': group['command_suffix'],
            'source_to_topic': group['source_to_topic'],
            'topic_input_sources': list(group['source_to_topic']),
        }},
        'safety_controller': {'ros__parameters': {
            'gravity_compensation_urdf_path': str(description / 'urdf/g1.urdf'),
            # Start with policy targets gated; enable through the ROS parameter service.
            'blend_ratio': 0.0,
            'publish_blended_command': True,
        }},
    }
    with tempfile.NamedTemporaryFile(mode='w', suffix='.yaml', delete=False) as stream:
        yaml.safe_dump(runtime, stream)
        runtime_file = stream.name
    controller_config = str(bringup / 'config/controller_manager.yaml')
    manager_parameters = [
        {'robot_description': robot_description, 'use_sim_time': use_sim_time},
        controller_config, runtime_file,
    ]
    if use_sim_time:
        manager_parameters.extend([
            str(bringup / 'config/mujoco_pid.yaml'), {'mujoco.lockstep': True},
        ])
    manager = Node(
        package='mujoco_ros2_control' if use_sim_time else 'controller_manager',
        executable='ros2_control_node', output='screen', parameters=manager_parameters,
    )
    state_publisher = Node(
        package='robot_state_publisher', executable='robot_state_publisher', output='screen',
        parameters=[{'robot_description': robot_description, 'use_sim_time': use_sim_time}],
    )
    heading_command = Node(
        package='g1_bringup', executable='heading_command.py', output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
    )
    active = ['joint_state_broadcaster', 'imu_sensor_broadcaster', *group['controllers']]
    spawner = Node(
        package='controller_manager', executable='spawner',
        name='spawner_inactive', output='screen',
        arguments=[
            *active, 'freeze_controller', 'disable_controller', '--inactive',
            '--param-file', controller_config, '--param-file', runtime_file,
            '--controller-manager-timeout', '60', '--service-call-timeout', '60',
        ],
    )
    activate = OpaqueFunction(function=_activate_controllers, args=[active])

    def activate_on_success(event, _context):
        """Activate controllers only after successful inactive configuration."""
        if event.returncode != 0:
            raise RuntimeError('Controller configuration failed; see spawner errors above.')
        return [activate]

    return [
        RegisterEventHandler(OnProcessExit(
            target_action=manager, on_exit=[Shutdown(reason='G1 controller manager exited')],
        )),
        manager, state_publisher, heading_command,
        RegisterEventHandler(OnProcessExit(
            target_action=spawner, on_exit=activate_on_success,
        )),
        spawner,
    ]
