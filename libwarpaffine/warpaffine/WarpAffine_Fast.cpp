// SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

/// \file
/// Performance-optimized implementation of the warp-affine operation.
///
/// This file provides WarpAffine_Fast, an optimized replacement for WarpAffine_Reference.
/// The algorithm is functionally identical to the reference — it maps each destination voxel
/// back to a source position via the inverse transformation and samples there — but it
/// applies three key optimizations to the inner loop:
///
/// **Optimization 1 — Incremental source-position computation:**
///   The reference computes `source = M_inv * [x, y, z, 1]` (a full 4x4 matrix-vector
///   multiply: 16 multiplies + 12 additions) for every destination voxel. Because the
///   inner loop iterates over x with unit stride, successive source positions differ by
///   exactly column 0 of M_inv. We therefore precompute the y/z-dependent base once per
///   scanline, and then only accumulate a constant 3-component delta per x-step
///   (3 additions instead of ~28 FLOPs).
///
/// **Optimization 2 — Direct pointer arithmetic:**
///   The reference calls Brick::GetPointerToPixel / GetConstPointerToPixel for every
///   voxel access, each of which invokes libCZI::Utils::GetBytesPerPixel at runtime.
///   Here we precompute base pointers and strides once, and advance the destination
///   pointer with a simple `++dst_ptr` (sizeof(t) step) in the inner loop. Source
///   pixel access is done via raw `char*` arithmetic with the precomputed strides.
///
/// **Optimization 3 — Plain scalar types instead of Eigen in the inner loop:**
///   The reference constructs an Eigen::Vector4d per voxel and evaluates the product
///   through Eigen's expression-template machinery. We replace this with three plain
///   `double` variables (src_x, src_y, src_z), eliminating all Eigen overhead from
///   the hot path. Eigen is only used once during setup to compute the matrix inverse.
///   
/// **Optimization 4 - scanline-based zone precomputation for classifying pixels as inside/outside:**  
///   Instead of classifying every pixel in the inner x-loop via a switch, we now solve the linear 
///   inequalities once per scanline to find the four x-positions where the classification transitions occur.
///   Before (per-pixel classification):
///     for each x:
///         switch (classify(src_x, src_y, src_z))   ← branch on every pixel
///             case kInside: ...
///             case kOnePixelOutside: ...
///             case kOutside: ...
///   After (scanline-based zone precomputation):
///     Zone 1 : [0, x_ext_start)           → memset zero
///     Zone 2 : [x_ext_start, x_in_start)  → tight loop : SampleTrilinearBorder
///     Zone 3 : [x_in_start, x_in_end)     → tight loop : SampleTrilinearInside  ← hot path, NO branching
///     Zone 4 : [x_in_end, x_ext_end)      → tight loop : SampleTrilinearBorder
///     Zone 5 : [x_ext_end, dst_w)         → memset zero
///
/// Together these changes reduce the per-voxel cost from roughly 28 FLOPs + 2 virtual-
/// dispatch-like calls + Eigen temporaries, down to 3 additions + direct memory access.
///
/// **Numerical equivalence:**
///   The trilinear interpolation formula, modf/floor usage, clamp-and-round logic, and
///   boundary classification (kInside / kOnePixelOutside / kOutside) all replicate the
///   reference implementation exactly. The only theoretical floating-point difference
///   comes from the incremental accumulation (`src_x += dx` repeated N times) vs. the
///   reference's per-pixel multiply (`M_inv * [x,y,z,1]`). For typical brick widths
///   (up to a few thousand voxels), the accumulated rounding error is on the order of
///   N * machine_epsilon * |delta| ≈ 1e-13, which is far too small to affect the final
///   lround() result for either nearest-neighbor or trilinear interpolation.

#include "WarpAffine_Fast.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace libCZI;

namespace
{
    /// Pre-extracted columns of the inverse transformation matrix, stored as raw doubles
    /// for efficient incremental source-position computation in the inner loop.
    ///
    /// Given an inverse matrix M_inv, the source-space position corresponding to a
    /// destination voxel at (x, y, z) is:
    ///
    ///     source = M_inv * [x, y, z, 1]^T
    ///            = col0 * x  +  col1 * y  +  col2 * z  +  col3
    ///
    /// We decompose this into:
    ///   - z_contrib  = col2 * z + col3                     (computed once per z-slice)
    ///   - yz_base    = col1 * y + z_contrib                (computed once per scanline)
    ///   - source_pos = yz_base  + col0 * x                 (incremented per x-step by adding col0)
    ///
    /// The struct stores only the first three rows of each column (the x/y/z components);
    /// the homogeneous w-component is always 1 and not needed.
    struct IncrementalTransform
    {
        double dx_src_x, dx_src_y, dx_src_z;       ///< Column 0 of inverse (rows 0-2): source-position delta when stepping +1 in destination x.
        double dy_src_x, dy_src_y, dy_src_z;       ///< Column 1 of inverse (rows 0-2): source-position delta when stepping +1 in destination y.
        double dz_src_x, dz_src_y, dz_src_z;       ///< Column 2 of inverse (rows 0-2): source-position delta when stepping +1 in destination z.
        double base_src_x, base_src_y, base_src_z; ///< Column 3 of inverse (rows 0-2): source position corresponding to destination (0,0,0).
    };

    /// Compute the inverse of the combined (translation * transformation) matrix and
    /// extract its columns into an IncrementalTransform for use in the inner loops.
    /// This is the only place where Eigen is used; everything from here on works with
    /// plain doubles.
    IncrementalTransform PrepareIncrementalTransform(const Eigen::Matrix4d& combined_transformation)
    {
        const Eigen::Matrix4d inv = combined_transformation.inverse();
        return IncrementalTransform{
            inv(0, 0), inv(1, 0), inv(2, 0),   // column 0, rows 0..2
            inv(0, 1), inv(1, 1), inv(2, 1),   // column 1, rows 0..2
            inv(0, 2), inv(1, 2), inv(2, 2),   // column 2, rows 0..2
            inv(0, 3), inv(1, 3), inv(2, 3),   // column 3, rows 0..2
        };
    }

    // ---- Nearest-neighbor warp ------------------------------------------------

    /// Compute the x-position boundaries that divide a destination scanline into three
    /// contiguous zones for nearest-neighbor interpolation.
    ///
    /// For NN, a destination voxel maps to a valid source voxel when
    ///     lround(src_i) ∈ [0, dim_i)   for all three axes.
    ///
    /// Since lround rounds half-integers away from zero, this corresponds to the
    /// continuous condition  src_i ∈ (-0.5, dim_i - 0.5)  (open on both sides because
    /// lround(-0.5) = -1 and lround(dim-0.5) = dim, both out of range).
    ///
    /// Because only two states exist (inside / outside), the scanline splits into just
    /// three zones:
    ///
    ///     [0, x_in_start)              — outside  (zero-fill)
    ///     [x_in_start, x_in_end)       — inside   (copy from source, no bounds check)
    ///     [x_in_end, dst_w)            — outside  (zero-fill)
    ///
    /// The implementation mirrors ComputeScanlineSegments: solve the linear inequalities
    /// for the continuous x-interval, convert to integer bounds, then verify ±1 boundary
    /// pixels using the actual lround + bounds check.
    ///
    /// Post-condition: 0 <= x_in_start <= x_in_end <= dst_w
    inline void ComputeNNScanlineSegments(
        double base_x, double base_y, double base_z,
        double dx_x, double dx_y, double dx_z,
        int src_w, int src_h, int src_d,
        uint32_t dst_w,
        uint32_t& x_in_start, uint32_t& x_in_end)
    {
        const double dw = static_cast<double>(dst_w);

        // The continuous condition for lround(f(x)) ∈ [0, dim) is f(x) ∈ (-0.5, dim-0.5).
        // We approximate this with the half-open interval [-0.5, dim-0.5) for the solver,
        // then the verification step handles the open-lower-bound edge case (lround(-0.5) = -1).
        auto narrow_half_open = [](double base, double dx, double lower, double upper,
                                   double& lo, double& hi)
        {
            if (lo >= hi) return;
            if (dx > 0.0)
            {
                lo = max(lo, (lower - base) / dx);
                hi = min(hi, (upper - base) / dx);
            }
            else if (dx < 0.0)
            {
                lo = max(lo, (upper - base) / dx);
                hi = min(hi, (lower - base) / dx);
            }
            else
            {
                if (base < lower || base >= upper) { lo = hi + 1; }
            }
        };

        auto clamp_to_uint32 = [](double v, uint32_t max_val) -> uint32_t
        {
            if (v <= 0.0) return 0;
            if (v >= static_cast<double>(max_val)) return max_val;
            return static_cast<uint32_t>(v);
        };

        // Solve for x where src_i ∈ [-0.5, dim_i - 0.5) for all axes.
        double in_lo = 0.0, in_hi = dw;
        narrow_half_open(base_x, dx_x, -0.5, static_cast<double>(src_w) - 0.5, in_lo, in_hi);
        narrow_half_open(base_y, dx_y, -0.5, static_cast<double>(src_h) - 0.5, in_lo, in_hi);
        narrow_half_open(base_z, dx_z, -0.5, static_cast<double>(src_d) - 0.5, in_lo, in_hi);

        if (in_lo >= in_hi)
        {
            x_in_start = x_in_end = 0;
        }
        else
        {
            x_in_start = clamp_to_uint32(ceil(in_lo), dst_w);
            x_in_end = clamp_to_uint32(ceil(in_hi), dst_w);
            if (x_in_start >= x_in_end) x_in_start = x_in_end = 0;
        }

        // Verify and adjust boundaries using the actual lround + bounds check.
        auto is_nn_inside = [&](uint32_t x) -> bool
        {
            int nn_x = static_cast<int>(lround(base_x + dx_x * x));
            int nn_y = static_cast<int>(lround(base_y + dx_y * x));
            int nn_z = static_cast<int>(lround(base_z + dx_z * x));
            return nn_x >= 0 && nn_x < src_w &&
                   nn_y >= 0 && nn_y < src_h &&
                   nn_z >= 0 && nn_z < src_d;
        };

        if (x_in_start > 0 && is_nn_inside(x_in_start - 1))
            --x_in_start;
        while (x_in_start < x_in_end && !is_nn_inside(x_in_start))
            ++x_in_start;

        if (x_in_end < dst_w && is_nn_inside(x_in_end))
            ++x_in_end;
        while (x_in_end > x_in_start && !is_nn_inside(x_in_end - 1))
            --x_in_end;
    }

    /// Fast nearest-neighbor warp for a single pixel type.
    ///
    /// For each scanline (y, z), ComputeNNScanlineSegments solves the linear inequalities
    /// to determine the x-range where lround(src) maps into the source brick.  The scanline
    /// is then split into three zones:
    ///
    ///     [0, x_in_start)        — outside:  zero-fill via memset
    ///     [x_in_start, x_in_end) — inside:   lround + copy (no bounds check)
    ///     [x_in_end, dst_w)      — outside:  zero-fill via memset
    ///
    /// The inside zone eliminates the per-pixel bounds-check branch that was present in the
    /// original implementation, allowing better instruction pipelining and branch prediction.
    /// Source positions are recomputed from the direct formula at the zone boundary and then
    /// accumulated incrementally within the zone.
    ///
    /// \tparam t  The pixel value type (uint8_t, uint16_t, or float).
    template <typename t>
    void FastNearestNeighborWarp(
        const Brick& source_brick,
        const Brick& destination_brick,
        const IncrementalTransform& tf)
    {
        const uint32_t dst_w = destination_brick.info.width;
        const uint32_t dst_h = destination_brick.info.height;
        const uint32_t dst_d = destination_brick.info.depth;

        const int src_w = static_cast<int>(source_brick.info.width);
        const int src_h = static_cast<int>(source_brick.info.height);
        const int src_d = static_cast<int>(source_brick.info.depth);

        const uint32_t dst_stride_line = destination_brick.info.stride_line;
        const uint32_t dst_stride_plane = destination_brick.info.stride_plane;
        const uint32_t src_stride_line = source_brick.info.stride_line;
        const uint32_t src_stride_plane = source_brick.info.stride_plane;

        char* dst_base = static_cast<char*>(destination_brick.data.get());
        const char* src_base = static_cast<const char*>(source_brick.data.get());

        for (uint32_t z = 0; z < dst_d; ++z)
        {
            const double z_contrib_x = tf.dz_src_x * z + tf.base_src_x;
            const double z_contrib_y = tf.dz_src_y * z + tf.base_src_y;
            const double z_contrib_z = tf.dz_src_z * z + tf.base_src_z;

            for (uint32_t y = 0; y < dst_h; ++y)
            {
                const double scanline_base_x = tf.dy_src_x * y + z_contrib_x;
                const double scanline_base_y = tf.dy_src_y * y + z_contrib_y;
                const double scanline_base_z = tf.dy_src_z * y + z_contrib_z;

                t* dst_ptr = reinterpret_cast<t*>(
                    dst_base +
                    static_cast<size_t>(z) * dst_stride_plane +
                    static_cast<size_t>(y) * dst_stride_line);

                // Determine the inside zone for this scanline.
                uint32_t x_in_start, x_in_end;
                ComputeNNScanlineSegments(
                    scanline_base_x, scanline_base_y, scanline_base_z,
                    tf.dx_src_x, tf.dx_src_y, tf.dx_src_z,
                    src_w, src_h, src_d, dst_w,
                    x_in_start, x_in_end);

                // Zone 1: [0, x_in_start) — outside, zero-fill.
                if (x_in_start > 0)
                    memset(dst_ptr, 0, static_cast<size_t>(x_in_start) * sizeof(t));

                // Zone 2: [x_in_start, x_in_end) — inside, branch-free copy.
                if (x_in_end > x_in_start)
                {
                    double src_x = scanline_base_x + tf.dx_src_x * x_in_start;
                    double src_y = scanline_base_y + tf.dx_src_y * x_in_start;
                    double src_z = scanline_base_z + tf.dx_src_z * x_in_start;
                    for (uint32_t x = x_in_start; x < x_in_end; ++x)
                    {
                        const int nn_x = static_cast<int>(lround(src_x));
                        const int nn_y = static_cast<int>(lround(src_y));
                        const int nn_z = static_cast<int>(lround(src_z));

                        const t* src_pixel = reinterpret_cast<const t*>(
                            src_base +
                            static_cast<size_t>(nn_z) * src_stride_plane +
                            static_cast<size_t>(nn_y) * src_stride_line +
                            static_cast<size_t>(nn_x) * sizeof(t));
                        dst_ptr[x] = *src_pixel;

                        src_x += tf.dx_src_x;
                        src_y += tf.dx_src_y;
                        src_z += tf.dx_src_z;
                    }
                }

                // Zone 3: [x_in_end, dst_w) — outside, zero-fill.
                if (x_in_end < dst_w)
                    memset(dst_ptr + x_in_end, 0, static_cast<size_t>(dst_w - x_in_end) * sizeof(t));
            }
        }
    }

    // ---- Trilinear warp -------------------------------------------------------

    /// Clamp an interpolated double value to the valid range of pixel type t, then round
    /// to the nearest representable value.
    ///
    /// This replicates the reference's return expression:
    ///     (c < 0) ? 0 : c > max ? max : static_cast<t>(lround(c))
    ///
    /// For integer pixel types (uint8_t, uint16_t), this rounds to the nearest integer
    /// and clamps to [0, max]. For float, lround produces a long which is then cast back
    /// to float — this matches the reference behavior (which effectively rounds float
    /// pixel values to the nearest integer).
    template <typename t>
    inline t ClampAndRound(double c)
    {
        if (c < 0)
            return static_cast<t>(0);
        if (c > static_cast<double>(numeric_limits<t>::max()))
            return numeric_limits<t>::max();
        return static_cast<t>(lround(c));
    }

    /// Sample a voxel value from the source brick using trilinear interpolation.
    /// This variant is used when the sampling position is fully **inside** the source brick,
    /// meaning pos is in [0, dim-1) for all axes and the 2x2x2 neighborhood at
    /// floor(pos)..floor(pos)+1 is entirely in-bounds. No clamping is needed.
    ///
    /// The trilinear interpolation follows the standard decomposition into three successive
    /// linear interpolations (see https://en.wikipedia.org/wiki/Trilinear_interpolation):
    ///   1. Interpolate along x (4 pairs → 4 intermediate values c00, c01, c10, c11)
    ///   2. Interpolate along y (2 pairs → 2 intermediate values c0, c1)
    ///   3. Interpolate along z (1 pair → final value c)
    ///
    /// The 8 source voxels are read via a single base pointer (p000) and stride-based
    /// offsets, which is significantly cheaper than 8 individual GetConstPointerToPixel calls
    /// that each redundantly recompute strides and bytes-per-pixel.
    ///
    /// The fractional parts (xd, yd, zd) are obtained via modf, matching the reference.
    /// The integer parts use static_cast<int> (truncation toward zero), which is correct
    /// here because positions are guaranteed >= 0 by the kInside classification.
    ///
    /// \tparam t  The pixel value type.
    /// \param  src_base          Raw pointer to the start of the source brick's data.
    /// \param  src_stride_line   Byte distance between consecutive lines (y, y+1) at the same z.
    /// \param  src_stride_plane  Byte distance between consecutive planes (z, z+1).
    /// \param  pos_x, pos_y, pos_z  Continuous source position (guaranteed in-bounds).
    /// \returns  The trilinear-interpolated, clamped, and rounded pixel value.
    template <typename t>
    inline t SampleTrilinearInside(
        const char* src_base,
        uint32_t src_stride_line,
        uint32_t src_stride_plane,
        double pos_x, double pos_y, double pos_z)
    {
        // Decompose each coordinate into integer and fractional parts.
        double dummy;
        const double xd = modf(pos_x, &dummy);
        const double yd = modf(pos_y, &dummy);
        const double zd = modf(pos_z, &dummy);

        const int ix = static_cast<int>(pos_x);
        const int iy = static_cast<int>(pos_y);
        const int iz = static_cast<int>(pos_z);

        // Compute the base address of voxel (ix, iy, iz) — the "c000" corner.
        // All other 7 neighbors are reached by adding sizeof(t) (x+1),
        // src_stride_line (y+1), and/or src_stride_plane (z+1).
        const char* p000 = src_base +
            static_cast<size_t>(iz) * src_stride_plane +
            static_cast<size_t>(iy) * src_stride_line +
            static_cast<size_t>(ix) * sizeof(t);

        //   Naming: cXYZ where X/Y/Z ∈ {0,1} indicate offsets from (ix,iy,iz).
        const t c000 = *reinterpret_cast<const t*>(p000);
        const t c100 = *reinterpret_cast<const t*>(p000 + sizeof(t));                                // x+1
        const t c010 = *reinterpret_cast<const t*>(p000 + src_stride_line);                          // y+1
        const t c110 = *reinterpret_cast<const t*>(p000 + src_stride_line + sizeof(t));              // x+1, y+1
        const t c001 = *reinterpret_cast<const t*>(p000 + src_stride_plane);                         // z+1
        const t c101 = *reinterpret_cast<const t*>(p000 + src_stride_plane + sizeof(t));             // x+1, z+1
        const t c011 = *reinterpret_cast<const t*>(p000 + src_stride_plane + src_stride_line);       // y+1, z+1
        const t c111 = *reinterpret_cast<const t*>(p000 + src_stride_plane + src_stride_line + sizeof(t)); // x+1, y+1, z+1

        // Step 1: interpolate along x (4 edges parallel to the x-axis)
        const double c00 = c000 * (1 - xd) + c100 * xd;
        const double c01 = c001 * (1 - xd) + c101 * xd;
        const double c10 = c010 * (1 - xd) + c110 * xd;
        const double c11 = c011 * (1 - xd) + c111 * xd;

        // Step 2: interpolate along y (2 edges parallel to the y-axis)
        const double c0 = c00 * (1 - yd) + c10 * yd;
        const double c1 = c01 * (1 - yd) + c11 * yd;

        // Step 3: interpolate along z
        const double c = c0 * (1 - zd) + c1 * zd;

        return ClampAndRound<t>(c);
    }

    /// Sample a voxel value from the source brick using trilinear interpolation,
    /// allowing the sampling position to be **up to one pixel outside** the brick.
    ///
    /// This is the boundary-handling counterpart to SampleTrilinearInside. It is invoked
    /// when GetPixelPositionForTriLinear returns kOnePixelOutside, meaning the point is
    /// within [-1, dim] on each axis — close enough for interpolation, but some of the
    /// 8 neighbors would lie outside the valid range.
    ///
    /// The strategy is to **clamp** each neighbor coordinate to [0, dim-1] before reading.
    /// For example, if the sample point is at x=-0.3, then floor(x)=-1 and floor(x)+1=0.
    /// After clamping, both the "x" and "x+1" reads come from x=0, and the interpolation
    /// weights cause the result to equal the boundary value — effectively extending the
    /// brick's edge outward by one pixel ("clamp to edge" semantics).
    ///
    /// Unlike SampleTrilinearInside, this variant uses floor() instead of integer
    /// truncation for the integer part. This is necessary because truncation toward zero
    /// gives the wrong result for negative positions (e.g. static_cast<int>(-0.3) = 0,
    /// but we need -1). The reference uses the same approach.
    ///
    /// The interpolation formula, weight computation (via modf), and final clamping/rounding
    /// are identical to the "inside" variant.
    ///
    /// \tparam t  The pixel value type.
    /// \param  src_base          Raw pointer to the start of the source brick's data.
    /// \param  src_stride_line   Byte distance between consecutive lines.
    /// \param  src_stride_plane  Byte distance between consecutive planes.
    /// \param  src_w, src_h, src_d  Source brick dimensions (for clamping).
    /// \param  pos_x, pos_y, pos_z  Continuous source position (may be slightly negative).
    /// \returns  The trilinear-interpolated, clamped, and rounded pixel value.
    template <typename t>
    inline t SampleTrilinearBorder(
        const char* src_base,
        uint32_t src_stride_line,
        uint32_t src_stride_plane,
        int src_w, int src_h, int src_d,
        double pos_x, double pos_y, double pos_z)
    {
        double dummy;
        const double xd = modf(pos_x, &dummy);
        const double yd = modf(pos_y, &dummy);
        const double zd = modf(pos_z, &dummy);

        // Use floor (not truncation) to get the correct lower-corner for negative positions.
        const int ix = static_cast<int>(floor(pos_x));
        const int iy = static_cast<int>(floor(pos_y));
        const int iz = static_cast<int>(floor(pos_z));

        // Clamp the two neighbor coordinates per axis to the valid range [0, dim-1].
        const int x0 = max(ix, 0);
        const int x1 = min(ix + 1, src_w - 1);
        const int y0 = max(iy, 0);
        const int y1 = min(iy + 1, src_h - 1);
        const int z0 = max(iz, 0);
        const int z1 = min(iz + 1, src_d - 1);

        // Helper to read a voxel at an arbitrary (clamped) position.
        // Unlike the "inside" variant we cannot use a single base pointer + offsets
        // because the clamped coordinates may alias (e.g. x0 == x1 when clamped).
        auto sample = [&](int sx, int sy, int sz) -> t
        {
            return *reinterpret_cast<const t*>(
                src_base +
                static_cast<size_t>(sz) * src_stride_plane +
                static_cast<size_t>(sy) * src_stride_line +
                static_cast<size_t>(sx) * sizeof(t));
        };

        const t c000 = sample(x0, y0, z0);
        const t c100 = sample(x1, y0, z0);
        const t c010 = sample(x0, y1, z0);
        const t c110 = sample(x1, y1, z0);
        const t c001 = sample(x0, y0, z1);
        const t c101 = sample(x1, y0, z1);
        const t c011 = sample(x0, y1, z1);
        const t c111 = sample(x1, y1, z1);

        // Same trilinear formula as SampleTrilinearInside.
        const double c00 = c000 * (1 - xd) + c100 * xd;
        const double c01 = c001 * (1 - xd) + c101 * xd;
        const double c10 = c010 * (1 - xd) + c110 * xd;
        const double c11 = c011 * (1 - xd) + c111 * xd;

        const double c0 = c00 * (1 - yd) + c10 * yd;
        const double c1 = c01 * (1 - yd) + c11 * yd;

        const double c = c0 * (1 - zd) + c1 * zd;

        return ClampAndRound<t>(c);
    }

    // ---- Scanline segment computation -----------------------------------------

    /// Compute the x-position boundaries that divide a destination scanline into five
    /// contiguous zones for trilinear interpolation.
    ///
    /// For a given scanline (fixed y, z), the source position is a linear function of x:
    ///
    ///     src(x) = scanline_base + col0 * x
    ///
    /// Because a linear function crosses each boundary at most once, the classification
    /// of pixels along the scanline follows a fixed order:
    ///
    ///     [0, x_ext_start)              — kOutside          (zero-fill)
    ///     [x_ext_start, x_in_start)     — kOnePixelOutside  (border sampling with clamped reads)
    ///     [x_in_start, x_in_end)        — kInside           (fast interior sampling, no clamping)
    ///     [x_in_end, x_ext_end)         — kOnePixelOutside  (border sampling with clamped reads)
    ///     [x_ext_end, dst_w)            — kOutside          (zero-fill)
    ///
    /// The "inside" zone is where all three source coordinates satisfy
    ///     0 <= src_i < dim_i - 1
    /// (so the full 2x2x2 trilinear neighborhood is in-bounds).
    ///
    /// The "extended" zone additionally includes positions where
    ///     -1 <= src_i <= dim_i
    /// (where clamped trilinear interpolation is still meaningful).
    ///
    /// This function solves the linear inequalities to find the four boundary x-positions.
    /// Because floating-point rounding can shift the result by ±1 pixel, the computed
    /// boundaries are verified and adjusted against the direct formula (base + dx * x) to
    /// guarantee exact agreement with the per-pixel classification.
    ///
    /// Post-condition: 0 <= x_ext_start <= x_in_start <= x_in_end <= x_ext_end <= dst_w
    inline void ComputeScanlineSegments(
        double base_x, double base_y, double base_z,
        double dx_x, double dx_y, double dx_z,
        int src_w, int src_h, int src_d,
        uint32_t dst_w,
        uint32_t& x_ext_start, uint32_t& x_ext_end,
        uint32_t& x_in_start, uint32_t& x_in_end)
    {
        const double dw = static_cast<double>(dst_w);

        // --- Helper: narrow a continuous interval by intersecting with a linear constraint ---
        //
        // For a half-open range [lower, upper):
        //   Given f(x) = base + dx*x, intersect [lo, hi) with {x : lower <= f(x) < upper}.
        //   - If dx > 0: f is increasing, so x >= (lower-base)/dx  and  x < (upper-base)/dx
        //   - If dx < 0: f is decreasing, inequalities flip
        //   - If dx == 0: f is constant, the constraint is either always or never satisfied
        auto narrow_half_open = [](double base, double dx, double lower, double upper,
                                   double& lo, double& hi)
        {
            if (lo >= hi) return;
            if (dx > 0.0)
            {
                lo = max(lo, (lower - base) / dx);
                hi = min(hi, (upper - base) / dx);
            }
            else if (dx < 0.0)
            {
                lo = max(lo, (upper - base) / dx);
                hi = min(hi, (lower - base) / dx);
            }
            else
            {
                if (base < lower || base >= upper) { lo = hi + 1; }
            }
        };

        // For a closed range [lower, upper]:
        //   Intersect [lo, hi] with {x : lower <= f(x) <= upper}.
        auto narrow_closed = [](double base, double dx, double lower, double upper,
                                double& lo, double& hi)
        {
            if (lo > hi) return;
            if (dx > 0.0)
            {
                lo = max(lo, (lower - base) / dx);
                hi = min(hi, (upper - base) / dx);
            }
            else if (dx < 0.0)
            {
                lo = max(lo, (upper - base) / dx);
                hi = min(hi, (lower - base) / dx);
            }
            else
            {
                if (base < lower || base > upper) { lo = hi + 1; }
            }
        };

        // Helper: clamp a double to [0, max_val] and convert to uint32_t.
        auto clamp_to_uint32 = [](double v, uint32_t max_val) -> uint32_t
        {
            if (v <= 0.0) return 0;
            if (v >= static_cast<double>(max_val)) return max_val;
            return static_cast<uint32_t>(v);
        };

        // --- Compute continuous x-interval for "inside" region ---
        //     Inside: src_i in [0, dim_i - 1) for all three axes.
        double in_lo = 0.0, in_hi = dw;
        narrow_half_open(base_x, dx_x, 0.0, static_cast<double>(src_w - 1), in_lo, in_hi);
        narrow_half_open(base_y, dx_y, 0.0, static_cast<double>(src_h - 1), in_lo, in_hi);
        narrow_half_open(base_z, dx_z, 0.0, static_cast<double>(src_d - 1), in_lo, in_hi);

        // --- Compute continuous x-interval for "extended" region ---
        //     Extended: src_i in [-1, dim_i] for all three axes.
        double ext_lo = 0.0, ext_hi = dw;
        narrow_closed(base_x, dx_x, -1.0, static_cast<double>(src_w), ext_lo, ext_hi);
        narrow_closed(base_y, dx_y, -1.0, static_cast<double>(src_h), ext_lo, ext_hi);
        narrow_closed(base_z, dx_z, -1.0, static_cast<double>(src_d), ext_lo, ext_hi);

        // --- Convert continuous intervals to integer x-positions ---
        //
        // For the half-open inside interval [in_lo, in_hi):
        //   First valid integer x >= in_lo:  ceil(in_lo)
        //   Past-end (first integer >= in_hi): ceil(in_hi)
        //   (See analysis: for strict <, last valid int < threshold, so past-end = ceil(threshold))
        //
        // For the closed extended interval [ext_lo, ext_hi]:
        //   First valid integer x >= ext_lo:  ceil(ext_lo)
        //   Past-end (first integer > ext_hi): floor(ext_hi) + 1

        if (in_lo >= in_hi)
        {
            x_in_start = x_in_end = 0;
        }
        else
        {
            x_in_start = clamp_to_uint32(ceil(in_lo), dst_w);
            x_in_end = clamp_to_uint32(ceil(in_hi), dst_w);
            if (x_in_start >= x_in_end) x_in_start = x_in_end = 0;
        }

        if (ext_lo > ext_hi)
        {
            x_ext_start = x_ext_end = 0;
            x_in_start = x_in_end = 0;
        }
        else
        {
            x_ext_start = clamp_to_uint32(ceil(ext_lo), dst_w);
            x_ext_end = clamp_to_uint32(floor(ext_hi) + 1.0, dst_w);
            if (x_ext_start >= x_ext_end)
            {
                x_ext_start = x_ext_end = 0;
                x_in_start = x_in_end = 0;
            }
        }

        // --- Verify and adjust boundary pixels ---
        //
        // The continuous computation can be off by ±1 pixel due to floating-point rounding
        // (e.g., a threshold that falls exactly on an integer boundary).  We verify the
        // outermost pixels of each zone using the direct formula (base + dx * x) and
        // adjust by at most one position in each direction.  This guarantees exact
        // agreement with the per-pixel classification that the reference uses.

        auto is_inside = [&](uint32_t x) -> bool
        {
            double sx = base_x + dx_x * x;
            double sy = base_y + dx_y * x;
            double sz = base_z + dx_z * x;
            return sx >= 0 && sx < (src_w - 1) &&
                   sy >= 0 && sy < (src_h - 1) &&
                   sz >= 0 && sz < (src_d - 1);
        };

        auto is_extended = [&](uint32_t x) -> bool
        {
            double sx = base_x + dx_x * x;
            double sy = base_y + dx_y * x;
            double sz = base_z + dx_z * x;
            return sx >= -1 && sx <= src_w &&
                   sy >= -1 && sy <= src_h &&
                   sz >= -1 && sz <= src_d;
        };

        // Expand/contract extended boundaries (at most ±1 per side).
        if (x_ext_start > 0 && is_extended(x_ext_start - 1))
            --x_ext_start;
        while (x_ext_start < x_ext_end && !is_extended(x_ext_start))
            ++x_ext_start;

        if (x_ext_end < dst_w && is_extended(x_ext_end))
            ++x_ext_end;
        while (x_ext_end > x_ext_start && !is_extended(x_ext_end - 1))
            --x_ext_end;

        // Expand/contract inside boundaries (at most ±1 per side).
        if (x_in_start > x_ext_start && is_inside(x_in_start - 1))
            --x_in_start;
        while (x_in_start < x_in_end && !is_inside(x_in_start))
            ++x_in_start;

        if (x_in_end < x_ext_end && is_inside(x_in_end))
            ++x_in_end;
        while (x_in_end > x_in_start && !is_inside(x_in_end - 1))
            --x_in_end;

        // Enforce the ordering invariant:
        //   0 <= x_ext_start <= x_in_start <= x_in_end <= x_ext_end <= dst_w
        x_in_start = max(x_in_start, x_ext_start);
        x_in_end = min(x_in_end, x_ext_end);
        if (x_in_start > x_in_end) x_in_end = x_in_start;
    }

    /// Fast trilinear warp for a single pixel type.
    ///
    /// This mirrors the reference's TriLinearWarp but applies four optimizations:
    ///   1. Incremental source-position computation (col0 accumulation per x-step)
    ///   2. Direct pointer arithmetic (precomputed strides, no GetPointerToPixel)
    ///   3. Raw doubles instead of Eigen types in the inner loop
    ///   4. Scanline-based zone precomputation (no per-pixel classification)
    ///
    /// For each scanline (y, z), ComputeScanlineSegments solves the linear inequalities
    /// to determine four x-positions that divide the scanline into five contiguous zones:
    ///
    ///     [0, x_ext_start)           — outside:  zero-fill via memset
    ///     [x_ext_start, x_in_start)  — border:   SampleTrilinearBorder (clamped reads)
    ///     [x_in_start, x_in_end)     — inside:   SampleTrilinearInside (fast, no clamping)
    ///     [x_in_end, x_ext_end)      — border:   SampleTrilinearBorder (clamped reads)
    ///     [x_ext_end, dst_w)         — outside:  zero-fill via memset
    ///
    /// The interior zone (zone 3) is the hot path — it processes the vast majority of
    /// pixels with no branching and no bounds checking, calling SampleTrilinearInside
    /// directly.  The border zones (2 and 4) are typically very thin (a few pixels wide)
    /// and use the clamped SampleTrilinearBorder.  The outside zones (1 and 5) are
    /// zero-filled in bulk with memset.
    ///
    /// Source positions are recomputed from the direct formula (base + dx * x) at each
    /// zone boundary, then accumulated incrementally within each zone.  This avoids
    /// accumulation drift across zone boundaries.
    ///
    /// \tparam t  The pixel value type (uint8_t, uint16_t, or float).
    template <typename t>
    void FastTriLinearWarp(
        const Brick& source_brick,
        const Brick& destination_brick,
        const IncrementalTransform& tf)
    {
        const uint32_t dst_w = destination_brick.info.width;
        const uint32_t dst_h = destination_brick.info.height;
        const uint32_t dst_d = destination_brick.info.depth;

        const int src_w = static_cast<int>(source_brick.info.width);
        const int src_h = static_cast<int>(source_brick.info.height);
        const int src_d = static_cast<int>(source_brick.info.depth);

        const uint32_t dst_stride_line = destination_brick.info.stride_line;
        const uint32_t dst_stride_plane = destination_brick.info.stride_plane;
        const uint32_t src_stride_line = source_brick.info.stride_line;
        const uint32_t src_stride_plane = source_brick.info.stride_plane;

        char* dst_base = static_cast<char*>(destination_brick.data.get());
        const char* src_base = static_cast<const char*>(source_brick.data.get());

        for (uint32_t z = 0; z < dst_d; ++z)
        {
            // z-dependent part of the source position, constant for the entire slice.
            const double z_contrib_x = tf.dz_src_x * z + tf.base_src_x;
            const double z_contrib_y = tf.dz_src_y * z + tf.base_src_y;
            const double z_contrib_z = tf.dz_src_z * z + tf.base_src_z;

            for (uint32_t y = 0; y < dst_h; ++y)
            {
                // Source position at x=0 for this scanline (the "scanline base").
                const double scanline_base_x = tf.dy_src_x * y + z_contrib_x;
                const double scanline_base_y = tf.dy_src_y * y + z_contrib_y;
                const double scanline_base_z = tf.dy_src_z * y + z_contrib_z;

                t* dst_ptr = reinterpret_cast<t*>(
                    dst_base +
                    static_cast<size_t>(z) * dst_stride_plane +
                    static_cast<size_t>(y) * dst_stride_line);

                // Determine the five zones for this scanline by solving the linear
                // inequalities on the source coordinates.
                uint32_t x_ext_start, x_ext_end, x_in_start, x_in_end;
                ComputeScanlineSegments(
                    scanline_base_x, scanline_base_y, scanline_base_z,
                    tf.dx_src_x, tf.dx_src_y, tf.dx_src_z,
                    src_w, src_h, src_d, dst_w,
                    x_ext_start, x_ext_end, x_in_start, x_in_end);

                // Zone 1: [0, x_ext_start) — outside, zero-fill.
                // All supported pixel types (uint8_t, uint16_t, float) represent zero
                // as all-zero bytes, so memset is safe and fast.
                if (x_ext_start > 0)
                    memset(dst_ptr, 0, static_cast<size_t>(x_ext_start) * sizeof(t));

                // Zone 2: [x_ext_start, x_in_start) — border (clamped trilinear sampling).
                if (x_in_start > x_ext_start)
                {
                    double src_x = scanline_base_x + tf.dx_src_x * x_ext_start;
                    double src_y = scanline_base_y + tf.dx_src_y * x_ext_start;
                    double src_z = scanline_base_z + tf.dx_src_z * x_ext_start;
                    for (uint32_t x = x_ext_start; x < x_in_start; ++x)
                    {
                        dst_ptr[x] = SampleTrilinearBorder<t>(
                            src_base, src_stride_line, src_stride_plane,
                            src_w, src_h, src_d, src_x, src_y, src_z);
                        src_x += tf.dx_src_x;
                        src_y += tf.dx_src_y;
                        src_z += tf.dx_src_z;
                    }
                }

                // Zone 3: [x_in_start, x_in_end) — inside (fast path, no classification).
                // This is the hot loop for the vast majority of pixels.
                if (x_in_end > x_in_start)
                {
                    double src_x = scanline_base_x + tf.dx_src_x * x_in_start;
                    double src_y = scanline_base_y + tf.dx_src_y * x_in_start;
                    double src_z = scanline_base_z + tf.dx_src_z * x_in_start;
                    for (uint32_t x = x_in_start; x < x_in_end; ++x)
                    {
                        dst_ptr[x] = SampleTrilinearInside<t>(
                            src_base, src_stride_line, src_stride_plane,
                            src_x, src_y, src_z);
                        src_x += tf.dx_src_x;
                        src_y += tf.dx_src_y;
                        src_z += tf.dx_src_z;
                    }
                }

                // Zone 4: [x_in_end, x_ext_end) — border (clamped trilinear sampling).
                if (x_ext_end > x_in_end)
                {
                    double src_x = scanline_base_x + tf.dx_src_x * x_in_end;
                    double src_y = scanline_base_y + tf.dx_src_y * x_in_end;
                    double src_z = scanline_base_z + tf.dx_src_z * x_in_end;
                    for (uint32_t x = x_in_end; x < x_ext_end; ++x)
                    {
                        dst_ptr[x] = SampleTrilinearBorder<t>(
                            src_base, src_stride_line, src_stride_plane,
                            src_w, src_h, src_d, src_x, src_y, src_z);
                        src_x += tf.dx_src_x;
                        src_y += tf.dx_src_y;
                        src_z += tf.dx_src_z;
                    }
                }

                // Zone 5: [x_ext_end, dst_w) — outside, zero-fill.
                if (x_ext_end < dst_w)
                    memset(dst_ptr + x_ext_end, 0, static_cast<size_t>(dst_w - x_ext_end) * sizeof(t));
            }
        }
    }

    // ---- Pixel-type dispatch --------------------------------------------------
    // The template warp functions above are instantiated for each supported pixel type.
    // These dispatch functions select the correct instantiation based on the runtime
    // pixel type of the source brick, mirroring the reference's DoNearestNeighbor /
    // DoLinearInterpolation.

    /// Dispatch nearest-neighbor warp to the correct template instantiation based on pixel type.
    void DoFastNearestNeighbor(
        const Brick& source_brick,
        const Brick& destination_brick,
        const IncrementalTransform& tf)
    {
        switch (source_brick.info.pixelType)
        {
        case PixelType::Gray16:
            FastNearestNeighborWarp<uint16_t>(source_brick, destination_brick, tf);
            break;
        case PixelType::Gray8:
            FastNearestNeighborWarp<uint8_t>(source_brick, destination_brick, tf);
            break;
        case PixelType::Gray32Float:
            FastNearestNeighborWarp<float>(source_brick, destination_brick, tf);
            break;
        default:
        {
            ostringstream ss;
            ss << "Unsupported pixel type: " << static_cast<int>(source_brick.info.pixelType);
            throw runtime_error(ss.str());
        }
        }
    }

    /// Dispatch trilinear warp to the correct template instantiation based on pixel type.
    void DoFastLinearInterpolation(
        const Brick& source_brick,
        const Brick& destination_brick,
        const IncrementalTransform& tf)
    {
        switch (source_brick.info.pixelType)
        {
        case PixelType::Gray16:
            FastTriLinearWarp<uint16_t>(source_brick, destination_brick, tf);
            break;
        case PixelType::Gray8:
            FastTriLinearWarp<uint8_t>(source_brick, destination_brick, tf);
            break;
        case PixelType::Gray32Float:
            FastTriLinearWarp<float>(source_brick, destination_brick, tf);
            break;
        default:
        {
            ostringstream ss;
            ss << "Unsupported pixel type: " << static_cast<int>(source_brick.info.pixelType);
            throw runtime_error(ss.str());
        }
        }
    }
} // anonymous namespace

void WarpAffine_Fast::Execute(
    const Eigen::Matrix4d& transformation,
    const IntPos3& destination_brick_position,
    Interpolation interpolation,
    const Brick& source_brick,
    const Brick& destination_brick)
{
    return WarpAffine_Fast::ExecuteFunction(transformation, destination_brick_position, interpolation, source_brick, destination_brick);
}

/*static*/void WarpAffine_Fast::ExecuteFunction(
    const Eigen::Matrix4d& transformation,
    const IntPos3& destination_brick_position,
    Interpolation interpolation,
    const Brick& source_brick,
    const Brick& destination_brick)
{
    // The source brick sits with its corner at the origin in a continuous coordinate system.
    // The caller supplies a 'transformation' that maps source coordinates to a global frame,
    // and a 'destination_brick_position' that says where the destination brick's corner is
    // in that same global frame.
    //
    // To find, for a destination voxel at (x, y, z), which source voxel it corresponds to,
    // we need:
    //     source_pos = (translation * transformation)^{-1} * [x, y, z, 1]^T
    //
    // where 'translation' shifts by -destination_brick_position so that destination voxel
    // (0,0,0) maps to destination_brick_position in the global frame.
    //
    // We combine translation and transformation first, then compute the inverse once in
    // PrepareIncrementalTransform and decompose it into the IncrementalTransform columns.
    Eigen::Matrix4d translation;
    translation << 1, 0, 0, -destination_brick_position.x_position,
        0, 1, 0, -destination_brick_position.y_position,
        0, 0, 1, -destination_brick_position.z_position,
        0, 0, 0, 1;
    const auto combined = translation * transformation;
    const IncrementalTransform tf = PrepareIncrementalTransform(combined);

    switch (interpolation)
    {
    case Interpolation::kNearestNeighbor:
        DoFastNearestNeighbor(source_brick, destination_brick, tf);
        break;
    case Interpolation::kBilinear:
        DoFastLinearInterpolation(source_brick, destination_brick, tf);
        break;
    default:
        throw invalid_argument("Only nearest-neighbor and linear interpolation are supported.");
    }
}
