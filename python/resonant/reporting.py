"""Reports: tables, CSV and HTML (standard library); plots with matplotlib when installed."""
import csv
import html
import json


def write_csv(path: str, rows) -> None:
    rows = list(rows)
    if not rows:
        raise ValueError("no rows")
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def write_html(path: str, title: str, sections) -> None:
    """sections: [(heading, value)], values are text, dicts, lists or image file names ending in .png."""
    parts = [f"<!DOCTYPE html><html><head><meta charset='utf-8'><title>{html.escape(title)}</title>"
             "<style>body{font-family:sans-serif;margin:2em}table{border-collapse:collapse}td,th{border:1px solid #bbb;padding:3px 8px}</style>"
             f"</head><body><h1>{html.escape(title)}</h1>"]
    for heading, value in sections:
        parts.append(f"<h2>{html.escape(str(heading))}</h2>")
        if isinstance(value, str) and value.endswith(".png"):
            parts.append(f"<img src='{html.escape(value)}' style='max-width:100%'>")
        elif isinstance(value, dict):
            parts.append("<table>" + "".join(f"<tr><th>{html.escape(str(k))}</th><td>{html.escape(json.dumps(v) if not isinstance(v, str) else v)}</td></tr>"
                                             for k, v in value.items()) + "</table>")
        else:
            parts.append(f"<pre>{html.escape(value if isinstance(value, str) else json.dumps(value, indent=2))}</pre>")
    parts.append("</body></html>\n")
    with open(path, "w", encoding="utf-8") as f:
        f.write("".join(parts))


def plot_field(field: dict, path: str = None, title: str = "|p|"):
    """An image of |p| of a plane field result (matplotlib); saved to `path` when given."""
    import matplotlib
    if path is not None:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    magnitude = np.abs(field["pressure"])[0]
    fig, ax = plt.subplots()
    image = ax.imshow(magnitude, origin="lower", cmap="viridis")
    fig.colorbar(image, ax=ax, label="Pa")
    ax.set_title(title)
    if path is not None:
        fig.savefig(path, dpi=100)
        plt.close(fig)
    return fig
