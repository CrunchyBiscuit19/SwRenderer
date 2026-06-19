# Image-Based Lighting

The renderer turns a single equirectangular HDR environment into the three baked maps the geometry shaders sample, and how those maps reconstruct ambient lighting at runtime. 

$N$ normal, $V$ view (surface to camera), $L$ light (surface to source), $H$ half-vector, $R$ reflection, and $a = r^2$  as the GGX roughness remap of the material roughness $r$. 
Accumulation in a sampling loop is written $x \leftarrow x + \dots$.

## What is IBL

The reflectance equation gives outgoing radiance toward the camera as an integral over the hemisphere $\Omega$ of incoming directions.

$L_o(V) = \displaystyle\int_{\Omega} f_r(L, V)\, L_i(L)\, (N \cdot L)\, \mathrm{d}L$,      where $L_i(L)$ is the environment radiance arriving from every direction for a point lit by the environment. 

Evaluating this integral per pixel per frame is too expensive, so IBL pre-integrates everything that does not depend on the runtime surface and stores the result in textures. 

The Cook-Torrance BRDF $f_r$ splits into a diffuse and a specular lobe, and each lobe gets its own precomputation.

The environment is stored as an equirectangular map, so every direction $d$ to texture lookup goes through `dirToEquirectUv`. 
With $\phi = \operatorname{atan2}(d_z, d_x)$ and $\theta = \arcsin(d_y)$.

$\mathbf{uv} = \left(\dfrac{\phi}{2\pi} + \dfrac{1}{2},\ \ \dfrac{\theta}{\pi} + \dfrac{1}{2}\right)$

`equirectUvToDir` is the exact inverse. $\phi$ is longitude (wraps, so the sampler repeats in U) and $\theta$ is latitude (clamps at the poles, so the sampler clamps in V). 

### Texel Coverage

Every texel of the environment map corresponds to a direction the shaded point could look toward, and together all $W \times H$ texels cover every possible direction. 
The full set of directions around a point forms a sphere. 

The size of "all directions combined" is measured in **steradians**, the 3D analogue of the radian. 
Just as a full circle is $2\pi$ radians, a full sphere of directions is $4\pi$ steradians. 
The amount of that surrounding view a patch (an area on the sphere) takes up is its **solid angle**, and saying a patch *subtends* a solid angle just means "as seen from the center, it fills up that much of your view."

To find how much of the view one texel takes up, split the whole sphere evenly across all the texels.
$\Omega_\text{texel} = \dfrac{4\pi}{W \cdot H} = \dfrac{\text{all directions}}{\text{texel count}}$

An equirectangular map stretches its top and bottom rows the way a flat world map balloons the polar regions, so a texel near the poles actually covers a thinner sliver of directions than one near the equator. 
The exact per-texel value varies with latitude, but $\dfrac{4\pi}{W \cdot H}$ is the mean over the whole image and is accurate enough while avoiding per-texel trigonometry.

Every bake needs $\Omega_\text{texel}$ for **mip selection**. 
Each Monte-Carlo sample $\Omega_\text{sample}$ stands in for a small patch of directions, and the shader reads from the environment at the mip level where the texel covers the same patch the sample represents. 
(So the light level inside the patch is pre-averaged over that texel.)
So $\Omega_\text{texel}$ is the yardstick the mip math in sections 2 and 3 measures against.

## Diffuse Irradiance Map

> Radiance $L_{i}(L)$ — The light traveling along a single ray/direction (a brightness-per-direction).
> Irradiance $E(N)$ — The total light landing on a surface, summed over every incoming direction in the hemisphere, with each direction weighted by its grazing angle ($N \cdot L$).

`SwIBLIrradiance.comp.slang` bakes a small (64x32) map where each texel stores the **irradiance** arriving at a surface whose normal points in that texel's direction.

For a Lambertian surface the diffuse BRDF is constant, $f_r = \dfrac{C_{\text{diffuse}}}{\pi}$, so it pulls out of the integral and what remains is the cosine-weighted integral of incoming radiance.

$E(N) = \displaystyle\int_{\Omega} L_i(L)\, (N \cdot L)\, \mathrm{d}L$

The shader evaluates this by Riemann-summing over the hemisphere in spherical coordinates around $N$. 
With a tangent basis $(\text{right}, \text{up}, N)$,  it walks $\phi$ in $[0, 2\pi)$ and $\theta$ in $[0, \pi/2)$ at a fixed step $\delta = 0.025$, taking the tangent-space sample.

$\hat{s} = (\sin\theta\cos\phi,\ \sin\theta\sin\phi,\ \cos\theta)$,  then rotated into world space as `sampleVec`, and accumulating as $E \leftarrow E + L_i(\text{sampleVec})\,\cos\theta\,\sin\theta.$

The $\cos\theta$ is the $N \cdot L$ cosine term, and the extra $\sin\theta$ is the spherical-area element $\mathrm{d}L = \sin\theta\,\mathrm{d}\theta\,\mathrm{d}\phi$ that converts the uniform grid in $(\theta, \phi)$ into a correct solid-angle measure. 

After summing $n$ samples the result is normalized:

$E(N) = \dfrac{\pi}{n} \displaystyle\sum_{i=1}^{n} L_i\,\cos\theta_i\,\sin\theta_i$

The $\pi$ factor folds the BRDF's $\dfrac{1}{\pi}$ and the hemisphere's solid angle into the stored value so that at runtime the diffuse term is simply $\text{albedo}\cdot E(N)$ with no further $\pi$ (see section 5). 
This is the standard convention where the $\pi$ in the bake and the $\dfrac{1}{\pi}$ in the BRDF cancel.

**Mip selection.** Each sample stands in for a patch of solid angle $\Omega_\text{sample} = \sin\theta\,\delta^2$. 
A sub-texel-sized bright source (the sun) would alias badly if point-sampled at full resolution, so the shader reads the environment from the mip whose texel covers a matching solid angle:

$\text{mip} = \dfrac{1}{2} \log_2\!\left(\dfrac{\Omega_\text{sample}}{\Omega_\text{texel}}\right)$

The factor of $\dfrac{1}{2}$ is because solid angle scales with the **square** of linear texel size, and each mip halves linear size (quarters solid angle), so $\log_2$ of an area ratio is twice the mip step. 
This pre-averages the sun into its surrounding sky so the diffuse term is stable.

## Specular Pre-Filter Map

The specular lobe cannot be reduced to a single environment integral because it depends on both view and roughness. 
Epic's **split-sum approximation** factors it into two independent pieces.

$\text{specular}_\text{IBL} \approx \underbrace{\text{prefiltered}(R, r)}_{\text{Prefilter}} \cdot \underbrace{(F_0\,\text{scale} + \text{bias})}_{\text{Integration LUT}}$

`SwIBLPrefilter.comp.slang` bakes the first factor. The map (128x64) is **mip-chained**, and the mip index encodes roughness. 
Mip level $m$ is baked with $r = \dfrac{m}{m_\text{max}}$ where $m_\text{max} = \text{mipCount} - 1$. 
The sharpest mip is a mirror reflection and each coarser mip is blurred by a wider GGX lobe.

The split-sum's first simplification is to assume the normal, view and reflection all coincide, $N = V = R$. With that, the prefiltered color is the GGX-weighted average of the environment around $R$

$\text{prefiltered}(R) = \dfrac{\displaystyle\sum_i L_i(L_i)\,(N \cdot L_i)}{\displaystyle\sum_i (N \cdot L_i)}$

Samples are drawn by **GGX importance sampling** so they concentrate where the lobe has weight. 
The low-discrepancy Hammersley sequence supplies the 2D sample points, where $\Phi_2(i)$ is the Van der Corput radical inverse (bit-reversal) of $i$:

$\xi_i = \left(\dfrac{i}{N_s},\ \Phi_2(i)\right)$

`importanceSampleGGX` maps a Hammersley point to a half-vector $H$ distributed by the GGX normal distribution, via the inverse CDF.

$\phi = 2\pi\,\xi_x \qquad \cos\theta_h = \sqrt{\dfrac{1 - \xi_y}{1 + (a^2 - 1)\,\xi_y}}, \qquad a = r^2.$

The light direction follows by reflecting the view about $H$, namely $L = 2(V \cdot H)\,H - V$. Only samples with $N \cdot L > 0$ contribute, weighted by $N \cdot L$, which both filters back-facing samples and biases toward grazing-correct energy.

**Mip selection by PDF.** Because samples are importance-sampled, the local sample density varies, so the per-sample solid angle comes from the GGX PDF rather than a fixed grid:

$\text{pdf} = \dfrac{D(N \cdot H)\,(N \cdot H)}{4\,(V \cdot H)}, \qquad \Omega_\text{sample} = \dfrac{1}{N_s\,\text{pdf}}, \qquad \text{mip} = \dfrac{1}{2}\log_2\!\left(\dfrac{\Omega_\text{sample}}{\Omega_\text{texel}}\right)$

with $\text{mip} = 0$ forced at $r = 0$. This is the same solid-angle-matching trick as the diffuse bake, here preventing fireflies from concentrated bright sources on the rougher (blurrier) mips. The GGX distribution itself is the shared one in `SwBRDF.h.slang`:

$D(N \cdot H) = \dfrac{a^2}{\pi\big((N \cdot H)^2 (a^2 - 1) + 1\big)^2}, \qquad a = r^2.$

## BRDF Integration LUT

`SwIBLBrdfLut.comp.slang` bakes the second split-sum factor, the $(\text{scale}, \text{bias})$ pair. 

This term is **environment-independent**: it only depends on $N \cdot V$ and roughness, so it is a 512x512 two-channel LUT baked once at startup (parameterized as $u = N \cdot V$, $v = r$).

Starting from the Cook-Torrance specular BRDF and the Fresnel-Schlick term $F = F_0 + (1 - F_0)(1 - V \cdot H)^5$, the $F_0$ factors out of the integral linearly, leaving two scalar integrals. 
The shader fixes $N = (0, 0, 1)$, builds $V$ from $N \cdot V$, importance-samples GGX exactly as the prefilter does, and accumulates with
$f_c = (1 - V \cdot H)^5$, then divides both sums by the sample count.

$g_\text{vis} = \dfrac{G\,(V \cdot H)}{(N \cdot H)(N \cdot V)}, \qquad \text{scale} \leftarrow \text{scale} + (1 - f_c)\,g_\text{vis}, \qquad \text{bias} \leftarrow \text{bias} + f_c\,g_\text{vis}$

$g_\text{vis}$ is the Smith geometry term divided by the importance-sampling PDF and BRDF denominator, all of which collapse to this compact expression. 
The geometry function uses the **IBL** remap of $k$ (distinct from the direct-lighting remap in `SwBRDF.h.slang`):

$k = \dfrac{r^2}{2}, \qquad G = G_1(N \cdot V, k)\,G_1(N \cdot L, k), \qquad G_1(x, k) = \dfrac{x}{x(1 - k) + k}.$

$\text{scale}$ multiplies $F_0$ and $\text{bias}$ is added, so at runtime $F_0\,\text{scale} + \text{bias}$ reconstitutes the full Fresnel-weighted, geometry-attenuated specular integral for any $F_0$.

## At Runtime

`ambientIBL` in `shaders/Geometry/SwGeometry.h.slang` samples the three maps and combines them. With $F_0 = \operatorname{lerp}(0.04,\ \text{albedo},\ \text{metallic})$:

**Diffuse.** Sampling the irradiance map along $N$ gives $E(N)$, and

$k_D = (1 - F_0)(1 - \text{metallic}), \qquad \text{ambient} \leftarrow \text{ambient} + k_D \cdot E(N) \cdot \text{albedo}.$

$k_D$ is the energy left over after specular reflection: $(1 - F_0)$ removes the reflected fraction and $(1 - \text{metallic})$ kills diffuse on metals (which have no diffuse lobe).
Note the diffuse Fresnel uses the normal-incidence $F_0$ rather than the view-dependent $F$, keeping the diffuse fill camera-invariant. 

**Specular.** With $R = \operatorname{reflect}(-V, N)$, the prefilter is sampled at LOD $r \cdot m_\text{max}$ and the LUT at $(N \cdot V,\ r)$:

$\text{ambient} \leftarrow \text{ambient} + \text{prefiltered}(R) \cdot \big(F_0\,\text{envBrdf}_x + \text{envBrdf}_y\big).$

The prefilter LOD $r \cdot m_\text{max}$ is the inverse of the bake's $r = \dfrac{m}{m_\text{max}}$, so a surface's roughness selects the matching pre-blurred mip (trilinear filtering interpolates between adjacent roughness levels). 
The LUT lookup supplies $(\text{scale}, \text{bias})$, completing the split sum.

**AO and intensity.** The combined ambient is modulated by the material's ambient occlusion and a global scalar.

$\text{result} = \text{iblIntensity} \cdot \text{ambientIBL}(\dots) \cdot \text{ao}$

$\text{iblIntensity}$ is fed as $\dfrac{\text{getIblIntensity}()}{\text{getEnvAvgLuminance}()}$ , where $\text{mEnvAvgLuminance}$ is the cosine-weighted average luminance of the HDR. 
Both baked maps are **linear** in the environment pixels, so dividing the final result by that average is identical to having normalized the environment to unit mean brightness before baking.

## Skybox draw

`SwIBLSkybox.vert/frag.slang` rasterize the same equirect HDR as the visible backdrop. 
A unit cube is drawn with the camera's **translation stripped from the view matrix** $V_0$, so the cube stays centered on the camera.

$\text{clip} = P \cdot V_0 \cdot \text{position}$

The interpolated cube-local position is used directly as a world direction and mapped to the equirect with the same $\operatorname{atan2} / \arcsin$ formula from section 1. 
Depth test is disabled and the draw blends behind already-rendered geometry (source factor $1 - \alpha_\text{dst}$), so the sky only fills pixels the scene did not cover. 
Crucially the skybox is shown at the HDR's **full** brightness. 
Only the light the surfaces *receive* is normalized, not the backdrop itself.

## Map Summary

| Map        | Shader                 | Size          | Format  | Encodes                                                        |
| ---------- | ---------------------- | ------------- | ------- | -------------------------------------------------------------- |
| Irradiance | `SwIBLIrradiance.comp` | 64x32         | RGBA16F | cosine-convolved diffuse irradiance per normal                 |
| Prefilter  | `SwIBLPrefilter.comp`  | 128x64 + mips | RGBA16F | GGX-prefiltered specular, roughness per mip                    |
| BRDF LUT   | `SwIBLBrdfLut.comp`    | 512x512       | RG16F   | split-sum $(\text{scale}, \text{bias})$ over $(N \cdot V,\ r)$ |

## Related Files

| File                                     | Role                                                          |
| ---------------------------------------- | ------------------------------------------------------------- |
| `shaders/IBL/SwIBLIrradiance.comp.slang` | Diffuse irradiance convolution                                |
| `shaders/IBL/SwIBLPrefilter.comp.slang`  | GGX specular prefilter (one dispatch per mip)                 |
| `shaders/IBL/SwIBLBrdfLut.comp.slang`    | Environment-BRDF pre-integration                              |
| `shaders/IBL/SwIBL.h.slang`              | Hammersley, Van der Corput, `importanceSampleGGX`             |
| `shaders/BRDF/SwBRDF.h.slang`            | Equirect mapping, `distributionGGX`, Fresnel/geometry         |
| `shaders/Geometry/SwGeometry.h.slang`    | `ambientIBL` / `shadeLit` runtime application                 |
| `src/System/SwIBL.cpp`                   | Bake orchestration, HDR load, average-luminance normalization |
