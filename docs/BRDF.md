# Cook-Torrance BRDF — `SwBRDF_h.slang` Explained

This file implements a **Cook-Torrance physically-based BRDF**, the standard model for realistic metal and dielectric surfaces.

## The Big Picture

The whole thing computes one value (per color channel): *how much light arriving from direction $L$ bounces toward your eye $V$ off a surface with normal $N$.* It splits into **specular** (sharp reflections) and **diffuse** (soft scattered light).

The specular part is the classic Cook-Torrance form:

$$
f_{\text{spec}} = \frac{D \cdot G \cdot F}{4\,(N\cdot V)(N\cdot L)}
$$

where $D$, $G$, and $F$ are the three helper functions described below.

## The Three Pieces of Specular

### D — Distribution (`distributionGGX`)

Answers: *what fraction of microscopic surface facets are tilted to reflect $L$ straight into $V$?* This needs the half-vector $H = \text{normalize}(V + L)$ — the direction a facet must face to bounce $L \to V$.

$$
D = \frac{a^2}{\pi\big((N\cdot H)^2(a^2-1)+1\big)^2}, \qquad a = \text{roughness}^2
$$

Smooth surfaces produce a spike at the highlight (a tiny bright dot). Rough surfaces spread it out (a broad, dull sheen). The `max(..., 1e-7)` in code just avoids divide-by-zero.

### G — Geometry (`geometrySmith` + `geometrySchlickGGX`)

Answers: *what fraction of those facets are actually visible and lit, rather than shadowing or blocking each other?* Rough surfaces self-occlude more.

Smith's method splits this into two independent checks — one for the view direction, one for the light — and multiplies them:

$$
G = G_1(N\cdot V)\cdot G_1(N\cdot L), \qquad G_1(x) = \frac{x}{x(1-k)+k}
$$

The remapping

$$
k = \frac{(\text{roughness}+1)^2}{8}
$$

is the one recommended specifically for **direct lighting** (image-based lighting uses a different $k$).

### F — Fresnel (`fresnelSchlick`)

Answers: *how reflective is the surface at this viewing angle?* Everything becomes mirror-like at grazing angles (think of the glare across a table at sunset).

$$
F = F_0 + (1-F_0)(1-\cos\theta)^5
$$

$F_0$ is the reflectance when looking straight on. The line

```slang
float3 F0 = lerp(float3(0.04f), albedo, metallic);
```

encodes a key PBR fact: **dielectrics reflect ~4% of light colorlessly**, while **metals reflect their albedo color**. The `metallic` parameter blends between these two behaviors.

## The Diffuse Part

```slang
float3 kD = (float3(1.f) - F) * (1.f - metallic);
float3 diffuse = kD * albedo / PI;
```

### What is `kD`?

`kD` is the **diffuse weight** — the fraction of incoming light that ends up scattering diffusely (the soft, matte component) rather than reflecting specularly or being absorbed.

The name comes from the convention of splitting reflected light into two parts:

- **$k_S$** — the *specular* fraction (light that bounces off sharply, mirror-like)
- **$k_D$** — the *diffuse* fraction (light that penetrates slightly, scatters, and re-emerges in all directions)

These should add up to no more than 1 — you cannot reflect more light than arrives. The Fresnel term $F$ *already* gives the specular fraction at this angle, so the diffuse fraction is simply whatever is left:

$$
k_S = F \qquad\Rightarrow\qquad k_D = 1 - k_S = 1 - F
$$

That is the first factor in the code. The second factor, $(1 - \text{metallic})$, kills diffuse for metals: their free electrons absorb any light that penetrates the surface, so none of it scatters back out.

`kD` is a per-channel `float3` (not a single scalar) because Fresnel $F$ is colored, so the diffuse weight can differ across R, G, and B.

> **Note:** this is a slightly simplified form. $F$ here is the Fresnel evaluated at the half-vector, whereas a strictly correct $k_S$ would integrate Fresnel over the whole specular lobe. Most real-time engines use this approximation anyway — it is cheap and looks right.

### Energy conservation

Putting it together, the two factors express **energy conservation**:

- Whatever fraction $F$ reflects specularly *cannot* also scatter diffusely, so multiply by $(1 - F)$.
- Metals have **no diffuse** at all (free electrons absorb transmitted light), so multiply by $(1 - \text{metallic})$.

The $/\pi$ normalizes the Lambertian diffuse term so the surface never emits more energy than it receives:

$$
f_{\text{diff}} = \frac{k_D \cdot \text{albedo}}{\pi}
$$

## The Final Line

```slang
return (diffuse + specular) * radiance * NdotL;
```

- `radiance` is the incoming light color and intensity.
- $N\cdot L$ is **Lambert's cosine law** — light hitting at a glancing angle is spread over more area, so it contributes less:

$$
L_o = (f_{\text{diff}} + f_{\text{spec}}) \cdot \text{radiance} \cdot (N\cdot L)
$$

The early `if (NdotL <= 0)` simply skips surfaces facing away from the light.

## Why All the `max(..., 1e-4)` Guards

These are numerical safety rails. Dot products can land at exactly zero (grazing angles), which would cause division by zero and produce `NaN`/`Inf` pixels — usually visible as black spots or "firefly" artifacts. Clamping to a tiny epsilon keeps the math stable without visibly changing the result.

## In Short

| Term | Role |
|------|------|
| **D** | Says *where* highlights form (sharp for smooth, broad for rough). |
| **G** | Dims highlights to account for roughness self-shadowing. |
| **F** | Brightens edges at grazing angles and colors metal reflections. |
| **Diffuse** | Handles the soft matte light left over after specular takes its share. |
