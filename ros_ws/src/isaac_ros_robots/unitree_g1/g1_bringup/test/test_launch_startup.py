# SPDX-License-Identifier: Apache-2.0
"""Verify startup fails visibly when the controller chain cannot activate."""

import ast
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock


class StartupTest(unittest.TestCase):
    def setUp(self):
        """Isolate controller activation with fake ROS clients and an executor."""
        path = Path(__file__).resolve().parents[1] / 'launch/g1.launch.py'
        tree = ast.parse(path.read_text())
        function = next(node for node in tree.body if isinstance(node, ast.FunctionDef)
                        and node.name == '_activate_controllers')
        self.request = SimpleNamespace(timeout=SimpleNamespace(sec=0))
        request_factory = Mock(return_value=self.request)
        request_factory.STRICT = 2
        self.node = Mock()
        self.client = self.node.create_client.return_value
        self.future = self.client.call_async.return_value
        self.future.done.return_value = True
        self.future.result.return_value = SimpleNamespace(ok=True)
        self.executor = Mock()
        self.rclpy = Mock()
        self.rclpy.create_node.return_value = self.node
        namespace = {
            'Context': Mock(),
            'SingleThreadedExecutor': Mock(return_value=self.executor),
            'rclpy': self.rclpy,
            'SwitchController': SimpleNamespace(Request=request_factory),
        }
        exec(compile(ast.Module(body=[function], type_ignores=[]), str(path), 'exec'),
             namespace)
        self.activate = namespace['_activate_controllers']

    def tearDown(self):
        """Require the temporary ROS context to be cleaned up on every path."""
        self.executor.shutdown.assert_called_once()
        self.node.destroy_node.assert_called_once()
        self.rclpy.shutdown.assert_called_once()

    def test_activates_the_whole_chain_strictly(self):
        """Activate all requested controllers together in strict mode."""
        controllers = ['joint_state_broadcaster', 'safety_controller', 'inference_controller']
        self.assertEqual(self.activate(None, controllers), [])
        self.assertEqual(self.request.activate_controllers, controllers)
        self.assertEqual(self.request.strictness, 2)
        self.assertFalse(self.request.activate_asap)
        self.assertEqual(self.request.timeout.sec, 10)

    def test_rejected_switch_fails(self):
        """Surface a controller-manager rejection to the launch caller."""
        self.future.result.return_value.ok = False
        with self.assertRaisesRegex(RuntimeError, 'switch failed'):
            self.activate(None, ['inference_controller'])

    def test_timed_out_switch_fails(self):
        """Surface an unfinished activation request instead of continuing."""
        self.future.done.return_value = False
        with self.assertRaisesRegex(RuntimeError, 'timed out'):
            self.activate(None, ['inference_controller'])

    def test_missing_service_does_not_send_a_request(self):
        """Fail before sending activation when the manager is unavailable."""
        self.client.wait_for_service.return_value = False
        with self.assertRaisesRegex(RuntimeError, 'not available'):
            self.activate(None, ['inference_controller'])
        self.client.call_async.assert_not_called()


if __name__ == '__main__':
    unittest.main()
