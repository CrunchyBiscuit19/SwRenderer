# The Cook-Torrance BRDF

A consolidated reference covering the diffuse and specular terms, following the original microfacet formulation (Cook & Torrance, 1982) as commonly implemented in modern real-time and offline renderers.

## Notation

- $n$ — unit surface normal
- $l$ — unit vector toward the light
- $v$ — unit vector toward the viewer
- $h$ — unit half-vector, $h = \dfrac{l+v}{\|l+v\|}$
- $N\cdot L$, $N\cdot V$ — normalized dot products of $n$ with $l$ and $v$ (angle of each vector relative to the macroscopic surface normal)
- $N\cdot H$ — normalized dot product of $n$ with the half-vector (how closely the half-vector aligns with the macroscopic normal; this is what the distribution term cares about)
- $V\cdot H$ (equivalently $L\cdot H$) — normalized dot product of $v$ or $l$ with the half-vector (the local angle of incidence relative to the specific microfacet doing the reflecting)

## Overall structure

$f_r = k_d \cdot f_{\text{diffuse}} + f_{\text{specular}}$

Cook-Torrance is, at its core, just the specular half of the model; the diffuse term is typically a plain Lambertian term bolted on alongside it. $k_d = (1-k_s)\cdot(1-\text{metallic})$ in metallic-workflow implementations.

## 1. Diffuse term

$f_{\text{diffuse}} = C_{\text{diffuse}} \cdot \pi^{-1}$

Plain Lambertian reflectance: constant radiance in every direction, scaled by the diffuse albedo $C_{\text{diffuse}}$ and normalized by $\pi$ so the BRDF conserves energy ($\int_{\text{hemisphere}} \cos\theta\, d\omega = \pi$, so a unit-albedo Lambertian surface must return $1/\pi$, not $1$, for outgoing radiance to balance incoming irradiance).

**What it means.** Unlike Disney's diffuse model, the original Cook-Torrance paper does not add any grazing-angle retro-reflection correction here, it's unmodified Lambert. This is the main practical difference from Disney's diffuse term: Cook-Torrance treats diffuse and specular as two cleanly separated phenomena (subsurface-scattered light vs. microfacet-reflected light) without trying to fold extra empirical realism into the diffuse half. Some modern implementations multiply this by $(1-F)$, using the Fresnel term to "steal" the fraction of light that went into specular reflection from what's available to scatter diffusely, but that's a later addition, not part of Cook-Torrance's original formulation.

## 2. Specular term

$f_{\text{specular}} = \dfrac{D(h)\cdot F(v,h)\cdot G(l,v,h)}{4(N\cdot L)(N\cdot V)}$

This is the heart of the model: a microfacet theory result stating that a rough surface behaves as a huge collection of tiny, perfectly smooth mirror facets, each oriented slightly differently, and the BRDF emerges from statistically describing how those facets are distributed, how much they individually reflect, and how much they block each other.

### 2a. Normal distribution function, $D$

Describes what fraction of microfacets are oriented exactly toward $h$, i.e., oriented correctly to reflect $l$ into $v$. 

**GGX / Trowbridge-Reitz** uses more realistic, longer "tails" that better match measured real-world materials:

$D(h) = \dfrac{\alpha^2}{\pi\left[(N\cdot H)^2(\alpha^2-1)+1\right]^2}$

where $\alpha = \text{roughness}^2$ is the standard remapping (squaring roughness before using it as $\alpha$ tends to give a more perceptually linear roughness slider).

**Beckmann Form**, for reference:

$D(h) = \dfrac{1}{\pi\alpha^2(N\cdot H)^4}\exp\!\left(\dfrac{(N\cdot H)^2-1}{\alpha^2(N\cdot H)^2}\right)$

**What it means.** As $\alpha \to 0$ (perfectly smooth), $D$ becomes a sharp spike: almost all microfacets must point exactly along $h$ for any reflection to occur, producing a tight, mirror-like highlight. As $\alpha$ grows (rougher), the distribution spreads out, microfacets point every which way, and the highlight broadens and dims. The factor of $N\cdot H$ in the denominator, rather than $V\cdot H$ or $L\cdot H$, reflects that this term is purely about microfacet orientation relative to the macroscopic surface, not about the light or view direction individually.

### 2b. Fresnel term, $F$

The same Schlick approximation seen in the Disney model, here in its original, untinted form:

$F(v,h) = F_0 + (1-F_0)(1-V\cdot H)^5$

$F_0$ is the reflectance at normal incidence, derived from the index of refraction for dielectrics, or simply set to the metal's reflectance color for conductors. In a metallic-workflow implementation:

$F_0 = \text{lerp}(0.04,\ C_{\text{diffuse}},\ \text{metallic})$

where $0.04$ is the typical reflectance for common dielectrics (plastics, wood, skin) at normal incidence.

**What it means.** This is the genuine physical Fresnel effect: light reflecting directly off a microfacet's surface, with reflectance rising toward $1$ at grazing angles regardless of $F_0$. The angle used is $V\cdot H$ (equal to $L\cdot H$ by construction), the local angle of incidence relative to the specific microfacet aligned with $h$, not the macroscopic angle to $n$. This is the same role Fresnel plays in Disney's specular term; Cook-Torrance is in fact where this exact usage pattern originates, Disney's specular term is built directly on this foundation.

### 2c. Geometric attenuation (masking-shadowing) term, $G$

Accounts for microfacets blocking each other, either shadowing incoming light before it reaches a facet, or masking outgoing light before it leaves. The original Cook-Torrance paper used a geometric derivation assuming V-shaped grooves:

$G(l,v,h) = \min\left(1,\ \dfrac{2(N\cdot H)(N\cdot V)}{V\cdot H},\ \dfrac{2(N\cdot H)(N\cdot L)}{V\cdot H}\right)$

This is the classic form, three separate masking limits (no occlusion, masking, shadowing) combined with a $\min$.

**Modern alternative: Smith-GGX.** Most current implementations replace this with the statistically-derived Smith form, which pairs naturally with the GGX distribution above:

$G(l,v,h) = G_1(l)\cdot G_1(v)$

with the Schlick-GGX approximation for each direction:

$k = \dfrac{(\text{roughness}+1)^2}{8}, \qquad G_1(x) = \dfrac{N\cdot x}{(N\cdot x)(1-k)+k}$

(This is the identical geometry term discussed for Disney's specular lobe, since Disney's model is itself a Cook-Torrance derivative; the two aren't separate formulas, Disney inherits this piece directly.)

**What it means.** Without this term, a very rough surface viewed at a grazing angle would appear to reflect more light than physically possible, microfacets in front block ones behind, and this term accounts for that lost energy. The $4(N\cdot L)(N\cdot V)$ in the overall specular denominator (below) and the $G$ term work together: $G$ handles facet-on-facet occlusion, while the denominator corrects for the change of variables between microfacet-local and macroscopic shading frames.

### 2d. The normalization denominator

$\dfrac{1}{4(N\cdot L)(N\cdot V)}$

This isn't a separate named term, but it's worth understanding on its own. It arises from the Jacobian of transforming the microfacet distribution (defined over half-vector orientations) into the rendering equation's outgoing-radiance space (defined over light/view directions), with the factor of $4$ coming from the relationship $d\omega_h = \dfrac{d\omega_i}{4(V\cdot H)}$ in that change of variables. Without this correction, the BRDF would not correctly conserve energy across different light/view angles.

## Parameter summary

The minimal Cook-Torrance specular lobe needs just two physical inputs: a roughness value (feeding $\alpha$ in $D$, and $k$ in $G$) and a reflectance-at-normal-incidence value $F_0$ (feeding $F$). Layered with a Lambertian diffuse term and a metallic/dielectric split, this is the backbone underlying most modern metallic-roughness PBR pipelines (glTF, Unreal, Unity's standard shader, and Disney's own specular lobe), the differences between them mostly come down to which $D$ and $G$ variants are chosen and how diffuse/specular energy is balanced, not the overall structure.

## Relationship to the Disney model

Disney's specular term, covered in the companion document on the Disney principled BRDF, is a direct descendant of Cook-Torrance: same $\dfrac{D\cdot F\cdot G}{4(N\cdot L)(N\cdot V)}$ structure, same Schlick-Fresnel, same Smith-GGX geometry term. What Disney adds on top is anisotropic stretching of $D$ and $G$ (splitting roughness into $\alpha_x/\alpha_y$), and artist-facing tint/parameterization layers (`specularTint`, the `metallic`-aware $F_0$ blend). The core physics of the specular lobe, however, is unchanged from Cook-Torrance; Disney's real departures from Cook-Torrance are entirely on the diffuse side (the empirical retro-reflection, subsurface, and sheen terms it adds, none of which have any Cook-Torrance counterpart).
