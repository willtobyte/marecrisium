from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain
from pathlib import Path


class Game(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        for package in [
            "box2d/3.1.1",
            "entt/3.16.0",
            "miniaudio/0.11.22",
            "mimalloc/3.3.2",
            "physfs/3.2.0",
            "libspng/0.7.4",
            "sdl/3.4.8",
            "sqlite3/3.53.2",
            "luajit/2.1-20260616",
            "opusfile/0.12",
            "unordered_dense/4.8.1",
            "yyjson/0.12.0",
            "zstd/1.5.7",
        ]:
            self.requires(package)

    def configure(self):
        self.options["miniaudio"].header_only = True

        self.options["mimalloc"].shared = False
        self.options["mimalloc"].secure = False
        self.options["mimalloc"].override = True
        self.options["mimalloc"].single_object = self.settings.os != "Windows"

        self.options["opusfile"].http = False

    def generate(self):
        license_output = Path(self.build_folder) / "LICENSES"
        with license_output.open("w", encoding="utf-8") as out:
            for dep in self.dependencies.values():
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

        toolchain.generate()
        CMakeDeps(self).generate()
