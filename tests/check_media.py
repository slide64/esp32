"""Validate generated demo media and preserve custom files on rerun."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import wave
from PIL import Image

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory)
    command = [sys.executable, str(root/'scripts/make_media.py'), '--output', directory]
    subprocess.run(command, check=True)
    for path in (output/'monsters').glob('*.gif'):
        with Image.open(path) as image:
            assert image.size == (360, 360) and image.n_frames > 1
            assert image.info['loop'] == 0
            duration = 0
            for i in range(image.n_frames):
                image.seek(i)
                image.load()
                duration += image.info['duration']
            assert duration == 1920
    for path in (output/'audio').glob('*.wav'):
        with wave.open(str(path)) as audio:
            assert (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) == (1, 2, 24000)
            assert audio.getnframes() > 0
            assert len(audio.readframes(audio.getnframes())) == audio.getnframes()*2
    custom = output/'audio/demon.wav'
    custom.write_bytes(b'user supplied recording')
    before = {p: hashlib.sha256(p.read_bytes()).digest() for p in output.rglob('*') if p.is_file()}
    assert len(before) == 7
    subprocess.run(command, check=True)
    assert all(hashlib.sha256(p.read_bytes()).digest() == checksum for p, checksum in before.items())
print('3 GIF, 4 WAV et preservation des fichiers existants : OK')
