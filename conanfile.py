# pylint: disable=missing-module-docstring,missing-class-docstring,missing-function-docstring
from conan import ConanFile


class DemuxCppRecipe(ConanFile):
    # project details
    name = "demux-cpp"
    version = "0.5.0"
    license = "Apache-2.0"
    url = "https://github.com/lshlyapnikov/demux-cpp"
    # settings
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

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
