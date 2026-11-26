# Acoustic Models, Conventions and Units

**Status:** current for REQ-CHAT-20261126-003.

## Conventions

| Subject | Convention |
|---|---|
| Time dependence | `p(x, t) = Re{ P(x) e^{+jωt} }`, `ω = 2πf` |
| Outgoing wave | `e^{-jkr}/r`, `k = ω/c` |
| Phase sign | a positive source phase leads; a delay `τ` multiplies by `e^{-jωτ}` |
| Amplitudes | complex amplitudes are **peak** values; `p_rms = |P|/√2` |
| Units | SI: Pa, m, s, Hz, kg/m³, m/s, W/m²; angles in radians |
| Emitter axis | an emitter radiates along its local **+Z**; directivity angle `θ` is measured from it |

Local emitter directions are turned into world directions with the object's
world matrix (`transformDirection`), and array element positions with
`transformPoint`.

## Source strength

An emitter's **amplitude** `A` is its free-field pressure amplitude (peak)
at the reference distance `r_ref = 1 m` on its axis, in pascal. Its field is

```text
P_i(x) = A_i · e^{j(φ_i − ω τ_i)} · (r_ref / r_i) · D_i(θ_i) · e^{−α r_i} · e^{−j k r_i}
```

with `φ_i` the initial phase, `τ_i` the time delay, `D_i` the directivity
and `α` the medium's amplitude attenuation coefficient (Np/m). The total
field is the sum over the sources, `P(x) = Σ_i P_i(x)`.

The strength belongs to each source. Nothing is divided by the number of
sources, so adding an emitter never changes what another emitter's
amplitude means (test `SourceCountDoesNotRescaleAmplitudes`).

## Directivity

| Model | `D(θ)` | Meaning |
|---|---|---|
| Omnidirectional | `1` | ideal point source |
| Cardioid family | `((1 + cos θ)/2)^n`, `n ≥ 0` | directional point source; `n = 1` is a cardioid |
| Baffled circular piston | `2 J₁(k a sin θ)/(k a sin θ)` for `θ < 90°`, `0` behind | far-field pattern of a rigid piston of radius `a` in an infinite baffle |

The piston pattern is a **far-field** pattern applied at every distance. It
is an approximation closer than about `a²/λ` to a physical transducer. An
emitter can be marked as standing for a physical transducer; that flag is
stored and shown, but no measured frequency response, phase response,
impedance or calibration is modelled yet. An ideal point source is not a
model of a real transducer, and the application never presents it as one.

## Near a source

`1/r` diverges at a point source. Within `max(source radius, minimum
distance)` of a source, that source's contribution is held at its value on
that sphere, the sample is flagged `nearSource`, and field statistics leave
flagged samples out. The result is finite everywhere and the user is told
where the model does not apply (tests `NoSingularityAtOrNearASource`,
`SamplesInsideASourceAreFlaggedAndKeptOutOfTheStatistics`).

## Field quantities

| Quantity | Definition | Unit | Validity |
|---|---|---|---|
| Complex pressure | `P` | Pa (peak) | the model's |
| Magnitude | `|P|` | Pa (peak) | |
| Phase | `arg P` | rad | |
| RMS pressure | `|P|/√2` | Pa | harmonic signal |
| Sound pressure level | `20 log₁₀(p_rms/p_ref)` | dB re `p_ref` | `p_ref` = 20 µPa in air, 1 µPa in water; always stated with the value |
| Particle velocity | `V = −∇P/(jωρ)` | m/s (peak) | exact for omnidirectional sources; the gradient of a directivity is neglected otherwise, and reports say so |
| Active intensity | `I = ½ Re{P V*}` | W/m² | as particle velocity |
| Plane-wave intensity | `|P|²/(2ρc)` | W/m² | **approximation**: one progressive wave far from sources only |
| Instantaneous pressure | `Re{P e^{jωt}}` | Pa | |
| Relative intensity (display) | `|P|²/|P|²_max` | — | a normalized picture, never labelled as intensity in W/m² |

The plane-wave formula is not used to report intensity. Test
`PlaneWaveFormulaIsWrongInAStandingWave` shows why: midway between two equal
sources the active intensity is zero while the formula gives four times one
source's intensity.

## Medium

The propagation medium is a project setting, chosen explicitly. It is never
taken from the material assigned to an object.

* **Air (ideal gas):** `c = √(γ R T)`, `ρ = p/(R T)` with `γ = 1.4`,
  `R = 287.05 J/(kg K)`, `p = 101 325 Pa`. Theoretical; humidity, pressure
  changes and absorption are not modelled. The temperature is configurable.
* **Water (nominal):** `ρ = 998 kg/m³`, `c = 1482 m/s`. Illustrative, fixed
  values; the temperature dependence of water is not modelled.
* **Custom:** the user's density and speed of sound.

Attenuation is one coefficient at the simulated frequency, entered by the
user (zero by default).

Distinctions the model keeps: the *propagation medium* (used), the *solid
material* of an object, a *reflector material* and a *transducer material*
(all stored on objects, none used by the free-field model).

## Assumptions stored with every result

Free field; objects do not influence the field; point sources with
spherical spreading and linear superposition; harmonic steady state at one
frequency; the near-source limit; how directivity and attenuation were
treated; and how many sources at other frequencies were left out.
