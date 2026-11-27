// Built-in workflow templates (RES-AUTO-009): one or more per domain, each
// made only of implemented actions. They are data: the same JSON a user or a
// workflow plugin writes.
#include "coupling/WorkflowActions.h"

#include "foundation/Log.h"

namespace resonant::coupling {

namespace {

const char* const kTemplates[] = {
    R"json({
  "format": "resonant.workflow", "version": 1,
  "id": "acoustic.linear-array-focus", "workflowVersion": "1", "category": "acoustic",
  "name": "Acoustic: linear array, uniform against focused excitation",
  "description": "Builds a linear array of omnidirectional emitters, a focus target and a field plane, calculates the free field with uniform excitation, focuses the array on the target by phase, calculates the field again and reports both.",
  "requires": [], "expectedSeconds": 2, "docs": ["acoustics.focusing", "automation.workflows"],
  "parameters": [
    {"id": "frequency", "label": "Frequency", "type": "number", "default": 40000, "unit": "Hz"},
    {"id": "elements", "label": "Elements", "type": "integer", "default": 8, "unit": ""},
    {"id": "spacing", "label": "Element spacing", "type": "number", "default": 0.0043, "unit": "m"},
    {"id": "focusX", "label": "Focus x", "type": "number", "default": 0.01, "unit": "m"},
    {"id": "focusZ", "label": "Focus z", "type": "number", "default": 0.06, "unit": "m"}
  ],
  "steps": [
    {"id": "frequency", "action": "settings.set", "params": {"key": "acoustics", "values": {"frequencyHz": "${frequency}"}}},
    {"id": "array", "action": "scene.add", "params": {"type": "array", "id": "wf-array", "name": "Linear array",
      "component": {"layout": "linear", "countX": "${elements}", "spacingX": "${spacing}", "element": {"frequencyHz": "${frequency}", "radius": 0.001}}}},
    {"id": "target", "action": "scene.add", "params": {"type": "target", "id": "wf-focus", "name": "Focus", "position": ["${focusX}", 0, "${focusZ}"]}},
    {"id": "plane", "action": "scene.add", "params": {"type": "region", "id": "wf-plane", "name": "Field plane", "position": [0, 0, 0.045],
      "component": {"kind": "planeXZ", "size": [0.1, 0.01, 0.08], "countU": 61, "countV": 49}}},
    {"id": "uniform", "action": "acoustics.field", "params": {"region": "wf-plane"}, "dependsOn": ["array", "plane"]},
    {"id": "uniformAtFocus", "action": "acoustics.pressure", "params": {"points": [["${focusX}", 0, "${focusZ}"]]}},
    {"id": "focus", "action": "acoustics.focus", "params": {"array": "wf-array", "target": "wf-focus", "control": "phase"}},
    {"id": "focused", "action": "acoustics.field", "params": {"region": "wf-plane"}, "dependsOn": ["focus"]},
    {"id": "focusedAtFocus", "action": "acoustics.pressure", "params": {"points": [["${focusX}", 0, "${focusZ}"]]}},
    {"id": "improved", "action": "workflow.check", "params": {"value": "${steps.focusedAtFocus.values.0.magnitude}",
      "min": "${steps.uniformAtFocus.values.0.magnitude}", "message": "focusing did not raise the pressure at the focus"}},
    {"id": "report", "action": "report.html", "params": {"path": "linear-array-focus.html", "title": "Linear array: uniform and focused",
      "sections": [{"heading": "Uniform excitation", "data": "${steps.uniform}"}, {"heading": "Focused by phase", "data": "${steps.focused}"},
                   {"heading": "Pressure at the focus", "data": {"uniform": "${steps.uniformAtFocus.values.0}", "focused": "${steps.focusedAtFocus.values.0}"}}]}}
  ],
  "outputs": ["steps.uniform.maxPressure", "steps.focused.maxPressure", "steps.focusedAtFocus.values.0.magnitude"]
})json",

    R"json({
  "format": "resonant.workflow", "version": 1,
  "id": "acoustic.reflector-optimization", "workflowVersion": "1", "category": "optimization",
  "name": "Acoustic: optimize a parabolic reflector for a focus",
  "description": "Builds a parabolic reflector lit by a distant source and a target in front of it, selects the Kirchhoff reflector model and sweeps the focal length for the largest pressure at the target with the optimization framework.",
  "requires": [], "expectedSeconds": 5, "docs": ["acoustics.reflectors", "optimization.geometry"],
  "parameters": [
    {"id": "target", "label": "Target distance from the vertex", "type": "number", "default": 0.08, "unit": "m"},
    {"id": "source", "label": "Source distance", "type": "number", "default": 2.0, "unit": "m"},
    {"id": "evaluations", "label": "Sweep points", "type": "integer", "default": 25, "unit": ""}
  ],
  "steps": [
    {"id": "model", "action": "settings.set", "params": {"key": "acoustics", "values": {"model": "acoustic.reflector.kirchhoff", "reflectorElementsPerWavelength": 5}}},
    {"id": "reflector", "action": "scene.add", "params": {"type": "manipulator", "id": "wf-reflector", "name": "Parabolic reflector", "kind": "reflector.parabolic",
      "parameters": {"focalLength": 0.06, "aperture": 0.2}, "material": "mat-aluminum"}},
    {"id": "source", "action": "scene.add", "params": {"type": "emitter", "id": "wf-source", "name": "Distant source", "position": [0, 0, "${source}"],
      "rotation": [3.141592653589793, 0, 0], "component": {"radius": 0.002}}},
    {"id": "target", "action": "scene.add", "params": {"type": "target", "id": "wf-target", "name": "Wanted focus", "position": [0, 0, "${target}"]}},
    {"id": "optimize", "action": "optimization.run", "params": {"template": "acoustic.reflector-focus", "maxEvaluations": "${evaluations}"}},
    {"id": "report", "action": "report.html", "params": {"path": "reflector-optimization.html", "title": "Parabolic reflector for a focus",
      "sections": [{"heading": "Optimization", "data": "${steps.optimize}"}]}}
  ],
  "outputs": ["steps.optimize.best"]
})json",

    R"json({
  "format": "resonant.workflow", "version": 1,
  "id": "mechanical.modal-and-forced", "workflowVersion": "1", "category": "mechanical",
  "name": "Mechanical: modal and forced response, three thicknesses compared",
  "description": "Builds an aluminium cantilever with a shaker and a sensor at the tip, runs the modal analysis for three thicknesses (design variants), then the forced response of the last variant, and writes a report and a CSV of the natural frequencies.",
  "requires": [], "expectedSeconds": 3, "docs": ["mechanics.modal", "automation.workflows"],
  "parameters": [
    {"id": "length", "label": "Length", "type": "number", "default": 0.5, "unit": "m"},
    {"id": "thickness", "label": "Thickness", "type": "number", "default": 0.005, "unit": "m"},
    {"id": "thicknesses", "label": "Thicknesses compared", "type": "list", "default": [0.004, 0.005, 0.006], "unit": "m"}
  ],
  "steps": [
    {"id": "beam", "action": "scene.add", "params": {"type": "structure", "id": "wf-beam", "name": "Aluminium cantilever", "kind": "box",
      "parameters": {"width": "${length}", "depth": 0.03, "height": "${thickness}"}, "material": "mat-aluminum",
      "component": {"idealization": "beam", "supports": ["clamped", "free", "free", "free", "free", "free"]}}},
    {"id": "shaker", "action": "scene.add", "params": {"type": "actuator", "id": "wf-shaker", "name": "Shaker", "position": [0.245, 0, 0.0025]}},
    {"id": "sensor", "action": "scene.add", "params": {"type": "sensor", "id": "wf-sensor", "name": "Tip sensor", "position": [0.25, 0, 0.0025]}},
    {"id": "variants", "action": "mechanics.modal", "params": {"structure": "wf-beam", "modes": 4, "set": {"parameters": {"height": "${thicknesses}"}}},
      "repeat": {"parameter": "thicknesses"}},
    {"id": "forced", "action": "mechanics.harmonic", "params": {"structure": "wf-beam", "fMin": 5, "fMax": 200, "points": 200}},
    {"id": "csv", "action": "io.write-csv", "params": {"path": "cantilever-modes.csv", "rows": [
      {"thickness": 0.004, "f1": "${steps.variants.0.frequencies.0}", "f2": "${steps.variants.0.frequencies.1}"},
      {"thickness": 0.005, "f1": "${steps.variants.1.frequencies.0}", "f2": "${steps.variants.1.frequencies.1}"},
      {"thickness": 0.006, "f1": "${steps.variants.2.frequencies.0}", "f2": "${steps.variants.2.frequencies.1}"}]}},
    {"id": "report", "action": "report.html", "params": {"path": "cantilever.html", "title": "Cantilever: modes and forced response",
      "sections": [{"heading": "Natural frequencies per thickness", "data": "${steps.variants}"}, {"heading": "Forced response", "data": "${steps.forced}"}]}}
  ],
  "outputs": ["steps.variants", "steps.forced.peaks"]
})json",

    R"json({
  "format": "resonant.workflow", "version": 1,
  "id": "em.dipole-pattern", "workflowVersion": "1", "category": "electromagnetic",
  "name": "Electromagnetic: a dipole's impedance and radiation pattern",
  "description": "Builds a copper half-wave dipole, reports its induced-EMF impedance, directivity and return loss, and exports its normalized radiation pattern as CSV.",
  "requires": [], "expectedSeconds": 1, "docs": ["em.antennas", "em.patterns"],
  "parameters": [
    {"id": "frequency", "label": "Frequency", "type": "number", "default": 1e9, "unit": "Hz"},
    {"id": "length", "label": "Length", "type": "number", "default": 0.15, "unit": "m"}
  ],
  "steps": [
    {"id": "dipole", "action": "scene.add", "params": {"type": "antenna", "id": "wf-dipole", "name": "Half-wave dipole", "kind": "antenna.dipole",
      "parameters": {"length": "${length}", "wireRadius": 0.001}, "material": "mat-copper", "component": {"frequencyHz": "${frequency}"}}},
    {"id": "antenna", "action": "em.antenna", "params": {"object": "wf-dipole"}},
    {"id": "pattern", "action": "em.pattern", "params": {"object": "wf-dipole", "resolutionDeg": 2, "csv": "dipole-pattern.csv"}},
    {"id": "report", "action": "report.html", "params": {"path": "dipole.html", "title": "Half-wave dipole",
      "sections": [{"heading": "Antenna", "data": "${steps.antenna}"}, {"heading": "Pattern", "data": "${steps.pattern}"}]}}
  ],
  "outputs": ["steps.antenna", "steps.pattern.directivityDbi"]
})json",

    R"json({
  "format": "resonant.workflow", "version": 1,
  "id": "em.array-steering", "workflowVersion": "1", "category": "electromagnetic",
  "name": "Electromagnetic: steer an array and reduce its sidelobes",
  "description": "Builds a linear array of half-wave dipoles, analyses its broadside pattern, steers it by phase, applies a Dolph-Chebyshev taper and compares directivity and sidelobes of the three configurations.",
  "requires": [], "expectedSeconds": 2, "docs": ["em.arrays", "em.beamforming"],
  "parameters": [
    {"id": "elements", "label": "Elements", "type": "integer", "default": 12, "unit": ""},
    {"id": "steerPhi", "label": "Beam direction in the array's XY plane (90 = broadside)", "type": "number", "default": 70, "unit": "deg"},
    {"id": "sidelobes", "label": "Chebyshev sidelobe level", "type": "number", "default": 30, "unit": "dB"}
  ],
  "steps": [
    {"id": "array", "action": "scene.add", "params": {"type": "em-array", "id": "wf-em-array", "name": "Dipole array",
      "component": {"layout": "linear", "countX": "${elements}", "spacingX": 0.15, "frequencyHz": 1e9, "pattern": "halfWaveDipole"}}},
    {"id": "broadside", "action": "em.pattern", "params": {"object": "wf-em-array", "resolutionDeg": 1}},
    {"id": "steer", "action": "em.steer", "params": {"array": "wf-em-array", "thetaDeg": 90, "phiDeg": "${steerPhi}"}},
    {"id": "steered", "action": "em.pattern", "params": {"object": "wf-em-array", "resolutionDeg": 1}},
    {"id": "taper", "action": "em.taper", "params": {"array": "wf-em-array", "sidelobeDb": "${sidelobes}"}},
    {"id": "tapered", "action": "em.pattern", "params": {"object": "wf-em-array", "resolutionDeg": 1, "csv": "tapered-pattern.csv"}},
    {"id": "lower", "action": "workflow.check", "params": {"value": "${steps.tapered.sidelobeDb}", "max": "${steps.steered.sidelobeDb}",
      "message": "the taper did not lower the sidelobes"}},
    {"id": "table", "action": "io.write-json", "params": {"path": "array-steering.json",
      "data": {"broadside": "${steps.broadside}", "steered": "${steps.steered}", "tapered": "${steps.tapered}"}}}
  ],
  "outputs": ["steps.broadside.sidelobeDb", "steps.steered.mainPhiDeg", "steps.tapered.sidelobeDb"]
})json",
};

}  // namespace

void registerBuiltinWorkflows(automation::WorkflowLibrary& library)
{
    for (const char* text : kTemplates) {
        auto w = automation::workflowFromJson(nlohmann::json::parse(text));
        if (!w) {
            logError("automation", "a built-in workflow is invalid: " + w.message());
            continue;
        }
        library.add(w.take(), "built-in");
    }
}

}  // namespace resonant::coupling

namespace resonant::automation {

WorkflowLibrary& WorkflowLibrary::instance()
{
    static WorkflowLibrary* library = [] {
        auto* created = new WorkflowLibrary;
        coupling::registerBuiltinWorkflows(*created);
        return created;
    }();
    return *library;
}

}  // namespace resonant::automation
