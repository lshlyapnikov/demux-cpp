# cspell:disable
# pylint: disable=missing-module-docstring,missing-class-docstring,missing-function-docstring,line-too-long
import os
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, CMakeDeps, cmake_layout
from conan.tools.files import copy

class DemuxCppRecipe(ConanFile):
    name = "demux-cpp"
    version = "0.6.2-dev"
    package_type = "library"

    # Optional metadata
    license = "Apache-2.0"
    url = "https://github.com/lshlyapnikov/demux-cpp"
    homepage = "https://github.com/lshlyapnikov/demux-cpp"
    description = "C++ Lock-free Demultiplexer Queue"
    topics = ("libraries", "cpp")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "test/*"

    def requirements(self):
        if self.settings.build_type == "RelWithDebInfo": # pylint: disable=no-member
            self.requires("gperftools/2.17.2")
        self.requires("boost/1.88.0")
        self.requires("xxhash/0.8.3")
        self.requires("hdrhistogram-c/0.11.8")
        self.requires("gtest/1.17.0")
        self.requires("rapidcheck/cci.20231215")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

        if self.settings.build_type == "RelWithDebInfo": # pylint: disable=no-member
            self.options["gperftools"].build_cpu_profiler = True
            self.options["gperftools"].build_heap_profiler = True

        boost_options = self.options["boost"]
        # see https://github.com/conan-io/conan-center-index/blob/master/recipes/boost/all/conanfile.py
        boost_options.without_contract = True
        boost_options.without_date_time = False # required for log
        boost_options.without_exception = False # required for log
        boost_options.without_graph = True
        boost_options.without_graph_parallel = True
        boost_options.without_headers = True
        boost_options.without_log = False # enable log
        boost_options.without_nowide = True
        boost_options.without_program_options = True
        boost_options.without_regex = False # required for log
        boost_options.without_serialization = True
        boost_options.without_test = True
        boost_options.without_thread = False # required for log
        boost_options.without_type_erasure = True
        boost_options.without_yap = True
        boost_options.without_winapi = True
        boost_options.without_wave = True
        boost_options.without_variant2 = True
        boost_options.without_variant = True
        boost_options.without_uuid = True
        boost_options.without_url = True
        boost_options.without_unordered = True
        boost_options.without_type_index = True
        boost_options.without_tuple = True
        boost_options.without_tokenizer = True
        boost_options.without_timer = True
        boost_options.without_throw_exception = True
        boost_options.without_system = False # required for log
        boost_options.without_stl_interfaces = True
        boost_options.without_stacktrace = False
        boost_options.without_spirit = True
        boost_options.without_sort = True
        boost_options.without_smart_ptr = True
        boost_options.without_signals2 = True
        boost_options.without_scope = True
        boost_options.without_redis = True
        boost_options.without_rational = True
        boost_options.without_ratio = True
        boost_options.without_random = False # required for log
        boost_options.without_python = True
        boost_options.without_ptr_container = True
        boost_options.without_property_map = True
        boost_options.without_process = True
        boost_options.without_predef = True
        boost_options.without_pool = True
        boost_options.without_poly_collection = True
        boost_options.without_pfr = True
        boost_options.without_parameter = True
        boost_options.without_outcome = True
        boost_options.without_optional = True
        boost_options.without_mysql = True
        boost_options.without_multi_index = True
        boost_options.without_msm = True
        boost_options.without_mqtt5 = True
        boost_options.without_mpl = True
        boost_options.without_mpi = True
        boost_options.without_mp11 = True
        boost_options.without_move = True
        boost_options.without_math = True
        boost_options.without_logic = True
        boost_options.without_lockfree = True
        boost_options.without_locale = True
        boost_options.without_lexical_cast = True
        boost_options.without_lambda2 = True
        boost_options.without_json = True
        boost_options.without_iterator = True
        boost_options.without_iostreams = True
        boost_options.without_intrusive = True
        boost_options.without_interprocess = True
        boost_options.without_integer = True
        boost_options.without_heap = True
        boost_options.without_hana = True
        boost_options.without_geometry = True
        boost_options.without_function_types = True
        boost_options.without_function = True
        boost_options.without_format = True
        boost_options.without_flyweight = True
        boost_options.without_filesystem = False # required for log
        boost_options.without_fiber = True
        boost_options.without_endian = True
        boost_options.without_dynamic_bitset = True
        boost_options.without_dll = True
        boost_options.without_detail = True
        boost_options.without_describe = True
        boost_options.without_crc = True
        boost_options.without_coroutine2 = True
        boost_options.without_coroutine = True
        boost_options.without_core = True
        boost_options.without_conversion = True
        boost_options.without_context = True
        boost_options.without_container_hash = True
        boost_options.without_container = False # required for thread
        boost_options.without_concept_check = True
        boost_options.without_compat = True
        boost_options.without_cobalt = True
        boost_options.without_chrono = False # required for thread
        boost_options.without_charconv = True
        boost_options.without_bind = True
        boost_options.without_bimap = True
        boost_options.without_beast = True
        boost_options.without_atomic = False # required for log
        boost_options.without_assign = True
        boost_options.without_assert = True
        boost_options.without_asio = True
        boost_options.without_array = True
        boost_options.without_any = True

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        # Create the include directory in the package folder
        for package_dir in ["demux/core", "demux/example", "demux/util", "demux/test"]:
            copy(self, "*.h",
                src=os.path.join(self.source_folder, f"src/{package_dir}"),
                dst=os.path.join(self.package_folder, f"include/{package_dir}")
            )
        # Create the lib directory in the package folder
        copy(self, pattern="*.a",
             src=self.build_folder,
             dst=os.path.join(self.package_folder, "lib"),
             keep_path=False
        )

    def package_info(self):
        self.cpp_info.libs = ["demux-cpp"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.includedirs = ["include"]
