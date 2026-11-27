# Inverse Design of Source Configurations

Given what the field should be at target points, find the source weights
that produce it. For linear physics at one frequency the targets respond
linearly to complex source weights w:

```text
achieved = A w,    A (targets x sources): the response of each target to a unit source
minimize   sum_m q_m |(A w - b)_m|^2 + (lambda + mu) sigma_max^2 ||w||^2
subject to |w_i| <= W            (bounds, when given)
           w_i = 0, i not in S   (sparse excitation, when asked)
```

lambda (regularization) and mu (power penalty) are relative to the largest
singular value squared of the weighted matrix: dimensionless and comparable
between problems. ||w||^2 is proportional to the drive power of identical
sources, so mu is an engineering trade-off and lambda a numerical
stabilization; both bias the solution towards smaller weights, and the
result says so.

## Methods (`optimization/LinearInverse`)

| Option | Method |
|---|---|
| regularized | Tikhonov by the singular value decomposition of the weighted matrix (no matrix inverted) |
| minimum norm | the pseudo-inverse (singular values below rcond x sigma_max dropped): the exact solution of smallest norm for a consistent system, least squares otherwise |
| bounds | the bounded problem is convex; it is solved by an accelerated projected gradient method (FISTA) to a relative step of 1e-12, the result satisfies the optimality conditions |
| sparse | orthogonal matching pursuit picks at most k sources, then the problem is solved on them: a good, not a guaranteed optimal, selection |

The result reports weights, achieved values, residual and relative residual,
condition number, rank, active sources, whether a bound is active, and a
**classification**: an exact solution (residual at numerical precision), an
exact solution of smallest norm of an underdetermined system (other exact
solutions exist), an approximate least-squares solution, or an approximate
solution that misses the targets ("the wanted field is not reachable with
these sources"). Notes state the regularization bias, ill-conditioning and
rank deficiency, and that the solution is exact for the model only.

## Where the response comes from

| Adapter | Sources | Targets | Response |
|---|---|---|---|
| acoustic | emitters and array elements at the analysis frequency | point targets (wanted amplitude and phase, or zero for suppression); line, plane, surface and volume targets sampled at half a wavelength | free-field or Kirchhoff-reflector transfer of a unit source (the full-wave model has no matrix here: re-evaluate the solution with it) |
| mechanical | actuators | vibration targets (wanted displacement along their axis, zero for suppression) | finite-element transfer at the harmonic frequency, one factorization of the dynamic stiffness |
| electromagnetic | array elements | configured directions with a wanted level (dB) or a null | element pattern x exp(j k r^ . r_n) (array factor) |

The weights become design values: amplitude \|w_i\| and phase arg w_i of each
source, delays set to zero (the delay is folded into the phase at this
frequency). In the Studio (Analysis tab, Solve) the solution is evaluated
with the problem's physical model and stored as a candidate of a
"linear-inverse" run, so it is compared, re-evaluated and promoted like any
other.

## Relation to the domains' own solvers

The acoustic module's `solveInverse` (point targets, clipping of amplitudes),
the mechanical `solveTargets` and the electromagnetic `synthesizeBeams`
remain for their panels. The framework reproduces their solutions:
`AcousticOptimization.InverseDesignAgreesWithTheAcousticSolverAndTheEvaluation`
(the same weights to 1e-9), `MechanicalOptimization.ActuatorInverseDesignAgreesWithTheVibrationTargeting`
(their regularization (lambda sigma)^2 equals ours with lambda^2).

## Validation

`OptimizationInverse.*`: square systems exactly; overdetermined systems
against the normal equations and Tikhonov against its closed form (1e-10);
underdetermined systems against the pseudo-inverse; bounded weights against
the optimality conditions; sparse excitation recovers a planted support.
`EmOptimization.ANullIsPlacedExactlyByTheLinearInverseAndCheckedByTheEvaluation`:
a null more than 100 dB down, confirmed by the evaluated pattern.
