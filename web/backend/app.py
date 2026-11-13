#!/usr/bin/env python3
"""API HTTP del mini compilador y servidor del frontend estatico.

Solo usa la biblioteca estandar. Configuracion por variables de entorno:
  COMPILER_BIN   ruta del compilador ("mock" usa tests_api/mock_compiler.py)
  RUNTIME_C      ruta de runtime.c
  PORT, HOST     direccion de escucha (8080, 0.0.0.0)
  ALLOW_ORIGIN   valor de Access-Control-Allow-Origin (desactivado si vacio)
  MAX_CONCURRENT peticiones de compilacion simultaneas (4)
  ASM_LINK_FLAGS opciones extra para gcc al ensamblar
"""
import json
import mimetypes
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

HERE = os.path.dirname(os.path.abspath(__file__))
WEB_DIR = os.path.dirname(HERE)
REPO_DIR = os.path.dirname(WEB_DIR)
FRONTEND_DIR = os.environ.get("FRONTEND_DIR", os.path.join(WEB_DIR, "frontend"))
EXAMPLES_DIR = os.environ.get("EXAMPLES_DIR", os.path.join(WEB_DIR, "examples"))

API_VERSION = "1.0"
MAX_CODE_BYTES = 100 * 1024
MAX_STDIN_BYTES = 64 * 1024
MAX_BODY_BYTES = 1024 * 1024
MAX_OUTPUT_BYTES = 256 * 1024
DEFAULT_TIMEOUT = 5.0
MAX_TIMEOUT = 10.0
COMPILE_TIMEOUT = 15
ASSEMBLE_TIMEOUT = 30
MEM_LIMIT = 512 * 1024 * 1024
FSIZE_LIMIT = 10 * 1024 * 1024
NPROC_LIMIT = 64
NOFILE_LIMIT = 64
MAX_COMPILER_OUTPUT = 64 * 1024 * 1024

MODES = ("scan", "ast", "check", "asm", "run", "all")
PASSES = ("fold", "inline", "sethi")


def resolve_compiler():
    value = os.environ.get("COMPILER_BIN", "/app/compiler")
    if value == "mock":
        value = os.path.join(REPO_DIR, "tests_api", "mock_compiler.py")
    return value


def compiler_command():
    path = resolve_compiler()
    if path.endswith(".py"):
        return [sys.executable, path]
    return [path]


def runtime_path():
    value = os.environ.get("RUNTIME_C")
    if value:
        return value
    return os.path.join(REPO_DIR, "runtime", "runtime.c")


def max_concurrent():
    try:
        return max(1, int(os.environ.get("MAX_CONCURRENT", "4")))
    except ValueError:
        return 4


SLOTS = threading.BoundedSemaphore(max_concurrent())


def run_available():
    if platform.system() != "Linux" or platform.machine().lower() not in ("x86_64", "amd64"):
        return False, "la ejecucion requiere Linux x86-64"
    if shutil.which("gcc") is None:
        return False, "gcc no esta disponible"
    if not os.path.isfile(runtime_path()):
        return False, "runtime.c no esta disponible"
    return True, None


class Sanitizer:
    """Oculta rutas internas en cualquier texto devuelto al cliente."""

    def __init__(self, extra=()):
        paths = [REPO_DIR, WEB_DIR, HERE, "/app", os.path.dirname(resolve_compiler()),
                 os.path.dirname(runtime_path())]
        paths.extend(extra)
        seen = []
        for p in paths:
            p = (p or "").rstrip("/")
            if len(p) > 1 and p not in seen:
                seen.append(p)
        seen.sort(key=len, reverse=True)
        self.paths = seen
        self.extra = list(extra)

    def text(self, s):
        for p in self.paths:
            s = re.sub(re.escape(p) + r"/", "", s)
            s = re.sub(re.escape(p) + r"(?![\w.-])", "", s)
        return s

    def walk(self, obj):
        if isinstance(obj, str):
            return self.text(obj)
        if isinstance(obj, list):
            return [self.walk(x) for x in obj]
        if isinstance(obj, dict):
            return {k: self.walk(v) for k, v in obj.items()}
        return obj


def err(phase, message, line=None, column=None):
    return {"phase": phase, "message": message, "line": line, "column": column}


def run_limited(argv, stdin_bytes, timeout, cwd, env, limit_bytes):
    """Ejecuta un proceso con timeout, captura acotada y grupo de procesos propio."""
    start = time.monotonic()
    proc = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, cwd=cwd, env=env, start_new_session=True)
    bufs = {"out": bytearray(), "err": bytearray()}
    over = {"out": False, "err": False}

    def pump(stream, key):
        try:
            while True:
                chunk = stream.read(65536)
                if not chunk:
                    break
                room = limit_bytes - len(bufs[key])
                if room > 0:
                    bufs[key].extend(chunk[:room])
                if len(chunk) > room:
                    over[key] = True
        except (OSError, ValueError):
            pass

    def feed():
        try:
            if stdin_bytes:
                proc.stdin.write(stdin_bytes)
        except (OSError, ValueError):
            pass
        finally:
            try:
                proc.stdin.close()
            except (OSError, ValueError):
                pass

    threads = [threading.Thread(target=pump, args=(proc.stdout, "out"), daemon=True),
               threading.Thread(target=pump, args=(proc.stderr, "err"), daemon=True),
               threading.Thread(target=feed, daemon=True)]
    for t in threads:
        t.start()
    timed_out = False
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
    kill_group(proc)
    proc.wait()
    for t in threads[:2]:
        t.join(2)
    return {
        "returncode": proc.returncode,
        "timed_out": timed_out,
        "stdout": bytes(bufs["out"]),
        "stderr": bytes(bufs["err"]),
        "stdout_truncated": over["out"],
        "stderr_truncated": over["err"],
        "duration_ms": int((time.monotonic() - start) * 1000),
    }


def kill_group(proc):
    import signal
    try:
        os.killpg(proc.pid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError, OSError):
        try:
            proc.kill()
        except OSError:
            pass


def decode(b):
    return b.decode("utf-8", errors="replace")


def signal_name(num):
    import signal
    try:
        return signal.Signals(num).name
    except ValueError:
        return "SIG%d" % num


def clean_env(tmp):
    return {"PATH": "/usr/local/bin:/usr/bin:/bin", "LANG": "C", "LC_ALL": "C", "TMPDIR": tmp,
            "HOME": tmp}


def invoke_compiler(code, mode, optimize, passes):
    cmd = compiler_command() + ["--api", "--mode", mode, "-O1" if optimize else "-O0"]
    if optimize and passes:
        cmd += ["--passes", ",".join(passes)]
    try:
        res = run_limited(cmd, code.encode("utf-8"), COMPILE_TIMEOUT, "/",
                          {"PATH": "/usr/bin:/bin", "LANG": "C"}, MAX_COMPILER_OUTPUT)
    except OSError:
        return None, "no se pudo iniciar el compilador"
    if res["timed_out"]:
        return None, "el compilador excedio el tiempo limite"
    if res["returncode"] is None or res["returncode"] < 0:
        return None, "el compilador termino de forma anormal (%s)" % signal_name(-res["returncode"])
    try:
        data = json.loads(decode(res["stdout"]))
        if not isinstance(data, dict):
            raise ValueError("no es un objeto")
    except ValueError:
        return None, "el compilador no produjo una respuesta valida"
    return data, None


def assemble_and_run(asm, stdin, timeout, tmp, runtime):
    """Devuelve (info, errores, fase_alcanzada)."""
    info = {"stdout": None, "stderr": None, "exit_code": None, "duration_ms": None,
            "truncated": False, "stdout_truncated": False, "stderr_truncated": False,
            "signal": None}
    avail, why = run_available()
    if not avail:
        return info, [err("run", "No se puede ejecutar el programa en este servidor: %s." % why)], "assemble"
    src = os.path.join(tmp, "prog.s")
    exe = os.path.join(tmp, "prog")
    with open(src, "w", encoding="utf-8") as f:
        f.write(asm)
    flags = os.environ.get("ASM_LINK_FLAGS", "").split()
    try:
        res = run_limited(["gcc", "prog.s", runtime, "-o", "prog"] + flags, None, ASSEMBLE_TIMEOUT,
                          tmp, clean_env(tmp), 64 * 1024)
    except OSError:
        return info, [err("assemble", "no se pudo invocar gcc")], "assemble"
    if res["timed_out"] or res["returncode"] != 0 or not os.path.exists(exe):
        msg = decode(res["stderr"]).strip() or "fallo el ensamblado o el enlace"
        if res["timed_out"]:
            msg = "el ensamblado excedio el tiempo limite"
        return info, [err("assemble", msg)], "assemble"
    argv = [sys.executable, "-I", os.path.join(HERE, "sandbox_exec.py"), str(MEM_LIMIT),
            str(int(timeout) + 2), str(FSIZE_LIMIT), str(NPROC_LIMIT), str(NOFILE_LIMIT), exe]
    try:
        res = run_limited(argv, stdin.encode("utf-8"), timeout, tmp, clean_env(tmp), MAX_OUTPUT_BYTES)
    except OSError:
        return info, [err("run", "no se pudo iniciar el programa")], "run"
    info["stdout"] = decode(res["stdout"])
    info["stderr"] = decode(res["stderr"])
    info["duration_ms"] = res["duration_ms"]
    info["stdout_truncated"] = res["stdout_truncated"]
    info["stderr_truncated"] = res["stderr_truncated"]
    info["truncated"] = res["stdout_truncated"] or res["stderr_truncated"]
    errors = []
    if res["timed_out"]:
        errors.append(err("run", "tiempo limite excedido (%g s)" % timeout))
    elif res["returncode"] < 0:
        sig = signal_name(-res["returncode"])
        info["signal"] = sig
        info["exit_code"] = 128 - res["returncode"]
        errors.append(err("run", "el programa termino por la senal %s" % sig))
    else:
        info["exit_code"] = res["returncode"]
    return info, errors, "run"


def count_lines(asm):
    return len(asm.splitlines()) if isinstance(asm, str) else None


def compile_once(req, optimize, passes, tmp, runtime):
    """Un ciclo completo de compilacion (y ejecucion si el modo lo pide)."""
    mode = req["mode"]
    cmode = {"run": "asm"}.get(mode, mode)
    data, problem = invoke_compiler(req["code"], cmode, optimize, passes)
    if data is None:
        return {"success": False, "phase": "internal", "errors": [err("internal", problem)],
                "warnings": [], "tokens": None, "ast": None, "typed_ast": None, "layout": None,
                "assembly": None, "optimizations": None, "stdout": None, "stderr": None,
                "exit_code": None, "duration_ms": None, "truncated": False}, True
    data.setdefault("success", False)
    data.setdefault("phase", "done")
    for key in ("errors", "warnings"):
        if not isinstance(data.get(key), list):
            data[key] = []
    for key in ("tokens", "ast", "typed_ast", "layout", "assembly", "optimizations"):
        data.setdefault(key, None)
    run = {"stdout": None, "stderr": None, "exit_code": None, "duration_ms": None,
           "truncated": False, "stdout_truncated": False, "stderr_truncated": False, "signal": None}
    if mode in ("run", "all") and data["success"] and isinstance(data.get("assembly"), str):
        run, errors, phase = assemble_and_run(data["assembly"], req["stdin"], req["timeout"],
                                              tmp, runtime)
        data["phase"] = phase
        if errors:
            data["errors"] = data["errors"] + errors
            data["success"] = False
    data.update(run)
    return data, False


def strip_for_compare(resp):
    out = {k: v for k, v in resp.items() if k not in ("tokens", "ast", "typed_ast", "compare")}
    opt = out.get("optimizations")
    if isinstance(opt, dict):
        out["optimizations"] = {k: v for k, v in opt.items() if k not in ("ast_before", "ast_after")}
    return out


def handle_compile(req):
    ok, why = run_available()
    tmp = tempfile.mkdtemp(prefix="mc_")
    try:
        san = Sanitizer([tmp])
        runtime = runtime_path()
        optimize = req["optimize"]
        passes = req["passes"]
        resp, internal = compile_once(req, optimize, passes, tmp, runtime)
        if req["compare"] and not internal:
            all_passes = list(PASSES) if not req["passes_given"] else passes
            tmp0 = tempfile.mkdtemp(prefix="mc_")
            tmp1 = tempfile.mkdtemp(prefix="mc_")
            try:
                san = Sanitizer([tmp, tmp0, tmp1])
                r0, _ = compile_once(req, False, [], tmp0, runtime)
                r1, _ = compile_once(req, True, all_passes or list(PASSES), tmp1, runtime)
            finally:
                shutil.rmtree(tmp0, ignore_errors=True)
                shutil.rmtree(tmp1, ignore_errors=True)
            ran = r0.get("duration_ms") is not None and r1.get("duration_ms") is not None
            resp["compare"] = {
                "o0": strip_for_compare(r0),
                "o1": strip_for_compare(r1),
                "same_output": (r0.get("stdout") == r1.get("stdout")) if ran else None,
                "same_exit_code": (r0.get("exit_code") == r1.get("exit_code")) if ran else None,
                "asm_lines": {"o0": count_lines(r0.get("assembly")),
                              "o1": count_lines(r1.get("assembly"))},
            }
        resp["options"] = {"optimize": optimize, "passes": passes if optimize else []}
        return (502 if internal else 200), san.walk(resp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def bad_request(*messages):
    return 400, {"success": False, "errors": [err("request", m) for m in messages]}


def validate(body):
    try:
        req = json.loads(body.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return None, bad_request("El cuerpo no es JSON valido.")
    if not isinstance(req, dict):
        return None, bad_request("El cuerpo debe ser un objeto JSON.")
    problems = []
    code = req.get("code")
    if not isinstance(code, str):
        problems.append("El campo 'code' es obligatorio y debe ser texto.")
    elif len(code.encode("utf-8")) > MAX_CODE_BYTES:
        problems.append("El codigo supera el maximo de %d KB." % (MAX_CODE_BYTES // 1024))
    mode = req.get("mode", "all")
    if mode not in MODES:
        problems.append("Modo invalido; use uno de: %s." % ", ".join(MODES))
    optimize = req.get("optimize", False)
    if not isinstance(optimize, bool):
        problems.append("El campo 'optimize' debe ser booleano.")
    passes = req.get("passes", None)
    if passes is not None:
        if not isinstance(passes, list) or any(p not in PASSES for p in passes if isinstance(p, str)) \
                or any(not isinstance(p, str) for p in passes):
            problems.append("El campo 'passes' debe ser una lista con valores de: %s." % ", ".join(PASSES))
    stdin = req.get("stdin", "")
    if stdin is None:
        stdin = ""
    if not isinstance(stdin, str):
        problems.append("El campo 'stdin' debe ser texto.")
    elif len(stdin.encode("utf-8")) > MAX_STDIN_BYTES:
        problems.append("La entrada supera el maximo de %d KB." % (MAX_STDIN_BYTES // 1024))
    timeout = req.get("timeout", DEFAULT_TIMEOUT)
    if isinstance(timeout, bool) or not isinstance(timeout, (int, float)) or timeout <= 0 \
            or timeout != timeout:
        problems.append("El campo 'timeout' debe ser un numero positivo de segundos.")
    compare = req.get("compare", False)
    if not isinstance(compare, bool):
        problems.append("El campo 'compare' debe ser booleano.")
    if problems:
        return None, bad_request(*problems)
    passes_given = passes is not None
    if passes is None:
        passes = list(PASSES) if optimize else []
    passes = [p for p in PASSES if p in passes]
    if optimize and not passes:
        optimize = False
    if not optimize:
        passes = []
    return {"code": code, "mode": mode, "optimize": optimize, "passes": passes,
            "passes_given": passes_given, "stdin": stdin,
            "timeout": min(float(timeout), MAX_TIMEOUT), "compare": compare}, None


_version_cache = {}


def compiler_version():
    if "v" in _version_cache:
        return _version_cache["v"]
    value = None
    try:
        res = run_limited(compiler_command() + ["--version"], None, 3, "/",
                          {"PATH": "/usr/bin:/bin"}, 4096)
        text = decode(res["stdout"]).strip()
        if res["returncode"] == 0 and text and "\n" not in text and len(text) < 100 \
                and not text.startswith("{"):
            value = text
    except OSError:
        pass
    _version_cache["v"] = value
    return value


def gcc_version():
    if "g" in _version_cache:
        return _version_cache["g"]
    value = None
    if shutil.which("gcc"):
        try:
            out = subprocess.run(["gcc", "-dumpfullversion"], capture_output=True, timeout=5)
            value = decode(out.stdout).strip() or None
        except (OSError, subprocess.SubprocessError):
            pass
    _version_cache["g"] = value
    return value


def status_payload():
    ok, why = run_available()
    return {
        "api_version": API_VERSION,
        "compiler_version": compiler_version(),
        "compiler_available": os.path.exists(resolve_compiler()),
        "gcc_version": gcc_version(),
        "platform": "%s/%s" % (platform.system().lower(), platform.machine().lower()),
        "run_available": ok,
        "run_unavailable_reason": why,
        "limits": {
            "max_code_bytes": MAX_CODE_BYTES,
            "max_stdin_bytes": MAX_STDIN_BYTES,
            "max_output_bytes": MAX_OUTPUT_BYTES,
            "default_timeout": DEFAULT_TIMEOUT,
            "max_timeout": MAX_TIMEOUT,
            "memory_bytes": MEM_LIMIT,
            "max_concurrent": max_concurrent(),
        },
    }


def load_index():
    try:
        with open(os.path.join(EXAMPLES_DIR, "index.json"), encoding="utf-8") as f:
            data = json.load(f)
        return [e for e in data if isinstance(e, dict) and re.fullmatch(r"[a-z0-9_]+", str(e.get("id", "")))]
    except (OSError, ValueError):
        return []


def read_text(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except OSError:
        return None


def example_detail(eid):
    for e in load_index():
        if e["id"] == eid:
            code = read_text(os.path.join(EXAMPLES_DIR, eid + ".cpp"))
            if code is None:
                return None
            out = {"id": eid, "titulo": e.get("titulo", eid), "descripcion": e.get("descripcion", ""),
                   "code": code, "stdin": e.get("stdin", "")}
            expected = read_text(os.path.join(EXAMPLES_DIR, eid + ".expected"))
            if expected is not None:
                out["esperado"] = expected
            return out
    return None


SECURITY_HEADERS = {
    "X-Content-Type-Options": "nosniff",
    "X-Frame-Options": "DENY",
    "Referrer-Policy": "no-referrer",
    "Content-Security-Policy": ("default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
                                "img-src 'self' data:; object-src 'none'; base-uri 'none'; "
                                "frame-ancestors 'none'; form-action 'none'"),
}


class Handler(BaseHTTPRequestHandler):
    server_version = "MiniCompilerWeb"
    sys_version = ""
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        sys.stderr.write("%s %s\n" % (self.log_date_time_string(), fmt % args))

    def send_bytes(self, status, body, ctype, extra=None, head=False):
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store" if ctype.startswith("application/json") else "no-cache")
        for k, v in SECURITY_HEADERS.items():
            self.send_header(k, v)
        origin = os.environ.get("ALLOW_ORIGIN", "")
        if origin:
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("Vary", "Origin")
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        if not head:
            self.wfile.write(body)

    def send_json(self, status, obj, extra=None, head=False):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_bytes(status, body, "application/json; charset=utf-8", extra, head)

    def do_OPTIONS(self):
        origin = os.environ.get("ALLOW_ORIGIN", "")
        self.send_response(204 if origin else 405)
        if origin:
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.send_header("Access-Control-Max-Age", "600")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_HEAD(self):
        self.route_get(head=True)

    def do_GET(self):
        self.route_get(head=False)

    def route_get(self, head):
        path = unquote(urlparse(self.path).path)
        if path == "/api/health":
            return self.send_json(200, {"status": "ok"}, head=head)
        if path == "/api/status":
            return self.send_json(200, status_payload(), head=head)
        if path == "/api/examples":
            items = [{"id": e["id"], "titulo": e.get("titulo", e["id"]),
                      "descripcion": e.get("descripcion", "")} for e in load_index()]
            return self.send_json(200, items, head=head)
        if path.startswith("/api/examples/"):
            detail = example_detail(path[len("/api/examples/"):])
            if detail is None:
                return self.send_json(404, {"success": False,
                                            "errors": [err("request", "Ejemplo no encontrado.")]}, head=head)
            return self.send_json(200, detail, head=head)
        if path.startswith("/api/"):
            return self.send_json(404, {"success": False, "errors": [err("request", "Ruta no encontrada.")]},
                                  head=head)
        self.serve_static(path, head)

    def serve_static(self, path, head):
        if path == "/":
            path = "/index.html"
        rel = os.path.normpath(path.lstrip("/"))
        full = os.path.realpath(os.path.join(FRONTEND_DIR, rel))
        root = os.path.realpath(FRONTEND_DIR)
        if "\x00" in path or not (full == root or full.startswith(root + os.sep)) or not os.path.isfile(full):
            return self.send_bytes(404, "No encontrado".encode("utf-8"), "text/plain; charset=utf-8", head=head)
        with open(full, "rb") as f:
            body = f.read()
        ctype = mimetypes.guess_type(full)[0] or "application/octet-stream"
        if ctype.startswith("text/") or ctype in ("application/javascript", "application/json"):
            ctype += "; charset=utf-8"
        self.send_bytes(200, body, ctype, head=head)

    def do_POST(self):
        path = unquote(urlparse(self.path).path)
        if path != "/api/compile":
            return self.send_json(404, {"success": False, "errors": [err("request", "Ruta no encontrada.")]})
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            return self.send_json(411, {"success": False,
                                        "errors": [err("request", "Falta el encabezado Content-Length.")]})
        if length < 0 or length > MAX_BODY_BYTES:
            self.close_connection = True
            remaining = length if 0 < length <= 16 * MAX_BODY_BYTES else 0
            while remaining > 0:
                chunk = self.rfile.read(min(65536, remaining))
                if not chunk:
                    break
                remaining -= len(chunk)
            return self.send_json(413, {"success": False,
                                        "errors": [err("request", "La peticion es demasiado grande.")]})
        body = self.rfile.read(length)
        req, bad = validate(body)
        if bad:
            return self.send_json(*bad)
        if not SLOTS.acquire(blocking=False):
            return self.send_json(429, {"success": False,
                                        "errors": [err("request", "Servidor ocupado; intente de nuevo en unos segundos.")]},
                                  {"Retry-After": "2"})
        try:
            status, resp = handle_compile(req)
        except Exception:  # no filtrar detalles internos
            sys.stderr.write("error interno en /api/compile\n")
            import traceback
            traceback.print_exc()
            status, resp = 500, {"success": False, "errors": [err("internal", "Error interno del servidor.")]}
        finally:
            SLOTS.release()
        self.send_json(status, resp)


def main():
    host = os.environ.get("HOST", "0.0.0.0")
    port = int(os.environ.get("PORT", "8080"))
    srv = ThreadingHTTPServer((host, port), Handler)
    srv.daemon_threads = True
    sys.stderr.write("escuchando en %s:%d\n" % (host, port))
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
