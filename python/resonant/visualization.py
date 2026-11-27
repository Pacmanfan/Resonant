"""Plots of results (matplotlib, optional)."""
from .reporting import plot_field  # noqa: F401


def plot_pattern_cut(pattern: dict, phi_index: int = 0, path: str = None, title: str = "Pattern cut"):
    """|E| (dB) along theta at one phi column of a pattern result."""
    import matplotlib
    if path is not None:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    m = np.asarray(pattern["magnitude"])[:, phi_index]
    theta = np.arange(m.size) * pattern["step_deg"]
    fig, ax = plt.subplots()
    ax.plot(theta, 20 * np.log10(np.maximum(m, 1e-6)))
    ax.set_xlabel("theta (deg)")
    ax.set_ylabel("level (dB)")
    ax.set_title(title)
    ax.set_ylim(-60, 1)
    if path is not None:
        fig.savefig(path, dpi=100)
        plt.close(fig)
    return fig
