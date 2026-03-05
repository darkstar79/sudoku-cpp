from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class SudokuConan(ConanFile):
    name = "sudoku"
    version = "1.0.0"
    
    # Package metadata
    license = "GPL-3.0-or-later"
    description = "Offline Sudoku Game"
    topics = ("sudoku", "game", "desktop", "cpp23")
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tests": True
    }
    
    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "tests/*"
    
    def configure(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
    
    def requirements(self):
        # Core dependencies - updated to newer versions to fix gettext/0.21 issues
        self.requires("sdl/3.2.18")
        self.requires("imgui/1.92.0")
        self.requires("spdlog/1.15.3")
        self.requires("yaml-cpp/0.8.0")
        self.requires("zlib/1.3.1")  # For save file compression
        self.requires("libsodium/1.0.18")  # For save file encryption
        # self.requires("openssl/3.5.1")  # Temporarily disabled due to Perl FindBin issue

        # Force newer mpg123 version to fix GCC 15 compatibility issues
        self.requires("mpg123/1.33.0", override=True)

        # Testing framework (conditional)
        if self.options.with_tests:
            self.requires("catch2/3.4.0")
    
    def build_requirements(self):
        self.tool_requires("cmake/3.28.1")
    
    def layout(self):
        # Simplified layout: build/Debug or build/Release (no nesting)
        # This creates a cleaner structure than cmake_layout's multi-config approach
        self.folders.build = f"build/{str(self.settings.build_type)}"
        self.folders.generators = f"build/{str(self.settings.build_type)}/generators"
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        
        tc = CMakeToolchain(self)
        # Use Ninja generator for faster builds
        tc.generator = "Ninja"
        
        # C++23 configuration
        tc.variables["CMAKE_CXX_STANDARD"] = "23"
        tc.variables["CMAKE_CXX_STANDARD_REQUIRED"] = "ON"
        tc.variables["CMAKE_CXX_EXTENSIONS"] = "OFF"
        
        # Static linking for single executable distribution
        if self.settings.os == "Windows":
            tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        
        tc.generate()
        
        # Copy ImGui backend files for SDL3 integration
        # Note: ImGui package structure varies, so we use a simple approach
        # The backend files will be manually maintained for now
        self.output.info("ImGui SDL3 backend files should be manually copied to src/imgui_backends/")
        self.output.info("Backend files needed: imgui_impl_sdl3.h/cpp, imgui_impl_opengl3.h/cpp, imgui_impl_opengl3_loader.h")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        # Note: Tests are built but not run automatically.
        # To run tests manually:
        #   ./build/Release/bin/tests/unit_tests
        #   ./build/Release/bin/tests/integration_tests

    # def configure_options(self):
    #     # Configure dependency options for single executable
    #     self.options["sdl"].shared = False
    #     self.options["imgui"].shared = False
    #     self.options["spdlog"].shared = False
    #     self.options["yaml-cpp"].shared = False
    #     # self.options["openssl"].shared = False  # Temporarily disabled
    #     
    #     # ImGui backend configuration
    #     self.options["imgui"].with_sdl = True
    #     self.options["imgui"].with_opengl3 = True
    #     
    #     # Disable OpenSSL in pulseaudio to avoid Perl FindBin issues
    #     self.options["pulseaudio"].with_openssl = False
    #     
    #     # Try to avoid problematic dependencies
    #     self.options["spdlog"].no_exceptions = False