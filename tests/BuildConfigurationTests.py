"""Structural regression checks for the VS2022 build graph; Python 3 stdlib only."""
import re
import unittest
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
GUI = "{AD716A36-81C8-4B22-95F6-38DB353C3001}"

class BuildConfigurationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sln = (ROOT / "PatchCode-GuiClient-Windows.sln").read_text(encoding="utf-8-sig")
        cls.project = ET.parse(ROOT / "PatchCode-GuiClient-Windows.csproj").getroot()
        cls.groups = {re.search(r"==\s*'([^']+)'", g.get("Condition", "")).group(1): g
                      for g in cls.project.findall("m:PropertyGroup", NS)
                      if re.search(r"==\s*'([^']+)'", g.get("Condition", ""))}

    def test_only_gui_solution_is_exposed(self):
        self.assertEqual([p.relative_to(ROOT).as_posix() for p in ROOT.rglob("*.sln")],
                         ["PatchCode-GuiClient-Windows.sln"])

    def test_gui_is_registered_and_enabled_in_all_four_configurations(self):
        self.assertIn('"PatchCode-GuiClient-Windows.csproj", "' + GUI + '"', self.sln)
        for config in ("Debug", "Release"):
            for platform in ("x64", "x86"):
                key = config + "|" + platform
                for mapping in ("ActiveCfg", "Build.0"):
                    self.assertIn(GUI + "." + key + "." + mapping + " = " + key, self.sln)

    def test_explicit_project_configurations(self):
        self.assertEqual(set(self.groups), {c + "|" + a for c in ("Debug", "Release") for a in ("x64", "x86")})
        for key, group in self.groups.items():
            self.assertEqual(group.find("m:PlatformTarget", NS).text, key.split("|")[1])
            self.assertTrue(group.find("m:OutputPath", NS).text)

    def test_intermediates_are_isolated(self):
        values = [g.find("m:IntermediateOutputPath", NS).text for g in self.groups.values()]
        self.assertEqual(len(values), len(set(values)))

    def test_native_project_references(self):
        refs = self.project.findall("m:ItemGroup/m:ProjectReference", NS)
        self.assertEqual(len(refs), 2)
        for ref in refs:
            self.assertTrue((ROOT / ref.get("Include").replace("\\", "/")).is_file())
            self.assertEqual(ref.find("m:ReferenceOutputAssembly", NS).text, "false")
            self.assertEqual(ref.find("m:SetPlatform", NS).text, "Platform=$(NativePlatform)")
        for key, group in self.groups.items():
            self.assertEqual(group.find("m:NativePlatform", NS).text, "Win32" if key.endswith("x86") else "x64")

    def test_common_props_and_csharp_targets_are_imported(self):
        imports = [n.get("Project") for n in self.project.findall("m:Import", NS)]
        self.assertTrue(any("Microsoft.Common.props" in p for p in imports))
        self.assertTrue(any("Microsoft.CSharp.targets" in p for p in imports))

    def test_sources_exist_and_framework_is_preserved(self):
        for n in self.project.findall("m:ItemGroup/m:Compile", NS):
            self.assertTrue((ROOT / n.get("Include").replace("\\", "/")).is_file())
        self.assertEqual(self.project.find("m:PropertyGroup/m:TargetFrameworkVersion", NS).text, "v4.8")
        self.assertEqual(self.project.find("m:PropertyGroup/m:StartupObject", NS).text, "PatchCodeGui.Program")

    def test_native_outputs_are_independent_of_solution_directory(self):
        for name in ("patchcode", "diffcode"):
            tree = ET.parse(ROOT / "third_party/patchcode/vs2022" / (name + ".vcxproj"))
            outputs = tree.findall(".//m:OutDir", NS)
            self.assertEqual(len(outputs), 4)
            for output in outputs:
                self.assertIn("$(MSBuildThisFileDirectory)", output.text)
                self.assertNotIn("$(SolutionDir)", output.text)

    def test_native_staging_checks_missing_files(self):
        target = self.project.find("m:Target[@Name='CopyNativeTools']", NS)
        self.assertEqual(target.get("AfterTargets"), "Build")
        self.assertIn("StageNativeTools", target.get("Condition"))
        self.assertIsNotNone(target.find("m:Error", NS))
        self.assertEqual(len(target.findall("m:Copy", NS)), 2)

    def test_native_solution_mappings(self):
        for c in ("Debug", "Release"):
            for a, native in (("x64", "x64"), ("x86", "Win32")):
                lines = [line for line in self.sln.splitlines() if "." + c + "|" + a + ".Build.0" in line and GUI not in line]
                self.assertEqual(len(lines), 2)
                for line in lines:
                    self.assertTrue(line.endswith(" = " + c + "|" + native))

if __name__ == "__main__":
    unittest.main(verbosity=2)
