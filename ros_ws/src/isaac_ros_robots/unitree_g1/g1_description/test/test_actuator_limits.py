# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
# All rights reserved.
# SPDX-License-Identifier: Apache-2.0
"""Keep MuJoCo hip-roll torque limits consistent with the robot description."""

from pathlib import Path
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]


class ActuatorLimitsTest(unittest.TestCase):
    """Check simulated motor limits against the joint and URDF limits."""

    def test_hip_roll_limits_match(self):
        """Both hip-roll motors must permit the full declared joint effort."""
        model = ET.parse(ROOT / 'mjcf/g1_29dof_rev_1_0_box_foot_improved_collision.xml')
        urdf = ET.parse(ROOT / 'urdf/g1.urdf')
        joint_default = model.find(".//default[@class='hip_roll']/joint")
        joint_limits = [float(value) for value in joint_default.get('actuatorfrcrange').split()]
        for side in ('left', 'right'):
            name = f'{side}_hip_roll_joint'
            with self.subTest(joint=name):
                motor = model.find(f"./actuator/motor[@joint='{name}']")
                control_limits = [float(value) for value in motor.get('ctrlrange').split()]
                effort = float(urdf.find(f"./joint[@name='{name}']/limit").get('effort'))
                self.assertEqual(control_limits, [-effort, effort])
                self.assertEqual(control_limits, joint_limits)


if __name__ == '__main__':
    unittest.main()
