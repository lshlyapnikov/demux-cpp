# pylint: disable=missing-module-docstring,missing-class-docstring,missing-function-docstring,line-too-long
import os
from conan import ConanFile
from conan.tools.files import copy

class DemuxCppRecipe(ConanFile):
    # Project details
    name = "demux-cpp"
    version = "0.5.1"

    # Optional metadata
    license = "Apache-2.0"
    homepage = "https://github.com/lshlyapnikov/demux-cpp"
    description = "C++ Lock-free Demultiplexer Queue"
    topics = ("libraries", "cpp")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    package_type = "static-library"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*"

    def requirements(self):
        self.requires("boost/1.83.0")
        self.requires("xxhash/0.8.2")
        self.requires("hdrhistogram-c/0.11.8")
        self.requires("gtest/1.15.0")
        self.requires("rapidcheck/cci.20230815")

    def configure(self):
        self.options["boost"].shared = False

    def layout(self):
        # Uncomment next line to separate Release and Debug builds. Keeping 1 build at-a-time for simplicity.
        #cmake_layout(self)
        pass

    # def generate(self):
    #     deps = CMakeDeps(self)
    #     deps.generate()
    #     tc = CMakeToolchain(self)
    #     tc.generate()

    # def build(self):
    #     cmake = CMake(self)
    #     cmake.configure()
    #     cmake.build()

    # def package(self):
    #     cmake = CMake(self)
    #     cmake.install()

    # def source(self):
    #     # this copy does not work, try to copy demux, example and util folders
    #     copy(self, "*.h", "src/demux", "include/demux")
    #     copy(self, "*.h", "src/util", "include/util")

    def package(self):
        # Create the include directory in the package folder
        for package_dir in ["demux", "example", "util"]:
            copy(self, "*.h",
                src=os.path.join(self.source_folder, f"src/{package_dir}"),
                dst=os.path.join(self.package_folder, f"include/{package_dir}")
            )

    def package_info(self):
        self.cpp_info.includedirs = ["include"]
