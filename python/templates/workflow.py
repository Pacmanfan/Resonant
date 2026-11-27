"""Run a workflow template on a copy of the project and print its steps."""
import resonant

for w in resonant.automation.templates():
    print(f"{w['id']:<36} {w['name']}")
run = resonant.automation.run(project.copy(), "em.dipole-pattern")  # noqa: F821
print(run["status"], run["message"])
