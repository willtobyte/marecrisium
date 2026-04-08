from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class CarimboFuzz(ConanFile):
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires("physfs/3.2.0")
        self.requires("zstd/1.5.7")
        self.requires("unordered_dense/4.8.1")

    def generate(self):
        CMakeToolchain(self).generate()
        CMakeDeps(self).generate()
