#!/usr/bin/env python3
"""Build native PS5 recompilation archives with optional dependency/game setup."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', action='store_true')
    parser.add_argument('--exefs', type=Path, help='Generate private game code first; existing generation is preserved')
    parser.add_argument('--jobs', type=int, default=6)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.setup:
        subprocess.run([sys.executable, str(ROOT / 'ps5/tools/setup-deps.py')], check=True)
    if args.exefs:
        subprocess.run([sys.executable, str(ROOT / 'ps5/tools/generate-game.py'),
                        '--exefs', str(args.exefs.resolve())], check=True)
    env = os.environ.copy()
    env['LA_BUILD_JOBS'] = str(args.jobs)
    subprocess.run(['bash', str(ROOT / 'ps5/tools/build.sh'), '-DLA_COMPILE_VULKAN=ON'], env=env, check=True)


if __name__ == '__main__':
    main()
