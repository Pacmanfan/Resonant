"""A parameter study: change one value, calculate, tabulate.

Edit VALUES and the body of the loop. Each iteration works on its own copy,
so the open project is unchanged unless you apply a result.
"""
import resonant

VALUES = [0.003, 0.0043, 0.006]
results = []
for value in VALUES:
    p = project.copy()  # noqa: F821
    # Change the design, for example: p.object("Array").set_component({"spacingX": value})
    # Calculate, for example:        r = resonant.acoustics.pressure(p, [(0, 0, 0.06)])
    results.append({"value": value})
for r in results:
    print(r)
