from conan import ConanFile
from conan.tools.scm import Version
from conan.tools.files import get, chdir, replace_in_file, copy, rmdir
from conan.tools.microsoft import is_msvc, MSBuildToolchain, VCVars, unix_path
from conan.tools.layout import basic_layout
from conan.tools.gnu import Autotools, AutotoolsToolchain
from conan.tools.apple import is_apple_os
import os


required_conan_version = ">=2.0"


class LuajitConan(ConanFile):
    name = "luajit"
    license = "MIT"
    url = "https://github.com/willtobyte/marecrisium"
    homepage = "https://luajit.org"
    description = (
        "LuaJIT v2.1 rolling release packaged for marecrisium. "
        "Built with LUAJIT_UNWIND_EXTERNAL so C++ exceptions can propagate "
        "through LuaJIT VM frames (required on macOS arm64)."
    )
    topics = ("lua", "jit")
    provides = "lua"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def layout(self):
        basic_layout(self, src_folder="src")

    def source(self):
        get(
            self,
            **self.conan_data["sources"][self.version],
            destination=self.source_folder,
            strip_root=True,
        )

    def generate(self):
        if is_msvc(self):
            MSBuildToolchain(self).generate()
            VCVars(self).generate()
        else:
            tc = AutotoolsToolchain(self)
            # Belt-and-suspenders: v2.1 already auto-defines this for Darwin,
            # but force it everywhere so non-Darwin targets also get C++
            # exception interop.
            tc.extra_defines.append("LUAJIT_UNWIND_EXTERNAL")
            tc.generate()

    def _patch_sources(self):
        if is_msvc(self):
            return
        makefile = os.path.join(self.source_folder, "src", "Makefile")
        replace_in_file(self, makefile, "BUILDMODE= mixed", "BUILDMODE= static")
        replace_in_file(
            self,
            makefile,
            "TARGET_DYLIBPATH= $(TARGET_LIBPATH)/$(TARGET_DYLIBNAME)",
            "TARGET_DYLIBPATH= $(TARGET_DYLIBNAME)",
        )
        replace_in_file(
            self,
            makefile,
            "TARGET_T= $(LUAJIT_T) $(LUAJIT_SO)",
            "TARGET_T= $(LUAJIT_T) $(LUAJIT_A)",
        )
        replace_in_file(
            self,
            makefile,
            "TARGET_DEP= $(LIB_VMDEF) $(LUAJIT_SO)",
            "TARGET_DEP= $(LIB_VMDEF) $(LUAJIT_A)",
        )
        if "clang" in str(self.settings.compiler):
            replace_in_file(self, makefile, "CC= $(DEFAULT_CC)", "CC= clang")

    @property
    def _macosx_deployment_target(self):
        # v2.1 Makefile aborts if this isn't set on Apple targets. Fall back to
        # a sane default when the profile doesn't pin os.version.
        version = self.settings.get_safe("os.version")
        if version:
            return version
        if is_apple_os(self):
            return "11.0"
        return None

    @property
    def _make_arguments(self):
        args = [f"PREFIX={unix_path(self, self.package_folder)}"]
        if is_apple_os(self) and self._macosx_deployment_target:
            args.append(f"MACOSX_DEPLOYMENT_TARGET={self._macosx_deployment_target}")
        return args

    @property
    def _luajit_include_folder(self):
        return f"luajit-{Version(self.version).major}.{Version(self.version).minor}"

    def build(self):
        self._patch_sources()
        if is_msvc(self):
            with chdir(self, os.path.join(self.source_folder, "src")):
                self.run("msvcbuild.bat static", env="conanbuild")
        else:
            with chdir(self, self.source_folder):
                Autotools(self).make(args=self._make_arguments)

    def package(self):
        copy(
            self,
            "COPYRIGHT",
            dst=os.path.join(self.package_folder, "licenses"),
            src=self.source_folder,
        )
        src_folder = os.path.join(self.source_folder, "src")
        include_folder = os.path.join(
            self.package_folder, "include", self._luajit_include_folder
        )
        if is_msvc(self):
            for header in (
                "lua.h",
                "lualib.h",
                "lauxlib.h",
                "luaconf.h",
                "lua.hpp",
                "luajit.h",
            ):
                copy(self, header, src=src_folder, dst=include_folder)
            copy(
                self,
                "lua51.lib",
                src=src_folder,
                dst=os.path.join(self.package_folder, "lib"),
            )
        else:
            with chdir(self, self.source_folder):
                Autotools(self).install(args=self._make_arguments + ["DESTDIR="])
            rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
            rmdir(self, os.path.join(self.package_folder, "lib", "lua"))
            rmdir(self, os.path.join(self.package_folder, "share"))
            rmdir(self, os.path.join(self.package_folder, "bin"))

    def package_info(self):
        self.cpp_info.libs = ["lua51" if is_msvc(self) else "luajit-5.1"]
        self.cpp_info.set_property("pkg_config_name", "luajit")
        self.cpp_info.includedirs = [
            os.path.join("include", self._luajit_include_folder)
        ]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.extend(["m", "dl"])
