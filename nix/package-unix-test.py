"""Check Unix SDK contents, relocation metadata, and archive integrity."""

import hashlib
import importlib.util
from pathlib import Path
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("package_unix", ROOT / "nix/package-unix.py")
packager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packager)


class UnixPackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.prefix = self.root / "installed"
        self.source = self.root / "source"
        for relative in (
            "bin/mustachec", "include/mustache/mustache.hpp",
            "include/mustache/mustache_config.h", "include/mustache/mustache_export.hpp",
            "lib/libmustache.a", "lib/libmustache.so.6.0.0", "lib/libmustache.6.0.0.dylib",
            "lib/cmake/mustache/mustacheConfig.cmake",
            "lib/cmake/mustache/mustacheConfigVersion.cmake",
            "lib/cmake/mustache/mustacheStaticTargets.cmake",
            "lib/cmake/mustache/mustacheStaticTargets-release.cmake",
            "lib/cmake/mustache/mustacheSharedTargets.cmake",
            "lib/cmake/mustache/mustacheSharedTargets-release.cmake",
        ):
            path = self.prefix / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(relative)
        (self.prefix / "bin/mustachec").chmod(0o755)
        for alias, target in (
            ("libmustache.so", "libmustache.so.6"),
            ("libmustache.so.6", "libmustache.so.6.0.0"),
            ("libmustache.dylib", "libmustache.6.dylib"),
            ("libmustache.6.dylib", "libmustache.6.0.0.dylib"),
        ):
            (self.prefix / "lib" / alias).symlink_to(target)
        pc = self.prefix / "lib/pkgconfig/mustache.pc"
        pc.parent.mkdir()
        pc.write_text("prefix=/nix/store/example\nlibdir=${prefix}/lib\nLibs: -L${libdir} -lmustache\nCflags: -I${prefix}/include\n")
        for relative in ("LICENSE.md", "docs/unix-binaries.md", "vendor/cista/LICENSE", "vendor/xxhash/LICENSE"):
            path = self.source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(relative)
        self.json_license = self.root / "json-license"
        self.json_license.write_text("JSON license")

    def test_packages_preserve_sdk_linkage_symlinks_and_executable(self):
        for platform, linkages in (
            ("linux-x64-glibc", ("static", "shared")),
            ("linux-x64-musl", ("static",)),
            ("macos-aarch64", ("static", "shared")),
        ):
            for linkage in linkages:
                with self.subTest(platform=platform, linkage=linkage):
                    name = f"libmustache-0.6.0-{platform}-{linkage}"
                    package = self.root / name
                    packager.stage_sdk(self.prefix, self.source, self.json_license, package, platform, linkage)
                    archive = packager.write_archive(package, self.root / "archives", name)
                    with tarfile.open(archive) as tar:
                        names = {member.name.removeprefix("./") for member in tar.getmembers()}
                        self.assertIn("include/mustache/mustache.hpp", names)
                        self.assertIn("lib/cmake/mustache/mustacheConfig.cmake", names)
                        self.assertIn("LICENSE.md", names)
                        self.assertIn("licenses/nlohmann-json/LICENSE", names)
                        self.assertEqual("lib/libmustache.a" in names, linkage == "static")
                        selected = "Static" if linkage == "static" else "Shared"
                        other = "Shared" if linkage == "static" else "Static"
                        self.assertIn(f"lib/cmake/mustache/mustache{selected}Targets.cmake", names)
                        self.assertNotIn(f"lib/cmake/mustache/mustache{other}Targets.cmake", names)
                    self.assertTrue((package / "bin/mustachec").stat().st_mode & 0o111)
                    if linkage == "shared":
                        library = "libmustache.dylib" if platform == "macos-aarch64" else "libmustache.so"
                        self.assertTrue((package / "lib" / library).is_symlink())
                        self.assertTrue((package / "lib" / library).resolve().is_file())
                    pc = (package / "lib/pkgconfig/mustache.pc").read_text()
                    self.assertIn("prefix=${pcfiledir}/../..\n", pc)
                    self.assertNotIn("/nix/store", pc)
                    self.assertEqual("-DMUSTACHE_STATIC_DEFINE" in pc, linkage == "static")
                    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
                    self.assertEqual(Path(str(archive) + ".sha256").read_text(), f"{digest} *{archive.name}\n")

    def test_missing_static_library_prevents_packaging(self):
        (self.prefix / "lib/libmustache.a").unlink()
        with self.assertRaises(FileNotFoundError):
            packager.stage_sdk(self.prefix, self.source, self.json_license,
                               self.root / "package", "linux-x64-glibc", "static")


if __name__ == "__main__":
    unittest.main()
