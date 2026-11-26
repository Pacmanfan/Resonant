# Hybrid Active / Passive Focusing

**Status:** current for REQ-CHAT-20261126-004.

Hybrid focusing drives an emitter array in the presence of a reflector so
that direct and reflected sound add at the target.

## Method

With the reflector model selected, the response of element `i` at the
target is `H_i = direct_i + reflected_i`, both calculated by the model.
Phase focusing sets

```text
φ_i = −arg H_i   (mod 2π)
```

so every element's total response arrives in phase and the field at the
target is `Σ |H_i|`, the largest these elements can produce there with unit
amplitudes. With the free-field model `H_i` is the direct path alone and
the same formula gives the ordinary phase focus `k r_i`.

Inverse design for several targets uses the same per-element responses in
its propagation matrix, so it too accounts for the reflector.

## Workflow

1. Create an emitter array. 2. Add a reflector and set its geometry,
material and boundary condition. 3. Define a target. 4. Select the
reflector field model. 5. In the Focus tab choose phase control and
calculate: the note says "Hybrid focusing". 6. Apply. 7. Run a field
simulation and inspect the field and the comparison table.

## What is and is not coupled

The interaction between each emitter and the reflector is calculated by the
reflector model (one reflection, Kirchhoff approximation). A free-field
array result is never overlaid on a reflector mesh: with the free-field
model selected the application states that the manipulators are ignored.

Time-delay focusing compensates the direct paths only; the application says
so when the reflector model is selected. Lenses, waveguides and cavities do
not take part in any field calculation, so there is no hybrid result with
them.

## Example

`examples/manipulators/d-hybrid-focusing.resonant`: a 6 × 6 array facing a
parabolic reflector, target near its focus. With the stored phases the
field at the target equals the sum of the response magnitudes, more than
1.5 times what the array delivers there without the reflector.
