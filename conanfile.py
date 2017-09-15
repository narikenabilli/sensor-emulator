from conans import ConanFile, CMake

class PocoTimerConan(ConanFile):

    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "Poco/1.7.5@lasote/stable",
        "json/2.0.10@jjones646/stable",
        "OpenSSL/1.0.2k@lasote/stable")
        
    generators = "txt", "cmake"
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
