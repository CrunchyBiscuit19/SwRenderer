# Rendering Techniques

## Fast Approximate Anti-Aliasing (FXAA)

* Post-processing anti-aliasing technique that takes the final image, detects edges, and smoothes it out by colouring the diagonal pixels around the edges.
* Luma is a scalar value that represents the brightness of a pixel, calculated from its RGB values.
* Specific implementation used created by [Matt DesLauriers](https://github.com/mattdesl/glsl-fxaa/)

1. For every pixel in the image, read it and its diagonal neighbours' colours.
2. Calculate the luma for each pixel and its neighbours.
3. Calculate the luma range (maximum and minimum luma values) among the current pixel and its neighbours.
4. Early exit if the range is below a certain threshold, since it indicates the pixels are not edges.
5. Determine edge directions, then normalize and clamp those directions.
6. Calculate a narrow and wide blend (narrow and wide being how much of the neighbouring pixels to blend in).
7. Overwrite the current pixel with wide blend, unless its luma is outside the luma range, in which case overwrite it with the narrow blend.

## Two-Pass Hierarchical Z-Buffer (Hi-Z) Culling

* 2 phase culling via frustum and occlusion culling.

1. For each frame, keep a bit mask of which render items are visible.
2. 1st Phase
    * In the next frame, perform frustum culling only on opaque render items that were visible in the previous frame.
    * Mark opaque render items from step 2 which are not frustum-culled for drawing.
3. 2nd Phase
    * Perform frustum + occlusion culling on all render items. 
    * Mark render items which are not culled, AND haven't already been marked for drawing in 1st phase, for drawing.

## Physically Based Rendering (PBR)

### The Rendering Equation
* Outgoing light of surface = Emitted light + Reflected light
* Emissive and reflected light can be modelled separately.
* Most surfaces are either not emissive, or can be modelled as an additive constant.

### Reflected Light
* For an incoming light $L_{i}$ the contribution to outgoing light $L_{o}$ is determined by the angle $\theta_{i}$ between $L_{i}$ and $\vec{n}$. 
* $L_{o} = L_{i}\cos\theta_{i} = L_{i}(-\vec{\omega_{i}} \cdot \vec{n})$  when $-\vec{\omega_{i}}$ and $\vec{n}$ are normalized direction vectors.

### Bidirectional Reflectance Distribution Functions (BRDFs)
* Model currently does equal scattering of lights in all directions. Most surfaces do not scatter lights equally.
* Let $f_{r}(x, \vec{\omega_{i}}, \vec{\omega{o}})$ be the BRDF.
* For every incoming light $L_{i}$ the corresponding outgoing light is $L_{o} = f_{r}(x, \vec{\omega_{i}}, \vec{\omega{o}}) \cdot L_{i}\cos\theta_{i}$.
* Technically, since there are infinite incoming light rays around a position, calculate the total outgoing light as $L_{o} = \displaystyle\int_{\Omega} f_{r}(x, \vec{\omega_{i}}, \vec{\omega{o}}) \cdot L_{i}\cos\theta_{i} \space d \vec{w}_{i}$.
* In real time rendering, only a few light sources are computed $L_{o} = \displaystyle\sum_{j=0}^{c} f_{r}(x, \vec{\omega_{i}}, \vec{\omega{o}}) \cdot L_{i}\cos\theta_{i}$ where $c$ is number of light sources.
* The final rendering equation is $L_{o} = L_{e} + \displaystyle\sum_{j=0}^{c} f_{r}(x, \vec{\omega_{i}}, \vec{\omega{o}}) \cdot L_{i}\cos\theta_{i}$ when emissive lights are considered.
* BRDFs are a subset of BSDFs, which aim to model more variables in lighting.

### Laws of Physics
1. Law of Conservation of Energy: Outgoing light must not produce more energy than the incoming light.
2. Helmholtz Reciprocity Principle: Asserts that the BRDF result remains unchanged when the incoming and outgoing light directions are interchanged $f_{r}(x, \vec{\omega_{i}}, \vec{\omega{o}}) == f_{r}(x, \vec{\omega_{o}}, \vec{\omega_{i}})$.

### Blinn-Phong — Lambertian Diffuse + Phong Specular
* To make modelling easier, split BRDF into diffuse $f_{\text{diffuse}}(x, \vec{\omega_{i}}, \vec{\omega{o}})$ and specular $f_{\text{specular}}(x, \vec{\omega_{i}}, \vec{\omega{o}})$. 
* Lambertian Reflectance — Incoming light diffuses equally in all direction from a position.
* Lambertian Diffuse — $f_{\text{diffuse}}(x, \vec{\omega_{i}}, \vec{\omega{o}}) = C_{\text{diffuse}}$ where $C_{\text{diffuse}}$ represents the base colour of a surface.
* Specular Reflectance — Incoming light reflects in one particular direction. Should only be observed when reflection direction aligns closely with view direction.
	* The width of the blue band depends on the shininess of the surface. More shiny => narrower band.
* Phong Specular — $f_{\text{specular}}(x, \vec{\omega_{i}}, \vec{\omega{o}}) = (\vec{\omega_{o}} \cdot \vec{R})^{\text{shininess}} = (\vec{n} \cdot \vec{H})^{\text{shininess}}$ But the latter is cheaper since reflection calculation is avoided.
* Blinn-Phong = $f_{\text{diffuse}}(x, \vec{\omega_{i}}, \vec{\omega{o}}) + f_{\text{specular}}(x, \vec{\omega_{i}}, \vec{\omega{o}})$
* Does not follow the Law of Conservation of Energy.

### Metallic-Roughness PBR
* Uses Cook-Torrance BRDF.



