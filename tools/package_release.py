#!/usr/bin/env python3
"""Create a reviewable LSSE VST3 package without publishing it."""

from __future__ import annotations

import argparse
import hashlib
import shutil
from pathlib import Path


PLUGINS = (
    "MEMORY",
    "TEXTURE",
    "MACHINE",
    "MATERIAL",
    "IMPACT",
    "CREATURE",
    "MOTION",
    "ENVIRONMENT",
    "CONVOLUTION",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--version", default="1.1.0")
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    return parser.parse_args()


def locate_bundle(build_dir: Path, plugin: str, configuration: str) -> Path:
    candidates = [
        path
        for path in build_dir.rglob(f"{plugin}.vst3")
        if path.is_dir() and "_artefacts" in str(path)
    ]
    preferred = [path for path in candidates if configuration.lower() in str(path).lower()]
    selected = preferred or candidates
    if len(selected) != 1:
        rendered = "\n".join(f"  {path}" for path in selected) or "  none"
        raise RuntimeError(f"expected one {plugin}.vst3 bundle, found:\n{rendered}")
    bundle = selected[0]
    payloads = [
        path
        for path in bundle.rglob("*")
        if path.is_file() and path.name != "moduleinfo.json" and path.stat().st_size > 0
    ]
    if not payloads:
        raise RuntimeError(f"{bundle} exists but contains no plugin binary")
    return bundle


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    args = parse_args()
    root = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()
    package_name = f"LSSE-{args.version}-{args.platform}"
    package_dir = output_dir / package_name

    if package_dir.exists():
        shutil.rmtree(package_dir)
    vst3_dir = package_dir / "VST3"
    vst3_dir.mkdir(parents=True)

    for plugin in PLUGINS:
        shutil.copytree(
            locate_bundle(build_dir, plugin, args.configuration),
            vst3_dir / f"{plugin}.vst3",
        )

    for filename in ("README.md", "INSTALL.md", "THIRD_PARTY_NOTICES.md"):
        shutil.copy2(root / filename, package_dir / filename)
    shutil.copy2(root / "LICENSE", package_dir / "LICENSE-LSSE.txt")

    juce_licence_candidates = (
        build_dir / "_deps" / "juce-src" / "LICENSE.md",
        root / "build" / "_deps" / "juce-src" / "LICENSE.md",
    )
    juce_licence = next((path for path in juce_licence_candidates if path.is_file()), None)
    if juce_licence is None:
        rendered = "\n".join(f"  {path}" for path in juce_licence_candidates)
        raise RuntimeError(f"JUCE licence file was not found in:\n{rendered}")

    vst3_licence = (
        juce_licence.parent
        / "modules"
        / "juce_audio_processors_headless"
        / "format_types"
        / "VST3_SDK"
        / "LICENSE.txt"
    )
    licence_sources = {
        juce_licence: "LICENSE-JUCE.md",
        vst3_licence: "LICENSE-VST3-SDK.txt",
    }
    for source, destination in licence_sources.items():
        if not source.is_file():
            raise RuntimeError(f"required licence file was not found: {source}")
        shutil.copy2(source, package_dir / destination)

    checksum_lines = []
    for file_path in sorted(path for path in package_dir.rglob("*") if path.is_file()):
        relative = file_path.relative_to(package_dir).as_posix()
        checksum_lines.append(f"{sha256(file_path)}  {relative}")
    (package_dir / "SHA256SUMS.txt").write_text(
        "\n".join(checksum_lines) + "\n", encoding="utf-8"
    )

    archive = shutil.make_archive(str(output_dir / package_name), "zip", output_dir, package_name)
    print(f"created {package_dir}")
    print(f"created {archive}")


if __name__ == "__main__":
    main()
