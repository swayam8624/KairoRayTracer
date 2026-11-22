# Ray tracing from one camera sample to a pixel: a source-guided laboratory

Start with [Renderer.cppm](../Renderer.cppm), [Scene.cppm](../Scene.cppm), [PathIntegrator.cppm](../PathIntegrator.cppm), [Sampler.cppm](../Sampler.cppm), [Shading.cppm](../Shading.cppm) and [Film.cppm](../Film.cppm). Geometry and the KairoSpatial BVH contribute ray-hit algorithms, while the CPU renderer owns sampling, threads, statistics and image formation.

## 1. Follow one pixel through the renderer

A pixel (x,y) is not a ray until it receives a sampling offset. The renderer calls StratifiedPixelSample with pixel coordinates, sample index, samples-per-pixel and user-configured seed. The fractional image coordinates are

\[
u=(x+\Delta x)/W,\qquad v=(y+\Delta y)/H.
\]

Camera::GenerateRay converts that sensor point to a world-space origin/direction. A RenderMode chooses the integrator: normal/depth visualizer, Whitted reflection, direct PBR, diffuse path tracer or other debug modes.

**Exercise:** trace the center of a 4×4 image, identify the jitter used for the first sample, and compute its normalized (u,v). Compare one and four samples per pixel on the same scene and document how sample offset affects aliasing.

## 2. One primitive's lifetime in the scene

Scene owns dense arrays of Materials, Textures and Primitives (spheres/triangles). A triangle stores a MaterialIndex and optional UV/vertex normal data. The KairoSpatial BVH stores only primitive index and conservative AABB, not the full shading metadata.

BuildAcceleration walks the *current* primitive array, extracts bounds and builds BVH nodes. Adding or changing primitives after that requires rebuilding the index. An old BVH primitive index cannot be treated as a persistent KairoAssets ID.

Scene::Intersect uses BVH traversal with exact ray/primitive intersection callbacks to choose the nearest hit. Full SurfaceHit shading metadata is reconstructed for the chosen primitive. BruteForceIntersect checks every primitive without relying on BVH topology and should be used as an independent comparison oracle.

**Exercise:** put two spheres along one camera ray, with the farther one stored at a smaller array index. Confirm Intersect and BruteForceIntersect select the same *nearest distance*, not the first stored sphere. Then change primitive order and rebuild the BVH.

## 3. Shadow rays are existence queries

To light a surface, DirectLambert forms a ray from slightly above the surface point toward a light. It uses RayBiasForHit to reduce self-shadowing caused by floating-point intersection noise, and Scene::Occluded tests whether *any* primitive blocks the route.

For a point or spot light, the maximum shadow-ray distance stops before the light itself. A directional light is modeled as far away, so its shadow ray effectively has no finite physical emitter location.

Occluded need not return the closest hit's UV, barycentric coordinates or material, because a Boolean blocked/unblocked decision is sufficient. This makes its API and algorithm distinct from the full nearest-hit Intersect call.

**Exercise:** place an opaque sphere between a lit surface and a point light, then move it beyond the light. A correct max-distance shadow test should reject only the blocker on the segment to the emitter.

## 4. Sample a cosine-weighted diffuse direction

PathIntegrator::CosineHemisphereDirection draws two numbers in [0,1). Set azimuth \(\phi=2\pi u\), radius \(\rho=\sqrt v\), and local normal component \(y=\sqrt{1-v}\).

The local sample direction is approximately

\[
d=(\rho\cos\phi,\sqrt{1-v},\rho\sin\phi).
\]

A tangent/bitangent basis rotates this direction around the current surface normal. For a Lambertian cosine-weighted hemisphere the corresponding PDF is

\[
p_\omega(d)=\frac{\max(0,n\cdot d)}{\pi}.
\]

This choice aligns more sampling probability with high-cosine directions, reducing variance for a diffuse surface compared with many uniform hemisphere distributions.

**Exercise:** for u=0.25 and v=0.36, compute \(\phi=\pi/2\), radius 0.6, normal component 0.8. The local direction is (0,0.8,0.6). Verify its length is approximately one and that rotating the normal to a different world axis preserves dot(n,d)=0.8.

## 5. Recursion and throughput

TracePath stops at configured MaxDepth. A miss returns environment radiance; a hit on an emissive material returns emission. For ordinary materials, DirectLambert calculates direct contributions; one random cosine-weighted bounce recursively samples an indirect path.

The implemented indirect expression multiplies by diffuse albedo and compensates for Russian roulette survival. After depth 2, survivalProbability is clamped between a lower floor and 0.95 using albedo components. A stopped path contributes only its direct term, while a surviving indirect term is divided by survival probability.

This specific implementation is a bounded **diffuse** recursive estimator. It is not general path tracing over all metallic, dielectric, transmission or volumetric BSDFs just because those materials may exist elsewhere in the library.

**Exercise:** use a simple scene with one matte wall and an environment light. Compare MaxDepth 0,1,2,4; record mean radiance and noise across seeds, not just a single flattering screenshot.

## 6. Multiple importance sampling is more than a weight

PowerHeuristic computes a power-heuristic weight \(w_A=p_A^2/(p_A^2+p_B^2)\) for two PDFs. A complete multiple importance sampling estimator also needs the contributions of the complementary sampling strategies, properly converted measures, normalization and their individual probabilities.

The area-light code generates a deterministic 2×2 sample-position pattern using sample-index modulo two. **If Samples exceeds four, positions repeat.** It also constructs light and cosine-BSDF PDFs and a PowerHeuristic weight. The source does not establish a complete unbiased two-strategy light-plus-BSDF MIS estimator by itself.

This distinction matters scientifically: a picture becoming less noisy when increasing Samples is not proof that the expected radiance converges to the physically correct reference. The area and solid-angle PDF measures must be used consistently in the contribution.

**Exercise:** run an area-light-only scene at 1,2,4,8,16 samples and inspect unique emitter sample coordinates. Compare a brute-force high-quality reference and record both bias and variance. Design a separate functional test before changing source sampling logic.

## 7. CPU tiles and deterministic random streams

Renderer::Render partitions the image into nonoverlapping rectangular tiles. Each worker atomically claims a tile index, then writes only its pixels. Since no two tiles overlap, the Film can be shared without a pixel mutex. Every worker writes into its own RenderStats slot; stats are combined only after all workers join.

Crucially, the path sampler's RNG seed is derived from (pixel x,y,sample,global seed), *not* thread index or completion order. This permits repeatable sampling under different tile schedules, subject to numerical and platform assumptions.

**Exercise:** render the same scene with one and four worker threads and compare the resulting Film pixels, statistics and elapsed time. Record the same seed, image resolution and compiler flags. Timing differences are not evidence of image-quality differences.

## 8. What renderer statistics do and do not count

The renderer reports primary/shadow rays, hits/misses, recursion depth, BVH traversal and wall-clock render duration. ApplyPrimaryFog performs an additional first-hit lookup but deliberately passes a null statistics pointer. Its intersection work is omitted from the existing counted diagnostic rays.

A comparable performance benchmark must declare whether it measures CPU wall time, BVH traversals, intersection operations or final image noise. A lower ray count alone does not prove lower render time, and a GPU timestamp is not available from this CPU Film pipeline.

## 9. Tests and next experiments

Use the repository's [full reading order](SOURCE_READING_ORDER.md) to find exact integrator, BVH, sampling and image tests. Add property tests comparing accelerated and brute-force hit identities, repeatability across thread counts, ray-bias self-shadow cases, bounded recursion, finite radiance under zero-distance lights, area-light sample uniqueness and MIS comparison against a high-quality reference.

Keep the source SHA, image seed, number of samples, depth, light geometry, compiler, CPU, render modes and error metrics with each output. Scientific claims about unbiasedness require actual paired estimators and a reference study—not merely a helper named PowerHeuristic.

*Evidence: this guide is based on source inspection. No new CPU render or benchmark was executed for this documentation pass.*
