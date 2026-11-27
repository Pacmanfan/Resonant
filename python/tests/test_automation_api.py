"""The automation API from the outside (REQ-CHAT-20261126-012).

Against a real headless host (resonant-automation) and the gRPC bridge:
clients over gRPC and the local socket, structured errors, sessions and
permissions, optimistic concurrency between clients, idempotent retries,
transactions, events with snapshots and resumption, jobs and streamed
results, the MCP adapter's tools and permission levels, the in-process
transport, and call latencies. Skipped when this build has no automation API
(RESONANT_AUTOMATION_HOST / RESONANT_API_DESCRIPTORS unset or missing).
"""

import json
import os
import pathlib
import shutil
import statistics
import subprocess
import tempfile
import threading
import time
import unittest

HOST = os.environ.get("RESONANT_AUTOMATION_HOST", "")
DESCRIPTORS = os.environ.get("RESONANT_API_DESCRIPTORS", "")
AVAILABLE = bool(HOST and DESCRIPTORS and os.path.isfile(HOST) and os.path.isfile(DESCRIPTORS))

try:
    import grpc  # noqa: F401

    HAVE_GRPC = True
except ImportError:  # pragma: no cover
    HAVE_GRPC = False


def _start_host(directory: pathlib.Path, *extra):
    socket_path = directory / "automation.sock"
    process = subprocess.Popen([HOST, "--socket", str(socket_path), *extra], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    line = process.stdout.readline()
    if not line:
        raise RuntimeError("the host did not start: " + process.stderr.read())
    info = json.loads(line)
    return process, info["socket"], pathlib.Path(info["tokenFile"]).read_text().strip()


@unittest.skipUnless(AVAILABLE, "this build has no automation API (protobuf) or the host was not given")
class AutomationApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from resonant_api import load_contract

        cls.contract = load_contract()
        cls.dir = pathlib.Path(tempfile.mkdtemp(prefix="resonant-api-"))
        (cls.dir / "projects").mkdir()
        cls.host, cls.socket, cls.token = _start_host(cls.dir, "--project-dir", str(cls.dir / "projects"), "--allow-files")
        cls.server = None
        if HAVE_GRPC:
            from resonant_api.bridge import serve

            cls.server, port = serve(cls.socket, cls.token, port=0, contract=cls.contract)
            cls.target = f"127.0.0.1:{port}"

    @classmethod
    def tearDownClass(cls):
        if cls.server is not None:
            cls.server.stop(0).wait()
        cls.host.terminate()
        cls.host.wait(10)
        cls.host.stdout.close()
        cls.host.stderr.close()
        shutil.rmtree(cls.dir, ignore_errors=True)

    def client(self, name="test", over="grpc", **kw):
        from resonant_api import connect

        if over == "grpc" and HAVE_GRPC:
            return connect(grpc_target=self.target, token=self.token, client_name=name, contract=self.contract, **kw)
        return connect(socket_path=self.socket, token=self.token, client_name=name, contract=self.contract, **kw)

    # --- basics ---------------------------------------------------------------------------------

    def test_crud_over_both_transports_with_structured_errors(self):
        from resonant_api import ApiError

        for over in ("grpc", "local"):
            with self.client("crud " + over, over) as api:
                made = api.GeometryService.CreatePrimitive(kind_id="cone", parameters={"radius": 0.02, "height": 0.05})
                oid = made.object.object_id
                self.assertEqual(made.object.geometry.kind_id, "cone")
                got = api.SceneService.GetObject(object_id=oid)
                self.assertEqual(got.object.revision, made.object.revision)
                with self.assertRaises(ApiError) as e:
                    api.GeometryService.UpdateGeometryParameters(object_id=oid, parameters={"radius": -1})
                self.assertEqual(e.exception.code, "INVALID_ARGUMENT")
                self.assertEqual(e.exception.violations[0][0], "parameters.radius")
                with self.assertRaises(ApiError) as e:
                    api.SceneService.GetObject(object_id="no-such-object")
                self.assertEqual(e.exception.status_name, "NOT_FOUND")
                api.SceneService.DeleteObjects(object_ids=[oid])

    def test_a_dry_run_reports_without_changing(self):
        with self.client("dry") as api:
            before = api.ProjectService.GetProject().project
            plan = api.GeometryService.CreatePrimitive(kind_id="box", header={"dry_run": True})
            self.assertTrue(plan.header.dry_run)
            self.assertEqual(len(plan.changes.created), 1)
            after = api.ProjectService.GetProject().project
            self.assertEqual(after.revision, before.revision)
            self.assertEqual(after.object_count, before.object_count)

    def test_retries_with_an_idempotency_key_apply_once(self):
        with self.client("retry") as api:
            count = api.ProjectService.GetProject().project.object_count
            first = api.GeometryService.CreatePrimitive(kind_id="box", header={"idempotency_key": "make-one-box"})
            again = api.GeometryService.CreatePrimitive(kind_id="box", header={"idempotency_key": "make-one-box"})
            self.assertTrue(again.header.replayed)
            self.assertEqual(first.changes.created[0].id, again.changes.created[0].id)
            self.assertEqual(api.ProjectService.GetProject().project.object_count, count + 1)

    def test_sessions_hold_only_what_they_asked_for_within_the_policy(self):
        from resonant_api import ApiError

        with self.client("reader", permissions=["read"]) as api:
            with self.assertRaises(ApiError) as e:
                api.GeometryService.CreatePrimitive(kind_id="box")
            self.assertEqual(e.exception.code, "PERMISSION_DENIED")
        with self.client("admin please", permissions=["read", "admin"]) as api:
            self.assertEqual(list(api.session.permissions), [1])  # admin is not in this host's policy
        # Files: only names inside the project directory.
        with self.client("files", permissions=["read", "modify", "files"]) as api:
            saved = api.ProjectService.SaveProject(file_name="saved.resonant")
            self.assertEqual(saved.project.file_name, "saved.resonant")
            self.assertTrue((self.dir / "projects" / "saved.resonant").is_file())
            with self.assertRaises(ApiError):
                api.ProjectService.SaveProject(file_name="../escape.resonant")
            self.assertFalse((self.dir / "escape.resonant").exists())

    def test_capabilities_list_every_operation_and_what_is_not_available(self):
        with self.client("caps") as api:
            caps = api.ApplicationService.GetCapabilities()
            ids = {c.capability_id: c for c in caps.capabilities}
            self.assertTrue(ids["solver.acoustic.freefield.analytical"].available)
            self.assertFalse(ids["feature.hardware-outputs"].available)
            self.assertEqual(len(caps.operations), len([m for m in self.contract.methods if m.startswith("/resonant.v1.")]))

    # --- several clients ----------------------------------------------------------------------------

    def test_conflicting_edits_from_two_clients(self):
        from resonant_api import ApiError

        with self.client("alice") as alice, self.client("bob") as bob:
            made = alice.GeometryService.CreatePrimitive(kind_id="sphere")
            oid, rev = made.object.object_id, made.object.revision
            alice.SceneService.UpdateObject(object_id=oid, name="Alice's", expected_revision=rev)
            with self.assertRaises(ApiError) as e:
                bob.SceneService.UpdateObject(object_id=oid, name="Bob's", expected_revision=rev)
            self.assertEqual(e.exception.code, "CONFLICT")
            self.assertGreater(e.exception.current_revision, rev)
            self.assertEqual(bob.SceneService.GetObject(object_id=oid).object.name, "Alice's")

    def test_many_clients_at_once_lose_nothing_and_see_one_order(self):
        clients = [self.client(f"worker {k}") for k in range(4)]
        watcher = self.client("watcher")
        start = clients[0].ProjectService.GetProject().project.revision
        seen = []
        ready = threading.Event()

        def watch():
            for item in watcher.subscribe(categories=["EVENT_CATEGORY_SCENE"], include_snapshot=True):
                if item.WhichOneof("item") == "snapshot":
                    ready.set()
                    continue
                if item.WhichOneof("item") == "event" and item.event.type == "scene.object.created":
                    seen.append((item.event.sequence, item.event.source.session_id))
                    if len(seen) >= 100:
                        return

        t = threading.Thread(target=watch, daemon=True)
        t.start()
        self.assertTrue(ready.wait(10))

        def work(api):
            for _ in range(25):
                api.GeometryService.CreatePrimitive(kind_id="box")

        threads = [threading.Thread(target=work, args=(c,)) for c in clients]
        for th in threads:
            th.start()
        for th in threads:
            th.join(60)
        t.join(30)
        self.assertEqual(len(seen), 100)
        self.assertEqual([s for s, _ in seen], sorted(s for s, _ in seen))
        per = {}
        for _, session in seen:
            per[session] = per.get(session, 0) + 1
        self.assertEqual(sorted(per.values()), [25, 25, 25, 25])
        self.assertEqual(clients[0].ProjectService.GetProject().project.revision, start + 100)
        for c in clients + [watcher]:
            c.close()

    def test_a_subscription_resumes_after_a_sequence(self):
        with self.client("resume") as api:
            last = api.EventService.ListEvents(after_sequence=0, limit=1).last_sequence
            api.GeometryService.CreatePrimitive(kind_id="box", name="resumed")
            items = []
            for item in api.subscribe(after_sequence=last, timeout=10):
                items.append(item)
                if item.WhichOneof("item") == "event" and item.event.type == "project.changed":
                    break
            types = [i.event.type for i in items if i.WhichOneof("item") == "event"]
            self.assertIn("scene.object.created", types)

    def test_transactions_commit_once_or_detect_the_conflict(self):
        from resonant_api import ApiError

        with self.client("tx") as api, self.client("other") as other:
            t = api.TransactionService.BeginTransaction(description="two parts").transaction
            api.TransactionService.StageOperations(transaction_id=t.transaction_id, operations=[
                {"operation_id": "a", "create_primitive": {"kind_id": "box", "name": "Part A"}},
                {"operation_id": "b", "create_primitive": {"kind_id": "sphere", "name": "Part B"}},
            ])
            v = api.TransactionService.ValidateTransaction(transaction_id=t.transaction_id)
            self.assertTrue(v.transaction.validation.valid)
            done = api.TransactionService.CommitTransaction(transaction_id=t.transaction_id)
            self.assertEqual(done.transaction.state, 2)  # COMMITTED
            self.assertEqual(len(done.changes.created), 2)
            undo = api.ProjectService.Undo()
            self.assertIn("two parts", undo.undone)
            # Another client changes the project in between: the commit is refused.
            t2 = api.TransactionService.BeginTransaction(description="late").transaction
            api.TransactionService.StageOperations(transaction_id=t2.transaction_id, operations=[{"operation_id": "c", "create_primitive": {"kind_id": "box"}}])
            other.GeometryService.CreatePrimitive(kind_id="box")
            with self.assertRaises(ApiError) as e:
                api.TransactionService.CommitTransaction(transaction_id=t2.transaction_id)
            self.assertEqual(e.exception.code, "CONFLICT")

    # --- jobs and results -----------------------------------------------------------------------------

    def test_a_field_job_streams_its_result(self):
        import numpy as np

        with self.client("field") as api:
            api.ArrayService.CreateArray(count_x=8)
            region = api.SimulationService.CreateFieldRegion(
                name="Plane", kind="FIELD_REGION_KIND_PLANE_XY", count_u=41, count_v=31,
                local_transform={"position_meters": {"z_meters": 0.05}}).region
            plan = api.SimulationService.EstimateAcousticField(region_id=region.object_id).plan
            self.assertTrue(plan.allowed)
            self.assertEqual(plan.sample_points, 41 * 31)
            started = api.SimulationService.StartAcousticFieldSimulation(region_id=region.object_id)
            job = api.wait_for_job(started.job.job_id, 60)
            self.assertEqual(job.state, 6)
            self.assertEqual(len(job.results), 1)
            summary = api.ResultsService.GetResult(result_id=job.results[0].id).result
            self.assertEqual(summary.unit, "Pa")
            self.assertEqual((summary.grid.count_u, summary.grid.count_v), (41, 31))
            self.assertIn("not full-wave", summary.provenance.approximation)
            field = api.read_field_slice(summary.result_id)
            self.assertEqual(field.shape, (41, 31))
            self.assertEqual(field.dtype, np.complex64)
            self.assertAlmostEqual(float(np.abs(field).max()), summary.max_magnitude, places=3)
            coarse = api.read_field_slice(summary.result_id, stride=10)
            self.assertEqual(coarse.shape, (5, 4))
            point = api.ResultsService.ReadFieldSamples(result_id=summary.result_id, points_world_meters=[
                {"x_meters": 0.0, "y_meters": 0.0, "z_meters": 0.05}, {"x_meters": 5.0}]).samples
            self.assertTrue(point[0].inside)
            self.assertFalse(point[1].inside)

    def test_a_job_can_be_waited_for_and_cancelled_jobs_say_so(self):
        from resonant_api import ApiError

        with self.client("jobs") as api:
            run = api.WorkflowService.ExecuteWorkflow(workflow_id="em.dipole-pattern", apply_changes=False)
            job = api.wait_for_job(run.job.job_id, 60)
            self.assertEqual(job.state, 6)  # COMPLETED
            outcome = api.WorkflowService.GetWorkflowRun(job_id=run.job.job_id)
            self.assertTrue(outcome.finished)
            self.assertEqual(outcome.run["status"], "completed")
            with self.assertRaises(ApiError) as e:
                api.JobService.CancelJob(job_id=run.job.job_id)
            self.assertEqual(e.exception.code, "FAILED_PRECONDITION")

    # --- MCP -------------------------------------------------------------------------------------------

    def test_mcp_tools_follow_the_permission_levels(self):
        from resonant_api.mcp import McpAdapter

        with self.client("assistant", permissions=["read"], source_interface="SOURCE_INTERFACE_MCP") as api:
            reader = McpAdapter(api, ("read",))
            names = {t["name"] for t in reader.tools()}
            self.assertIn("get_scene_snapshot", names)
            self.assertNotIn("create_primitive", names)
            for tool in reader.tools():
                self.assertEqual(tool["inputSchema"]["type"], "object")
                self.assertNotIn("header", tool["inputSchema"]["properties"])
            refused = reader.call_tool("create_primitive", {"kind_id": "box"})
            self.assertFalse(refused["ok"])
            self.assertEqual(refused["error"]["code"], "PERMISSION_DENIED")
            ok = reader.call_tool("get_project", {})
            self.assertTrue(ok["ok"])
        with self.client("assistant", permissions=["read", "modify"], source_interface="SOURCE_INTERFACE_MCP") as api:
            writer = McpAdapter(api, ("modify",))
            count = writer.call_tool("get_project")["result"]["project"].get("object_count", 0)
            planned = writer.call_tool("create_primitive", {"kind_id": "box", "dry_run": True})
            self.assertTrue(planned["ok"])
            self.assertEqual(writer.call_tool("get_project")["result"]["project"].get("object_count", 0), count)
            made = writer.call_tool("create_primitive", {"kind_id": "box", "name": "From the assistant"})
            self.assertTrue(made["ok"], made)
            bad = writer.call_tool("create_primitive", {"kind_id": "box", "no_such_field": 1})
            self.assertFalse(bad["ok"])
            self.assertEqual(bad["error"]["code"], "INVALID_ARGUMENT")
            audit = api.AuditService.QueryAudit(operation="GeometryService.CreatePrimitive").records
            self.assertTrue(audit)
            self.assertTrue(all(r.source_interface == 4 for r in audit))  # MCP
            self.assertTrue(all(r.session_id == api.session_id for r in audit))

    # --- in process, latency ------------------------------------------------------------------------------

    def test_the_same_contract_in_process(self):
        try:
            import resonant
            from resonant._native import native
        except ImportError:
            self.skipTest("the resonant package is not importable here")
        if not hasattr(native, "AutomationSession"):
            self.skipTest("this module was built without the automation API")
        from resonant_api import connect

        project = resonant.Project("in process")
        with connect(project=project, client_name="local script", contract=self.contract) as api:
            api.GeometryService.CreatePrimitive(kind_id="box", name="Made in process")
            self.assertEqual([o.name for o in project.objects()], ["Made in process"])

    def test_call_latency(self):
        with self.client("latency", "local") as local:
            samples = []
            for _ in range(100):
                t = time.perf_counter()
                local.ProjectService.GetProject()
                samples.append(time.perf_counter() - t)
            local_ms = statistics.median(samples) * 1e3
        grpc_ms = float("nan")
        if HAVE_GRPC:
            with self.client("latency") as remote:
                samples = []
                for _ in range(100):
                    t = time.perf_counter()
                    remote.ProjectService.GetProject()
                    samples.append(time.perf_counter() - t)
                grpc_ms = statistics.median(samples) * 1e3
        print(f"\n[automation latency] GetProject median: local socket {local_ms:.2f} ms, gRPC via bridge {grpc_ms:.2f} ms")
        self.assertLess(local_ms, 50.0)


if __name__ == "__main__":
    unittest.main()
