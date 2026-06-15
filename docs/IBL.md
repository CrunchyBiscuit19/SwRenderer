# Image-Based Lighting: Normalization and Sun Aliasing

This note explains two corrections made to the `SwIBL` bake so that surfaces lit
purely by the environment (a cobblestone road, say) read as matte stone instead of
wet asphalt. Both bugs lived in the **diffuse** ambient term, not in the metallic or
specular math.

## Symptom

With the sun off and only image-based lighting active, every surface looked too
shiny. Forcing a surface more metallic made it *less* shiny, which is the opposite of
normal intuition. Isolating the diffuse ambient term from the specular one confirmed
the shine came entirely from the diffuse irradiance.

The metallic relationship is the giveaway. Metallic enters the ambient in two places:

```slang
float3 F0 = lerp(float3(0.04f), albedo, metallic);  // specular tint
float3 kD = (1.f - F) * (1.f - metallic);            // diffuse gate
```

The diffuse term scales with `(1 - metallic)`, so it grows as metallic drops. The
specular term scales with `F0`, which grows as metallic rises. Since the shine grew
as metallic fell, the shine had to be the diffuse term. That term was behaving
exactly as physically-based shading prescribes, so the real problem was that the
diffuse ambient was simply too strong and slightly biased. Two separate fixes
address those two issues.

## Fix 1: Environment Normalization

### Why it is needed

The scene mixes two light sources measured in unrelated units.

- The **sun** intensity is an artist slider (`Sw::Sunlight::mIntensity`, roughly 0 to
  5). It is an arbitrary value with no physical anchor.
- The **environment** intensity comes straight from the `.hdr` file. An HDR captured
  outdoors stores real physical radiance, so an average sky pixel might carry a
  luminance of 4, 10, or more.

Nothing forced those two scales to agree. If the sun slider sits at 1.0 but the HDR
averages 4.0, the omnidirectional ambient fill silently outweighs the sun by four
times. A uniformly bright, flat surface reads to the eye as glossy or wet, so the
problem looked like a shininess bug when it was really a brightness mismatch.

### The fix

When an environment loads, `SwIBL::System::reinitializeOnUpdate` measures the average
brightness of the HDR into `mEnvAvgLuminance`, and the IBL contribution is divided by
that average in `SwGeometry::System::refreshPushConstants`:

```cpp
mResources.mWorkPushConstants.mIblIntensity =
    mScene.getIBLSystem().getIblIntensity() / mScene.getIBLSystem().getEnvAvgLuminance();
```

This re-bases the environment to an average brightness of 1.0. After it,
`IBL Intensity = 1` means a unit-mean fill regardless of whether the source HDR was a
dim interior or a blazing desert. The slider becomes a predictable, art-directable
knob whose meaning does not change every time the skybox is swapped.

### Why it is mathematically free

The irradiance map and the prefilter map are both **linear** functions of the
environment pixel values. Double every pixel and both maps double. Scaling the final
IBL result by `1 / avg` therefore produces the identical image to dividing every
environment pixel by `avg` before baking. That lets the correction live as a single
divide in the push-constant feed, with no change to the bake shaders, and it leaves
the visible skybox at full brightness. Only the light the objects receive is
normalized, not the backdrop.

### Cosine weighting

The average is weighted by `cos(latitude)`, computed per row:

```cpp
const float latitude = (((y + 0.5f) / height) - 0.5f) * glm::pi<float>();
const double rowWeight = std::cos(latitude);
```

An equirectangular image stores the same number of pixels for the pinched poles as
for the wide equator, so a naive pixel average over-counts the poles. Weighting each
row by the cosine of its latitude turns it into a true average over the sphere
(solid angle) rather than over the image.

## Fix 2: Sun Aliasing in the Irradiance Bake

### What the irradiance bake computes

For each direction, the irradiance map answers a single question: if a surface faces
this way, how much total light does it gather from the entire hemisphere above it?
`SwIBLIrradiance.comp.slang` answers it numerically by sampling the environment in
roughly 15,000 directions across the hemisphere and summing them.

### The trap

Those samples are spaced about 0.025 radians apart (close to 1.4 degrees). The sun in
an HDR is tiny, around 0.0087 radians (close to 0.5 degrees), which is smaller than
the gap between samples. It is also enormously bright, often thousands of times
brighter than the surrounding sky.

So whether any sample ray lands on the sun is essentially luck.

- Miss it and the sun is under-counted, so the diffuse comes out too dark.
- Hit it dead center and that one sample is treated as representing its whole
  `0.025 x 0.025` patch, smearing the sun across an area roughly ten times its true
  size. The diffuse then comes out far too bright, and only on the hemisphere facing
  the sun.

This is **aliasing**, the result of point-sampling a signal (the sun) more finely than
the sample rate can represent. It produces an unstable, over-bright, direction-biased
diffuse term.

### The fix: sample a pre-blurred mip

A mip chain stores the same image at halving resolutions (full, half, quarter, and so
on), where each level is a pre-averaged, blurred version of the level above. Rather
than always reading the sharp full-resolution environment, each sample now reads from
the mip level whose single texel covers about the same solid angle as the sample's
hemisphere patch:

```slang
float saSample = sin(theta) * sampleDelta * sampleDelta; // solid angle this sample represents
float mip = 0.5f * log2(max(saSample, 1e-12f) / saTexel);
irradiance += environmentMap.SampleLevel(SwBRDF::dirToEquirectUv(sampleVec), max(mip, 0.f)).rgb
              * cos(theta) * sin(theta);
```

At that mip the sun has already been averaged together with the sky around it into a
single texel. It no longer matters whether a ray lands exactly on the sun's center,
because the value read already contains the sun's energy spread over the correct area.
The total energy is unchanged, but the result is stable and unbiased instead of a coin
flip between too dark and too bright.

`saTexel` is the solid angle of one environment texel, taking the equirect to span the
full sphere of `4 * PI` steradians:

```slang
float saTexel = 4.f * SwBRDF::PI / float(envExtent.x * envExtent.y);
```

The specular prefilter pass (`SwIBLPrefilter.comp.slang`) already used this same
solid-angle mip selection to avoid fireflies. The irradiance pass had not received the
same treatment, so this change brings it in line.

## How the Two Fixes Divide the Work

- **Normalization** corrects the overall magnitude. The ambient was globally too
  strong relative to the sun.
- **Mip sampling** corrects a directional bias and an instability. The sun was being
  double-counted into the diffuse on its side of the hemisphere.

Together they produce a correctly scaled, well-behaved ambient, so a rough dielectric
surface looks like matte stone instead of a glossy reflective slab.

## Related Files

| File | Role |
|------|------|
| `src/System/SwIBL.cpp` | Loads the HDR and computes `mEnvAvgLuminance`. |
| `src/System/SwIBL.h` | Stores `mEnvAvgLuminance` and exposes `getEnvAvgLuminance`. |
| `src/System/SwGeometry.cpp` | Divides IBL intensity by the average luminance. |
| `shaders/IBL/SwIBLIrradiance.comp.slang` | Solid-angle mip selection in the diffuse bake. |
| `shaders/IBL/SwIBLPrefilter.comp.slang` | The specular bake that already used this trick. |
| `shaders/Geometry/SwGeometry.h.slang` | `ambientIBL` and `shadeLit`, which consume the IBL maps. |
