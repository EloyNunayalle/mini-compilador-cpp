"""Pruebas del backend contra el compilador falso (o contra API_BASE si se define)."""
import json
import os
import sys
import threading
import unittest
import urllib.error
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "web", "backend"))
os.environ.setdefault("COMPILER_BIN", "mock")
import app  # noqa: E402

BASE = os.environ.get("API_BASE")
SERVER = None
CAN_RUN = app.run_available()[0]
SKIP_RUN = unittest.skipUnless(CAN_RUN, "requiere gcc en Linux x86-64")

HELLO = "#include <iostream>\nint main() {\n    int x = 2;\n    return 0;\n}\n"


def setUpModule():
    global BASE, SERVER
    if BASE:
        return
    SERVER = app.ThreadingHTTPServer(("127.0.0.1", 0), app.Handler)
    SERVER.daemon_threads = True
    threading.Thread(target=SERVER.serve_forever, daemon=True).start()
    BASE = "http://127.0.0.1:%d" % SERVER.server_address[1]


def tearDownModule():
    if SERVER:
        SERVER.shutdown()


def call(path, body=None, raw=None, method=None):
    data = raw if raw is not None else (json.dumps(body).encode() if body is not None else None)
    req = urllib.request.Request(BASE + path, data=data, method=method or ("POST" if data is not None else "GET"))
    if data is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.status, r.read(), dict(r.headers)
    except urllib.error.HTTPError as e:
        return e.code, e.read(), dict(e.headers)


def compile_(**kw):
    kw.setdefault("code", HELLO)
    status, body, _ = call("/api/compile", kw)
    return status, json.loads(body)


class Endpoints(unittest.TestCase):
    def test_health(self):
        s, b, _ = call("/api/health")
        self.assertEqual((s, json.loads(b)), (200, {"status": "ok"}))

    def test_status(self):
        s, b, _ = call("/api/status")
        d = json.loads(b)
        self.assertEqual(s, 200)
        self.assertIn("run_available", d)
        self.assertEqual(d["limits"]["max_code_bytes"], 102400)
        self.assertIn("gcc_version", d)

    def test_examples(self):
        s, b, _ = call("/api/examples")
        items = json.loads(b)
        self.assertGreaterEqual(len(items), 10)
        for it in items:
            self.assertTrue({"id", "titulo", "descripcion"} <= set(it))
        s, b, _ = call("/api/examples/lectura_cin")
        d = json.loads(b)
        self.assertIn("cin", d["code"])
        self.assertTrue(d["stdin"])
        self.assertEqual(call("/api/examples/..%2f..%2fetc%2fpasswd")[0], 404)
        self.assertEqual(call("/api/examples/no_existe")[0], 404)

    def test_static_and_traversal(self):
        s, b, h = call("/")
        self.assertEqual(s, 200)
        self.assertIn(b"<title>", b)
        self.assertIn("script-src 'self'", h.get("Content-Security-Policy", ""))
        self.assertEqual(call("/..%2f..%2fMakefile")[0], 404)
        self.assertEqual(call("/%2e%2e/backend/app.py")[0], 404)

    def test_cors_off_by_default(self):
        _, _, h = call("/api/health")
        self.assertNotIn("Access-Control-Allow-Origin", h)


class Phases(unittest.TestCase):
    def test_valid(self):
        s, d = compile_(mode="check")
        self.assertEqual(s, 200)
        self.assertTrue(d["success"])
        self.assertEqual(d["errors"], [])

    def test_lex_error(self):
        s, d = compile_(code="int main() {\n  ERROR_LEX\n}", mode="check")
        self.assertFalse(d["success"])
        self.assertEqual(d["phase"], "lex")
        e = d["errors"][0]
        self.assertEqual((e["phase"], e["line"], e["column"]), ("lex", 2, 3))

    def test_parse_error(self):
        s, d = compile_(code="int main() {\n ERROR_PARSE\n}", mode="ast")
        self.assertEqual(d["phase"], "parse")
        self.assertEqual(d["errors"][0]["line"], 2)

    def test_type_error(self):
        s, d = compile_(code="int main() {\n  int a;\n  ERROR_TYPE\n}", mode="check")
        self.assertEqual(d["phase"], "typecheck")
        self.assertEqual(d["errors"][0]["line"], 3)
        self.assertIsNotNone(d["errors"][0]["column"])

    def test_tokens_ast_typed_layout(self):
        code = "struct P {\n int x;\n double y;\n};\nint main() {\n int a = 3;\n return 0;\n}\n"
        _, d = compile_(code=code, mode="all")
        self.assertEqual(d["tokens"][0]["type"], "KW_STRUCT")
        self.assertEqual(d["ast"]["kind"], "Program")
        self.assertIn("offset", d["typed_ast"]["children"][0]["children"][0]["children"][0]["attrs"])
        rec = d["layout"]["records"][0]
        self.assertEqual((rec["name"], rec["size"]), ("P", 16))
        self.assertEqual(rec["fields"][1]["offset"], 8)
        self.assertIn("class", rec["abi"])
        self.assertTrue(d["layout"]["functions"])

    def test_scan_only_has_tokens(self):
        _, d = compile_(mode="scan")
        self.assertTrue(d["tokens"])
        self.assertIsNone(d["ast"])

    def test_assembly(self):
        _, d = compile_(mode="asm")
        self.assertIn("main:", d["assembly"])
        self.assertIsNone(d["exit_code"])
        self.assertIsNone(d["duration_ms"])

    def test_warning(self):
        _, d = compile_(code="int main() { /* WARN_DEMO */ return 0; }", mode="check")
        self.assertTrue(d["success"])
        self.assertEqual(len(d["warnings"]), 1)


class Optimization(unittest.TestCase):
    def test_default_o0(self):
        _, d = compile_(mode="asm")
        self.assertFalse(d["optimizations"]["enabled"])
        self.assertEqual(d["options"], {"optimize": False, "passes": []})

    def test_o1_all_passes(self):
        _, d = compile_(mode="asm", optimize=True)
        self.assertTrue(d["optimizations"]["enabled"])
        self.assertEqual(d["optimizations"]["passes"], ["fold", "inline", "sethi"])

    def test_single_pass(self):
        _, d = compile_(mode="asm", optimize=True, passes=["fold"], code="OPT_DEMO")
        self.assertEqual(d["optimizations"]["passes"], ["fold"])
        self.assertEqual(list(d["optimizations"]["stats"]), ["fold"])
        self.assertGreater(d["optimizations"]["stats"]["fold"]["count"], 0)

    def test_stats(self):
        _, d = compile_(mode="asm", optimize=True, code="OPT_DEMO")
        st = d["optimizations"]["stats"]
        self.assertTrue(all(st[k]["count"] > 0 for k in ("fold", "inline", "sethi")))
        self.assertTrue(d["optimizations"]["log"])

    def test_empty_passes_means_o0(self):
        _, d = compile_(mode="asm", optimize=True, passes=[])
        self.assertFalse(d["optimizations"]["enabled"])

    def test_compare(self):
        _, d = compile_(mode="asm", compare=True)
        c = d["compare"]
        self.assertLess(c["asm_lines"]["o1"], c["asm_lines"]["o0"])
        self.assertNotIn("tokens", c["o0"])
        self.assertNotIn("ast", c["o1"])
        self.assertFalse(c["o0"]["optimizations"]["enabled"])
        self.assertTrue(c["o1"]["optimizations"]["enabled"])
        self.assertIsNone(c["same_output"])

    @SKIP_RUN
    def test_compare_run(self):
        _, d = compile_(mode="all", compare=True)
        c = d["compare"]
        self.assertTrue(c["same_output"])
        self.assertTrue(c["same_exit_code"])


class Validation(unittest.TestCase):
    def bad(self, **kw):
        s, b, _ = call("/api/compile", kw if "raw" not in kw else None, raw=kw.get("raw"))
        d = json.loads(b)
        self.assertEqual(s, 400, d)
        self.assertFalse(d["success"])
        self.assertTrue(d["errors"])

    def test_invalid_json(self):
        self.bad(raw=b"{no es json")

    def test_not_object(self):
        self.bad(raw=b"[1,2]")

    def test_missing_code(self):
        self.bad(mode="all")

    def test_wrong_types(self):
        self.bad(code=5)
        self.bad(code="x", optimize="si")
        self.bad(code="x", passes="fold")
        self.bad(code="x", passes=["magia"])
        self.bad(code="x", stdin=3)
        self.bad(code="x", timeout="5")
        self.bad(code="x", timeout=-1)
        self.bad(code="x", mode="otro")
        self.bad(code="x", compare=1)

    def test_code_too_big(self):
        self.bad(code="a" * (100 * 1024 + 1))

    def test_stdin_too_big(self):
        self.bad(code="x", stdin="a" * (64 * 1024 + 1))

    def test_body_too_big(self):
        s, _, _ = call("/api/compile", raw=b" " * (2 * 1024 * 1024))
        self.assertEqual(s, 413)

    def test_busy_returns_429(self):
        saved = app.SLOTS
        app.SLOTS = app.threading.BoundedSemaphore(1)
        app.SLOTS.acquire()
        try:
            s, b, h = call("/api/compile", {"code": "x", "mode": "scan"})
        finally:
            app.SLOTS = saved
        self.assertEqual(s, 429)
        self.assertIn("Retry-After", h)

    def test_unknown_route(self):
        self.assertEqual(call("/api/nada")[0], 404)
        self.assertEqual(call("/api/compile", raw=None, method="GET")[0], 404)


class Paths(unittest.TestCase):
    def test_sanitizer(self):
        san = app.Sanitizer(["/tmp/mc_abc"])
        out = san.text("error en /tmp/mc_abc/prog.s y %s/web/backend/app.py /app/compiler /apple" % app.REPO_DIR)
        self.assertNotIn("/tmp/mc_abc", out)
        self.assertNotIn(app.REPO_DIR, out)
        self.assertNotIn("/app/", out)
        self.assertIn("/apple", out)

    def test_no_paths_in_responses(self):
        _, d = compile_(mode="all", code="RUN_ASMERR")
        text = json.dumps(d)
        for p in (app.REPO_DIR, "/app/", "/tmp/mc_"):
            self.assertNotIn(p, text)


@SKIP_RUN
class Running(unittest.TestCase):
    def test_run_stdout(self):
        _, d = compile_(mode="run")
        self.assertTrue(d["success"], d["errors"])
        self.assertIn("hola desde el mock", d["stdout"])
        self.assertEqual(d["exit_code"], 0)
        self.assertEqual(d["phase"], "run")
        self.assertIsNotNone(d["duration_ms"])

    def test_all_mode_runs(self):
        _, d = compile_(mode="all")
        self.assertIn("hola", d["stdout"])
        self.assertTrue(d["tokens"])

    def test_stdin(self):
        _, d = compile_(mode="run", code="RUN_STDIN", stdin="entrada de prueba\n")
        self.assertEqual(d["stdout"], "entrada de prueba\n")

    def test_exit_code(self):
        _, d = compile_(mode="run", code="RUN_EXIT3")
        self.assertEqual(d["exit_code"], 3)
        self.assertTrue(d["success"])

    def test_timeout(self):
        _, d = compile_(mode="run", code="RUN_LOOP", timeout=1)
        self.assertFalse(d["success"])
        self.assertIsNone(d["exit_code"])
        self.assertEqual(d["errors"][-1]["phase"], "run")
        self.assertIn("tiempo limite excedido", d["errors"][-1]["message"])

    def test_signal(self):
        _, d = compile_(mode="run", code="RUN_SEGV")
        self.assertEqual(d["signal"], "SIGSEGV")
        self.assertFalse(d["success"])
        self.assertIn("SIGSEGV", d["errors"][-1]["message"])

    def test_big_output_truncated(self):
        _, d = compile_(mode="run", code="RUN_BIG", timeout=10)
        self.assertTrue(d["truncated"])
        self.assertLessEqual(len(d["stdout"]), app.MAX_OUTPUT_BYTES)

    def test_assemble_error(self):
        _, d = compile_(mode="run", code="RUN_ASMERR")
        self.assertEqual(d["phase"], "assemble")
        self.assertEqual(d["errors"][-1]["phase"], "assemble")

    def test_timeout_is_clamped(self):
        _, d = compile_(mode="run", timeout=999)
        self.assertTrue(d["success"])


class RunUnavailable(unittest.TestCase):
    @unittest.skipIf(CAN_RUN, "solo cuando no hay gcc x86-64")
    def test_clear_error(self):
        s, d = compile_(mode="run")
        self.assertEqual(s, 200)
        self.assertFalse(d["success"])
        self.assertEqual(d["errors"][-1]["phase"], "run")
        self.assertIn("No se puede ejecutar", d["errors"][-1]["message"])


if __name__ == "__main__":
    unittest.main()
