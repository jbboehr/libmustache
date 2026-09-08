"""Publish the complete, verified Windows build matrix for a version tag."""

import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys


def publish_release(artifact_directory, environment):
    tag = environment.get("GITHUB_REF_NAME", "")
    if (environment.get("GITHUB_EVENT_NAME") != "push"
            or environment.get("GITHUB_REF_TYPE") != "tag"
            or not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag)):
        raise ValueError("Windows releases are published only for version tag pushes")

    directory = Path(artifact_directory).resolve()
    expected = {
        f"libmustache-{tag[1:]}-windows-{architecture}-{toolset}-md-{linkage}.zip"
        for architecture in ("x86", "x64")
        for toolset in ("v142", "v143")
        for linkage in ("static", "shared")
    }
    if {path.name for path in directory.glob("*.zip")} != expected:
        raise ValueError("Release assets do not contain the complete Windows build matrix")

    assets = []
    for name in sorted(expected):
        archive = directory / name
        checksum = archive.with_suffix(".zip.sha256")
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if not checksum.is_file() or checksum.read_text(encoding="ascii") != f"{digest} *{name}\n":
            raise ValueError(f"Missing or incorrect checksum for {name}")
        assets.extend([str(archive), str(checksum)])

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
    if len(sys.argv) != 2:
        raise SystemExit("usage: publish-windows-release.py ARTIFACT_DIRECTORY")
    publish_release(sys.argv[1], os.environ)
