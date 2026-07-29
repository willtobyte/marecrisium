from pathlib import Path
from typing import Any, cast

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class Game(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        api = cast(Any, self)
        for package in [
            "box2d/3.1.1",
            "entt/3.16.0",
            "miniaudio/0.11.22",
            "mimalloc/3.3.2",
            "sdl/3.4.8",
            "stb/cci.20240531",
            "sqlite3/3.53.3",
            "luajit/2.1-20260720",
            "opusfile/0.12",
            "yyjson/0.12.0",
            "zstd/1.5.7",
        ]:
            api.requires(package)

    def configure(self):
        api = cast(Any, self)
        api.options["miniaudio"].header_only = True

        api.options["mimalloc"].shared = False
        api.options["mimalloc"].secure = False
        api.options["mimalloc"].override = True
        api.options["mimalloc"].single_object = api.settings.os != "Windows"

        api.options["opusfile"].http = False

    def generate(self):
        api = cast(Any, self)
        license_output = Path(api.build_folder) / "LICENSES"
        with license_output.open("w", encoding="utf-8") as out:
            for dep in api.dependencies.values():
                if dep.is_build_context or not dep.package_folder:
                    continue

                pid = f"{dep.ref.name}/{dep.ref.version}"
                for file in Path(dep.package_folder).rglob("*"):
                    if not file.is_file():
                        continue

                    name = file.name.lower()
                    if name.startswith(("license", "copying", "copyright")):
                        text = file.read_text(encoding="utf-8", errors="ignore").strip()
                        out.write(f"{pid}\n{text}\n\n")

        toolchain = CMakeToolchain(self)
        for definition in [
            "STBI_NO_FAILURE_STRINGS",
            "STBI_NO_HDR",
            "STBI_NO_LINEAR",
            "STBI_NO_STDIO",
            "STBI_ONLY_PNG",
        ]:
            toolchain.preprocessor_definitions[definition] = None
        toolchain.generate()
        CMakeDeps(self).generate()
