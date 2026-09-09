"""Publish the complete, verified binary build matrix for a version tag."""

import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys


def validate_release(artifact_directory, version):
    version = str(version)
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("Release version must be numeric semver")
    directory = Path(artifact_directory).resolve()
    expected = {
        f"libmustache-{version}-windows-{architecture}-{toolset}-md-{linkage}.zip"
        for architecture in ("x86", "x64")
        for toolset in ("v142", "v143")
        for linkage in ("static", "shared")
    }
    expected |= {
        f"libmustache-{version}-{platform}-{linkage}.tar.gz"
        for platform, linkages in (
            ("linux-x64-glibc", ("static", "shared")),
            ("linux-x64-musl", ("static",)),
            ("macos-aarch64", ("static", "shared")),
        )
        for linkage in linkages
    }
    archives = {*directory.glob("*.zip"), *directory.glob("*.tar.gz")}
    if {path.name for path in archives} != expected:
        raise ValueError("Release assets do not contain the complete build matrix")

    assets = []
    for name in sorted(expected):
        archive = directory / name
        checksum = directory / (name + ".sha256")
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if not checksum.is_file() or checksum.read_text(encoding="ascii") != f"{digest} *{name}\n":
            raise ValueError(f"Missing or incorrect checksum for {name}")
        assets.extend([str(archive), str(checksum)])
    return assets


def publish_release(artifact_directory, environment):
    tag = environment.get("GITHUB_REF_NAME", "")
    if (environment.get("GITHUB_EVENT_NAME") != "push"
            or environment.get("GITHUB_REF_TYPE") != "tag"
            or not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag)):
        raise ValueError("Releases are published only for version tag pushes")

    assets = validate_release(artifact_directory, tag[1:])

    repository = environment["GITHUB_REPOSITORY"]
    existing = subprocess.run(
        ["gh", "release", "view", tag, "--repo", repository, "--json", "id"],
        capture_output=True, text=True, check=False,
    )
    if existing.returncode == 0:
        command = ["gh", "release", "upload", tag, "--clobber"]
    else:
        command = ["gh", "release", "create", tag, "--verify-tag", "--title", tag, "--generate-notes"]
    subprocess.run(command + ["--repo", repository] + assets, check=True)


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "validate":
        validate_release(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 3 and sys.argv[1] == "publish":
        publish_release(sys.argv[2], os.environ)
    else:
        raise SystemExit(
            "usage: publish-release.py validate ARTIFACT_DIRECTORY VERSION | "
            "publish ARTIFACT_DIRECTORY"
        )
