#!/usr/bin/env python3
"""Install verified SCons from the pioarduino mirror when the PIO registry is blocked.

Uses PlatformIO's local package API. The platformio namespace is a local
compatibility alias required by Core's lookup, not a claim of registry origin.
No package source or integrity check is patched.
"""
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import urllib.request

from platformio.package.manager.core import get_core_package_dir
from platformio.package.manager.tool import ToolPackageManager
from platformio.package.meta import PackageSpec

URL = 'https://github.com/pioarduino/scons/releases/download/4.8.1/scons-local-4.8.1.tar.gz'
SHA256 = '289b1abf389b730ab5a1018ed9fc84e22d301e0d16c64548eb9db5bf8ba59d06'


def main():
    root = Path(os.environ['PLATFORMIO_CORE_DIR']).resolve()
    installed = get_core_package_dir('tool-scons', auto_install=False)
    if not installed:
        with urllib.request.urlopen(URL, timeout=60) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != SHA256:
            raise RuntimeError('SCons checksum mismatch')
        source = root/'local-scons-4.8.1'
        source.mkdir(parents=True, exist_ok=True)
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
            archive.extractall(source, filter='data')
        manifest = {'name': 'tool-scons', 'version': '4.40801.0', 'license': 'MIT',
                    'description': 'SCons 4.8.1, checksum-verified pioarduino mirror'}
        (source/'package.json').write_text(json.dumps(manifest))
        ToolPackageManager().install(PackageSpec(owner='platformio', name='tool-scons', uri=source.as_uri()))
        installed = get_core_package_dir('tool-scons', auto_install=False)
    if not installed:
        raise RuntimeError('SCons local installation not found by PlatformIO')
    subprocess.run([sys.executable, str(Path(installed)/'scons.py'), '--version'], check=True)


if __name__ == '__main__':
    main()
