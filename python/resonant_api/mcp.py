"""The Model Context Protocol adapter of the automation API (REQ-CHAT-20261126-012).

    python3 -m resonant_api.mcp [--socket PATH] [--allow modify,execute,files]

Exposes chosen resonant.v1 methods as MCP tools to an AI assistant. The tool
list, descriptions and input schemas come from the contract (the .proto
comments), and every call goes through the same API, sessions, permissions,
validation and audit trail as any other client: the session is opened as an
MCP client (source interface MCP) and holds only the permissions allowed here.

By default only reading tools are offered. --allow modify adds project
changes (each one undoable, dry runs available), --allow execute adds jobs
(simulations, optimizations, workflows), --allow files adds open / save
within the application's project directory. No tool drives hardware outputs:
the API has none. Not a sandbox: allowed tools act on the open project.

McpAdapter is independent of any MCP SDK (and tested without one);
serve_stdio() runs it with the `mcp` package when that is installed.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from typing import Any, Dict, List, Optional

from google.protobuf import descriptor as _descriptor
from google.protobuf import json_format

from .client import Client, connect
from .contract import Contract
from .transports import ApiError

LEVELS = ("read", "modify", "execute", "files")
PERMISSION_OF_LEVEL = {"read": "PERMISSION_READ", "modify": "PERMISSION_MODIFY", "execute": "PERMISSION_EXECUTE", "files": "PERMISSION_FILES"}


@dataclass(frozen=True)
class ToolSpec:
    method: str  # "SceneService.GetObject"
    level: str  # read, modify, execute, files


# The tools offered (sessions, transactions and streams stay with programmatic clients).
TOOLS: Dict[str, ToolSpec] = {
    "get_application_info": ToolSpec("ApplicationService.GetApplicationInfo", "read"),
    "get_capabilities": ToolSpec("ApplicationService.GetCapabilities", "read"),
    "get_diagnostics": ToolSpec("ApplicationService.GetDiagnostics", "read"),
    "get_project": ToolSpec("ProjectService.GetProject", "read"),
    "validate_project": ToolSpec("ProjectService.ValidateProject", "read"),
    "get_scene_snapshot": ToolSpec("SceneService.GetSceneSnapshot", "read"),
    "list_objects": ToolSpec("SceneService.ListObjects", "read"),
    "get_object": ToolSpec("SceneService.GetObject", "read"),
    "validate_object": ToolSpec("SceneService.ValidateObject", "read"),
    "list_geometry_kinds": ToolSpec("GeometryService.ListGeometryKinds", "read"),
    "validate_geometry": ToolSpec("GeometryService.ValidateGeometry", "read"),
    "list_materials": ToolSpec("MaterialService.ListMaterials", "read"),
    "get_material": ToolSpec("MaterialService.GetMaterial", "read"),
    "list_emitters": ToolSpec("EmitterService.ListEmitters", "read"),
    "get_emitter": ToolSpec("EmitterService.GetEmitter", "read"),
    "get_array": ToolSpec("ArrayService.GetArray", "read"),
    "list_solvers": ToolSpec("SimulationService.ListSolvers", "read"),
    "estimate_acoustic_field": ToolSpec("SimulationService.EstimateAcousticField", "read"),
    "list_results": ToolSpec("ResultsService.ListResults", "read"),
    "get_result": ToolSpec("ResultsService.GetResult", "read"),
    "read_field_samples": ToolSpec("ResultsService.ReadFieldSamples", "read"),
    "list_jobs": ToolSpec("JobService.ListJobs", "read"),
    "get_job": ToolSpec("JobService.GetJob", "read"),
    "list_events": ToolSpec("EventService.ListEvents", "read"),
    "query_audit": ToolSpec("AuditService.QueryAudit", "read"),
    "list_workflow_templates": ToolSpec("WorkflowService.ListWorkflowTemplates", "read"),
    "get_workflow_run": ToolSpec("WorkflowService.GetWorkflowRun", "read"),
    "list_optimization_problems": ToolSpec("OptimizationService.ListOptimizationProblems", "read"),
    "list_optimization_runs": ToolSpec("OptimizationService.ListOptimizationRuns", "read"),
    "list_coupling_models": ToolSpec("MultiphysicsService.ListCouplingModels", "read"),
    "list_physical_systems": ToolSpec("MultiphysicsService.ListPhysicalSystems", "read"),
    "get_physical_system": ToolSpec("MultiphysicsService.GetPhysicalSystem", "read"),
    "validate_physical_system": ToolSpec("MultiphysicsService.ValidatePhysicalSystem", "read"),
    "get_coupled_result": ToolSpec("MultiphysicsService.GetCoupledResult", "read"),
    "set_physical_system": ToolSpec("MultiphysicsService.SetPhysicalSystem", "modify"),
    "delete_physical_system": ToolSpec("MultiphysicsService.DeletePhysicalSystem", "modify"),
    "start_coupled_simulation": ToolSpec("MultiphysicsService.StartCoupledSimulation", "execute"),
    "list_analysis_kinds": ToolSpec("FunctionalStructureService.ListAnalysisKinds", "read"),
    "list_functional_structures": ToolSpec("FunctionalStructureService.ListFunctionalStructures", "read"),
    "get_functional_structure": ToolSpec("FunctionalStructureService.GetFunctionalStructure", "read"),
    "get_structure_parameters": ToolSpec("FunctionalStructureService.GetStructureParameters", "read"),
    "validate_functional_structure": ToolSpec("FunctionalStructureService.ValidateFunctionalStructure", "read"),
    "get_functional_result": ToolSpec("FunctionalStructureService.GetFunctionalResult", "read"),
    "explain_candidates": ToolSpec("FunctionalStructureService.ExplainCandidates", "read"),
    "set_functional_structure": ToolSpec("FunctionalStructureService.SetFunctionalStructure", "modify"),
    "delete_functional_structure": ToolSpec("FunctionalStructureService.DeleteFunctionalStructure", "modify"),
    "set_structure_parameters": ToolSpec("FunctionalStructureService.SetStructureParameters", "modify"),
    "analyze_structure": ToolSpec("FunctionalStructureService.AnalyzeStructure", "execute"),
    "sweep_structure": ToolSpec("FunctionalStructureService.SweepStructure", "execute"),
    "compute_structure_sensitivity": ToolSpec("FunctionalStructureService.ComputeStructureSensitivity", "execute"),
    "optimize_structure": ToolSpec("FunctionalStructureService.OptimizeStructure", "execute"),
    "create_effective_material": ToolSpec("FunctionalStructureService.CreateEffectiveMaterial", "modify"),
    "get_multiscale_model": ToolSpec("MultiscaleService.GetMultiscaleModel", "read"),
    "list_representations": ToolSpec("MultiscaleService.ListRepresentations", "read"),
    "get_representation": ToolSpec("MultiscaleService.GetRepresentation", "read"),
    "list_derivation_methods": ToolSpec("MultiscaleService.ListDerivationMethods", "read"),
    "list_model_cards": ToolSpec("MultiscaleService.ListModelCards", "read"),
    "list_multiscale_couplings": ToolSpec("MultiscaleService.ListMultiscaleCouplings", "read"),
    "list_external_solvers": ToolSpec("MultiscaleService.ListExternalSolvers", "read"),
    "get_multiscale_result": ToolSpec("MultiscaleService.GetMultiscaleResult", "read"),
    "get_material_lineage": ToolSpec("MultiscaleService.GetMaterialLineage", "read"),
    "get_local_state": ToolSpec("MultiscaleService.GetLocalState", "read"),
    "set_representation": ToolSpec("MultiscaleService.SetRepresentation", "modify"),
    "delete_representation": ToolSpec("MultiscaleService.DeleteRepresentation", "modify"),
    "generate_microstructure": ToolSpec("MultiscaleService.GenerateMicrostructure", "modify"),
    "create_atomistic_structure": ToolSpec("MultiscaleService.CreateAtomisticStructure", "modify"),
    "assign_derived_properties": ToolSpec("MultiscaleService.AssignDerivedProperties", "modify"),
    "derive_material_properties": ToolSpec("MultiscaleService.DeriveMaterialProperties", "execute"),
    "iterate_region_coupling": ToolSpec("MultiscaleService.IterateRegionCoupling", "execute"),
    "identify_resonances": ToolSpec("ResonanceService.IdentifyResonances", "read"),
    "create_primitive": ToolSpec("GeometryService.CreatePrimitive", "modify"),
    "update_geometry_parameters": ToolSpec("GeometryService.UpdateGeometryParameters", "modify"),
    "update_object": ToolSpec("SceneService.UpdateObject", "modify"),
    "delete_objects": ToolSpec("SceneService.DeleteObjects", "modify"),
    "duplicate_objects": ToolSpec("SceneService.DuplicateObjects", "modify"),
    "create_group": ToolSpec("SceneService.CreateGroup", "modify"),
    "create_material": ToolSpec("MaterialService.CreateMaterial", "modify"),
    "delete_material": ToolSpec("MaterialService.DeleteMaterial", "modify"),
    "assign_material": ToolSpec("MaterialService.AssignMaterial", "modify"),
    "remove_material_assignment": ToolSpec("MaterialService.RemoveMaterialAssignment", "modify"),
    "create_emitter": ToolSpec("EmitterService.CreateEmitter", "modify"),
    "update_emitter": ToolSpec("EmitterService.UpdateEmitter", "modify"),
    "create_array": ToolSpec("ArrayService.CreateArray", "modify"),
    "create_field_region": ToolSpec("SimulationService.CreateFieldRegion", "modify"),
    "set_array_excitation": ToolSpec("ArrayService.SetArrayExcitation", "modify"),
    "focus_array": ToolSpec("ArrayService.FocusArray", "modify"),
    "execute_batch": ToolSpec("TransactionService.ExecuteBatch", "modify"),
    "undo": ToolSpec("ProjectService.Undo", "modify"),
    "redo": ToolSpec("ProjectService.Redo", "modify"),
    "start_acoustic_field_simulation": ToolSpec("SimulationService.StartAcousticFieldSimulation", "execute"),
    "cancel_job": ToolSpec("JobService.CancelJob", "execute"),
    "wait_for_job": ToolSpec("JobService.WaitForJob", "read"),
    "execute_workflow": ToolSpec("WorkflowService.ExecuteWorkflow", "execute"),
    "start_optimization": ToolSpec("OptimizationService.StartOptimization", "execute"),
    "save_project": ToolSpec("ProjectService.SaveProject", "files"),
    "open_project": ToolSpec("ProjectService.OpenProject", "files"),
}

_MUTATING_LEVELS = {"modify", "files"}
_SCALARS = {
    _descriptor.FieldDescriptor.TYPE_DOUBLE: {"type": "number"},
    _descriptor.FieldDescriptor.TYPE_FLOAT: {"type": "number"},
    _descriptor.FieldDescriptor.TYPE_INT32: {"type": "integer"},
    _descriptor.FieldDescriptor.TYPE_SINT32: {"type": "integer"},
    _descriptor.FieldDescriptor.TYPE_UINT32: {"type": "integer", "minimum": 0},
    _descriptor.FieldDescriptor.TYPE_INT64: {"type": "integer"},
    _descriptor.FieldDescriptor.TYPE_UINT64: {"type": "integer", "minimum": 0},
    _descriptor.FieldDescriptor.TYPE_BOOL: {"type": "boolean"},
    _descriptor.FieldDescriptor.TYPE_STRING: {"type": "string"},
    _descriptor.FieldDescriptor.TYPE_BYTES: {"type": "string", "contentEncoding": "base64"},
}


class McpAdapter:
    """Tools over a Client (whose session holds the allowed permissions)."""

    def __init__(self, client: Client, allow=("read",)):
        unknown = set(allow) - set(LEVELS)
        if unknown:
            raise ValueError(f"unknown permission levels: {sorted(unknown)} (known: {', '.join(LEVELS)})")
        self.client = client
        self.contract: Contract = client.contract
        self.allowed = {"read", *allow}

    # --- schema ------------------------------------------------------------------------------

    def _field_comment(self, message_name: str, field_name: str) -> str:
        doc = self.contract.messages.get(message_name)
        if doc:
            for f in doc.fields:
                if f.name == field_name:
                    return f.comment
        return ""

    def _schema_of(self, desc, depth: int = 0) -> Dict[str, Any]:
        if desc.full_name == "google.protobuf.Struct":
            return {"type": "object"}
        if desc.full_name == "google.protobuf.Timestamp":
            return {"type": "string", "format": "date-time"}
        if depth > 6:
            return {"type": "object"}
        props: Dict[str, Any] = {}
        for f in desc.fields:
            if f.name == "header" and f.message_type and f.message_type.full_name == "resonant.v1.RequestHeader":
                continue
            props[f.name] = self._field_schema(desc.full_name, f, depth)
        return {"type": "object", "properties": props, "additionalProperties": False}

    def _field_schema(self, owner: str, f, depth: int) -> Dict[str, Any]:
        if f.message_type and f.message_type.GetOptions().map_entry:
            value = f.message_type.fields_by_name["value"]
            schema = {"type": "object", "additionalProperties": self._single(value, depth + 1)}
        else:
            schema = self._single(f, depth)
            if f.label == f.LABEL_REPEATED:
                schema = {"type": "array", "items": schema}
        comment = self._field_comment(owner, f.name)
        if comment:
            schema = dict(schema, description=comment)
        return schema

    def _single(self, f, depth: int) -> Dict[str, Any]:
        if f.type == f.TYPE_MESSAGE:
            return self._schema_of(f.message_type, depth + 1)
        if f.type == f.TYPE_ENUM:
            return {"type": "string", "enum": [v.name for v in f.enum_type.values if v.number != 0]}
        return dict(_SCALARS.get(f.type, {"type": "string"}))

    def tools(self) -> List[Dict[str, Any]]:
        """The allowed tools: name, description, inputSchema (JSON Schema)."""
        out = []
        for name, spec in TOOLS.items():
            if spec.level not in self.allowed:
                continue
            method = self.contract.method(spec.method)
            desc = self.contract.pool.FindMessageTypeByName(method.input_type)
            schema = self._schema_of(desc)
            if spec.level in _MUTATING_LEVELS:
                schema["properties"]["dry_run"] = {"type": "boolean", "description": "validate and report the change without applying it"}
                schema["properties"]["idempotency_key"] = {"type": "string", "description": "a retried call with the same key is applied once"}
            text = method.comment or re.sub(r"(?<!^)(?=[A-Z])", " ", method.short_name).lower()
            level_note = {"read": "Reads only.", "modify": "Changes the project (one undo step).",
                          "execute": "Starts or controls a job.", "files": "Opens or saves a project file."}[spec.level]
            out.append({"name": name, "description": f"{text} {level_note}".strip(), "inputSchema": schema})
        return out

    # --- calls ------------------------------------------------------------------------------------

    def call_tool(self, name: str, arguments: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """{"ok": True, "result": {...}} or {"ok": False, "error": {...}}; never raises for a refused call."""
        spec = TOOLS.get(name)
        if spec is None or spec.level not in self.allowed:
            return {"ok": False, "error": {"code": "PERMISSION_DENIED" if spec else "NOT_FOUND",
                                           "message": f"the tool {name!r} is not offered" + (" (its permission level is not allowed)" if spec else "")}}
        args = dict(arguments or {})
        header = {}
        if spec.level in _MUTATING_LEVELS:
            if args.pop("dry_run", False):
                header["dry_run"] = True
            key = args.pop("idempotency_key", "")
            if key:
                header["idempotency_key"] = key
        try:
            response = self.client.call(spec.method, header=header or None, **args)
        except json_format.ParseError as e:
            return {"ok": False, "error": {"code": "INVALID_ARGUMENT", "message": str(e)}}
        except ApiError as e:
            return {"ok": False, "error": {"code": e.code, "message": e.message, "violations": [{"field": f, "description": d} for f, d in e.violations],
                                           "current_revision": e.current_revision, "request_id": e.request_id}}
        return {"ok": True, "result": json_format.MessageToDict(response, preserving_proto_field_name=True)}


def open_adapter(allow=("read",), socket_path: Optional[str] = None, token_file: Optional[str] = None, **connect_args) -> McpAdapter:
    """An adapter with its own MCP session holding exactly the allowed permissions."""
    levels = {"read", *allow}
    client = connect(socket_path=socket_path, token_file=token_file, client_name=connect_args.pop("client_name", "MCP assistant"),
                     permissions=[PERMISSION_OF_LEVEL[level] for level in LEVELS if level in levels],
                     source_interface="SOURCE_INTERFACE_MCP", **connect_args)
    return McpAdapter(client, tuple(levels))


def serve_stdio(adapter: McpAdapter):  # pragma: no cover - needs an MCP client on stdin/stdout
    """Runs the adapter as an MCP server on stdin / stdout (needs the `mcp` package)."""
    import anyio
    import mcp.types as types
    from mcp.server.lowlevel import Server
    from mcp.server.stdio import stdio_server

    server = Server("resonant")

    @server.list_tools()
    async def list_tools() -> List[types.Tool]:
        return [types.Tool(name=t["name"], description=t["description"], inputSchema=t["inputSchema"]) for t in adapter.tools()]

    @server.call_tool()
    async def call_tool(name: str, arguments: Dict[str, Any]):
        result = await anyio.to_thread.run_sync(adapter.call_tool, name, arguments)
        text = json.dumps(result.get("result") if result["ok"] else {"error": result["error"]}, indent=2)
        if not result["ok"]:
            raise RuntimeError(text)
        return [types.TextContent(type="text", text=text)]

    async def run():
        async with stdio_server() as (read, write):
            await server.run(read, write, server.create_initialization_options())

    anyio.run(run)


def main(argv=None) -> int:  # pragma: no cover
    parser = argparse.ArgumentParser(prog="python3 -m resonant_api.mcp", description="Resonant's tools for MCP assistants.")
    parser.add_argument("--socket", help="the application's local endpoint")
    parser.add_argument("--token-file", help="the token file (default: beside the socket)")
    parser.add_argument("--allow", default="", help="extra permission levels: modify, execute, files (comma separated)")
    parser.add_argument("--list-tools", action="store_true", help="print the offered tools as JSON and exit")
    args = parser.parse_args(argv)
    allow = tuple(a.strip() for a in args.allow.split(",") if a.strip())
    adapter = open_adapter(allow, args.socket, args.token_file)
    if args.list_tools:
        print(json.dumps(adapter.tools(), indent=2))
        return 0
    serve_stdio(adapter)
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
