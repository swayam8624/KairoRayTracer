# CPU Ray Tracer laboratory: deterministic pixels, BVH hits, threaded tiles and honest light sampling

Read [Sampler.cppm](../Sampler.cppm), [Renderer.cppm](../Renderer.cppm), [Camera.cppm](../Camera.cppm), [Scene.cppm](../Scene.cppm), [PathIntegrator.cppm](../PathIntegrator.cppm), [Film.cppm](../Film.cppm) and [ImageIO.cppm](../ImageIO.cppm). The companion [path tracing / BVH / light sampling guide](PATH_TRACING_LIGHT_SAMPLING_AND_BVH_LAB.md) explores the broader renderer; this lesson follows **one pixel** through a parallel CPU render and derives its numerical and concurrency guarantees.

## 1. Separate three coordinate domains

A Film pixel uses integer (x,y), with x in [0,width−1] and y in [0,height−1]. Sampler produces an offset (dx,dy) within that pixel, usually inside [0,1). Renderer converts this to normalized film coordinates:

\[
u=(x+dx)/\mathrm{width},\qquad v=(y+dy)/\mathrm{height}.
\]

Camera::GenerateRay then maps film coordinates through the configured camera model into a world-space Rayf. The Scene owns the geometry and material data that ray may intersect; Film owns CPU image storage, **not a native GPU framebuffer**.

**Worked example:** width=800, height=600, pixel (399,299), center sample (0.5,0.5). Then u=399.5/800≈0.499375 and v=299.5/600≈0.499167. These values differ slightly from exact center (0.5,0.5) because an even-sized image has no single central pixel.

## 2. Derive per-pixel stratification

StratifiedPixelSample takes (x,y,sampleIndex,samplesPerPixel,seedOffset). With SPP > 1 it chooses side=ceil(sqrt(SPP)). It indexes a stratum at sx=sampleIndex mod side, sy=floor(sampleIndex/side), then hashes a mix of x, y, sample index and user seed to obtain two repeatable jitters.

For SPP=4, side=2 and the four samples visit a 2×2 grid with jitter within each cell. For SPP=5, side=3, but only five of nine possible cells are visited; the last row is incomplete. This is stratified jitter, not a perfectly balanced full square lattice or a low-discrepancy Sobol sequence.

When SPP <=1 the method returns the deterministic center (dx,dy)=(0.5,0.5). Hash01 maps an integer input to a repeatable fractional value. It is not cryptographic randomness.

**Exercise:** calculate sx/sy for sample indices 0..4 at SPP=5. Explain which strata remain unvisited. Change only SampleSeed and verify that the stratum index stays the same while jitter changes.

## 3. Why thread count should not alter random rays

Renderer's Trace path derives its mutable xorshift bounce-RNG seed from the fixed pixel coordinates, sample index and SampleSeed. Neither the primary pixel offset nor initial path RNG uses the current worker index.

Suppose worker 0 renders tile 3 in one run, but worker 5 renders tile 3 in another. Both should generate the same ray/seed sequence for each pixel/sample, provided the scene data and settings are unchanged. Random draws *within* a path depend on which surfaces it hits, but not on the tile scheduler.

This makes the render reproducible under different tile-claim order. It does **not** prove bit-for-bit cross-platform floating-point equivalence: compiler math, CPU architecture and FMA choices can change final numerical results.

**Exercise:** compare the pixel sample offsets at thread counts 1 and 8, with identical seed and SPP. Then compare full linear Film values under fixed toolchain settings, reporting a max absolute error and any nonzero differences instead of assuming all platforms must be exact.

## 4. Derive tile ownership and data-race avoidance

Renderer divides Film width/height into rectangular tiles of at most TileSize on a side. TilesX=ceil(width/tileSize), TilesY=ceil(height/tileSize), and TileCount=TilesX×TilesY.

With width=10, height=7 and TileSize=4, there are three columns and two rows: 6 tiles. Their last column/row are cropped to width=2 and height=3. Every valid pixel belongs to one—and only one—tile.

Workers claim whole tile IDs using atomic fetch_add. The atomic operation ensures no tile is claimed twice; relaxed ordering is sufficient for unique integer distribution because individual pixel writes happen on disjoint memory locations and all threads are joined before Film becomes visible to the caller.

Each worker accumulates its own RenderStats, indexed by worker ID. The main thread merges worker statistics after joining. If a future integrator uses one shared mutable RNG or writes to arbitrary neighboring pixels without synchronization, this race-avoidance argument no longer holds.

**Exercise:** enumerate the 6 tile rectangles for the 10×7 case and prove they form a disjoint cover. Add a test that records a per-pixel write counter, then verify every counter is exactly one after rendering.

## 5. From acceleration candidates to a true surface hit

Scene::BuildAcceleration builds a KairoSpatial BVH from the current Primitives vector, using each primitive's AABB and its scene-local index. SAH or Morton selection changes traversal/build behavior but does not supply surface normals, UVs or material values.

Scene::Intersect passes an exact primitive intersection callback into KairoSpatial's Raycast. BVH box checks can prune search, but the callback must still intersect the actual sphere or triangle. After choosing the nearest hit, Scene intersects the winner again to recover renderer-specific SurfaceHit fields such as UVs and material index.

Scene::BruteForceIntersect provides a simple independent geometric oracle. If the scene's Primitives are edited or reindexed, BuildAcceleration must be called again or the old BVH's index-to-primitive mapping can become invalid.

**Exercise:** create a triangle whose AABB the ray crosses but whose interior the ray misses. A BVH node hit cannot be treated as a surface hit. Then mutate triangle vertices and compare BVH versus brute-force results before and after rebuilding the acceleration.

## 6. Shadow rays use a different query contract

An ordinary camera ray wants the *nearest* valid hit with shading data. A shadow query asks only whether *any* blocker exists between the surface and the light. Scene::Occluded uses the any-hit BVH path and a maximum distance to the finite light; it does not need material UVs or the nearest candidate.

The shadow ray is offset slightly from the visible surface normal to avoid numerical self-intersection, and finite-light probes stop just short of their destination to avoid treating the emitter as a blocker. Too-small bias causes acne, while too-large bias can detach shadows from surfaces.

**Exercise:** move a point light behind a thin occluder. Compare Intersect(ray) and Occluded(shadowRay,maxDistance), and vary maxDistance across the blocker. Record blocker existence separately from nearest-hit material.

## 7. Diffuse Monte Carlo continuation

PathIntegrator::CosineHemisphereDirection maps two random numbers into a hemisphere around the surface normal. For Lambert diffuse sampling, probability density is \(p(\omega)=\cos\theta/\pi\). The Lambert BRDF is \(f_r=\mathrm{albedo}/\pi\), and the rendering-equation contribution includes \(\cos\theta\), so \(f_r\cos\theta/p(\omega)\) simplifies to albedo for this idealized diffuse bounce.

TracePath returns environment radiance on misses and emission for emissive hits, computes explicit direct Lambert lighting and recursively follows one diffuse bounce until the depth limit or Russian roulette stops it. It currently **does not trace a full family of specular/dielectric BSDF lobes in its path continuation**; the Whitted/PBR integrators have different supported behaviors. The name “path” alone does not imply full material coverage.

After depth 2, roulette terminates based on survivalProbability in [0.15,0.95]. Surviving indirect contributions are divided by that probability to avoid expected darkening *from that termination alone*, conditional on the rest of the estimator being correct.

**Exercise:** with albedo=(0.2,0.4,0.6), probability 0.6, a surviving indirect sample's value is divided by 0.6. Compute expected contribution across many continue/terminate decisions. Compare mean with a no-roulette reference for the same simplified diffuse scene.

## 8. A serious MIS and area-light limitation

PowerHeuristic computes a weight \(w_A=p_A^2/(p_A^2+p_B^2)\). This is a standard *weight formula*, not a complete multiple-importance-sampling (MIS) estimator by itself. A correct two-technique MIS estimator must account for both sampled techniques, their actual PDFs in consistent measures and the weights applied to the correct path contributions.

The current area-light path samples a fixed 2×2 grid based on sample%2 and (sample/2)%2. If an area light requests 8 samples, it does not obtain eight distinct positions; it revisits the same four locations. It also applies a PDF-derived power weight to the explicit light probe without a fully demonstrated complementary BSDF-sampled direct-light contribution. Do **not** claim this is an unbiased physical reference implementation or full MIS merely because the weight appears in code.

**Exercise:** enumerate area sample (u,v) for sample indices 0..7. Confirm positions 4..7 repeat 0..3. Then design a fair reference comparison against a high-SPP independent area-light integrator; evaluate mean bias separately from per-frame variance, with fixed camera/emitter geometry.

## 9. Film and ImageIO are downstream of linear transport

Renderer accumulates Color3f samples in linear radiance and divides by SPP once per pixel. Film stores these values. ImageIO chooses PNG/PPM transfer or HDR encoding, potentially including exposure, tone mapping and clamping in output paths.

Comparing screenshots alone can hide numerical bias because transfer curves, tonemapping and 8-bit clipping compress differences. For algorithm validation, compare linear HDR/Film pixels first and preserve the exact output transformation.

**Exercise:** render the same scene to HDR and PNG with fixed SPP and seed, then identify which pixel differences arise only from transfer/clamping versus the underlying integrator.

## 10. Acceptance and falsification

Start with [RayTracerTests.cpp](../tests/RayTracerTests.cpp), [OfflineRenderJobTests.cpp](../tests/OfflineRenderJobTests.cpp) and [SharedLightSemanticsTests.cpp](../tests/SharedLightSemanticsTests.cpp). Test deterministic pixel jitter, incomplete strata at non-square SPP, tile coverage, disjoint writes, exact BVH versus brute force, shadow segment limits, bounce-depth cutoff, roulette mean and area-light position uniqueness.

For any measurement record scene SHA, Film resolution, SPP, seed, max depth, acceleration mode, threads, compiler/CPU, HDR output and linear numerical oracle. Keep **functional geometric correctness**, **unbiasedness**, **Monte Carlo variance**, **thread safety** and **render speed** as separate claims.

*Source-derived laboratory. No new C++ build, image render, statistical campaign or thread sanitizer was run during this documentation pass.*
