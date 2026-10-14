#!/usr/bin/env python3
"""Compile the production checkpoint block with a controlled Vulkan provider."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
out = root / 'build/cache-writer-test'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'runtime/nvn/vulkan/device.c').read_text()
start = source.index('static pthread_mutex_t cache_save_lock')
end = source.index('// ================================================================== swapchain', start)
(out / 'production-cache-writer.inc').write_text(source[start:end])
subprocess.run(['clang', '-g', '-fsanitize=address,undefined', '-pthread', '-I', str(out),
                str(root / 'ps5/tests/cache-writer-harness.c'), '-o', str(out / 'test')], check=True)
subprocess.run([str(out / 'test')], check=True, timeout=10)
