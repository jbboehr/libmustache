"""Create drafts or publish releases from the verified binary build matrix."""

import hashlib
import json
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


def publish_release(artifact_directory, environment, *, draft_version=None):
    event = environment.get("GITHUB_EVENT_NAME")
    ref_type = environment.get("GITHUB_REF_TYPE")
    ref = environment.get("GITHUB_REF_NAME", "")
    draft = draft_version is not None
    tag = f"v{draft_version}" if draft else ref
    if draft:
        release_branch = ref_type == "branch" and (ref == "release" or ref.startswith("release/"))
        if not (event == "workflow_dispatch" or (event == "push" and release_branch)):
            raise ValueError("Drafts require a release branch push or an explicit manual run")
        if ref_type == "tag" and ref != tag:
            raise ValueError("Tag does not match the draft package version")
        target = environment["GITHUB_SHA"]
    elif event != "push" or ref_type != "tag" or not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag):
        raise ValueError("Releases are published only for version tag pushes")

    assets = validate_release(artifact_directory, tag[1:])

    repository = environment["GITHUB_REPOSITORY"]
    existing = subprocess.run(
        ["gh", "release", "view", tag, "--repo", repository, "--json", "isDraft"],
        capture_output=True, text=True, check=False,
    )
    if existing.returncode == 0:
        existing_draft = json.loads(existing.stdout)["isDraft"]
        if draft and not existing_draft:
            raise ValueError(f"Release {tag} is already published; bump the package version before drafting")
        subprocess.run(["gh", "release", "upload", tag, "--clobber", "--repo", repository] + assets, check=True)
        if existing_draft:
            options = ["--target", target] if draft else ["--draft=false", "--tag", tag, "--verify-tag"]
            subprocess.run(["gh", "release", "edit", tag, "--repo", repository] + options, check=True)
    else:
        options = ["--draft", "--target", target] if draft else ["--verify-tag"]
        command = ["gh", "release", "create", tag, "--title", tag, "--generate-notes"]
        subprocess.run(command + options + ["--repo", repository] + assets, check=True)


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "validate":
        validate_release(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 4 and sys.argv[1] == "draft":
        publish_release(sys.argv[2], os.environ, draft_version=sys.argv[3])
    elif len(sys.argv) == 3 and sys.argv[1] == "publish":
        publish_release(sys.argv[2], os.environ)
    else:
        raise SystemExit(
            "usage: publish-release.py validate ARTIFACT_DIRECTORY VERSION | "
            "draft ARTIFACT_DIRECTORY VERSION | "
            "publish ARTIFACT_DIRECTORY"
        )
