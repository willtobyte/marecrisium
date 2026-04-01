from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class CarimboTools(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires("zstd/1.5.7")

    def generate(self):
        CMakeToolchain(self).generate()
        CMakeDeps(self).generate()
