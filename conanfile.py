from pathlib import Path
from typing import Any, cast

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class Game(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self) -> None:
        recipe = cast(Any, self)
        for package in (
            "miniaudio/0.11.25",
            "mimalloc/3.5.1",
            "sdl/3.4.14",
            "sentry-native/0.14.2",
            "simde/0.8.4-rc3",
            "stb/cci.20240531",
            "sqlite3/3.53.4",
            "luajit/2.1-20260908",
            "yyjson/0.12.0",
            "zstd/1.5.7",
        ):
            recipe.requires(package)

    def configure(self) -> None:
        recipe = cast(Any, self)
        options = recipe.options
        options["miniaudio"].header_only = True

        options["sentry-native"].backend = "inproc"
        options["sentry-native"].shared = False

        options["mimalloc"].shared = False
        options["mimalloc"].secure = False
        options["mimalloc"].override = True
        options["mimalloc"].single_object = recipe.settings.os != "Windows"

    def generate(self) -> None:
        recipe = cast(Any, self)
        destination = Path(recipe.build_folder) / "LICENSES"
        with destination.open("w", encoding="utf-8") as output:
            for dependency in recipe.dependencies.values():
                if dependency.is_build_context or not dependency.package_folder:
                    continue

                reference = f"{dependency.ref.name}/{dependency.ref.version}"
                seen: set[str] = set()
                for f in Path(dependency.package_folder).rglob("*"):
                    if not f.is_file():
                        continue

                    name = f.name.lower()
                    if not name.startswith(("license", "copying", "copyright")):
                        continue

                    text = f.read_text("utf-8", errors="ignore").strip()
                    if text in seen:
                        continue

                    seen.add(text)
                    output.write(f"{reference}\n{text}\n\n")

            fonts = Path(recipe.recipe_folder) / "assets" / "fonts"
            for f in sorted(fonts.rglob("LICENSE")):
                if not f.is_file():
                    continue

                reference = f"fonts/{f.parent.relative_to(fonts).as_posix()}"
                text = f.read_text("utf-8", errors="ignore").strip()
                output.write(f"{reference}\n{text}\n\n")

        toolchain = CMakeToolchain(self)
        for definition in (
            "STBI_NO_LINEAR",
            "STBI_NO_STDIO",
            "STBI_ONLY_PNG",
            "STB_VORBIS_NO_INTEGER_CONVERSION",
            "STB_VORBIS_NO_PUSHDATA_API",
            "STB_VORBIS_NO_STDIO",
        ):
            toolchain.preprocessor_definitions[definition] = None
        toolchain.generate()
        CMakeDeps(self).generate()
