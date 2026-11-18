"""Local-only HTTP adapter for the C++ application. Python standard library only."""
import argparse
import atexit
import http.cookies
import json
from pathlib import Path
import secrets
import subprocess
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent


def command(body, token):
    action = body['action']
    fields = [action, token]
    if action == 'login':
        fields += [body['login'], body['password']]
    elif action == 'register':
        fields += [body['name'], body.get('mobile', ''), body.get('email', ''), body['password']]
    elif action in ('ticket', 'cancel'):
        fields += [body['pnr']]
    elif action in ('search', 'quote', 'book'):
        fields += [body['source'], body['destination'], body['date'], body['classCode']]
        if action != 'search':
            travellers = body.get('travellers', [])
            fields += [body['train'], str(len(travellers) if action == 'book' else body['count'])]
            if action == 'book':
                fields += [body['requestId'], str(body['paise'])]
                for t in travellers:
                    fields += [t['name'], str(t['age']), t['gender'], t['id']]
    elif action not in ('history', 'logout'):
        raise ValueError('Unknown action')
    if any(not isinstance(s, str) or any(ord(c) < 32 for c in s) or len(s) > 200 for s in fields):
        raise ValueError('Invalid or oversized field')
    # std::quoted protocol: literal UTF-8, with quote and backslash escaping.
    return ' '.join('"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"' for s in fields) + '\n'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bridge', default='./build-final/rrs_web_bridge')
    parser.add_argument('--database', default='web-demo.db')
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--seed-demo', action='store_true')
    args = parser.parse_args()
    process = subprocess.Popen([args.bridge, args.database] + (['--seed-demo'] if args.seed_demo else []),
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, encoding='utf-8', bufsize=1)
    atexit.register(process.terminate)
    if process.stdout.readline().strip() != '{"ready":true}':
        raise SystemExit('The C++ backend could not start. Check its error output above.')

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass  # No credentials, PNRs or request bodies in access logs.

        def valid_host(self):
            return self.headers.get('Host') in (f'localhost:{args.port}', f'127.0.0.1:{args.port}')

        def reply(self, status, content, kind='application/json', cookie=None):
            self.send_response(status)
            self.send_header('Content-Type', kind + '; charset=utf-8')
            self.send_header('Content-Length', str(len(content)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'")
            if cookie:
                self.send_header('Set-Cookie', cookie)
            self.end_headers()
            self.wfile.write(content)

        def do_GET(self):
            if not self.valid_host():
                return self.reply(403, b'{}')
            assets = {'/': ('index.html', 'text/html'), '/app.js': ('app.js', 'text/javascript'), '/style.css': ('style.css', 'text/css')}
            item = assets.get(urlparse(self.path).path)
            if not item:
                return self.reply(404, b'{}')
            self.reply(200, (ROOT / item[0]).read_bytes(), item[1])

        def do_POST(self):
            # Same-origin JSON requests only; reject DNS rebinding and cross-site form posts.
            origin = self.headers.get('Origin')
            if (not self.valid_host() or self.path != '/api' or
                    origin not in (f'http://localhost:{args.port}', f'http://127.0.0.1:{args.port}') or
                    self.headers.get('Content-Type') != 'application/json'):
                return self.reply(403, b'{"ok":false,"error":"Request origin rejected"}')
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 16384:
                    raise ValueError('Request is too large')
                body = json.loads(self.rfile.read(length))
                jar = http.cookies.SimpleCookie(self.headers.get('Cookie', ''))
                token = jar['rrs_session'].value if 'rrs_session' in jar else secrets.token_hex(32)
                if len(token) != 64 or any(c not in '0123456789abcdef' for c in token):
                    token = secrets.token_hex(32)
                process.stdin.write(command(body, token)); process.stdin.flush()
                result = json.loads(process.stdout.readline())
                cookie = f'rrs_session={token}; HttpOnly; SameSite=Strict; Path=/'
                if body['action'] == 'logout':
                    cookie = 'rrs_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0'
                self.reply(200, json.dumps(result).encode(), cookie=cookie)
            except (ValueError, KeyError, TypeError, BrokenPipeError):
                self.reply(400, b'{"ok":false,"error":"Invalid request or backend unavailable"}')

    server = HTTPServer(('127.0.0.1', args.port), Handler)
    print(f'Railway frontend ready: http://localhost:{args.port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close(); process.terminate(); process.wait()


if __name__ == '__main__':
    main()
