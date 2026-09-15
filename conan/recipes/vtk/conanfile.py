import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import get, save


class VtkConan(ConanFile):
    name = "vtk"
    version = "9.5.0"
    package_type = "shared-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"shared": [True]}
    default_options = {"shared": True}

    def requirements(self):
        # VTK's Qt GUI modules must use the same shared Qt ABI as Shrimp.
        self.requires("qt/6.8.3", options={"shared": True})

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self,
            url="https://www.vtk.org/files/release/9.5/VTK-9.5.0.tar.gz",
            strip_root=True)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        # VTK selects Qt6 explicitly, but asks it for a historical 5.9 minimum.
        # CMakeDeps rejects that request on the major version alone. The recipe
        # itself fixes Qt at 6.8.3, so accept the version probe in this context.
        save(self, os.path.join(self.generators_folder, "Qt6ConfigVersion.cmake"),
             'set(PACKAGE_VERSION "6.8.3")\n'
             'set(PACKAGE_VERSION_COMPATIBLE TRUE)\n'
             'set(PACKAGE_VERSION_EXACT FALSE)\n')

        toolchain = CMakeToolchain(self)
        toolchain.variables["BUILD_SHARED_LIBS"] = True
        toolchain.variables["VTK_BUILD_TESTING"] = False
        toolchain.variables["VTK_BUILD_EXAMPLES"] = False
        toolchain.variables["VTK_BUILD_DOCUMENTATION"] = False
        toolchain.variables["VTK_BUILD_WRAPPING"] = False
        toolchain.variables["VTK_GROUP_ENABLE_Qt"] = "NO"
        toolchain.variables["VTK_QT_VERSION"] = "6"
        toolchain.variables["VTK_MODULE_ENABLE_VTK_GUISupportQt"] = "YES"
        toolchain.variables["VTK_MODULE_ENABLE_VTK_RenderingQt"] = "YES"
        toolchain.variables["VTK_MODULE_ENABLE_VTK_InteractionWidgets"] = "YES"
        toolchain.variables["VTK_MODULE_ENABLE_VTK_GUISupportQtQuick"] = "NO"
        toolchain.variables["VTK_MODULE_ENABLE_VTK_RenderingOpenGL2"] = "YES"
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.builddirs = ["lib/cmake/vtk-9.5"]
        self.cpp_info.set_property("cmake_find_mode", "none")
