# SPDX-License-Identifier: Apache-2.0
"""Check that the imported policies retain their artifacts and ROS input contract."""

import hashlib
import json
from pathlib import Path
import struct
import unittest

import yaml


ROOT = Path(__file__).resolve().parents[1]


class PolicyBundlesTest(unittest.TestCase):
    """Validate the artifacts and complete input wiring of every shipped bundle."""

    def test_bundles(self):
        """Check model integrity, joint names, pipeline sources, and feedback types."""
        controller = yaml.safe_load((ROOT / 'config/controller_manager.yaml').read_text())
        joints = controller['safety_controller']['ros__parameters']['joints']
        group = yaml.safe_load((ROOT / 'config/controller_groups.yaml').read_text())['velocity']
        bundles = list((ROOT / 'policies').glob('*/*.yaml'))
        self.assertTrue(bundles, 'No policy bundles found')
        for bundle in bundles:
            with self.subTest(bundle=bundle.stem):
                config = yaml.safe_load(bundle.read_text())
                self.assertEqual(config['pipeline']['configs']['frequency'], 50.0)
                manager_rate = controller['controller_manager']['ros__parameters']['update_rate']
                decimation = controller['inference_controller']['ros__parameters']['decimation']
                self.assertEqual(manager_rate / decimation, 50.0)
                provenance = json.loads((bundle.parent / 'provenance.json').read_text())
                self.assertTrue(provenance['checkpoint'].endswith('.pt'))
                self.assertIn(provenance['wandb_run'], bundle.stem)
                self.assertEqual(provenance['policy_frequency_hz'], 50)
                for name, model in config['models'].items():
                    parameters = model['parameters']
                    data = (bundle.parent / parameters['model_path']).read_bytes()
                    self.assertEqual(hashlib.sha256(data).hexdigest(), parameters['sha256sum'])
                    # Every model input must be supplied externally or by feedback.
                    external = config['pipeline']['inputs'][name]
                    feedback = {
                        destination.split('/', 1)[1]
                        for destinations in config['pipeline']['feedback_flow'].values()
                        for destination in destinations
                        if destination.startswith(name + '/')
                    }
                    self.assertIn('heading_target', external)
                    self.assertEqual(len(external), len(set(external)))
                    self.assertEqual(
                        set(external) | feedback,
                        {item['name'] for item in model['inputs']},
                    )
                    inputs = {item.get('kind'): item for item in model['inputs']}
                    self.assertEqual(inputs['command/body/velocity']['shape'], [1, 3])
                    self.assertEqual(inputs['command/body/target_rotation']['shape'], [1, 4])
                    for kind in ('command/body/velocity', 'command/body/target_rotation'):
                        self.assertIn(kind, group['source_to_topic'])
                    self.assertEqual(
                        set(inputs['state/joint/position']['element_names'][0]), set(joints),
                    )
                    outputs = {item.get('kind'): item for item in model['outputs']}
                    self.assertEqual(
                        set(outputs['target/joint/position']['element_names'][0]), set(joints),
                    )
                    # Runtime feedback tensors use FP32, including the adapted counters.
                    for item in model['inputs'] + model['outputs']:
                        self.assertEqual(item['dtype'], 'float32')
                state = (bundle.parent / config['pipeline']['initial_values']).read_bytes()
                header_length = struct.unpack('<Q', state[:8])[0]
                header = json.loads(state[8:8 + header_length])
                for destinations in config['pipeline']['feedback_flow'].values():
                    for destination in destinations:
                        self.assertIn(destination, header)
                        self.assertEqual(header[destination]['dtype'], 'F32')


if __name__ == '__main__':
    unittest.main()
