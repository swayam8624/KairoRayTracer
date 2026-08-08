# Phase 6 shared authored scene semantics

The offline renderer now carries the shared EngineCore scene lighting contract beyond point and rectangle-area emitters. `DirectionalLight` preserves authored illuminance with infinite-distance shadow rays, while `SpotLight` preserves position, forward direction, range, candela intensity, and inner/outer cone falloff. Whitted, PBR, and path-traced direct-light evaluation consume these light types through the same scene snapshot.

Beauty rendering also supports shared linear and exponential distance fog. Fog is applied to primary beauty samples using the authored fog color, density, near distance, and far distance; debug normal/depth/shadow modes remain diagnostic views and are intentionally not fogged.

The Phase 6 acceptance matrix builds these semantics with the shared Assets branch on the supported standalone toolchains before the umbrella repository pins the reviewed commit.
