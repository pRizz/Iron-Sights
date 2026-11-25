"""ROM-free tests for build identity, ZIP contents and native cache validation."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]


def module(name):
    spec=importlib.util.spec_from_file_location(name,HERE/(name+".py"))
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result


build_info=module("build_info")
packager=module("package_windows")
linux_packager=module("package_linux")


class Distribution(unittest.TestCase):
    def test_identity_and_clean_ci_requirements(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            def git(*args):
                return subprocess.check_output(["git","-C",str(root),*args],text=True,stderr=subprocess.DEVNULL).strip()
            git("init","--initial-branch=main")
            git("config","user.name","Distribution test");git("config","user.email","test@example.invalid")
            (root/"source.c").write_text("int main(void) {return 0;}\n")
            git("add","source.c");git("-c","core.hooksPath=/dev/null","commit","-m","fixture")
            sha=git("rev-parse","HEAD")
            env=dict(GITHUB_ACTIONS="true",GITHUB_SHA=sha,GITHUB_RUN_ID="123",GITHUB_REPOSITORY="pRizz/Iron-Sights",
                     MIDAS_RELEASE_TAG="build-12.1-"+sha[:12],MIDAS_BUILD_TIME_UTC="2026-10-23T17:00:00Z")
            info=build_info.metadata(root,env)
            self.assertTrue(info['ci']);self.assertFalse(info['dirty'])
            self.assertEqual(info['commit'],sha)
            self.assertTrue(info['workflow_url'].endswith('/actions/runs/123'))
            self.assertEqual(info['built_at'],env['MIDAS_BUILD_TIME_UTC'])
            (root/"source.c").write_text("changed\n")
            info=build_info.metadata(root,env)
            self.assertTrue(info['dirty']);self.assertFalse(info['ci']);self.assertEqual(info['workflow_url'],"")
            git("checkout","--","source.c")
            (root/"new.c").write_text("untracked source")
            self.assertTrue(build_info.metadata(root,env)['dirty'])

    def test_architecture_reaches_the_manifest_but_not_the_compiled_header(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root=Path(folder);env={"SOURCE_DATE_EPOCH":"0"}
            outputs={arch:root/arch/"port_buildinfo.h" for arch in ("i686","x86_64")}
            # Act
            for arch,output in outputs.items():
                build_info.write(root,output,env,arch)
            # Assert
            for arch,output in outputs.items():
                self.assertEqual(json.loads((output.parent/"build-info.json").read_text())['architecture'],arch)
            self.assertEqual(outputs["i686"].read_text(),outputs["x86_64"].read_text())

    def test_metadata_without_git_and_with_fixed_epoch(self):
        with tempfile.TemporaryDirectory() as folder:
            info=build_info.metadata(Path(folder),{"SOURCE_DATE_EPOCH":"0"})
            self.assertFalse(info['ci']);self.assertEqual(info['built_at'],"1970-01-01T00:00:00Z")
            self.assertEqual(info['source_url'],"")

    def test_allowlisted_package_never_includes_player_or_rom_data(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);build=root/"build";out=root/"dist"
            (build/"generated").mkdir(parents=True)
            info=build_info.metadata(root,{"SOURCE_DATE_EPOCH":"0"})
            (build/"generated/build-info.json").write_text(json.dumps(info))
            for source in packager.FILES:
                path=build/source;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b"test file")
            for source in ("Data/user.z64","Data/Saves/eeprom.bin","Data/Profiles/user.geprofile","midas.ini","previous.exe","ProjectMidas.exe"):
                path=build/source;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b"must not ship")
            # Third-party texture packs are external mods; a stray copy in the build never ships.
            textures=build/"Data/TexturePacks";textures.mkdir(parents=True)
            (textures/"hd.istex").write_text("must not ship")
            # So are the Enhanced fonts (FORK-052): the player downloads them on request.
            fonts=build/"Data/FontPacks/gepd";fonts.mkdir(parents=True)
            (fonts/"GOLDENEYE#CDE33EA7#4#1_rgb.png").write_text("must not ship");(fonts/"SOURCE.txt").write_text("must not ship")
            license=root/"SDL-LICENSE";license.write_text("license fixture")
            path=packager.package(build,out,sdl_license=license)
            with zipfile.ZipFile(path) as archive:
                names=archive.namelist()
                self.assertIn("SDL2.dll",names);self.assertIn("Data/README.txt",names)
                self.assertIn("IronSights.exe",names);self.assertNotIn("ProjectMidas.exe",names)
                self.assertIn("build-info.json",names);self.assertIn("SHA256SUMS.txt",names)
                self.assertNotIn("Enhanced-Fonts-LICENSE.txt",names);self.assertNotIn("Enhanced-Fonts-NOTICE.txt",names)
                self.assertIn("Save-Transfer-LICENSE.txt",names)
                self.assertIn("Open-Assets-LICENSE.txt",names)
                self.assertIn(b"Iron Sights assets contributors",archive.read("Open-Assets-LICENSE.txt"))
                self.assertIn(b"Copyright 2011-2014 Kazuho Oku",archive.read("Save-Transfer-LICENSE.txt"))
                self.assertIn(b"Copyright (C) 2017 okdshin",archive.read("Save-Transfer-LICENSE.txt"))
                self.assertFalse(any(n.startswith("Data/TexturePacks/") for n in names))
                self.assertFalse(any(n.startswith("Data/FontPacks/") for n in names))
                self.assertFalse(any(n.endswith((".z64",".bin",".geprofile")) for n in names))
                self.assertNotIn("midas.ini",names);self.assertNotIn("previous.exe",names)
            previous=path.read_bytes();packager.package(build,out,sdl_license=license)
            self.assertEqual(previous,path.read_bytes())

    def test_packagers_refuse_a_comparison_build(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);build=root/"build";out=root/"dist"
            build.mkdir()
            (build/"CMakeCache.txt").write_text("GE_COMPARE:BOOL=ON\nGE_VIDEO:BOOL=ON\n")
            license=root/"SDL-LICENSE";license.write_text("license fixture")
            # Act, Assert
            with self.assertRaisesRegex(ValueError,"comparison build"):
                packager.package(build,out,sdl_license=license)
            with self.assertRaisesRegex(ValueError,"comparison build"):
                linux_packager.package(build,out)
            self.assertFalse(out.exists())

    def test_packagers_accept_a_tree_without_the_comparison_option(self):
        # Arrange
        with tempfile.TemporaryDirectory() as folder:
            build=Path(folder)
            (build/"CMakeCache.txt").write_text("GE_COMPARE:BOOL=OFF\nGE_COMPARE_LIBM:BOOL=OFF\n")
            # Act, Assert (no exception)
            packager.refuse_comparison_build(build)
            linux_packager.refuse_comparison_build(build)

    def test_i686_target_keeps_its_names_and_runtime(self):
        # Arrange: package_linux.py's literals at f04e0a7, before the table existed.
        expected=dict(suffix="i686",elf_class=1,elf_machine=3,system_libdir="/usr/lib/i386-linux-gnu",
                      runtime_url="https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-i686",
                      runtime_sha256="e72ea0b140a0a16e680713238a6f30aad278b62c4ca17919c554864124515498",
                      dockerfile="scripts/port/linux-release.Dockerfile",baseline="Ubuntu 22.04 / glibc 2.35",
                      start_here="scripts/port/linux/START-HERE.txt",steam_deck="scripts/port/linux/STEAM-DECK.txt",
                      sums_name="SHA256SUMS-linux.txt",info_prefix="linux",system_libraries=())
        # Act
        record=linux_packager.TARGETS["i686"]._asdict()
        names=linux_packager.output_names(linux_packager.TARGETS["i686"])
        # Assert
        self.assertEqual(record,expected)
        self.assertEqual(names,dict(appdir="Iron-Sights-linux-i686.AppDir",folder="Iron-Sights-linux-i686",
                                    tarball="Iron-Sights-linux-i686.tar.gz",appimage="Iron-Sights-linux-i686.AppImage",
                                    steamdeck="Iron-Sights-steamdeck-i686.zip",steamdeck_folder="Iron-Sights-steamdeck-i686",
                                    runtime="runtime-i686",sums="SHA256SUMS-linux.txt",
                                    build_info="linux-build-info.json",build_inputs="linux-build-inputs.json"))

    def test_x86_64_outputs_are_named_for_the_architecture(self):
        # Act
        names=linux_packager.output_names(linux_packager.TARGETS["x86_64"])
        # Assert
        self.assertEqual(names,dict(appdir="Iron-Sights-linux-x86_64.AppDir",folder="Iron-Sights-linux-x86_64",
                                    tarball="Iron-Sights-linux-x86_64.tar.gz",appimage="Iron-Sights-linux-x86_64.AppImage",
                                    steamdeck="Iron-Sights-steamdeck-x86_64.zip",steamdeck_folder="Iron-Sights-steamdeck-x86_64",
                                    runtime="runtime-x86_64",sums="SHA256SUMS-linux-x86_64.txt",
                                    build_info="linux-x86_64-build-info.json",build_inputs="linux-x86_64-build-inputs.json"))

    def assert_cache_refused(self,line,message):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange
            root=Path(folder);build=root/"build";out=root/"dist"
            build.mkdir()
            (build/"CMakeCache.txt").write_text(line+"\n")
            # Act, Assert
            with self.assertRaisesRegex(ValueError,message):
                linux_packager.package(build,out,arch="x86_64")
            self.assertFalse(out.exists())

    def test_packager_refuses_a_sanitizer_build(self):
        self.assert_cache_refused("GE_SANITIZE:BOOL=ON","sanitizer build")

    def test_packager_refuses_an_oracle_build(self):
        self.assert_cache_refused("GE_LP64_ORACLE:BOOL=ON","oracle build")

    def test_packager_refuses_a_headless_build(self):
        self.assert_cache_refused("GE_HEADLESS:BOOL=ON","headless build")

    def test_packager_refuses_a_build_without_video(self):
        self.assert_cache_refused("GE_VIDEO:BOOL=OFF","without video")

    def test_packager_refuses_a_build_with_compiled_in_assets(self):
        self.assert_cache_refused("GE_ROMDATA_FALLBACK:BOOL=ON","compiled-in asset copies")

    def test_packager_refuses_an_executable_of_the_other_width(self):
        headers={"i686":b"\x7fELF\x01\x01\x01"+bytes(11)+b"\x03\x00","x86_64":b"\x7fELF\x02\x01\x01"+bytes(11)+b"\x3e\x00"}
        for game,target in (("i686","x86_64"),("x86_64","i686")):
            with self.subTest(game=game,target=target),tempfile.TemporaryDirectory() as folder:
                # Arrange
                root=Path(folder);build=root/"build";out=root/"dist"
                build.mkdir();(build/"IronSights").write_bytes(headers[game])
                # Act, Assert
                with self.assertRaisesRegex(ValueError,"is not an "+target+" executable"):
                    linux_packager.package(build,out,arch=target)
                self.assertFalse(out.exists())

    def test_library_from_a_32_bit_folder_is_refused_at_x86_64(self):
        # Arrange
        ldd="\tlinux-vdso.so.1 (0x00007ffd)\n\tlibfoo.so => /usr/lib/i386-linux-gnu/libfoo.so (0xf7e00000)\n"
        # Act, Assert
        with self.assertRaisesRegex(ValueError,"32-bit library folder"):
            linux_packager.resolved_libraries(ldd,linux_packager.TARGETS["x86_64"])

    def test_library_from_the_i386_folder_is_bundled_at_i686(self):
        # Arrange
        ldd="\tlinux-gate.so.1 (0xf7f00000)\n\tlibfoo.so => /usr/lib/i386-linux-gnu/libfoo.so (0xf7e00000)\n"
        # Act
        paths=linux_packager.resolved_libraries(ldd,linux_packager.TARGETS["i686"])
        # Assert
        self.assertEqual(paths,{"libfoo.so":Path("/usr/lib/i386-linux-gnu/libfoo.so")})

    def test_x86_64_takes_the_cxx_runtime_from_the_system_and_i686_bundles_it(self):
        # Arrange
        ldd=("\tlibstdc++.so.6 => /usr/lib/x/libstdc++.so.6 (0x1)\n\tlibgcc_s.so.1 => /usr/lib/x/libgcc_s.so.1 (0x2)\n"
             "\tlibfoo.so => /usr/lib/x/libfoo.so (0x3)\n")
        # Act
        bundled={arch:sorted(linux_packager.resolved_libraries(ldd,linux_packager.TARGETS[arch])) for arch in ("i686","x86_64")}
        # Assert
        self.assertEqual(bundled,{"i686":["libfoo.so","libgcc_s.so.1","libstdc++.so.6"],"x86_64":["libfoo.so"]})

    def test_bundling_copies_strips_and_relinks_every_library(self):
        with tempfile.TemporaryDirectory() as folder:
            # Arrange: a fake tree and system folder of ELF64 files; ldd, strip and patchelf stood in for.
            root=Path(folder);build=root/"build";system=root/"system";destination=root/"AppDir"
            elf=b"\x7fELF\x02"+bytes(15)
            for path in (build/"IronSights",build/"sentry-crash",build/"lib/libsentry.so",build/"lib/libSDL2-2.0.so.0",
                         system/"libcurl.so.4",system/"libasound.so.2",system/"libpulse.so.0",system/"libfoo.so.1"):
                path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(elf)
            target=linux_packager.TARGETS["x86_64"]._replace(system_libdir=str(system))
            ldd=f"\tlibfoo.so.1 => {system}/libfoo.so.1 (0x1)\n\tlibc.so.6 => /lib/libc.so.6 (0x2)\n"
            with mock.patch.object(linux_packager.subprocess,"check_output",return_value=ldd), \
                 mock.patch.object(linux_packager.subprocess,"run") as run:
                # Act
                paths=linux_packager.bundle_libraries(build,destination,target)
            # Assert
            names=["libsentry.so","libSDL2-2.0.so.0","libcurl.so.4","libasound.so.2","libpulse.so.0","libfoo.so.1"]
            self.assertEqual(sorted(paths),sorted(names))
            self.assertEqual(sorted(p.name for p in (destination/"lib").iterdir()),sorted(names))
            self.assertEqual(run.call_count,2*len(names))

    @unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("gcc"),"needs a C compiler that writes ELF")
    def test_build_inputs_record_whether_the_play_harness_is_compiled(self):
        # Both ELF classes: the i686 package reads a 32-bit executable's symbols, the x86_64 one a 64-bit one's.
        for width in ("-m32","-m64"):
            with self.subTest(width=width),tempfile.TemporaryDirectory() as folder:
                # Arrange
                root=Path(folder);source=root/"probe.c"
                source.write_text("void portPlayPlayerTick(void) {}\nint main(void) {return 0;}\n")
                subprocess.run(["gcc",width,str(source),"-o",str(root/"with")],check=True)
                source.write_text("int main(void) {return 0;}\n")
                subprocess.run(["gcc",width,str(source),"-o",str(root/"without")],check=True)
                # Act, Assert
                self.assertEqual(linux_packager.has_symbol(root/"with","portPlayPlayerTick"),True)
                self.assertEqual(linux_packager.has_symbol(root/"without","portPlayPlayerTick"),False)

    @unittest.skipUnless(shutil.which("gcc"),"Native cache codec test needs a C compiler")
    def test_native_cache_validation(self):
        with tempfile.TemporaryDirectory() as folder:
            binary=Path(folder)/"cache-test"
            subprocess.run(["gcc","-std=c11","-O2","-I"+str(ROOT/"port/include"),str(HERE/"test_icon_cache.c"),"-o",str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

    @unittest.skipUnless(shutil.which("gcc"),"Inline model test needs a C compiler")
    def test_runtime_inline_model_texels(self):
        with tempfile.TemporaryDirectory() as folder:
            binary=Path(folder)/"inline-test"
            flags=["-m32","-std=gnu11","-fms-extensions","-w","-DVERSION_US","-DLANG_US","-DREFRESH_NTSC","-D_LANGUAGE_C"]
            includes=[".","include","include/ultra64","include/PR","src","src/game","port/include"]
            subprocess.run(["gcc",*flags,*["-I"+str(ROOT/p) for p in includes],str(HERE/"test_inline_models.c"),"-o",str(binary)],check=True)
            subprocess.run([str(binary)],check=True)


if __name__ == "__main__":
    unittest.main()
