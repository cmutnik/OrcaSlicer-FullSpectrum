# Ironing Guide

## Quick Reference — Settings Cheat Sheet

| Mode | Min Angle | Max Angle | Z-Step | Firmware Required |
|------|-----------|-----------|--------|-------------------|
| SlopeSurfaces basic | 15° | 60° | off | any |
| SlopeSurfaces + Z-step | 15° | 60° | on | Klipper non-planar or Snapmaker non-planar |
| NonPlanar | — | — | — | Klipper non-planar or Snapmaker non-planar |

For Snapmaker firmware compatibility, confirm that `G1 Zx.xxx` mid-layer moves (without a `G92` reset) are accepted before running a full print. Test on a single-layer print first.

---

## Ironing Type Comparison

| Ironing Type | Surfaces Targeted | Orientation of Lines | Z Variation | Extra Config Options | Firmware | Best For | Limitations |
|---|---|---|---|---|---|---|---|
| **No Ironing** | None | — | None | — | Any | Fastest slicing; no surface post-processing | No smoothing at all |
| **Top Surfaces** | All `stTop` layers except the topmost | Configurable angle (default: follow infill) | None (planar) | flow, spacing, angle, inset | Any | General flat-top smoothing on all solid top layers (e.g. boxes, lids) | Misses curved/sloped walls; multiple layers ironed even when not visible |
| **Topmost Only** | Only the single highest `stTop` layer | Configurable angle | None (planar) | flow, spacing, angle, inset | Any | Fastest ironing; smooths only the very top face visible to viewer | Completely ignores all side slopes and intermediate flat layers |
| **All Solid** | All `stTop` + `stBottom` layers (not bridges) | Configurable angle | None (planar) | flow, spacing, angle, inset | Any | Maximum flat-surface coverage; useful for functional prints with multiple exposed solid faces | Irons bottom surfaces that are never visible; slowest of the planar modes; no benefit on angled walls |
| **Slope Surfaces** | Narrow `stTop` bands on angled walls (slope detected by band width vs. layer height) | Automatically aligned **parallel** to the slope contour via 2D PCA on the band polygon | Optional: linear Z ramp per line (`ironing_slope_zstep`) spans one full layer height across the band | `ironing_slope_min_angle` (default 15°), `ironing_slope_max_angle` (default 60°), `ironing_slope_zstep` (default off) | Any for basic; non-planar firmware for Z-step | Figurine torsos, chamfers, angled lids, 3DBenchy hull sides — any print with stair-stepped angled walls | Angle detection uses area/perimeter heuristic; very irregular polygons may be misclassified; Z-step approximates slope with linear ramp, not true mesh surface |
| **Non-Planar (mesh-accurate)** | All `stTop` surfaces (same collection as Top Surfaces) | Configurable angle | Per-point Z from AABB mesh raycasting — every ironing segment endpoint gets its own Z queried from the actual model mesh | None beyond standard ironing params | **Requires** non-planar firmware (Klipper non-planar macro, Snapmaker non-planar mode, or Marlin `NONPLANAR_ENABLED`) | Organic shapes, spheres, domes, figurines — any surface where the true mesh Z deviates from the nominal layer Z | High compute cost (AABB tree build + one ray per ironing point); silently degrades to flat ironing on non-capable firmware; open or non-manifold meshes produce fallback flat Z |

---

## Choosing the Right Mode

```
Does the print have only flat top/bottom surfaces?
    → Top Surfaces or Topmost Only

Does the print have angled walls (chamfers, ramps, figurine torsos)?
    → Slope Surfaces
    → Enable Z-stepping if your firmware supports mid-layer Z moves

Does the print have complex organic curves (spheres, domes, faces)?
    → Non-Planar
    → Requires non-planar-capable firmware — verify before printing

Do you need maximum coverage of all flat faces regardless of position?
    → All Solid

Do you want no ironing at all?
    → No Ironing
```

---

## Slope Surfaces — Angle Reference

The `ironing_slope_min_angle` and `ironing_slope_max_angle` settings filter which stair-step bands receive ironing passes.

| Surface Type | Approximate Angle | Include? |
|---|---|---|
| Nearly flat top face | 0–14° | No — already handled by Top Surfaces mode |
| Gentle chamfer (e.g. 15° bevel) | 15–25° | Yes (at default min 15°) |
| Figurine torso / typical slope | 25–50° | Yes |
| Steep wall / near-vertical | 60–89° | No (default max 60°) — nozzle cannot reach |

Increase `ironing_slope_max_angle` cautiously above 60° — the nozzle risks colliding with adjacent perimeters on steep walls.

---

## Non-Planar Firmware Setup Notes

### Klipper
Enable `[force_move]` and install a non-planar ironing macro that allows Z moves during a layer without triggering a layer-change routine. Community macro available in the Klipper repository.

### Snapmaker
Non-planar Z moves require firmware **≥ 2.x with non-planar mode enabled**. Verify with a single-layer test print using a GCode file that contains `G1 Z` moves at non-layer-boundary heights before committing to a full figurine print.

### Marlin
Build with `NONPLANAR_ENABLED` in `Configuration_adv.h`. Standard Marlin releases do not include this by default.

### Fallback Behaviour
If the firmware does not support mid-layer Z moves, **Non-Planar ironing silently degrades to flat ironing** at the nominal layer height. No print failure occurs, but the Z variation is lost and surface quality equals standard Top Surfaces ironing.
