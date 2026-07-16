# Depth Values to Linear Distance

A reference for converting a depth buffer sample back into a linear view-space distance, covering both the standard (non-reversed) and the reversed-Z conventions. The reversed-Z result is the one used by the clustered shading depth-slice code in `SwLightingClustersMarkActive.comp.slang`.

## Notation

- $n$ is the near plane distance, a positive value.
- $f$ is the far plane distance, a positive value.
- $d$ is the linear view-space distance in front of the camera, a positive value in $[n, f]$.
- $z_{\text{ndc}}$ is the standard NDC depth stored in the buffer, in $[0, 1]$ where $0$ is the near plane and $1$ is the far plane.
- $z_{\text{rndc}}$ is the reversed-Z NDC depth, in $[0, 1]$ where $1$ is the near plane and $0$ is the far plane.

## The core idea

A depth buffer does not store distance. It stores a projected depth value that has been squashed into $[0, 1]$ by the projection matrix. That squashing is nonlinear, so most of the buffer precision sits close to the camera and very little sits far away. To recover an honest linear distance $d$ we have to run the projection backwards.

Two things are entangled in that stored value and both must be undone.

- The nonlinear projection itself, which is what makes the value a depth rather than a distance.
- The choice of convention, either standard-Z where the near plane maps to $0$, or reversed-Z where the near plane maps to $1$. Reversed-Z is used because it distributes floating-point precision far more evenly across the range.

Recovering $d$ therefore reduces to a single algebraic inversion, plus one substitution if the buffer is reversed-Z.

## The forward relation

For a right-handed perspective projection with a $[0, 1]$ depth range, as produced by `glm::perspectiveRH_ZO`, a positive view-space distance $d$ maps to standard NDC depth by

$\displaystyle z_{\text{ndc}} = \dfrac{f \cdot (d - n)}{(f - n) \cdot d}$

This is the relation we invert. It comes from the relevant rows of the projection matrix, where the clip-space depth is $f \cdot (d - n) / (f - n)$ and the clip-space $w$ is $d$, so dividing the two gives the expression above.

## Standard-Z linearization

Start from the forward relation and solve for $d$.

$\displaystyle z_{\text{ndc}} = \dfrac{f \cdot (d - n)}{(f - n) \cdot d}$

Multiply through by $(f - n) \cdot d$ and expand the right side.

$\displaystyle d \cdot z_{\text{ndc}} \cdot (f - n) - f \cdot d = -f \cdot n$

Factor $d$ out of the left side.

$\displaystyle d \cdot \left( z_{\text{ndc}} \cdot (f - n) - f \right) = -f \cdot n$

Negate both sides so the leading term is positive.

$\displaystyle d \cdot \left( f - z_{\text{ndc}} \cdot (f - n) \right) = f \cdot n$

Divide to isolate $d$.

$\displaystyle d = \dfrac{f \cdot n}{f - z_{\text{ndc}} \cdot (f - n)}$

This is the finished formula for a standard-Z buffer. As a check, $z_{\text{ndc}} = 0$ returns $\dfrac{f \cdot n}{f} = n$ and $z_{\text{ndc}} = 1$ returns $\dfrac{f \cdot n}{n} = f$.

## Reversed-Z linearization

Reversed-Z stores the complement of the standard depth, so

$\displaystyle z_{\text{ndc}} = 1 - z_{\text{rndc}}$

Substitute that into the standard-Z formula.

$\displaystyle d = \dfrac{f \cdot n}{f - (1 - z_{\text{rndc}}) \cdot (f - n)}$

Expand the product in the denominator, taking care with the signs. Distributing the leading minus gives a positive $z_{\text{rndc}}$ term.

$\displaystyle f - (1 - z_{\text{rndc}}) \cdot (f - n) = f - (f - n) + z_{\text{rndc}} \cdot (f - n)$

The $f - (f - n)$ part collapses to $n$, which leaves

$\displaystyle d = \dfrac{f \cdot n}{n + z_{\text{rndc}} \cdot (f - n)}$

This is the finished formula for a reversed-Z buffer and matches the shader line `far * near / (near + ndcZ * (far - near))`. As a check, $z_{\text{rndc}} = 1$ at the near plane returns $\dfrac{f \cdot n}{n + (f - n)} = \dfrac{f \cdot n}{f} = n$ and $z_{\text{rndc}} = 0$ at the far plane returns $\dfrac{f \cdot n}{n} = f$.

## Comparison

$\displaystyle d_{\text{standard}} = \dfrac{f \cdot n}{f - z_{\text{ndc}} \cdot (f - n)}$

$\displaystyle d_{\text{reversed}} = \dfrac{f \cdot n}{n + z_{\text{rndc}} \cdot (f - n)}$

The two formulas have identical structure and identical cost, one multiply and one add. The only difference is the leading term of the denominator, which switches from $f$ to $n$ once the reversed-Z substitution is worked through. Reversed-Z therefore adds no runtime expense over standard-Z. The extra substitution is only a derivation step, not a shader step.

## Note on the inverse matrix alternative

The closed forms above assume a conventional finite perspective projection. A projection built by swapping the near and far planes, or one with an infinite far plane, would need different algebra. A fully general alternative is to unproject the pixel with the inverse projection matrix and read the resulting view-space $z$, which works for any projection because it inverts whatever matrix was actually used. That path is heavier, since it computes a full view-space position and discards the $x$ and $y$, so the closed form is preferred wherever only the distance is needed and the projection is known to be a standard perspective. The cluster AABB build in `SwLightingClustersBuild.comp.slang` uses the inverse matrix path because it genuinely needs the full view-space position.
