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
        self.requires("qt/6.8.3")
        self.requires("spdlog/1.15.1")
        self.requires("taskflow/3.8.0")

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





