"""Assemble relocatable Unix SDK archives from a CMake installation."""

import argparse
import gzip
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile


def stage_sdk(prefix, source, json_license, destination, platform, linkage):
    kind = "Static" if linkage == "static" else "Shared"
    files = [
        "bin/mustachec", "include/mustache/mustache.hpp",
        "include/mustache/mustache_config.h", "include/mustache/mustache_export.hpp",
        "lib/pkgconfig/mustache.pc", "lib/cmake/mustache/mustacheConfig.cmake",
        "lib/cmake/mustache/mustacheConfigVersion.cmake",
        f"lib/cmake/mustache/mustache{kind}Targets.cmake",
        f"lib/cmake/mustache/mustache{kind}Targets-release.cmake",
    ]
    if linkage == "static":
        files.append("lib/libmustache.a")
    else:
        pattern = "libmustache*.dylib" if platform == "macos-aarch64" else "libmustache.so*"
        libraries = sorted((prefix / "lib").glob(pattern))
        if not libraries:
            raise FileNotFoundError(f"Missing shared library: {prefix / 'lib' / pattern}")
        files.extend(str(path.relative_to(prefix)) for path in libraries)
    for relative in files:
        if not (prefix / relative).is_file():
            raise FileNotFoundError(f"Missing required package file: {prefix / relative}")
    destination.mkdir(parents=True)
    for relative in files:
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(prefix / relative, target, follow_symlinks=False)
    shutil.copytree(prefix / "include/mustache", destination / "include/mustache", dirs_exist_ok=True)
    shutil.copy2(source / "LICENSE.md", destination)
    shutil.copy2(source / "docs/unix-binaries.md", destination / "README.md")
    for dependency in ("cista", "xxhash"):
        target = destination / "licenses" / dependency
        target.mkdir(parents=True)
        shutil.copy2(source / "vendor" / dependency / "LICENSE", target)
    target = destination / "licenses/nlohmann-json"
    target.mkdir(parents=True)
    shutil.copy2(json_license, target / "LICENSE")
    pc = destination / "lib/pkgconfig/mustache.pc"
    text = re.sub(r"^prefix=.*$", "prefix=${pcfiledir}/../..", pc.read_text(), flags=re.MULTILINE)
    if linkage == "static":
        text = text.replace("Cflags:", "Cflags: -DMUSTACHE_STATIC_DEFINE", 1)
    pc.write_text(text)


def run(*arguments):
    return subprocess.check_output(arguments, text=True).strip()


def has_interpreter(path):
    result = subprocess.run(["patchelf", "--print-interpreter", str(path)],
                            capture_output=True, text=True)
    return result.returncode == 0 and bool(result.stdout.strip())


def relocate(package, platform):
    executable = package / "bin/mustachec"
    libraries = [path for path in (package / "lib").iterdir()
                 if path.is_file() and not path.is_symlink() and path.suffix != ".a"]
    binaries = [executable, *libraries]
    if platform.startswith("linux-"):
        # Fully static executables have no interpreter or RPATH, and patchelf
        # cannot process them at all.
        dynamic = has_interpreter(executable)
        relocatable = [executable, *libraries] if dynamic else libraries
        for binary in relocatable:
            run("patchelf", "--remove-rpath", str(binary))
            if "/" in run("patchelf", "--print-needed", str(binary)):
                raise ValueError(f"Non-relocatable dependency in {binary}")
        if dynamic:
            interpreter = ("/lib/ld-musl-x86_64.so.1" if platform.endswith("musl")
                           else "/lib64/ld-linux-x86-64.so.2")
            run("patchelf", "--set-interpreter", interpreter, str(executable))
            run("patchelf", "--set-rpath", "$ORIGIN/../lib", str(executable))
    else:
        for binary in binaries:
            for line in run("otool", "-L", str(binary)).splitlines()[1:]:
                dependency = line.strip().split(" (", 1)[0]
                if "libmustache" in Path(dependency).name:
                    run("install_name_tool", "-change", dependency,
                        "@rpath/" + Path(dependency).name, str(binary))
                elif dependency.startswith("/nix/store/"):
                    raise ValueError(f"Non-system dependency in {binary}: {dependency}")
            if binary != executable:
                run("install_name_tool", "-id", "@rpath/" + binary.name, str(binary))
            commands = run("otool", "-l", str(binary))
            for rpath in re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset", commands):
                if rpath.startswith("/"):
                    run("install_name_tool", "-delete_rpath", rpath, str(binary))
            run("codesign", "--force", "--sign", "-", str(binary))
    for metadata in (package / "lib").rglob("*"):
        if metadata.suffix in (".cmake", ".pc") and "/nix/store/" in metadata.read_text():
            raise ValueError(f"Non-relocatable package metadata: {metadata}")


def write_archive(package, output, name):
    output.mkdir(parents=True, exist_ok=True)
    archive = output / (name + ".tar.gz")
    def normalize(member):
        member.uid = member.gid = 0
        member.uname = member.gname = "root"
        member.mtime = 1
        return member
    with archive.open("wb") as stream, gzip.GzipFile(filename="", fileobj=stream, mode="wb", mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode="w") as tar:
            tar.add(package, arcname=".", filter=normalize)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    Path(str(archive) + ".sha256").write_text(f"{digest} *{archive.name}\n", encoding="ascii")
    return archive


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("prefix", "source", "json-license", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--platform", choices=("linux-x64-glibc", "linux-x64-musl", "macos-aarch64"), required=True)
    parser.add_argument("--linkage", choices=("static", "shared"), required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.version):
        parser.error("version must be X.Y.Z")
    with tempfile.TemporaryDirectory() as temporary:
        package = Path(temporary) / "sdk"
        stage_sdk(args.prefix, args.source, args.json_license, package, args.platform, args.linkage)
        relocate(package, args.platform)
        print(write_archive(package, args.output, f"libmustache-{args.version}-{args.platform}-{args.linkage}"))
