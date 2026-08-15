#!/usr/bin/env python3
"""Install the pinned Windows development toolchain into .tools/."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile


LLVM_VERSION = "22.1.8"
NINJA_VERSION = "1.13.2"
ORTOOLS_VERSION = "9.15.6755"
TOOLS_ROOT = Path(".tools").resolve()


def release_asset(repository: str, tag: str, name: str) -> tuple[str, str]:
    request = urllib.request.Request(
        f"https://api.github.com/repos/{repository}/releases/tags/{tag}",
        headers={"Accept": "application/vnd.github+json", "User-Agent": "SchedMesh-dev-setup"},
    )
    with urllib.request.urlopen(request) as response:
        release = json.load(response)
    asset = next((item for item in release["assets"] if item["name"] == name), None)
    if asset is None or not asset.get("digest", "").startswith("sha256:"):
        raise RuntimeError(f"GitHub release asset is missing or has no SHA-256 digest: {name}")
    return asset["browser_download_url"], asset["digest"].removeprefix("sha256:")


def download_verified(url: str, expected_hash: str, destination: Path) -> None:
    print(f"Downloading {destination.name}...", flush=True)
    request = urllib.request.Request(url, headers={"User-Agent": "SchedMesh-dev-setup"})
    digest = hashlib.sha256()
    with urllib.request.urlopen(request) as response, destination.open("wb") as output:
        while chunk := response.read(1024 * 1024):
            output.write(chunk)
            digest.update(chunk)
    if digest.hexdigest().lower() != expected_hash.lower():
        destination.unlink(missing_ok=True)
        raise RuntimeError(f"SHA-256 mismatch for {destination.name}")


def install_ninja(download_root: Path) -> Path:
    destination = TOOLS_ROOT / "ninja"
    executable = destination / "ninja.exe"
    if executable.is_file():
        return executable
    url, digest = release_asset("ninja-build/ninja", f"v{NINJA_VERSION}", "ninja-win.zip")
    archive = download_root / "ninja-win.zip"
    download_verified(url, digest, archive)
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as package:
        package.extractall(destination)
    return executable


def install_llvm(download_root: Path) -> Path:
    destination = TOOLS_ROOT / "llvm"
    clang_tidy = destination / "bin/clang-tidy.exe"
    if clang_tidy.is_file():
        return destination
    system_root = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "LLVM"
    system_tidy = system_root / "bin/clang-tidy.exe"
    if system_tidy.is_file():
        version = subprocess.run(
            [system_tidy, "--version"], check=True, capture_output=True, text=True
        ).stdout
        if f"LLVM version {LLVM_VERSION}" in version:
            print(f"Copying LLVM {LLVM_VERSION} from {system_root}...", flush=True)
            (destination / "bin").mkdir(parents=True, exist_ok=True)
            for name in ("clang.exe", "clang-cl.exe", "clang-format.exe", "clang-tidy.exe"):
                shutil.copy2(system_root / "bin" / name, destination / "bin" / name)
            for library in (system_root / "bin").glob("*.dll"):
                shutil.copy2(library, destination / "bin" / library.name)
            shutil.copytree(system_root / "lib/clang", destination / "lib/clang", dirs_exist_ok=True)
            return destination
    asset_name = f"LLVM-{LLVM_VERSION}-win64.exe"
    url, digest = release_asset("llvm/llvm-project", f"llvmorg-{LLVM_VERSION}", asset_name)
    installer = download_root / asset_name
    download_verified(url, digest, installer)
    destination.mkdir(parents=True, exist_ok=True)
    subprocess.run([installer, "/S", f"/D={destination}"], check=True)
    if not clang_tidy.is_file():
        raise RuntimeError("LLVM installer completed without producing clang-tidy.exe")
    return destination


def install_ortools(download_root: Path) -> Path:
    destination = TOOLS_ROOT / "ortools"
    existing_config = next(destination.rglob("ortoolsConfig.cmake"), None)
    if existing_config is not None:
        return existing_config.parents[3]
    asset_name = f"or-tools_x64_VisualStudio2022_cpp_v{ORTOOLS_VERSION}.zip"
    url, digest = release_asset("google/or-tools", "v9.15", asset_name)
    archive = download_root / asset_name
    download_verified(url, digest, archive)
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as package:
        package.extractall(destination)
    children = [path for path in destination.iterdir() if path.is_dir()]
    return children[0] if len(children) == 1 else destination


def msvc_environment() -> dict[str, str]:
    roots = (
        Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")),
        Path(os.environ.get("ProgramFiles", "C:/Program Files")),
    )
    candidates = [
        root / f"Microsoft Visual Studio/{year}/{edition}/VC/Auxiliary/Build/vcvars64.bat"
        for root in roots
        for year in ("2026", "2022", "2019")
        for edition in ("BuildTools", "Community", "Professional", "Enterprise")
    ]
    vcvars = next((path for path in candidates if path.is_file()), None)
    if vcvars is None:
        raise RuntimeError("MSVC C++ build tools are required (vcvars64.bat was not found)")
    output = subprocess.run(
        f'cmd.exe /d /c call "{vcvars}" >nul && set',
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    environment = os.environ.copy()
    environment.update(line.split("=", 1) for line in output.splitlines() if "=" in line)
    toolsets_root = (
        Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
        / "Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"
    )
    toolsets = [
        path for path in toolsets_root.glob("*")
        if (path / "include/optional").is_file()
        and (path / "bin/Hostx64/x64/link.exe").is_file()
    ]
    if toolsets:
        toolset = max(toolsets, key=lambda path: path.name)
        separator = os.pathsep
        include_parts = [
            part for part in environment.get("INCLUDE", "").split(separator)
            if "\\VC\\Tools\\MSVC\\" not in part
        ]
        library_parts = [
            part for part in environment.get("LIB", "").split(separator)
            if "\\VC\\Tools\\MSVC\\" not in part
        ]
        environment["INCLUDE"] = separator.join([str(toolset / "include"), *include_parts])
        environment["LIB"] = separator.join([str(toolset / "lib/x64"), *library_parts])
        environment["PATH"] = separator.join(
            [str(toolset / "bin/Hostx64/x64"), environment["PATH"]]
        )
        environment["VCToolsInstallDir"] = f"{toolset}{os.sep}"
        visual_studio = toolsets_root.parents[2]
        environment["VSINSTALLDIR"] = f"{visual_studio}{os.sep}"
        environment["VCINSTALLDIR"] = f"{visual_studio / 'VC'}{os.sep}"
        environment["VCToolsVersion"] = toolset.name
        environment["VisualStudioVersion"] = "17.0"
    return environment


def configure(ninja: Path, llvm: Path, ortools: Path) -> None:
    build = TOOLS_ROOT / "build"
    (build / "CMakeCache.txt").unlink(missing_ok=True)
    shutil.rmtree(build / "CMakeFiles", ignore_errors=True)
    dependencies = build / "_deps"
    if dependencies.is_dir():
        for dependency_build in dependencies.glob("*-build"):
            shutil.rmtree(dependency_build)
    environment = msvc_environment()
    system_includes = environment["INCLUDE"].replace(os.pathsep, ";")
    library_flags = " ".join(
        f'/LIBPATH:"{path}"' for path in environment["LIB"].split(os.pathsep) if path
    )
    subprocess.run(
        [
            shutil.which("cmake") or "cmake", "-S", ".", "-B", str(build), "-G", "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={ninja}",
            f"-DCMAKE_C_COMPILER={llvm / 'bin/clang-cl.exe'}",
            f"-DCMAKE_CXX_COMPILER={llvm / 'bin/clang-cl.exe'}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_C_FLAGS=/X",
            "-DCMAKE_CXX_FLAGS=/X",
            f"-DCMAKE_C_STANDARD_INCLUDE_DIRECTORIES={system_includes}",
            f"-DCMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES={system_includes}",
            f"-DCMAKE_EXE_LINKER_FLAGS={library_flags}",
            f"-DCMAKE_SHARED_LINKER_FLAGS={library_flags}",
            f"-DCMAKE_PREFIX_PATH={ortools}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DSCHEDMESH_BUILD_LEGACY=OFF",
            "-DSCHEDMESH_BUILD_TESTS=ON", "-DSCHEDMESH_FETCH_DEPENDENCIES=ON",
        ],
        check=True,
        env=environment,
    )


def main() -> int:
    if os.name != "nt":
        print("install-dev currently supports Windows; install LLVM and Ninja with your package manager.",
              file=sys.stderr)
        return 2
    TOOLS_ROOT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="schedmesh-dev-") as temporary:
        download_root = Path(temporary)
        ninja = install_ninja(download_root)
        llvm = install_llvm(download_root)
        ortools = install_ortools(download_root)
    configure(ninja, llvm, ortools)
    print("Development tools and compilation database are ready in .tools/.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"install-dev failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
