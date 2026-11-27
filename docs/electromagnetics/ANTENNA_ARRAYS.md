# Antenna Arrays

## Array objects (RES-EM-010)

An RF array is a scene object with an array component; its elements are
placed in the array's local frame:

* **Linear:** N elements along local X at spacing dx, centred.
* **Planar:** Nx x Ny elements in local XY at dx, dy.
* **Circular:** N elements on a ring of radius R in local XY.
* **Custom:** any list of local positions.

Element pattern: isotropic, short dipole and half-wave dipole (both along
local Z), or cosine (radiates into the half space +Z, a simple model of a
patch on a ground plane). Frequency and a common element amplitude (A for
dipole elements). Each element can override amplitude, phase, delay and
on/off; overrides are kept by element index and survive layout changes.

New arrays from the menu use half-wavelength spacing at 1 GHz: 8 dipoles
(linear), 4 x 4 cosine elements (planar), 12 dipoles (circular).

In the viewport each element is a cube whose brightness follows its
relative amplitude (grey when off), with a line along the array's local +Z.

## Editing

The Properties panel edits the layout and shows a table of elements
(position, amplitude, phase in degrees, delay in ns, on/off); edit a cell
to override one element, **Clear Element Settings** removes all overrides.

## Model limits

The array factor ignores mutual coupling, element-to-element scan
impedance and edge effects; closely spaced elements (under a quarter
wavelength) are warned about. Dipole-element arrays also radiate in the
analytical field evaluation, as superposed independent elements (still no
coupling). Arrays are not represented by the full-wave solver.

See `ARRAY_FACTOR_AND_BEAMFORMING.md` for analysis, steering and synthesis.
