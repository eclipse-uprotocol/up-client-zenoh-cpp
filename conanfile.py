from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import apply_conandata_patches, copy, export_conandata_patches, get, replace_in_file, rm, rmdir
import os

class UpClientZenoh(ConanFile):
    name = "up-client-zenoh-cpp"
    package_type = "library"
    license = "Apache-2.0 license"
    homepage = "https://github.com/eclipse-uprotocol"
    url = "https://github.com/conan-io/conan-center-index"
    description = "C++ uLink Library for zenoh transport"
    topics = ("ulink client", "transport")
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    conan_version = None
    generators = "CMakeDeps"
    version = "0.1.2-dev"
    exports_sources = "CMakeLists.txt", "lib/*", "test/*"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_testing": [True, False],
        "build_unbundled": [True, False],
        "zenoh_package": [True, False],
        "build_cross_compiling": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": False,
        "build_testing": False,
        "build_unbundled": True,
        "zenoh_package": False,
        "build_cross_compiling": False,
    }

    def requirements(self):
        self.requires("protobuf/3.21.12" + ("@cross/cross" if self.options.build_cross_compiling else ""))
        self.requires("spdlog/1.13.0")
        if self.options.build_testing:
            self.requires("gtest/1.14.0")
        if self.options.build_unbundled: #each componenet is built independently 
            self.requires("up-cpp/0.1.1-dev")
            if self.options.zenoh_package:
                self.requires("zenohc/cci.20240213")
        if self.options.build_cross_compiling :
            self.requires("zenohc/cci.20240213")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = self.options.build_testing
        tc.variables["CROSS_COMPILE"] = self.options.build_cross_compiling
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["up-client-zenoh-cpp"]

