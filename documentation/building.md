# Building WarpAffine

## overview

The WarpAffine-project is using the [CMake](https://cmake.org/) build system. It is a cross-platform build system that can generate native build files for many platforms and IDEs. 
The following external packages are required to build the project:

| component | description | referenced via | comment
|--|--|--|--|
| [cli11](https://github.com/CLIUtils/CLI11) | command line parser | CMake's [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) | |
| [libCZI](https://github.com/ZEISS/libczi.git) | CZI file format library | CMake's [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) | |
| [Intel® Integrated Performance Primitives](https://www.intel.com/content/www/us/en/developer/tools/oneapi/ipp.html#gs.1wjq61) |extensive library of ready-to-use, domain-specific functions that are highly optimized for diverse Intel architectures | Custom CMake-script for finding libraries/headers | can be built without |
| [Intel® Threading Building Blocks](https://github.com/oneapi-src/oneTBB/) | flexible C++ library that simplifies the work of adding parallelism to complex applications | CMake's [find_package](https://cmake.org/cmake/help/latest/command/find_package.html)|  | 
| [Eigen3](https://eigen.tuxfamily.org/) | C++ template library for linear algebra | CMake's [find_package](https://cmake.org/cmake/help/latest/command/find_package.html)| can be downloaded by CMake's [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) during build | 


## prerequisites

For installing the prerequisites, it is recommended to use the package manager of the respective platform.

With the [vcpkg-package-manager](https://vcpkg.io/en/), the following command will install Eigen3 and TBB:

On Windows:
```bash
    vcpkg install --triplet x64-windows tbb eigen3
```
On Linux:
```bash
    vcpkg install --triplet x64-linux tbb eigen3
```

For IPP, the [Intel® oneAPI Base Toolkit](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit.html) is required. Please see there for installation instructions.

It is possible to build the project without IPP, but then performance will suffer significantly. However - since IPP is only availabe on the x86-architecture, it is the only option on other platforms.
In order to allow for a build without IPP, the CMake-variable `WARPAFFINE_ALLOW_BUILD_WITHOUT_IPP` must be set to `ON`. Note that the CMake-script will still try to find IPP, but will not fail if it is not found.

The CMake build process will expect to find Eigen3 via CMake's find_package. However, it is also possible to download Eigen3 during the build process. In order to do so, the CMake-variable `WARPAFFINE_USE_PRIVATE_EIGEN3` must be set to `ON`.

## building

With all prerequisites in place, the project can be built with the following commands - asuming that the current directory is the project's root folder:

```bash
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
```

A screencast of the build process can be found [here](https://asciinema.org/a/595882).
