# The Disney Principled BRDF

A consolidated reference covering the diffuse, specular, and clearcoat terms, following Burley's original 2012 SIGGRAPH course notes and the 2015 "Extending the Disney BRDF to a BSDF" extension.

## Notation

- $n$ — unit surface normal
- $l$ — unit vector toward the light
- $v$ — unit vector toward the viewer
- $h$ — unit half-vector, $h = \dfrac{l+v}{\|l+v\|}$
- $N\cdot L$, $N\cdot V$ — normalized dot products of $n$ with $l$ and $v$ respectively (angle of each vector relative to the macroscopic surface normal)
- $L\cdot H$ (equivalently $V\cdot H$) — normalized dot product of $l$ or $v$ with the half-vector $h$ (the local angle of incidence relative to the microfacet doing the reflecting; these two dot products are equal because $h$ bisects $l$ and $v$ by construction)

## Overall structure

$f = (1 - \text{metallic}) \cdot f_{\text{diffuse}} + f_{\text{specular}} + f_{\text{clearcoat}}$

The diffuse term is suppressed as a surface becomes metallic, since metals have no subsurface scattering component, only reflection. The specular and clearcoat terms remain active regardless of `metallic`.

## 1. Diffuse term

$f_{\text{diffuse}} = C_{\text{diffuse}} \cdot \text{lerp}(F_D, F_{ss}, \text{subsurface}) + F_{\text{sheen}}$

$C_{\text{diffuse}}$ is the diffuse albedo: `baseColor` with the metallic contribution removed. $F_D$ models ordinary opaque diffuse reflection with grazing-angle retro-reflection; $F_{ss}$ approximates subsurface scattering; the two are blended by the `subsurface` parameter. $F_{\text{sheen}}$ is added on top, unblended, to fake a fabric-like grazing highlight.

### 1a. Opaque diffuse retro-reflection, $F_D$

$F_{d90} = 0.5 + 2 \cdot \text{roughness} \cdot (V\cdot H)^2$

$F_D = \text{lerp}\big(1,\ F_{d90},\ F_L\big) \cdot \text{lerp}\big(1,\ F_{d90},\ F_V\big)$

where the two Schlick weights are

$F_L = (1-N\cdot L)^5, \qquad F_V = (1-N\cdot V)^5$

**What it means.** A pure Lambertian surface (just $C_{\text{diffuse}}/\pi$) looks the same brightness from every angle. Real rough/matte surfaces (chalk, unglazed ceramic, skin) brighten somewhat at grazing angles, an effect that genuinely arises from light transmitting into and back out of a rough boundary, integrated over microfacet statistics. Burley didn't derive that integral; he noticed the Schlick weight $(1-\cos\theta)^5$ already has roughly the right "ramps up toward grazing" shape, repurposed it as a blend factor inside $\text{lerp}$, and tuned $F_{d90}$ so the effect scales with `roughness` (rougher surfaces show more of the brightening). $F_L$ and $F_V$ are evaluated independently at the light and view angles and multiplied together, both need to be near grazing for the full effect to appear.

**Why $1/\pi$.** This is the ordinary Lambertian normalization, required so the BRDF conserves energy: integrating $\cos\theta$ over a hemisphere gives $\pi$, so a unit-albedo diffuse BRDF must be $1/\pi$, not $1$, for outgoing radiance to balance incoming irradiance. The Schlick-weighted $\text{lerp}$ factors are a multiplicative reshaping on top of this base Lambertian term, they don't replace the normalization.

**Why $(V\cdot H)^2$, not $N\cdot$ something.** This particular angle is the light/view-to-half-vector angle, not the light/view-to-normal angle, because $F_{d90}$ is detecting how close $l$ and $v$ are to each other (a retroreflective configuration), which is exactly what the half-vector angle measures: when $l \approx v$, $h$ is close to bisecting a near-zero angle between them and $V\cdot H \to 1$.

### 1b. Subsurface approximation, $F_{ss}$

$F_{ss90} = \text{roughness} \cdot (V\cdot H)^2$

$F_{ss} = 1.25\left[\text{lerp}(1, F_{ss90}, F_L)\cdot\text{lerp}(1, F_{ss90}, F_V)\cdot\left(\dfrac{1}{N\cdot L + N\cdot V} - 0.5\right) + 0.5\right]$

using the same $F_L$, $F_V$ Schlick weights as $F_D$.

**What it means.** This is a cheap stand-in for proper subsurface light transport (Hanrahan–Krueger-style), meant to approximate materials like skin, wax, or marble where light enters, scatters internally, and exits some distance away. It reuses the identical Schlick-weight building blocks as $F_D$ but combines them differently and targets a different normal-incidence value ($F_{ss90}$ has no $+0.5$ offset), producing a softer, more scattered-looking falloff. The leading $1.25/\pi$ constant is an empirical fudge factor, tuned so $F_{ss}$ and $F_D$ roughly agree in magnitude at `subsurface = 0`, not a derived physical quantity.

### 1c. Sheen, $F_{\text{sheen}}$

$F_{\text{sheen}} = (1 - V\cdot H)^5 \cdot \text{sheen} \cdot C_{\text{sheen}}$

$C_{\text{sheen}} = \text{lerp}(\mathbf{1},\ C_{\text{tint}},\ \text{sheenTint})$

where $C_{\text{tint}}$ is the base color's hue/saturation with luminance normalized out, and `sheen`, `sheenTint` are artist-facing sliders. This models the soft, fuzzy, grazing-angle highlight seen on cloth and velvet. Unlike $F_D$/$F_{ss}$, this isn't blended via $\text{lerp}$; it's a flat additive term scaled directly by the Schlick weight.

## 2. Specular term

A standard Cook–Torrance microfacet model, with anisotropic GGX for the normal distribution and an artist-tinted Fresnel:

$f_{\text{specular}} = \dfrac{D(h)\cdot F(v,h)\cdot G(l,v,h)}{4(N\cdot L)(N\cdot V)}$

### 2a. Normal distribution function, $D$

Anisotropic GGX, controlling the size and shape (circular vs. elliptical) of the specular highlight:

$D(h) = \left(\pi\,\alpha_x\alpha_y\left[\left(\dfrac{h_x}{\alpha_x}\right)^2 + \left(\dfrac{h_y}{\alpha_y}\right)^2 + h_z^2\right]^2\right)^{-1}$

$\alpha_x$, $\alpha_y$ are derived from `roughness` and `anisotropic`, stretching the highlight along the tangent/bitangent directions for brushed-metal-style materials. $h_x, h_y, h_z$ are the components of the half-vector in the local tangent/bitangent/normal frame.

### 2b. Fresnel term, $F$

The Schlick approximation, but with $F_0$ built from artist parameters and blended toward `baseColor` as the surface becomes metallic:

$C_{\text{spec0}} = \text{lerp}\Big(\text{specular}\cdot\text{lerp}(\mathbf{1}, C_{\text{tint}}, \text{specularTint}),\ C_{\text{diffuse}},\ \text{metallic}\Big)$

$F(v,h) = \text{lerp}\big(C_{\text{spec0}},\ \mathbf{1},\ (1-V\cdot H)^5\big)$

**What it means.** This is the textbook, physically-motivated use of Schlick-Fresnel: specular reflection *is* the literal Fresnel reflection event at the microfacet surface, so the Schlick weight here is doing exactly what it was designed for, interpolating between the base reflectance $F_0$ and full reflectance ($1$) as the angle moves toward grazing. The angle used is $V\cdot H$ (equivalently $L\cdot H$), because Fresnel reflectance is about the local angle of incidence relative to the specific microfacet's own normal ($h$), not the macroscopic surface normal $n$.

### 2c. Geometric (masking-shadowing) term, $G$

$G(l,v,h) = G_1(l)\cdot G_1(v)$

The Smith form, evaluated per-direction and multiplied together; each $G_1$ accounts for microfacets blocking light arriving from, or leaving toward, one direction. A common real-time approximation (Schlick-GGX, from Karis's UE4 notes) folds roughness into a single remapped scalar $k$ for the isotropic case:

$k = \dfrac{(\text{roughness}+1)^2}{8}, \qquad G_1(x) = \dfrac{N\cdot x}{(N\cdot x)(1-k)+k}$

This isotropic version (used in most real-time engines) discards the $\alpha_x/\alpha_y$ anisotropic split that the full Disney $D$ term uses, trading accuracy for a single-scalar-roughness shortcut suited to analytic direct lighting.

## 3. Clearcoat term

A second, independent specular lobe layered on top, meant to emulate a thin lacquer coating (car paint, for instance). It is non-tinted (fixed $F_0\approx 0.04$, corresponding to a fixed IOR of about 1.5), non-anisotropic, and uses a simpler distribution (GTR1 / Berry) instead of GGX:

$f_{\text{clearcoat}} = \dfrac{D_c(h)\cdot F_c(v,h)\cdot G_c(l,v,h)}{4(N\cdot L)(N\cdot V)}$

$D_c(h) = \dfrac{\alpha_g^2 - 1}{\pi\ln(\alpha_g^2)\left[1+(\alpha_g^2-1)h_z^2\right]}$

$\alpha_g$ is interpolated from the `clearcoatGloss` slider (between roughly $0.1$, satin, and $0.001$, glossy). $F_c$ is plain Schlick with fixed $F_0 = 0.04$:

$F_c(v,h) = \text{lerp}(0.04,\ 1,\ (1-V\cdot H)^5)$

$G_c$ uses the same Smith form as the main specular term, but with a fixed roughness ($\alpha = 0.25$) regardless of the base surface's `roughness` parameter. The whole term is scaled by the `clearcoat` slider (0 to 1) before being added in.

## Parameter summary

`baseColor`, `metallic`, `subsurface`, `specular`, `roughness`, `specularTint`, `anisotropic`, `sheen`, `sheenTint`, `clearcoat`, `clearcoatGloss` — eleven artist-facing sliders, each mapped through the formulas above into the underlying physical quantities ($F_0$, $\alpha_x$, $\alpha_y$, etc.) that drive the actual shading math.

## Design philosophy

Burley's stated priority was intuitive, perceptually meaningful sliders for artists over strict physical correctness, while staying *plausible*: energy-conserving-ish, sensible at parameter extremes, and visually convincing, rather than derived from first-principles radiative transfer. This is why the same building block, the Schlick weight $(1-\cos\theta)^5$, recurs throughout the model in both physically rigorous roles (the specular and clearcoat Fresnel terms) and purely empirical, curve-fitted roles (the diffuse retro-reflection, subsurface approximation, and sheen terms): it's a cheap, smooth, already-implemented falloff shape that gets reused wherever a "brighter at grazing angles" effect is wanted, whether or not Fresnel reflectance is the actual underlying cause.
