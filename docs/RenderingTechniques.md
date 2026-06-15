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

