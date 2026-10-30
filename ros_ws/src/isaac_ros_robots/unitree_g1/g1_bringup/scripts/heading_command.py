#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Supply velocity and integrated target heading to the G1 policy at 50 Hz."""
import math

from geometry_msgs.msg import Pose, Twist
from isaac_ros_deploy_interfaces.msg import BodyCommand
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSHistoryPolicy, QoSProfile, QoSReliabilityPolicy
from sensor_msgs.msg import Imu


class HeadingCommand(Node):
    """Convert the latest velocity and IMU samples into policy commands."""

    def __init__(self):
        """Configure latest-sample subscriptions and the command publishing timer."""
        super().__init__('g1_heading_command')
        self.velocity = Twist()
        self.yaw = None
        self.target_yaw = None
        self.previous_time = None
        self.last_command_time = None
        self.velocity_pub = self.create_publisher(Twist, '/policy_cmd_vel', 10)
        self.heading_pub = self.create_publisher(BodyCommand, '/policy_heading', 10)
        command_qos = QoSProfile(
            history=QoSHistoryPolicy.KEEP_LAST, depth=1,
            reliability=QoSReliabilityPolicy.RELIABLE,
        )
        imu_qos = QoSProfile(
            history=QoSHistoryPolicy.KEEP_LAST, depth=1,
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
        )
        self.create_subscription(Twist, '/cmd_vel', self.on_command, command_qos)
        self.create_subscription(Imu, '/imu_sensor_broadcaster/imu', self.on_imu, imu_qos)
        self.create_timer(0.02, self.publish)

    def on_imu(self, msg):
        """Update measured yaw and initialize the target from the first IMU sample."""
        q = msg.orientation
        self.yaw = math.atan2(2.0 * (q.w*q.z + q.x*q.y), 1.0 - 2.0 * (q.y*q.y + q.z*q.z))
        if self.target_yaw is None:
            self.target_yaw = self.yaw

    def on_command(self, msg):
        """Store the latest velocity command and reset heading when it changes."""
        # A changed command corresponds to resampling the training command.
        if (msg.linear.x, msg.linear.y, msg.angular.z) != (
            self.velocity.linear.x, self.velocity.linear.y, self.velocity.angular.z
        ):
            self.target_yaw = self.yaw
        self.velocity = msg
        self.last_command_time = self.get_clock().now().nanoseconds * 1e-9

    def publish(self):
        """Publish velocity and integrated heading, stopping stale commands."""
        if self.yaw is None or self.target_yaw is None:
            return
        now = self.get_clock().now().nanoseconds * 1e-9
        dt = 0.02 if self.previous_time is None else max(0.0, min(now-self.previous_time, 0.1))
        self.previous_time = now
        if self.last_command_time is not None and now-self.last_command_time > 0.5:
            self.velocity = Twist()
        error = math.atan2(math.sin(self.yaw-self.target_yaw), math.cos(self.yaw-self.target_yaw))
        if abs(error) > math.pi/2:
            self.target_yaw = self.yaw
        self.target_yaw += self.velocity.angular.z * dt
        self.target_yaw = math.atan2(math.sin(self.target_yaw), math.cos(self.target_yaw))
        pose = Pose()
        pose.orientation.z = math.sin(self.target_yaw/2)
        pose.orientation.w = math.cos(self.target_yaw/2)
        msg = BodyCommand()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.names = ['pelvis']
        msg.pose = [pose]
        self.heading_pub.publish(msg)
        self.velocity_pub.publish(self.velocity)


def main():
    """Run the command adapter until shutdown."""
    rclpy.init()
    node = HeadingCommand()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
