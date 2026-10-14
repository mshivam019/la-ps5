#!/usr/bin/env python3
"""Fetch shaderc's pinned dependencies and build/validate it for PS5."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / ".deps/src/shaderc"
BUILD = ROOT / "build/ps5-shaderc"
TOOLCHAIN = ROOT / "ps5/cmake/native.cmake"
PATCH = ROOT / "ps5/patches/shaderc-2026.3-static-install.patch"
ARCHIVE = BUILD / "libshaderc/libshaderc_combined.a"
NATIVE_TEMPLATE = Path.home() / "ps5sdk/native-app-boilerplate"
SDK = NATIVE_TEMPLATE / ".deps/native/ps5-payload-sdk"

DEPENDENCIES = {
    "glslang": (
        "https://github.com/KhronosGroup/glslang.git",
        "168d452a4f460d24b588fed08477a81c44ee27a1",
    ),
    "spirv-headers": (
        "https://github.com/KhronosGroup/SPIRV-Headers.git",
        "29981f65241605e08b0ede4cfeb999fe3b723c6a",
    ),
    "spirv-tools": (
        "https://github.com/KhronosGroup/SPIRV-Tools.git",
        "b707790a898e44038547df54580022fc1cf89c3d",
    ),
}

REQUIRED_SYMBOLS = {
    "shaderc_compiler_initialize",
    "shaderc_compiler_release",
    "shaderc_compile_options_initialize",
    "shaderc_compile_options_release",
    "shaderc_compile_options_set_target_env",
    "shaderc_compile_options_set_optimization_level",
    "shaderc_compile_into_spv",
    "shaderc_result_get_compilation_status",
    "shaderc_result_get_error_message",
    "shaderc_result_get_length",
    "shaderc_result_get_bytes",
    "shaderc_result_release",
}


def run(command: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(command), flush=True)
    kwargs.setdefault("check", True)
    return subprocess.run(command, text=True, **kwargs)


def check_deps_file() -> None:
    contents = (SOURCE / "DEPS").read_text(encoding="utf-8")
    for name, (_, revision) in DEPENDENCIES.items():
        if revision not in contents:
            raise RuntimeError(
                f"{name} pin no longer matches shaderc/DEPS; update this script deliberately"
            )


def fetch_dependency(name: str, url: str, revision: str) -> None:
    destination = SOURCE / "third_party" / name
    if destination.exists() and not (destination / ".git").is_dir():
        raise RuntimeError(f"refusing to replace non-git dependency directory: {destination}")
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "init", str(destination)])
        run(["git", "-C", str(destination), "remote", "add", "origin", url])

    remote = run(
        ["git", "-C", str(destination), "remote", "get-url", "origin"],
        capture_output=True,
    ).stdout.strip()
    if remote != url:
        raise RuntimeError(f"unexpected origin for {name}: {remote}")

    current = run(
        ["git", "-C", str(destination), "rev-parse", "--verify", "HEAD"],
        capture_output=True,
        check=False,
    )
    if current.returncode != 0 or current.stdout.strip() != revision:
        run(["git", "-C", str(destination), "fetch", "--depth=1", "origin", revision])
        run(["git", "-C", str(destination), "checkout", "--detach", revision])

    actual = run(
        ["git", "-C", str(destination), "rev-parse", "HEAD"], capture_output=True
    ).stdout.strip()
    if actual != revision:
        raise RuntimeError(f"{name} is at {actual}, expected {revision}")
    if run(["git", "-C", str(destination), "status", "--porcelain"], capture_output=True).stdout:
        raise RuntimeError(f"pinned dependency has local modifications: {destination}")
    print(f"Verified {name} at {revision}")


def configure() -> None:
    if not TOOLCHAIN.is_file():
        raise RuntimeError(f"missing PS5 toolchain: {TOOLCHAIN}")
    if not SDK.is_dir():
        raise RuntimeError(f"missing PS5 payload SDK: {SDK}")
    # The toolchain sets this while CMake runs; Ninja starts compiler wrappers
    # in a later process, so export the identical value for the build as well.
    os.environ["PS5_PAYLOAD_SDK"] = str(SDK)

    run(
        [
            "cmake",
            "-S",
            str(SOURCE),
            "-B",
            str(BUILD),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DSHADERC_SKIP_TESTS=ON",
            "-DSHADERC_SKIP_EXAMPLES=ON",
            "-DSHADERC_SKIP_EXECUTABLES=ON",
            "-DSHADERC_SKIP_COPYRIGHT_CHECK=ON",
            "-DSHADERC_SKIP_INSTALL=ON",
            "-DSHADERC_ENABLE_WERROR_COMPILE=OFF",
            "-DSHADERC_ENABLE_HLSL=OFF",
            "-DBUILD_SHARED_LIBS=OFF",
        ]
    )


def apply_patch() -> None:
    forward = run(
        ["git", "apply", "--check", str(PATCH)], cwd=ROOT, capture_output=True, check=False
    )
    if forward.returncode == 0:
        run(["git", "apply", str(PATCH)], cwd=ROOT)
        print(f"Applied {PATCH.name}")
        return
    reverse = run(
        ["git", "apply", "--check", "--reverse", str(PATCH)],
        cwd=ROOT,
        capture_output=True,
        check=False,
    )
    if reverse.returncode != 0:
        raise RuntimeError(f"cannot apply or verify shaderc patch: {PATCH}")
    print(f"Verified already-applied {PATCH.name}")


def build() -> None:
    run(["cmake", "--build", str(BUILD), "--target", "shaderc_combined", "--parallel", "4"])


def validate() -> None:
    if not ARCHIVE.is_file() or ARCHIVE.stat().st_size == 0:
        raise RuntimeError(f"shaderc archive was not produced: {ARCHIVE}")

    ar = shutil.which("llvm-ar") or shutil.which("ar")
    readelf = shutil.which("llvm-readelf") or shutil.which("readelf")
    nm = shutil.which("llvm-nm") or shutil.which("nm")
    if not ar or not readelf or not nm:
        raise RuntimeError("validation requires ar, readelf, and nm")

    os.environ["PS5_PAYLOAD_SDK"] = str(SDK)
    c_compiler = NATIVE_TEMPLATE / "tooling/prospero-clang18"
    cxx_compiler = NATIVE_TEMPLATE / "tooling/prospero-clang18++"
    lld = SDK / "bin/prospero-lld"
    for tool in (c_compiler, cxx_compiler, lld):
        if not tool.is_file():
            raise RuntimeError(f"missing PS5 native tool: {tool}")
    target = run([str(cxx_compiler), "-print-target-triple"], capture_output=True).stdout.strip()
    if target != "x86_64-sie-ps5":
        raise RuntimeError(f"compiler wrapper targets {target}, expected x86_64-sie-ps5")

    members = run([ar, "t", str(ARCHIVE)], capture_output=True).stdout.splitlines()
    if not members:
        raise RuntimeError("shaderc archive has no object members")

    headers = run([readelf, "-h", str(ARCHIVE)], capture_output=True).stdout
    object_count = len(re.findall(r"^File: .*\(.+\)$", headers, re.MULTILINE))
    machines = re.findall(r"^\s*Machine:\s*(.+)$", headers, re.MULTILINE)
    types = re.findall(r"^\s*Type:\s*(\S+)", headers, re.MULTILINE)
    if object_count != len(members):
        raise RuntimeError(
            f"readelf inspected {object_count} objects, but archive contains {len(members)}"
        )
    if not machines or any(machine != "Advanced Micro Devices X86-64" for machine in machines):
        raise RuntimeError(f"archive contains a non-x86_64 object: {sorted(set(machines))}")
    if any(object_type != "REL" for object_type in types):
        raise RuntimeError(f"archive contains a non-relocatable ELF object: {sorted(set(types))}")

    symbols = run([nm, "--defined-only", "--extern-only", str(ARCHIVE)], capture_output=True).stdout
    defined = {line.rsplit(maxsplit=1)[-1] for line in symbols.splitlines() if line.strip()}
    missing = sorted(REQUIRED_SYMBOLS - defined)
    if missing:
        raise RuntimeError("archive is missing runtime shaderc APIs: " + ", ".join(missing))

    shared_outputs = [
        path for path in BUILD.rglob("*") if path.is_file() and path.suffix in {".so", ".dylib", ".dll"}
    ]
    if shared_outputs:
        raise RuntimeError("unexpected shared build outputs: " + ", ".join(map(str, shared_outputs)))

    smoke_object = BUILD / "shaderc-link-smoke.o"
    crt_object = BUILD / "shaderc-app-crt.o"
    smoke_elf = BUILD / "shaderc-link-smoke.elf"
    smoke_source = """\
#include <shaderc/shaderc.h>
int main(int, char **, char **) {
  shaderc_compiler_t compiler = shaderc_compiler_initialize();
  if (!compiler) return 1;
  shaderc_compiler_release(compiler);
  return 0;
}
"""
    run(
        [
            str(cxx_compiler),
            "-std=c++17",
            f"-I{SOURCE / 'libshaderc/include'}",
            "-x",
            "c++",
            "-c",
            "-",
            "-o",
            str(smoke_object),
        ],
        input=smoke_source,
    )
    run(
        [
            str(c_compiler),
            "-std=c11",
            "-c",
            str(NATIVE_TEMPLATE / "tooling/native/app_crt.c"),
            "-o",
            str(crt_object),
        ]
    )
    sdk_lib = SDK / "target/lib"
    runtime_flags = ["-lc++", "-lc++abi", "-lunwind", "-lc", "-lpthread"]
    unwind_symbols = [
        "--defsym=__eh_frame_start=ADDR(.eh_frame)",
        "--defsym=__eh_frame_end=ADDR(.eh_frame)+SIZEOF(.eh_frame)",
        "--defsym=__eh_frame_hdr_start=ADDR(.eh_frame_hdr)",
        "--defsym=__eh_frame_hdr_end=ADDR(.eh_frame_hdr)+SIZEOF(.eh_frame_hdr)",
    ]
    run(
        [
            str(lld),
            "-T",
            str(NATIVE_TEMPLATE / "tooling/native/ps5-pie.ld"),
            "--eh-frame-hdr",
            "--version-script",
            str(NATIVE_TEMPLATE / "tooling/native/app-symbols.map"),
            "-e",
            "_start",
            "-o",
            str(smoke_elf),
            *unwind_symbols,
            str(crt_object),
            str(smoke_object),
            f"-L{sdk_lib}",
            "--start-group",
            str(ARCHIVE),
            *runtime_flags,
            "--end-group",
            "--as-needed",
            *map(str, sorted(sdk_lib.glob("*.so"))),
        ]
    )
    linked = run([readelf, "-h", "-d", str(smoke_elf)], capture_output=True).stdout
    if "Machine:                           Advanced Micro Devices X86-64" not in linked:
        raise RuntimeError("shaderc link smoke test is not x86_64 ELF")
    for needed in ("libSceLibcInternal.sprx", "libkernel.sprx"):
        if f"Shared library: [{needed}]" not in linked:
            raise RuntimeError(f"shaderc link smoke test does not import {needed}")

    print(f"Validated {ARCHIVE}")
    print(f"  size: {ARCHIVE.stat().st_size} bytes")
    print(f"  target: {target}")
    print(f"  members: {len(members)} x86_64 ELF relocatable objects")
    print(f"  runtime C APIs: {len(REQUIRED_SYMBOLS)} present")
    print(f"  link smoke test: {smoke_elf}")
    print("Downstream static group: libshaderc_combined.a -lc++ -lc++abi -lunwind -lc -lpthread")
    print("Downstream unwind symbols: " + " ".join(unwind_symbols))
    print("Downstream PS5 imports: libSceLibcInternal.sprx libkernel.sprx")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--validate-only", action="store_true", help="validate the existing archive"
    )
    args = parser.parse_args()

    if not args.validate_only:
        check_deps_file()
        for name, (url, revision) in DEPENDENCIES.items():
            fetch_dependency(name, url, revision)
        apply_patch()
        configure()
        build()
    validate()


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1) from error
