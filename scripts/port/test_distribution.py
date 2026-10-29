"""ROM-free tests for build identity, ZIP contents and native cache validation."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]


def module(name):
    spec=importlib.util.spec_from_file_location(name,HERE/(name+".py"))
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result


build_info=module("build_info")
packager=module("package_windows")


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
            textures=build/"Data/TexturePacks";textures.mkdir(parents=True)
            (textures/"hd.istex").write_bytes(b"ISTEX001"+bytes((0,0,0,0,16,0,0,0)))
            for name in ("hd.json","HD-Textures-NOTICE.txt","puff-LICENSE.txt","unrelated.zip"):
                (textures/name).write_text("fixture")
            license=root/"SDL-LICENSE";license.write_text("license fixture")
            path=packager.package(build,out,sdl_license=license)
            with zipfile.ZipFile(path) as archive:
                names=archive.namelist()
                self.assertIn("SDL2.dll",names);self.assertIn("Data/README.txt",names)
                self.assertIn("IronSights.exe",names);self.assertNotIn("ProjectMidas.exe",names)
                self.assertIn("build-info.json",names);self.assertIn("SHA256SUMS.txt",names)
                self.assertIn("Enhanced-Fonts-LICENSE.txt",names);self.assertIn("Enhanced-Fonts-NOTICE.txt",names)
                self.assertIn("Data/TexturePacks/hd.istex",names)
                self.assertIn("Data/TexturePacks/HD-Textures-NOTICE.txt",names)
                self.assertNotIn("Data/TexturePacks/unrelated.zip",names)
                self.assertFalse(any(n.endswith((".z64",".bin",".geprofile")) for n in names))
                self.assertNotIn("midas.ini",names);self.assertNotIn("previous.exe",names)
            previous=path.read_bytes();packager.package(build,out,sdl_license=license)
            self.assertEqual(previous,path.read_bytes())

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
