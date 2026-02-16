# Notes

## Memory Requirements

The strategy currently used in warpaffine can be summarized as follows:

* A custom allocator ([BrickAllocator](https://github.com/ZEISS/warpaffine/blob/main/libwarpaffine/BrickAllocator.h)) is used to manage memory for the input bricks, the output bricks and
 the compressed output brick (if applicable).
* For this allocator, we reserve a fixed amount of memory (the "memory budget") that is used for the entire processing.
* We define a high-water mark for the allocator - if memory usage is above this mark, then reading the source is throttled.

By default, we reserve roughly the main memory size of the machine as the memory budget for the BrickAllocator.

Note that there is a minimal amount of memory required for the allocator, which is determined by the granularity of the processing
operations. This minimal amount is determined and checked for [here](https://github.com/ZEISS/warpaffine/blob/e1b47fa027f532fd6bfdbe56ad89fa0814b4f47b/libwarpaffine/configure.cpp#L42).  
It is the sum of the following:

* The memory size of the largest input tile, multiplied by the number of Z-slices of the input.
* The memory size of the largest output tile, multiplied by the number of Z-slices of the output.

The memory size of a tile is calculated by multiplying the tile's width and height with the number of bytes-per-pixel (e.g. 
2 for Gray16, and 1 for Gray8).   
The extent of an output tile can be specified with the command line parameter `-m` (or `--max-tile-extent`), where a default of 2048 is used.

The number of Z-slices of the output is determined by the number of Z-slices of the input, the geometric transformation applied to the input, and the ratio of the scaling factors in X/Y and Z direction.   
For the standard case (x-y scaling: 0.145 µm, z scaling: 0.200 µm) we have the following relationship:

| Mode of Operation                  | Z-slice Factor* |
|------------------------------------|---------|
| CoverGlassTransform                |  0.138  |
| CoverGlassTransform_and_xy_rotated |  0.138  |
| Deskew                             |  1      |

*Multiply source z-count by this factor to get destination z-count.

### Example

> **Scenario:** A source CZI with 1000 Z-slices, Gray16 pixel format, largest tile 2048×400 pixels,  
> using the default max tile extent of 2048 and standard scaling factors.

---

#### 📥 Input Brick Memory

$$
\underbrace{2048 \times 400}_{\text{tile size}} \times \underbrace{2}_{\text{bytes/pixel}} \times \underbrace{1000}_{\text{Z-slices}} = 1{,}638{,}400{,}000 \text{ bytes} \approx \mathbf{1.53\ GB}
$$

#### 📤 Output Brick Memory

$$
\underbrace{2048 \times 2048}_{\text{max tile extent}} \times \underbrace{2}_{\text{bytes/pixel}} \times \underbrace{1000 \times 0.138}_{\text{output Z-slices}} = 1{,}157{,}627{,}904 \text{ bytes} \approx \mathbf{1.08\ GB}
$$

---

#### 📊 Summary

| Component        | Dimensions         | Bytes/Pixel | Z-Slices | Size       |
|:-----------------|:------------------:|:-----------:|:--------:|----------:|
| Input Brick      | 2048 × 400         | 2           | 1000     | 1.53 GB   |
| Output Brick     | 2048 × 2048        | 2           | 138      | 1.08 GB   |
| **Total**        |                    |             |          | **2.60 GB** |

