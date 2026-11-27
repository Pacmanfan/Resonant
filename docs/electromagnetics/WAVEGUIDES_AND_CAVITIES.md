# Waveguides and Cavities

## Guide modes (RES-EM-016)

* **Rectangular a x b:** f_c = c / (2 sqrt(er mur)) sqrt((m/a)^2 + (n/b)^2);
  TE_mn (m, n >= 0, not both zero) and TM_mn (m, n >= 1). WR-90 (22.86 x
  10.16 mm): TE10 6.557 GHz, TE20 13.114, TE01 14.754, TE11/TM11 16.145.
* **Circular radius a:** TE_nm from zeros of J_n', TM_nm from zeros of J_n
  (bracketed on a fine grid and bisected).
* **Parallel plate:** TEM and TE_n / TM_n at n c / (2 d sqrt er).
* **Coaxial:** TEM with Z0, and the first higher mode TE11 from the
  approximation k_c = 2 / (a + b), stated as an approximation.

## Propagation

Above cut-off: beta = k sqrt(1 - (fc/f)^2), guide wavelength 2 pi / beta,
wave impedance eta / sqrt(1 - (fc/f)^2) (TE) or eta sqrt(1 - (fc/f)^2)
(TM), phase and group velocity with v_p v_g = c^2 / (er mur). Below it the
mode is evanescent and its decay (dB/m) is reported.

## Cavities

Rectangular a x b x d: TE_mnp and TM_mnp resonances in ascending order;
ideal walls, no loss, no coupling ports.

## The TE10 field

Ey = E0 sin(pi x / a) e^{-j beta z} with the matching Hx and Hz; its power
flux equals the closed-form TE10 power (checked). For a rectangular guide
object the Fields tab shows the field over the cross-section (peak E0 =
1 V/m) at the guide's frequency of interest, refused below cut-off.

## Guide objects

Electromagnetics > Add Rectangular Waveguide (EM), Circular Waveguide (EM),
Coaxial Line, Rectangular Cavity. The guide runs along local Z; walls are
drawn as thin solids; the filling is the object's material (vacuum when
none; a conductor is ignored with a note). The report lists the lowest
modes with cut-off, propagation or decay at the frequency of interest.
These objects are also bodies for the Yee solver (conducting walls).
