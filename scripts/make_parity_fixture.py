#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate stateful reference outputs independently of the C++ inference adapter."""
import json
from pathlib import Path
import struct
import sys

import numpy as np
import onnxruntime as ort
import yaml


def save_tensors(path, values):
    header, chunks, offset = {}, [], 0
    for name, value in values.items():
        value = np.ascontiguousarray(value, dtype=np.float32)
        data = value.tobytes()
        header[name] = dict(dtype="F32", shape=list(value.shape), data_offsets=[offset, offset + len(data)])
        chunks.append(data)
        offset += len(data)
    encoded = json.dumps(header).encode()
    encoded += b" " * (-len(encoded) % 8)
    path.write_bytes(struct.pack("<Q", len(encoded)) + encoded + b"".join(chunks))


def main():
    bundle = Path(sys.argv[1])
    output = Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)
    cfg = yaml.safe_load(bundle.read_text())
    model_name, model = next(iter(cfg["models"].items()))
    model_path = bundle.parent / model["parameters"]["model_path"]
    options = ort.SessionOptions()
    options.intra_op_num_threads = options.inter_op_num_threads = 1
    session = ort.InferenceSession(str(model_path), options, providers=["CPUExecutionProvider"])
    values = {item["name"]: np.zeros(item["shape"], np.float32) for item in model["inputs"]}
    rng = np.random.default_rng(1234)
    all_inputs, all_outputs = {}, {}
    for step in range(30):
        for item in model["inputs"]:
            if "kind" not in item:
                continue
            value = rng.normal(0.0, 0.05, item["shape"]).astype(np.float32)
            if item["kind"] in ("state/body/rotation", "command/body/target_rotation"):
                value[..., 3] += 1.0
                value /= np.linalg.norm(value, axis=-1, keepdims=True)
            values[item["name"]] = value
        all_inputs.update({f"{step}/{k}": v.copy() for k, v in values.items()})
        outputs = dict(zip([item.name for item in session.get_outputs()], session.run(None, values)))
        all_outputs.update({f"{step}/{k}": v.copy() for k, v in outputs.items()})
        for source, destinations in cfg["pipeline"]["feedback_flow"].items():
            for destination in destinations:
                values[destination.split("/", 1)[1]] = outputs[source.split("/", 1)[1]]
    save_tensors(output / "inputs.safetensors", all_inputs)
    save_tensors(output / "outputs.safetensors", all_outputs)
    print(model_path)


if __name__ == "__main__":
    main()
