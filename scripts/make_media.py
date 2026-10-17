#!/usr/bin/env python3
"""Crée des médias de démonstration originaux ; aucune voix enregistrée incluse."""
import argparse
import math
import random
import struct
import wave
from pathlib import Path
from PIL import Image, ImageDraw


def make_gif(name, destination):
    frames = []
    for frame in range(24):
        im = Image.new('RGB', (360, 360), '#020204')
        d = ImageDraw.Draw(im)
        breath = round(5 * math.sin(frame * math.tau / 24))
        if name == 'pumpkin':
            d.ellipse((38, 48 + breath, 322, 320), fill='#a83900', outline='#fa6700', width=7)
            for x in (100, 145, 215, 260):
                d.arc((x-35, 55+breath, x+35, 310), 80, 280, fill='#722000', width=4)
            d.polygon([(165, 57), (174, 25), (199, 23), (190, 61)], fill='#36520b')
            eye = '#ffdc26'
        elif name == 'demon':
            d.polygon([(55, 110), (42, 25), (127, 79), (230, 79), (318, 25), (302, 118)], fill='#9b091d')
            d.ellipse((50, 67+breath, 310, 316), fill='#670619', outline='#b41a29', width=5)
            eye = '#ff3434'
        else:
            d.rounded_rectangle((52, 55+breath, 308, 314), 70, fill='#29432b', outline='#567e38', width=5)
            d.line([(83, 88), (124, 98), (168, 80), (224, 99)], fill='#9aaa68', width=5)
            for x in (110, 140, 178, 210):
                d.line([(x, 83), (x+5, 101)], fill='#101c10', width=3)
            eye = '#a4ff31'
        blink = 2 if frame == 17 else 30+breath
        d.polygon([(70, 127), (154, 148), (143, 148+blink), (83, 148+blink)], fill=eye)
        d.polygon([(290, 127), (206, 148), (217, 148+blink), (277, 148+blink)], fill=eye)
        d.polygon([(85, 230), (118, 242), (141, 226), (170, 248), (198, 228), (221, 242), (276, 226), (246, 277+breath), (121, 277+breath)], fill=eye)
        for x in (125, 174, 222):
            d.polygon([(x, 227), (x+12, 253), (x+21, 230)], fill='#080808')
        frames.append(im.quantize(colors=32))
    frames[0].save(destination, save_all=True, append_images=frames[1:], duration=80, loop=0, disposal=2, optimize=False)


def make_sound(path, seconds, fundamental, seed):
    rng = random.Random(seed)
    rate = 24000
    with wave.open(str(path), 'wb') as out:
        out.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        data = bytearray()
        for i in range(int(seconds*rate)):
            t = i/rate
            fade = min(1, t/.2, (seconds-t)/.4)
            value = .18*math.sin(math.tau*fundamental*t + 1.8*math.sin(t*7))
            value += .08*math.sin(math.tau*(fundamental*1.498)*t)
            value += .025*rng.uniform(-1, 1)
            data.extend(struct.pack('<h', round(32767*fade*value)))
        out.writeframes(data)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, default=Path('media'))
    args = p.parse_args()
    monsters = args.output/'monsters'
    audio = args.output/'audio'
    monsters.mkdir(parents=True, exist_ok=True)
    audio.mkdir(parents=True, exist_ok=True)
    for i, name in enumerate(('pumpkin', 'demon', 'zombie')):
        if not (monsters/f'{name}.gif').exists():
            make_gif(name, monsters/f'{name}.gif')
        if not (audio/f'{name}.wav').exists():
            make_sound(audio/f'{name}.wav', 2.5, 65+i*21, i)
    if not (audio/'ambiance.wav').exists():
        make_sound(audio/'ambiance.wav', 8, 55, 4)
    print(f'Médias créés dans {args.output}. Les sons de scène sont des effets, à remplacer par vos voix WAV.')


if __name__ == '__main__':
    main()
