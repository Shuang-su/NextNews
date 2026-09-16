#!/usr/bin/env python3
"""Compare ZIP and unbundled SOG through the native decoder, including unsafe inputs."""
import argparse, json, pathlib, subprocess, tempfile, zipfile
p = argparse.ArgumentParser(); p.add_argument('sog', type=pathlib.Path); a = p.parse_args()
root = pathlib.Path(__file__).resolve().parents[2]
exe = root / '.local/sog-test-build/sog-test'
with tempfile.TemporaryDirectory(prefix='nextnews-unbundled-') as tmp:
    directory = pathlib.Path(tmp) / 'chunk'; directory.mkdir()
    with zipfile.ZipFile(a.sog) as z:
        for name in z.namelist():
            if '/' in name or '\\' in name or name in ('.', '..'): raise ValueError('Unsafe test fixture')
            (directory / name).write_bytes(z.read(name))
    metadata = directory / 'meta.json'; original = metadata.read_bytes()
    subprocess.run([str(exe), str(metadata), '--sog', str(a.sog.resolve())], check=True)
    def reject(label):
        r = subprocess.run([str(exe), str(metadata)], capture_output=True, text=True)
        if r.returncode != 1: raise AssertionError((label, r.returncode, r.stdout, r.stderr))
        print('PASS reject', label, r.stderr.strip())
    m = json.loads(original); name = m['means']['files'][0]; texture = directory / name
    content = texture.read_bytes(); texture.unlink(); reject('missing texture')
    outside = pathlib.Path(tmp) / name; outside.write_bytes(content); texture.symlink_to(outside); reject('symlink texture'); texture.unlink(); texture.write_bytes(content)
    m['means']['files'][0] = '../' + name; metadata.write_text(json.dumps(m)); reject('traversal')
    m = json.loads(original); m['quats']['files'][0] = m['scales']['files'][0]; metadata.write_text(json.dumps(m)); reject('duplicate texture')
    metadata.write_bytes(original); texture.write_bytes(content[:32]); reject('truncated texture')
print('PASS unbundled native decoder correctness and validation')
