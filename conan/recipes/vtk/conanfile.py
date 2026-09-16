import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import get, save

# --- VTK 9.5.0 sources ------------------------------------------------------
#
# Upstream publishes this archive at
#     https://www.vtk.org/files/release/9.5/VTK-9.5.0.tar.gz
# but neither that host nor gitlab.kitware.com (the same tag as a GitLab
# archive) is reachable from this network: a direct connection times out, and
# through the VPN the TLS handshake never completes -- measured with curl
# (schannel) and with Python's OpenSSL stack.
#
# The GitHub tag archive used below is a complete source tree, not a reduced
# one: VTK carries no submodules at v9.5.0 (there is no .gitmodules) and its
# .gitattributes marks only .git* and .hooks* as export-ignore, so nothing the
# build touches is missing.  Measured: 27056 entries, top directory VTK-9.5.0/.
#
# A checksum is required, not decorative.  SourcesCachingDownloader disables
# core.sources:download_cache entirely when a recipe calls download() without
# one ("Cannot cache download() without sha256 checksum"), so without the pin
# every build -- including an otherwise fully offline one -- would go to the
# network for this tarball.
#
# All URLs must serve the same bytes; that is what makes the mirror list safe
# and what the checksum verifies (the Chinese GitHub gateways were checked
# byte-for-byte against each other on a small file: identical).  Do NOT add the
# official vtk.org tarball to this list -- it is a different byte stream, so it
# cannot share this checksum.  A machine that can reach vtk.org should swap the
# list and recompute the hash instead.
VTK_SHA256 = "3d311ff2608e971d40222ae01016d404fb07d746292f77edd86786912767a9c1"
VTK_URLS = [
    # Reachable from this network (direct, ~940 KB/s measured for the 50 MB
    # archive).  ghfast.top does not implement Range, which is only relevant to
    # scripts/prefetch-sources.sh, not to Conan.
    "https://ghfast.top/https://github.com/Kitware/VTK/archive/refs/tags/v9.5.0.tar.gz",
    "https://gh-proxy.com/https://github.com/Kitware/VTK/archive/refs/tags/v9.5.0.tar.gz",
    # Reachable from a normal corporate/university network; both time out here.
    "https://github.com/Kitware/VTK/archive/refs/tags/v9.5.0.tar.gz",
    "https://codeload.github.com/Kitware/VTK/tar.gz/refs/tags/v9.5.0",
]


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
            url=VTK_URLS,
            sha256=VTK_SHA256,
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
