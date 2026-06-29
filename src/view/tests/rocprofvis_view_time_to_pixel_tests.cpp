// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Unit tests for TimePixelTransform (rocprofvis_time_to_pixel.{h,cpp}). This is
// the affine time<->pixel mapping at the heart of the timeline. It is pure
// math (the only ImGui dependency is ImVec2 for GetGraphSize) so it can be
// fully exercised without a render context.

#include "rocprofvis_time_to_pixel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace RocProfVis::View;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
// Build a transform over [0, range] ns mapped onto `width` px at zoom 1 (fully
// zoomed out) and force the mapping to be recomputed.
TimePixelTransform
MakeTransform(double min_ns, double max_ns, float width, float height = 100.0f)
{
    TimePixelTransform tpt;
    tpt.SetMinMaxX(min_ns, max_ns);
    tpt.SetGraphSize(width, height);
    tpt.ComputePixelMapping();
    return tpt;
}
}  // namespace

TEST_CASE("SetMinMaxX rejects degenerate ranges", "[tpt]")
{
    TimePixelTransform tpt;
    tpt.SetMinMaxX(100.0, 100.0);  // max == min -> ignored
    CHECK(tpt.GetMinX() != 100.0);  // still the sentinel default

    tpt.SetMinMaxX(200.0, 100.0);  // max < min -> ignored
    CHECK(tpt.GetMinX() != 200.0);

    tpt.SetMinMaxX(0.0, 1000.0);  // valid
    CHECK_THAT(tpt.GetMinX(), WithinAbs(0.0, 1e-9));
    CHECK_THAT(tpt.GetMaxX(), WithinAbs(1000.0, 1e-9));
    CHECK_THAT(tpt.GetRangeX(), WithinAbs(1000.0, 1e-9));
}

TEST_CASE("Fully zoomed out, pixels per ns spans the whole range", "[tpt]")
{
    // 1000 ns across 500 px -> 0.5 px/ns at zoom 1.
    TimePixelTransform tpt = MakeTransform(0.0, 1000.0, 500.0f);
    CHECK_THAT(tpt.GetPixelsPerNs(), WithinRel(0.5, 1e-9));
    CHECK_THAT(tpt.GetVWidth(), WithinRel(1000.0, 1e-9));
}

TEST_CASE("TimeToPixel maps view endpoints to [0, width]", "[tpt]")
{
    TimePixelTransform tpt = MakeTransform(0.0, 1000.0, 500.0f);

    // With offset 0 and zoom 1: view time 0 -> 0 px, view time 1000 -> 500 px.
    CHECK_THAT(tpt.TimeToPixel(0.0), WithinAbs(0.0f, 1e-4f));
    CHECK_THAT(tpt.TimeToPixel(1000.0), WithinAbs(500.0f, 1e-3f));
    CHECK_THAT(tpt.TimeToPixel(500.0), WithinAbs(250.0f, 1e-3f));
}

TEST_CASE("RawTimeToPixel accounts for the min_x offset", "[tpt]")
{
    // Range starts at 100 ns. Raw time 100 is the left edge.
    TimePixelTransform tpt = MakeTransform(100.0, 1100.0, 500.0f);
    CHECK_THAT(tpt.RawTimeToPixel(100.0), WithinAbs(0.0f, 1e-3f));
    CHECK_THAT(tpt.RawTimeToPixel(1100.0), WithinAbs(500.0f, 1e-3f));
}

TEST_CASE("PixelToTime is the inverse of the view mapping", "[tpt]")
{
    TimePixelTransform tpt = MakeTransform(0.0, 1000.0, 500.0f);
    // Halfway across the graph is halfway through the view.
    CHECK_THAT(tpt.PixelToTime(250.0f), WithinRel(500.0, 1e-6));
    CHECK_THAT(tpt.PixelToTime(0.0f), WithinAbs(0.0, 1e-6));
    CHECK_THAT(tpt.PixelToTime(500.0f), WithinRel(1000.0, 1e-6));
}

TEST_CASE("Normalize/Denormalize round-trip through min_x", "[tpt]")
{
    TimePixelTransform tpt;
    tpt.SetMinMaxX(100.0, 1100.0);
    CHECK_THAT(tpt.NormalizeTime(600.0), WithinAbs(500.0, 1e-9));
    CHECK_THAT(tpt.DenormalizeTime(500.0), WithinAbs(600.0, 1e-9));
    CHECK_THAT(tpt.DenormalizeTime(tpt.NormalizeTime(742.0)), WithinAbs(742.0, 1e-9));
}

TEST_CASE("SetZoom clamps to the minimum zoom-out extent", "[tpt]")
{
    TimePixelTransform tpt = MakeTransform(0.0, 1000.0, 500.0f);
    // Zoom < 1.0 is invalid (1.0 is fully zoomed out); it is clamped to 1.0.
    tpt.SetZoom(0.1f);
    CHECK_THAT(tpt.GetZoom(), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Zooming in narrows the view width and increases pixel density", "[tpt]")
{
    TimePixelTransform tpt = MakeTransform(0.0, 1000.0, 500.0f);
    const double base_width   = tpt.GetVWidth();
    const double base_density = tpt.GetPixelsPerNs();

    tpt.SetZoom(2.0f);
    tpt.ComputePixelMapping();

    CHECK(tpt.GetVWidth() < base_width);
    CHECK(tpt.GetPixelsPerNs() > base_density);
    // At zoom 2, view width is half the range.
    CHECK_THAT(tpt.GetVWidth(), WithinRel(500.0, 1e-6));
}

TEST_CASE("ValidateAndFixState caps zoom so density never exceeds 1 px/ns", "[tpt]")
{
    // Range 1000 ns over 500 px: max meaningful zoom is range/width = 2.0.
    TimePixelTransform tpt = MakeTransform(0.0, 1000.0, 500.0f);
    tpt.SetZoom(1000.0f);  // absurd zoom-in
    tpt.ComputePixelMapping();
    // Zoom is capped at range/width, so pixels_per_ns <= 1.0.
    CHECK(tpt.GetZoom() <= 2.0f + 1e-3f);
    CHECK(tpt.GetPixelsPerNs() <= 1.0 + 1e-6);
}

TEST_CASE("ValidateAndFixState returns false for an uninitialized range", "[tpt]")
{
    TimePixelTransform tpt;  // min/max at sentinel defaults: max <= min
    CHECK_FALSE(tpt.ValidateAndFixState());
}

TEST_CASE("ZoomAtPixel preserves the time under the cursor", "[tpt]")
{
    TimePixelTransform tpt = MakeTransform(0.0, 1000.0, 500.0f);

    // The time currently under pixel 250 (the middle) is 500 ns.
    const float cursor_px        = 250.0f;
    const double time_before     = tpt.PixelToTime(cursor_px);

    tpt.ZoomAtPixel(cursor_px, 1.0f);  // zoom in by 100%
    tpt.ComputePixelMapping();

    const double time_after = tpt.PixelToTime(cursor_px);
    CHECK_THAT(time_after, WithinAbs(time_before, 1e-3));
}

TEST_CASE("SetGraphSize ignores non-positive dimensions", "[tpt]")
{
    TimePixelTransform tpt;
    tpt.SetGraphSize(640.0f, 480.0f);
    CHECK_THAT(tpt.GetGraphSizeX(), WithinAbs(640.0f, 1e-6f));
    CHECK_THAT(tpt.GetGraphSizeY(), WithinAbs(480.0f, 1e-6f));

    tpt.SetGraphSize(0.0f, 100.0f);   // ignored
    tpt.SetGraphSize(100.0f, -1.0f);  // ignored
    CHECK_THAT(tpt.GetGraphSizeX(), WithinAbs(640.0f, 1e-6f));
    CHECK_THAT(tpt.GetGraphSizeY(), WithinAbs(480.0f, 1e-6f));

    ImVec2 size = tpt.GetGraphSize();
    CHECK_THAT(size.x, WithinAbs(640.0f, 1e-6f));
    CHECK_THAT(size.y, WithinAbs(480.0f, 1e-6f));
}

TEST_CASE("Reset restores zoom and pan to defaults", "[tpt]")
{
    TimePixelTransform tpt = MakeTransform(0.0, 1000.0, 500.0f);
    tpt.SetZoom(2.0f);
    tpt.SetViewTimeOffsetNs(100.0);
    tpt.ComputePixelMapping();

    tpt.Reset();
    CHECK_THAT(tpt.GetZoom(), WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(tpt.GetViewTimeOffsetNs(), WithinAbs(0.0, 1e-6));
}
