#!/usr/bin/env python3
import json
import os
import subprocess
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOST = "127.0.0.1"
PORT = int(os.environ.get("CMMC_UI_PORT", "8008"))


ARTIFACTS = {
    "tokens": ".tokens",
    "symtab": ".symtab",
    "parse": ".parse",
    "ast": ".ast",
    "ast_dot": ".ast.dot",
    "semantic": ".semantic.log",
    "ir": ".ll",
    "dfa": ".dfa.csv",
    "first": ".first",
    "follow": ".follow",
    "lr0": ".lr0",
    "slr": ".slr.csv",
}


SAMPLE = """const int base = 7;

int add(int a, int b) {
  return a + b;
}

int main() {
  int x = add(base, 5);
  if (x > 10) {
    return x;
  } else {
    return 0;
  }
}
"""


def read_text(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


class Handler(BaseHTTPRequestHandler):
    server_version = "CMMCUI/1.0"

    def log_message(self, fmt, *args):
        print("[ui]", fmt % args)

    def send_bytes(self, status: int, body: bytes, content_type: str):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_json(self, status: int, payload):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_bytes(status, body, "application/json; charset=utf-8")

    def do_GET(self):
        if self.path == "/" or self.path == "/index.html":
            self.send_bytes(200, read_text(ROOT / "ui" / "index.html").encode("utf-8"), "text/html; charset=utf-8")
            return
        if self.path == "/api/sample":
            self.send_json(200, {"source": SAMPLE})
            return
        self.send_json(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/api/compile":
            self.send_json(404, {"error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            source = payload.get("source", "")
            if not isinstance(source, str):
                raise ValueError("source must be a string")
        except Exception as exc:
            self.send_json(400, {"ok": False, "error": str(exc)})
            return

        if not (ROOT / "cmmc").exists():
            self.send_json(500, {"ok": False, "error": "cmmc not found; run make first"})
            return

        with tempfile.TemporaryDirectory(prefix="cmmc_ui_") as tmp:
            tmp_dir = Path(tmp)
            src = tmp_dir / "input.sy"
            out_dir = tmp_dir / "out"
            src.write_text(source, encoding="utf-8")
            cmd = [str(ROOT / "cmmc"), "--dump-all", str(src), "-o", str(out_dir)]
            try:
                proc = subprocess.run(
                    cmd,
                    cwd=str(ROOT),
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=8,
                )
                artifacts = {
                    name: read_text(out_dir / f"input{suffix}")
                    for name, suffix in ARTIFACTS.items()
                }
                self.send_json(
                    200,
                    {
                        "ok": proc.returncode == 0,
                        "returncode": proc.returncode,
                        "stdout": proc.stdout,
                        "stderr": proc.stderr,
                        "artifacts": artifacts,
                    },
                )
            except subprocess.TimeoutExpired:
                self.send_json(500, {"ok": False, "error": "compile timeout"})


def main():
    os.chdir(ROOT)
    httpd = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"C-- compiler UI: http://{HOST}:{PORT}")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
