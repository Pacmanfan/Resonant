"""The Python worker process of the Resonant console and script editor
(execution protocol version 1, ADR-000215).

The application starts ``python3 -m resonant.worker`` and talks to it with
one JSON object per line. Requests (stdin):

    {"id": n, "op": "exec", "code": "...", "mode": "single"|"block"|"exec", "filename": "<console>"}
    {"id": n, "op": "complete", "text": "proj"}            -> completions
    {"id": n, "op": "inspect", "expression": "project"}    -> repr and documentation
    {"id": n, "op": "set_project", "project": {...}}       -> the console's `project`
    {"id": n, "op": "load_plugins", "plugins": [dir, ...]} -> extensions of each plugin
    {"id": n, "op": "call", "plugin": id, "extension": id, "params": {...}}
    {"id": n, "op": "reset"}

Replies (stdout): {"id", "type": "stdout"|"stderr"|"result"|"error"|"done"|
"completions"|"inspect"|"plugins"|"apply"|"pull"|"message", ...}. Every
request ends with "done" (ok true/false). An error carries the traceback and
the line number in the submitted code. The application interrupts a running
request with SIGINT (KeyboardInterrupt); the worker process isolates Python
from the application, so a crash in a script or extension never takes the
application down.
"""
import code
import io
import json
import rlcompleter
import sys
import traceback

PROTOCOL_VERSION = 1


class Worker:
    def __init__(self, out):
        self._out = out
        self.namespace = {"__name__": "__console__"}
        self.plugins = {}
        self.current = None
        self._setup()

    def send(self, message: dict) -> None:
        if self.current is not None and "id" not in message:
            message["id"] = self.current
        self._out.write(json.dumps(message) + "\n")
        self._out.flush()

    def _setup(self):
        try:
            import resonant
            from resonant import app
            app._channel = self.send
            self.namespace.update({"resonant": resonant, "app": app})
            self.send({"type": "ready", "protocol": PROTOCOL_VERSION, "api": resonant.API_VERSION, "core": resonant.CORE_VERSION})
        except Exception as error:  # the console still works, without the API
            self.send({"type": "ready", "protocol": PROTOCOL_VERSION, "api": None, "error": f"{type(error).__name__}: {error}"})

    # --- requests ---
    def handle(self, request: dict) -> None:
        self.current = request.get("id")
        op = request.get("op")
        ok = True
        try:
            if op == "exec":
                ok = self._exec(request.get("code", ""), request.get("mode", "exec"), request.get("filename", "<console>"))
            elif op == "complete":
                self.send({"type": "completions", "items": self._complete(request.get("text", ""))})
            elif op == "inspect":
                self.send({"type": "inspect", **self._inspect(request.get("expression", ""))})
            elif op == "set_project":
                import resonant
                self.namespace["project"] = resonant.Project.from_json(json.dumps(request["project"]))
            elif op == "load_plugins":
                self.send({"type": "plugins", "plugins": self._load_plugins(request.get("plugins", []))})
            elif op == "call":
                ok = self._call(request)
            elif op == "reset":
                project = self.namespace.get("project")
                self.namespace = {"__name__": "__console__"}
                self._setup()
                if project is not None:
                    self.namespace["project"] = project
            else:
                raise ValueError(f"unknown request '{op}'")
        except KeyboardInterrupt:
            ok = False
            self.send({"type": "error", "error": "KeyboardInterrupt", "message": "stopped", "traceback": "", "line": None})
        except Exception as error:
            ok = False
            self.send({"type": "error", "error": type(error).__name__, "message": str(error), "traceback": traceback.format_exc(), "line": None})
        self.send({"type": "done", "ok": ok})
        self.current = None

    def _exec(self, source: str, mode: str, filename: str) -> bool:
        stdout, stderr = sys.stdout, sys.stderr
        sys.stdout = _Stream(self, "stdout")
        sys.stderr = _Stream(self, "stderr")
        try:
            if mode in ("single", "block"):
                # single: one interactive statement (expression values are shown);
                # block: several lines typed or pasted at the prompt.
                compiled = code.compile_command(source, filename, "single" if mode == "single" else "exec")
                if compiled is None:
                    self.send({"type": "incomplete"})
                    return True
            else:
                compiled = compile(source, filename, "exec")
            exec(compiled, self.namespace)
            return True
        except KeyboardInterrupt:
            self.send({"type": "error", "error": "KeyboardInterrupt", "message": "stopped by the user", "traceback": "", "line": None})
            return False
        except BaseException as error:  # SystemExit included: the worker keeps running
            tb = traceback.extract_tb(error.__traceback__)
            line = next((f.lineno for f in reversed(tb) if f.filename == filename), None)
            if isinstance(error, SyntaxError) and error.filename == filename:
                line = error.lineno
            self.send({"type": "error", "error": type(error).__name__, "message": str(error),
                       "traceback": "".join(traceback.format_exception(type(error), error, error.__traceback__)), "line": line})
            return False
        finally:
            sys.stdout.flush()
            sys.stdout, sys.stderr = stdout, stderr

    def _complete(self, text: str):
        completer = rlcompleter.Completer(self.namespace)
        items = []
        for state in range(200):
            match = completer.complete(text, state)
            if match is None:
                break
            if match not in items:
                items.append(match)
        return items

    def _inspect(self, expression: str) -> dict:
        value = eval(expression, self.namespace)  # noqa: S307 - the user's own console
        return {"repr": repr(value)[:2000], "valueType": type(value).__name__, "doc": (getattr(value, "__doc__", None) or "")[:4000]}

    def _load_plugins(self, directories):
        from resonant import plugins as plugin_api
        out = []
        for directory in directories:
            try:
                api = plugin_api.load_plugin(directory)
                self.plugins[api.plugin_id] = api
                out.append({"directory": directory, "id": api.plugin_id, "ok": True, "extensions": plugin_api.describe(api)})
            except Exception as error:
                out.append({"directory": directory, "ok": False, "error": f"{type(error).__name__}: {error}"})
        return out

    def _call(self, request: dict) -> bool:
        api = self.plugins.get(request.get("plugin"))
        if api is None or request.get("extension") not in api.extensions:
            raise ValueError(f"no extension {request.get('plugin')}/{request.get('extension')}")
        extension = api.extensions[request["extension"]]
        project = self.namespace.get("project")
        if project is None:
            raise ValueError("no project: the application has not sent one")
        params = request.get("params", {})
        stdout = sys.stdout
        sys.stdout = _Stream(self, "stdout")
        try:
            if extension["kind"] == "geometry-generator":
                result = extension["function"](project, **params)
                self.send({"type": "apply", "description": f"{extension['title']} ({api.plugin_id})", "project": json.loads(project.to_json())})
            elif extension["kind"] == "analysis":
                result = extension["function"](project)
            else:
                result = extension["function"](params.get("path"), project)
                if params.get("apply"):
                    self.send({"type": "apply", "description": f"{extension['title']} ({api.plugin_id})", "project": json.loads(project.to_json())})
        finally:
            sys.stdout = stdout
        self.send({"type": "result", "value": result if isinstance(result, (dict, list, str, int, float, bool)) or result is None else repr(result)})
        return True


class _Stream(io.TextIOBase):
    resonant_console = True

    def __init__(self, worker, name):
        self._worker, self._name = worker, name

    def write(self, text):
        if text:
            self._worker.send({"type": self._name, "text": text})
        return len(text)


def main() -> int:
    out = sys.stdout
    worker = Worker(out)
    while True:
        try:
            line = sys.stdin.readline()
        except KeyboardInterrupt:
            continue
        if not line:
            return 0
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except json.JSONDecodeError as error:
            worker.send({"type": "error", "error": "ProtocolError", "message": str(error), "traceback": "", "line": None})
            continue
        worker.handle(request)


if __name__ == "__main__":
    sys.exit(main())
