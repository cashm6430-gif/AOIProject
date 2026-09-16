from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class AOIProjectConan(ConanFile):
    name = "aoi-project"
    version = "0.1.0"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"
    options = {"with_shrimp": [True, False]}
    default_options = {
        "with_shrimp": False,
        # One host profile fixes the compiler, CRT, architecture and build type.
        # Qt is an unconditional require below, so this always takes effect.
        "qt/*:shared": True,
        "opencv/*:shared": True,
        # TaskControl only uses matrix, image processing, codecs and camera
        # calibration.  Disable unrelated OpenCV subsystems and their large
        # video/FFmpeg dependency graph.
        "opencv/*:calib3d": True,
        "opencv/*:dnn": False,
        "opencv/*:features2d": True,
        "opencv/*:flann": True,
        "opencv/*:gapi": False,
        "opencv/*:highgui": False,
        "opencv/*:imgcodecs": True,
        "opencv/*:imgproc": True,
        "opencv/*:ml": False,
        "opencv/*:objdetect": False,
        "opencv/*:photo": False,
        "opencv/*:stitching": False,
        "opencv/*:video": False,
        "opencv/*:videoio": False,
        # AOIProject does not ingest WebP.  Disabling it also avoids the
        # SharpYuv static-link edge in the current WebP package.
        "opencv/*:with_webp": False,
        "pcl/*:shared": True,
        # Shrimp renders through the private VTK package below.  Keep PCL as
        # a data-processing dependency so it cannot introduce another VTK ABI.
        "pcl/*:with_vtk": False,
        "pcl/*:visualization": False,
        # Shrimp loads PCD/PLY itself and converts it to VTK.  PCL therefore
        # needs only common, octree and io; turn off its optional SDKs too.
        "pcl/*:2d": False,
        "pcl/*:features": False,
        "pcl/*:filters": False,
        "pcl/*:geometry": False,
        "pcl/*:io": True,
        "pcl/*:kdtree": False,
        "pcl/*:keypoints": False,
        "pcl/*:ml": False,
        "pcl/*:octree": True,
        "pcl/*:outofcore": False,
        "pcl/*:people": False,
        "pcl/*:recognition": False,
        "pcl/*:registration": False,
        "pcl/*:sample_consensus": False,
        "pcl/*:search": False,
        "pcl/*:segmentation": False,
        "pcl/*:simulation": False,
        "pcl/*:stereo": False,
        "pcl/*:surface": False,
        "pcl/*:tracking": False,
        "pcl/*:with_flann": False,
        "pcl/*:with_libusb": False,
        "pcl/*:with_opencv": False,
        "pcl/*:with_opengl": False,
        "pcl/*:with_pcap": False,
        "pcl/*:with_png": False,
        "pcl/*:with_qhull": False,
        "pcl/*:with_qt": False,
    }

    def requirements(self):
        self.requires("eigen/3.4.0")
        self.requires("fmt/11.1.3")
        self.requires("opencv/4.10.0")
        # 1.15.1 is not published in ConanCenter. 1.14.1 is the newest
        # available recipe and is used independently of VTK.
        self.requires("pcl/1.14.1")
        self.requires("spdlog/1.15.1")
        self.requires("taskflow/3.8.0")

        # Qt is NOT optional: TaskControl is not headless.  53 of its 66 source
        # and header files use Qt Core types -- the runtime context stores
        # QVariant/QHash behind QReadWriteLock (core/AlgorithmContext.*), recipes
        # are parsed with QJsonArray (core/AlgorithmIO.h), the plugin loader is
        # QPluginLoader -- and TaskControl/CMakeLists.txt line 1 is
        #     find_package(Qt6 REQUIRED COMPONENTS Core Gui)
        # Runtime links AOI::TaskControl PUBLIC and both test executables link it
        # too, so no target in this repository can be configured without Qt.
        #
        # Do not "optimise" this back into an option.  A with_qt switch was tried
        # and it cannot be off: with_qt=False resolves a Qt-less graph fine and
        # then dies at TaskControl/CMakeLists.txt:1 with
        #     Could not find a package configuration file provided by "Qt6"
        # because the graph was never the thing that wanted Qt -- the code is.
        # The only genuinely optional dependency here is the private VTK package
        # below, which is what with_shrimp drives.
        self.requires("qt/6.8.3")

        if self.options.with_shrimp:
            # ConanCenter has no VTK recipe. These private references are
            # produced by the package wrappers in conan/recipes.
            self.requires("vtk/9.5.0@aoi/stable")

    def build_requirements(self):
        # Core regression tests use Catch2 v3 and are opt-in from CMake.
        self.test_requires("catch2/3.7.1")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()





