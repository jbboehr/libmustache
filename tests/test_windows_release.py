"""Exercise Windows package contents and the release publishing boundary."""

import hashlib
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock
import zipfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "publish_windows_release", ROOT / "scripts/publish-windows-release.py"
)
publisher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(publisher)


class PublishingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.assets = Path(self.temporary.name)
        self.environment = {
            "GITHUB_EVENT_NAME": "push",
            "GITHUB_REF_TYPE": "tag",
            "GITHUB_REF_NAME": "v0.6.0",
            "GITHUB_REPOSITORY": "example/libmustache",
        }
        for architecture in ("x86", "x64"):
            for toolset in ("v142", "v143"):
                for linkage in ("static", "shared"):
                    name = f"libmustache-0.6.0-windows-{architecture}-{toolset}-md-{linkage}.zip"
                    archive = self.assets / name
                    archive.write_bytes(b"test archive")
                    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
                    archive.with_suffix(".zip.sha256").write_text(
                        f"{digest} *{name}\n", encoding="ascii"
                    )

    def test_only_version_tag_pushes_can_publish(self):
        cases = (
            {"GITHUB_REF_TYPE": "branch", "GITHUB_REF_NAME": "master"},
            {"GITHUB_REF_TYPE": "branch", "GITHUB_REF_NAME": "develop"},
            {"GITHUB_EVENT_NAME": "pull_request"},
            {"GITHUB_EVENT_NAME": "workflow_dispatch"},
            {"GITHUB_REF_NAME": "nightly"},
        )
        for changes in cases:
            with self.subTest(changes=changes), mock.patch("subprocess.run") as run:
                with self.assertRaises(ValueError):
                    publisher.publish_release(self.assets, self.environment | changes)
                run.assert_not_called()

    def test_incomplete_matrix_cannot_publish(self):
        next(self.assets.glob("*.zip")).unlink()
        with mock.patch("subprocess.run") as run:
            with self.assertRaises(ValueError):
                publisher.publish_release(self.assets, self.environment)
            run.assert_not_called()

    def test_corrupted_archive_cannot_publish(self):
        next(self.assets.glob("*.zip")).write_bytes(b"corrupted archive")
        with mock.patch("subprocess.run") as run:
            with self.assertRaises(ValueError):
                publisher.publish_release(self.assets, self.environment)
            run.assert_not_called()

    def test_missing_checksum_cannot_publish(self):
        next(self.assets.glob("*.sha256")).unlink()
        with mock.patch("subprocess.run") as run:
            with self.assertRaises(ValueError):
                publisher.publish_release(self.assets, self.environment)
            run.assert_not_called()

    def test_new_release_contains_every_archive_and_checksum(self):
        with mock.patch("subprocess.run") as run:
            run.side_effect = [subprocess.CompletedProcess([], 1), subprocess.CompletedProcess([], 0)]
            publisher.publish_release(self.assets, self.environment)
        command = run.call_args.args[0]
        self.assertEqual(command[:4], ["gh", "release", "create", "v0.6.0"])
        self.assertIn("--verify-tag", command)
        self.assertIn("--generate-notes", command)
        # Windows temporary paths can use short names such as RUNNER~1.
        expected_assets = {str(path.resolve()) for path in self.assets.iterdir()}
        self.assertEqual(set(command) & expected_assets, expected_assets)
        self.assertTrue(run.call_args.kwargs["check"])

    def test_existing_release_uploads_without_replacing_notes(self):
        with mock.patch("subprocess.run") as run:
            run.return_value = subprocess.CompletedProcess([], 0)
            publisher.publish_release(self.assets, self.environment)
        command = run.call_args.args[0]
        self.assertEqual(command[:4], ["gh", "release", "upload", "v0.6.0"])
        self.assertIn("--clobber", command)
        self.assertNotIn("--generate-notes", command)


class PackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pwsh = os.environ.get("MUSTACHE_TEST_PWSH") or shutil.which("pwsh")
        if not cls.pwsh:
            raise unittest.SkipTest("PowerShell is required for the package tests")

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.prefix = self.root / "installed"
        self.source = self.root / "source"
        self.output = self.root / "output"
        for name in (
            "bin/mustache.dll", "bin/mustachec.exe", "lib/mustache.lib",
            "lib/mustache_static.lib", "include/mustache/mustache.hpp",
            "include/mustache/mustache_config.h", "include/mustache/mustache_export.hpp",
            "lib/cmake/mustache/mustacheConfig.cmake",
            "lib/cmake/mustache/mustacheConfigVersion.cmake",
            "lib/cmake/mustache/mustacheSharedTargets.cmake",
            "lib/cmake/mustache/mustacheSharedTargets-release.cmake",
            "lib/cmake/mustache/mustacheStaticTargets.cmake",
            "lib/cmake/mustache/mustacheStaticTargets-release.cmake",
        ):
            path = self.prefix / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())
        for name in ("LICENSE.md", "docs/windows-binaries.md", "vendor/cista/LICENSE", "vendor/xxhash/LICENSE"):
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(name, encoding="utf-8")
        self.json_license = self.root / "json-copyright"
        self.json_license.write_text("JSON license", encoding="utf-8")

    def package(self, linkage):
        return subprocess.run(
            [self.pwsh, "-NoProfile", "-File", str(ROOT / "scripts/package-windows.ps1"),
             "-InstallPrefix", str(self.prefix), "-SourceDirectory", str(self.source),
             "-JsonLicense", str(self.json_license),
             "-OutputDirectory", str(self.output), "-Version", "0.6.0",
             "-Architecture", "x64", "-Toolset", "v143", "-Linkage", linkage],
            text=True, capture_output=True, check=True,
        )

    def test_each_package_is_a_complete_sdk_with_only_its_linkage(self):
        for linkage in ("static", "shared"):
            with self.subTest(linkage=linkage):
                self.package(linkage)
                archive = self.output / f"libmustache-0.6.0-windows-x64-v143-md-{linkage}.zip"
                with zipfile.ZipFile(archive) as package:
                    names = set(package.namelist())
                    self.assertIn("include/mustache/mustache.hpp", names)
                    self.assertIn("include/mustache/mustache_config.h", names)
                    self.assertIn("lib/cmake/mustache/mustacheConfig.cmake", names)
                    self.assertIn("lib/cmake/mustache/mustacheConfigVersion.cmake", names)
                    self.assertIn("LICENSE.md", names)
                    self.assertIn("licenses/cista/LICENSE", names)
                    self.assertIn("licenses/xxhash/LICENSE", names)
                    self.assertEqual(package.read("licenses/nlohmann-json/LICENSE"), b"JSON license")
                    self.assertIn("README.md", names)
                    self.assertEqual(package.read("bin/mustachec.exe"), b"bin/mustachec.exe")
                    self.assertEqual("lib/mustache_static.lib" in names, linkage == "static")
                    self.assertEqual("lib/mustache.lib" in names, linkage == "shared")
                    self.assertEqual("bin/mustache.dll" in names, linkage == "shared")
                    selected = "Static" if linkage == "static" else "Shared"
                    other = "Shared" if linkage == "static" else "Static"
                    self.assertIn(f"lib/cmake/mustache/mustache{selected}Targets.cmake", names)
                    self.assertIn(f"lib/cmake/mustache/mustache{selected}Targets-release.cmake", names)
                    self.assertNotIn(f"lib/cmake/mustache/mustache{other}Targets.cmake", names)
                expected = f"{hashlib.sha256(archive.read_bytes()).hexdigest()} *{archive.name}\n"
                self.assertEqual(archive.with_suffix(".zip.sha256").read_text(), expected)

    def test_missing_runtime_dll_prevents_shared_package_creation(self):
        (self.prefix / "bin/mustache.dll").unlink()
        with self.assertRaises(subprocess.CalledProcessError):
            self.package("shared")
        self.assertEqual(list(self.output.glob("*.zip")), [])


if __name__ == "__main__":
    unittest.main()
