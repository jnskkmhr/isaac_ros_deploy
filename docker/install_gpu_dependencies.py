#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Install checksum-pinned runtime libraries; keep only the Triton ONNX backend."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

profile = json.loads(Path('/opt/gpu_dependencies.json').read_text())[sys.argv[1]]
architecture = subprocess.check_output(['dpkg', '--print-architecture'], text=True).strip()
if architecture != profile['architecture']:
    raise SystemExit(f"Profile requires {profile['architecture']}, found {architecture}. Build natively.")
cache = Path('/var/cache/deploy-gpu')
cache.mkdir(parents=True, exist_ok=True)

def download(artifact):
    path = cache / artifact['url'].rsplit('/', 1)[-1]
    def valid():
        return path.exists() and hashlib.file_digest(path.open('rb'), 'sha256').hexdigest() == artifact['sha256']
    if not valid():
        print('Downloading ' + path.name, flush=True)
        subprocess.run(['curl', '--fail', '--location', '--retry', '3', '--silent', '--show-error', artifact['url'], '-o', str(path)], check=True)
    if not valid():
        raise RuntimeError('Checksum mismatch: ' + str(path))
    return artifact, path

with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
    for artifact, archive in executor.map(download, profile['artifacts']):
        print('Installing ' + artifact['name'], flush=True)
        if archive.suffix == '.deb':
            subprocess.run(['dpkg', '-i', str(archive)], check=True)
            continue
        with tempfile.TemporaryDirectory() as temporary:
            with tarfile.open(archive) as tar:
                # Skip static libraries and unused Triton backends before extraction.
                members = []
                for member in tar.getmembers():
                    parts = Path(member.name).parts
                    if member.name.endswith('.a') and not artifact.get('keep_static', False):
                        continue
                    if artifact['name'] == 'triton' and 'backends' in parts:
                        index = parts.index('backends')
                        if len(parts) > index + 1 and parts[index + 1] != 'onnxruntime':
                            continue
                    members.append(member)
                tar.extractall(temporary, members=members, filter='data')
            roots = list(Path(temporary).iterdir())
            source = roots[0] if len(roots) == 1 and roots[0].is_dir() else Path(temporary)
            target = Path(artifact['destination'])
            target.mkdir(parents=True, exist_ok=True)
            for item in source.iterdir():
                if artifact['name'] == 'triton' and item.name not in ('include', 'lib', 'backends', 'LICENSE', 'LICENSE.txt', 'NVIDIA_Deep_Learning_Container_License.pdf'):
                    continue
                destination = target / item.name
                if item.is_dir():
                    shutil.copytree(item, destination, symlinks=True, dirs_exist_ok=True)
                else:
                    shutil.copy2(item, destination, follow_symlinks=False)
# The deployment runner uses ONNX Runtime's CUDA provider, not TensorRT engines.
# Its ONNX backend contains an optional TensorRT provider; remove that unused plugin.
for path in Path('/opt/tritonserver/backends/onnxruntime').glob('*providers_tensorrt*'):
    path.unlink()
subprocess.run(['ldconfig'], check=True)
