#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Adapt the seven-component G1 command to standard Isaac ROS message converters.

Preserves the original graph, including policy weights, observation processing,
action scaling and gains. Writes a separate bundle; never overwrites the input.
"""
import argparse
import hashlib
from pathlib import Path

import numpy as np
import onnx
from onnx import helper, TensorProto
from safetensors.numpy import load_file, save_file
import yaml


def prepare(source, destination):
    """Write a bundle with separate velocity/heading inputs and FP32 feedback."""
    source = Path(source).resolve()
    destination = Path(destination).resolve()
    if source.parent == destination:
        raise ValueError('Output must be a separate directory')
    config = yaml.safe_load(source.read_text())
    name, spec = next(iter(config['models'].items()))
    graph = onnx.load(source.parent / spec['parameters']['model_path'])
    command = next(i for i in spec['inputs'] if i['name'] == 'base_velocity')
    if command['shape'] != [1, 7]:
        raise ValueError('Expected G1 velocity plus target-heading quaternion input [1, 7]')
    command['shape'] = [1, 3]
    command['element_names'] = [['lin_vel_x', 'lin_vel_y', 'ang_vel_z']]
    heading = {'name': 'heading_target', 'dtype': 'float32', 'shape': [1, 4],
               'type': 'tensor', 'kind': 'command/body/target_rotation',
               'element_names': [['qx', 'qy', 'qz', 'qw']]}
    spec['inputs'].append(heading)
    config['pipeline']['inputs'][name].append('heading_target')
    graph_command = next(i for i in graph.graph.input if i.name == 'base_velocity')
    graph_command.type.tensor_type.shape.dim[1].dim_value = 3
    graph.graph.input.append(
        helper.make_tensor_value_info('heading_target', TensorProto.FLOAT, [1, 4])
    )
    for node in graph.graph.node:
        for j, value in enumerate(node.input):
            if value == 'base_velocity':
                node.input[j] = 'velocity_and_heading'
    prepend = [helper.make_node(
        'Concat', ['base_velocity', 'heading_target'], ['velocity_and_heading'], axis=1
    )]
    # The deployed ROS controller allocates feedback tensors as FP32. Cast only
    # the external counter interface; history indexing stays INT64 in the graph.
    counter_names = set()
    append = []
    for entries, values in (
        (spec['inputs'], graph.graph.input), (spec['outputs'], graph.graph.output)
    ):
        for entry in entries:
            if entry['dtype'] != 'int64':
                continue
            key = entry['name']
            if 'num_pushes' not in key:
                raise ValueError(f'Unexpected integer tensor: {key}')
            counter_names.add(key)
            entry['dtype'] = 'float32'
            next(v for v in values if v.name == key).type.tensor_type.elem_type = TensorProto.FLOAT
            internal = key + '_int64'
            if entries is spec['inputs']:
                for node in graph.graph.node:
                    for j, value in enumerate(node.input):
                        if value == key:
                            node.input[j] = internal
                prepend.append(helper.make_node('Cast', [key], [internal], to=TensorProto.INT64))
            else:
                for node in graph.graph.node:
                    for j, value in enumerate(node.output):
                        if value == key:
                            node.output[j] = internal
                    for j, value in enumerate(node.input):
                        if value == key:
                            node.input[j] = internal
                append.append(helper.make_node('Cast', [internal], [key], to=TensorProto.FLOAT))
    nodes = list(graph.graph.node)
    del graph.graph.node[:]
    graph.graph.node.extend(prepend + nodes + append)
    onnx.checker.check_model(graph)
    destination.mkdir(parents=True, exist_ok=True)
    onnx.save(graph, destination / spec['parameters']['model_path'])
    model_bytes = (destination / spec['parameters']['model_path']).read_bytes()
    spec['parameters']['md5sum'] = hashlib.md5(model_bytes).hexdigest()
    spec['parameters']['sha256sum'] = hashlib.sha256(model_bytes).hexdigest()
    state_file = config['pipeline']['initial_values']
    states = load_file(str(source.parent / state_file))
    states = {
        k: v.astype(np.float32) if k.split('/')[-1] in counter_names else v
        for k, v in states.items()
    }
    save_file(states, str(destination / state_file))
    (destination / state_file).chmod(0o644)
    output = destination / source.name
    output.write_text(yaml.safe_dump(config, sort_keys=False))
    print(output)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source')
    parser.add_argument('destination')
    args = parser.parse_args()
    prepare(args.source, args.destination)
