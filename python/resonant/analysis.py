"""Analysis helpers on results (NumPy)."""


def peak(field: dict) -> dict:
    """Largest |p| of a field result and its grid index (w, v, u)."""
    import numpy as np
    magnitude = np.abs(field["pressure"])
    index = np.unravel_index(int(np.argmax(magnitude)), magnitude.shape)
    return {"max_pressure": float(magnitude[index]), "index": tuple(int(i) for i in index)}


def width_6db(values, spacing: float) -> float:
    """Full width (same unit as spacing) of |values| >= peak / 2 around the peak of a 1D cut;
    NaN when the half level is not reached on both sides."""
    import numpy as np
    m = np.abs(np.asarray(values))
    k = int(np.argmax(m))
    half = m[k] / 2.0
    left = k
    while left > 0 and m[left] >= half:
        left -= 1
    right = k
    while right < len(m) - 1 and m[right] >= half:
        right += 1
    if m[left] >= half or m[right] >= half:
        return float("nan")
    return float((right - left) * spacing)
