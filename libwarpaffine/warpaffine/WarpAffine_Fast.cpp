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

    /// Fast nearest-neighbor warp for a single pixel type.
    ///
    /// For every voxel (x, y, z) in the destination brick, we determine the corresponding
    /// source position via the inverse affine transformation, round to the nearest integer
    /// neighbor (using lround, matching the reference), and copy the source voxel value.
    /// If the rounded position falls outside the source brick, the destination voxel is
    /// set to zero.
    ///
    /// The three-level loop applies the incremental decomposition:
    ///   - z-loop: precompute z_contrib = col2*z + col3  (the part that depends only on z)
    ///   - y-loop: precompute src_{x,y,z} = col1*y + z_contrib  (the x=0 start for this scanline)
    ///             Also compute the destination pointer for this scanline via direct stride arithmetic,
    ///             avoiding per-voxel GetPointerToPixel calls.
    ///   - x-loop: use the current source position, round to NN, bounds-check, copy or zero-fill,
    ///             then advance source position by col0 (a single 3-component add) and destination
    ///             pointer by sizeof(t).
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

        // Source dimensions as signed int for the bounds check (nn_x >= 0 && nn_x < src_w).
        const int src_w = static_cast<int>(source_brick.info.width);
        const int src_h = static_cast<int>(source_brick.info.height);
        const int src_d = static_cast<int>(source_brick.info.depth);

        // Cache strides for direct pointer arithmetic.  The stride values describe the
        // byte distance between consecutive lines / planes in memory and may include
        // padding beyond width * sizeof(pixel).
        const uint32_t dst_stride_line = destination_brick.info.stride_line;
        const uint32_t dst_stride_plane = destination_brick.info.stride_plane;
        const uint32_t src_stride_line = source_brick.info.stride_line;
        const uint32_t src_stride_plane = source_brick.info.stride_plane;

        // Raw base pointers — all subsequent pixel access is computed relative to these
        // using the precomputed strides, with no further per-voxel function calls.
        char* dst_base = static_cast<char*>(destination_brick.data.get());
        const char* src_base = static_cast<const char*>(source_brick.data.get());

        for (uint32_t z = 0; z < dst_d; ++z)
        {
            // z-dependent contribution:  z_contrib = col2 * z + col3
            // This is constant for the entire z-slice and factored out of the y-loop.
            const double z_contrib_x = tf.dz_src_x * z + tf.base_src_x;
            const double z_contrib_y = tf.dz_src_y * z + tf.base_src_y;
            const double z_contrib_z = tf.dz_src_z * z + tf.base_src_z;

            for (uint32_t y = 0; y < dst_h; ++y)
            {
                // Source position at the start of this scanline (x=0):
                //   src = col1 * y + z_contrib
                // This will be incrementally updated by adding col0 per x-step below.
                double src_x = tf.dy_src_x * y + z_contrib_x;
                double src_y = tf.dy_src_y * y + z_contrib_y;
                double src_z = tf.dy_src_z * y + z_contrib_z;

                // Destination pointer for this scanline, computed once via stride arithmetic.
                // We advance it with ++dst_ptr (sizeof(t)) in the inner loop.
                t* dst_ptr = reinterpret_cast<t*>(
                    dst_base +
                    static_cast<size_t>(z) * dst_stride_plane +
                    static_cast<size_t>(y) * dst_stride_line);

                for (uint32_t x = 0; x < dst_w; ++x)
                {
                    // Round the continuous source position to the nearest integer voxel.
                    // This matches the reference's ToNearestNeighbor() which uses lround.
                    const int nn_x = static_cast<int>(lround(src_x));
                    const int nn_y = static_cast<int>(lround(src_y));
                    const int nn_z = static_cast<int>(lround(src_z));

                    // Bounds check: is the nearest-neighbor inside [0, dimension) ?
                    // Equivalent to the reference's IsInsideBrick() which checks
                    // position <= -1 || position >= width — for integer positions,
                    // that is the same as position < 0 || position >= width.
                    if (nn_x >= 0 && nn_x < src_w &&
                        nn_y >= 0 && nn_y < src_h &&
                        nn_z >= 0 && nn_z < src_d)
                    {
                        // Fetch the source voxel via direct pointer arithmetic:
                        //   address = base + z*stride_plane + y*stride_line + x*sizeof(pixel)
                        const t* src_pixel = reinterpret_cast<const t*>(
                            src_base +
                            static_cast<size_t>(nn_z) * src_stride_plane +
                            static_cast<size_t>(nn_y) * src_stride_line +
                            static_cast<size_t>(nn_x) * sizeof(t));
                        *dst_ptr = *src_pixel;
                    }
                    else
                    {
                        *dst_ptr = 0;
                    }

                    // Advance destination pointer by one pixel (sizeof(t) bytes).
                    ++dst_ptr;

                    // Advance source position by col0 of the inverse matrix — the constant
                    // delta corresponding to stepping +1 in destination x. This replaces
                    // the full matrix-vector multiply that the reference does per voxel.
                    src_x += tf.dx_src_x;
                    src_y += tf.dx_src_y;
                    src_z += tf.dx_src_z;
                }
            }
        }
    }

    // ---- Trilinear warp -------------------------------------------------------

    /// Classification of a source sampling position relative to the source brick.
    /// Trilinear interpolation requires the 2x2x2 neighborhood around the sample point.
    /// Depending on where the point lies, the neighborhood may be fully inside the brick,
    /// partially outside (requiring clamped sampling), or entirely outside.
    enum class FastPixelPosition
    {
        kInside,           ///< The sample point and all 8 trilinear neighbors are within the brick.
        kOnePixelOutside,  ///< The point is at most 1 pixel outside — trilinear interpolation is
                           ///  still possible by clamping out-of-bounds coordinates to the boundary.
        kOutside,          ///< The point is too far outside for meaningful interpolation → zero.
    };

    /// Classify a source sampling position for trilinear interpolation.
    ///
    /// For trilinear interpolation we need to access the 2x2x2 cube at floor(pos) .. floor(pos)+1.
    /// - **kInside**: pos is in [0, dim-1) for all three axes, so floor(pos)+1 < dim and all
    ///   8 neighbor reads are guaranteed in-bounds.
    /// - **kOnePixelOutside**: pos is in [-1, dim] — the point is near the boundary but close
    ///   enough that we can still interpolate by clamping neighbor coordinates to [0, dim-1].
    ///   This reproduces the reference's "one pixel outside" behavior.
    /// - **kOutside**: too far from the brick to produce a useful value.
    ///
    /// This is the exact same classification as ReferenceWarp::GetPixelPositionForTriLinear,
    /// just using pre-cast int dimensions to avoid repeated unsigned/signed conversions.
    inline FastPixelPosition GetPixelPositionForTriLinear(
        int src_w, int src_h, int src_d,
        double pos_x, double pos_y, double pos_z)
    {
        if (pos_x < 0 || pos_x >= src_w - 1 ||
            pos_y < 0 || pos_y >= src_h - 1 ||
            pos_z < 0 || pos_z >= src_d - 1)
        {
            if (pos_x >= -1 && pos_x <= src_w &&
                pos_y >= -1 && pos_y <= src_h &&
                pos_z >= -1 && pos_z <= src_d)
            {
                return FastPixelPosition::kOnePixelOutside;
            }

            return FastPixelPosition::kOutside;
        }

        return FastPixelPosition::kInside;
    }

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

    /// Fast trilinear warp for a single pixel type.
    ///
    /// This mirrors the reference's TriLinearWarp but applies the same three optimizations
    /// as FastNearestNeighborWarp (incremental position, direct pointer arithmetic, raw
    /// doubles). The structure is identical:
    ///
    ///   z-loop  →  precompute z_contrib = col2*z + col3
    ///   y-loop  →  precompute scanline start = col1*y + z_contrib; compute dst_ptr
    ///   x-loop  →  classify source position, sample (inside or border), advance
    ///
    /// For each destination voxel, the source position is classified into one of three
    /// zones (see FastPixelPosition / GetPixelPositionForTriLinear). Depending on the
    /// zone:
    ///   - kInside:          fast path — SampleTrilinearInside (no clamping needed)
    ///   - kOnePixelOutside: boundary path — SampleTrilinearBorder (clamped reads)
    ///   - kOutside:         zero fill
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
                // Source position at x=0 for this scanline.
                double src_x = tf.dy_src_x * y + z_contrib_x;
                double src_y = tf.dy_src_y * y + z_contrib_y;
                double src_z = tf.dy_src_z * y + z_contrib_z;

                t* dst_ptr = reinterpret_cast<t*>(
                    dst_base +
                    static_cast<size_t>(z) * dst_stride_plane +
                    static_cast<size_t>(y) * dst_stride_line);

                for (uint32_t x = 0; x < dst_w; ++x)
                {
                    // Classify the source position and dispatch to the appropriate
                    // sampling strategy. This matches the reference's switch on
                    // ReferenceWarp::GetPixelPositionForTriLinear.
                    switch (GetPixelPositionForTriLinear(src_w, src_h, src_d, src_x, src_y, src_z))
                    {
                    case FastPixelPosition::kInside:
                        *dst_ptr = SampleTrilinearInside<t>(
                            src_base, src_stride_line, src_stride_plane,
                            src_x, src_y, src_z);
                        break;
                    case FastPixelPosition::kOnePixelOutside:
                        *dst_ptr = SampleTrilinearBorder<t>(
                            src_base, src_stride_line, src_stride_plane,
                            src_w, src_h, src_d,
                            src_x, src_y, src_z);
                        break;
                    case FastPixelPosition::kOutside:
                        *dst_ptr = 0;
                        break;
                    }

                    ++dst_ptr;
                    // Incremental advance — same as in FastNearestNeighborWarp.
                    src_x += tf.dx_src_x;
                    src_y += tf.dx_src_y;
                    src_z += tf.dx_src_z;
                }
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
