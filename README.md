# warpaffine

[![REUSE status](https://api.reuse.software/badge/github.com/ZEISS/libczi)](https://api.reuse.software/info/github.com/ZEISS/warpaffine)
[![CMake Linux-x64](https://github.com/ZEISS/warpaffine/actions/workflows/cmake_linux_x64.yml/badge.svg?branch=main&event=push)](https://github.com/ZEISS/warpaffine/actions/workflows/cmake_linux_x64)
[![CMake Windows-x64](https://github.com/ZEISS/warpaffine/actions/workflows/cmake_windows_x64.yml/badge.svg?branch=main&event=push)](https://github.com/ZEISS/warpaffine/actions/workflows/cmake_windows_x64)
[![MegaLinter](https://github.com/ZEISS/warpaffine/actions/workflows/mega-linter.yml/badge.svg?branch=main&event=push)](https://github.com/ZEISS/warpaffine/actions/workflows/mega-linter.yml)

This is an experimental effort at implementing a "Deskew operation" with the best performance possible.


With an acquisition from a [Lattice Lightsheet microscope](https://www.zeiss.com/microscopy/en/products/light-microscopes/light-sheet-microscopes/lattice-lightsheet-7.html) the image slices are created at a skewed angle. 
In order to create a regular volumetric image in Cartesian coordinates, the volume needs to undergo a geometric transformation (and affine transformation, hence the name of this project)
and needs to be resampled.


This project is operating on a CZI-file containing the raw data from acquisition, and outputs a new CZI-file containing the deskewed data.
The input data and the output data are expected/created with zstd-compression.

Please check additional documentation [here](documentation/documentation.md).

## Credits to Third Party Components
The authors and maintainers of warpaffine give a big shout-out to all the [helpers](./warpaffine/THIRD_PARTY_LICENSES_ARTIFACT_DISTRIBUTION.txt) that have been part in bringing this project to where it is today.

## Guidelines
[Code of Conduct](./CODE_OF_CONDUCT.md)  
[Contributing](./CONTRIBUTING.md)

## Disclaimer
ZEISS, ZEISS.com are registered trademarks of Carl Zeiss AG.
