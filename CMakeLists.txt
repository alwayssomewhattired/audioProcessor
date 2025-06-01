
cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(absolute_agony
src/absolute_agony.cpp
src/AudioFileParse.cpp
src/AudioUploader.cpp
src/FFTProcessor.cpp
src/WebSocketClient.cpp
 )




 find_package(PkgConfig REQUIRED)
 pkg_check_modules(JSONCPP REQUIRED jsoncpp)


target_include_directories(absolute_agony PRIVATE ${CMAKE_SOURCE_DIR}/packages
                                                  ${CMAKE_SOURCE_DIR}/packages/websocketpp
						                         ${CMAKE_SOURCE_DIR}/include
                                                  ${CMAKE_SOURCE_DIR}/lib
                                                  ${JSONCPP_INCLUDE_DIRS}
)


find_package(FFTW3 CONFIG REQUIRED)
find_package(AWSSDK CONFIG REQUIRED COMPONENTS s3 core)
find_package(Boost REQUIRED COMPONENTS system thread random)
find_package(SndFile REQUIRED)
# find_package(jsoncpp REQUIRED)
find_package(OpenSSL REQUIRED)



target_link_libraries(absolute_agony PRIVATE
    FFTW3::fftw3
    aws-cpp-sdk-s3
    aws-cpp-sdk-core
    Boost::system
    Boost::thread
    Boost::random
    OpenSSL::SSL
    OpenSSL::Crypto
    SndFile::sndfile
   # jsoncpp
   ${JSONCPP_LIBRARIES}
)
