version history                 {#version_history}
============

 version            | PR                                                | comment
 ------------------ |---------------------------------------------------| ---------------------------------------------------
 0.3.0              | N/A                                               | initial release
 0.3.1              | [3](https://github.com/ZEISS/warpaffine/pull/3)   | bugfix for a crash for "CZIs containing a single brick but have an S-index"
 0.3.2              | [5](https://github.com/ZEISS/warpaffine/pull/5)   | bugfix for a deadlock in rare case
 0.4.0              | [6](https://github.com/ZEISS/warpaffine/pull/6)   | set re-tiling id of sub-blocks to allow for more sensible stitching of resulting CZI
 0.5.0              | [7](https://github.com/ZEISS/warpaffine/pull/7)   | copy attachments from source document
 0.5.1              | [8](https://github.com/ZEISS/warpaffine/pull/8)   | fix z interval metadata for coverglass transformation
 0.5.2              | [9](https://github.com/ZEISS/warpaffine/pull/9)   | fix z interval metadata for deskew
 0.5.3              | [10](https://github.com/ZEISS/warpaffine/pull/10) | prepare changeable illumination angle