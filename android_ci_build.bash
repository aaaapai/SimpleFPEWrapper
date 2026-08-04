#!/bin/bash
# set -e

# git clone --depth 1 https://github.com/stbrumme/xxhash.git src/main/cpp/3rdparty/xxhash
# rm -rf MobileGlues-cpp/include/FastSTL
# git clone --depth 1 https://github.com/aaaapai/FastSTL.git MobileGlues-cpp/include/FastSTL
cmake_build () {
  ANDROID_ABI=$1
  mkdir -p build
  cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release -DANDROID_PLATFORM=24 -DANDROID_ABI=$ANDROID_ABI -DCMAKE_SYSTEM_NAME=Android -DANDROID_TOOLCHAIN=clang -DANDROID_ARM_MODE=arm -DCMAKE_MAKE_PROGRAM=$ANDROID_NDK_LATEST_HOME/prebuilt/linux-x86_64/bin/make -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_LATEST_HOME/build/cmake/android.toolchain.cmake
  cmake --build . --config Release --parallel 6
}

cmake_build arm64-v8a
