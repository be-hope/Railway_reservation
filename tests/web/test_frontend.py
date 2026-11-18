"""HTTP-to-C++ regression checks. Uses an isolated SQLite database."""
import argparse
from datetime import date, timedelta
import http.cookiejar
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from urllib.error import HTTPError, URLError
from urllib.request import Request, build_opener, HTTPCookieProcessor

ROOT = Path(__file__).resolve().parents[2]
BRIDGE = None


class FrontendTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
        cls.url = f'http://localhost:{port}'
        cls.process = subprocess.Popen([sys.executable, str(ROOT / 'web/server.py'), '--bridge', BRIDGE,
                                       '--database', str(Path(cls.temp.name) / 'test.db'), '--seed-demo', '--port', str(port)],
                                      cwd=cls.temp.name, stdout=subprocess.DEVNULL)
        for _ in range(100):
            try:
                build_opener().open(cls.url, timeout=1).close(); break
            except URLError:
                time.sleep(.1)
        else:
            cls.process.terminate(); raise RuntimeError('Server did not start')

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate(); cls.process.wait(); cls.temp.cleanup()

    def client(self):
        return build_opener(HTTPCookieProcessor(http.cookiejar.CookieJar()))

    def api(self, client, action, **body):
        request = Request(self.url + '/api', data=json.dumps(dict(action=action, **body)).encode(),
                          headers={'Content-Type': 'application/json', 'Origin': self.url})
        with client.open(request, timeout=20) as response:
            return json.load(response)

    def test_complete_booking_and_permissions(self):
        a, b = self.client(), self.client()
        self.assertFalse(self.api(a, 'history')['ok'])
        self.assertFalse(self.api(a, 'login', login='9876500001', password='incorrect')['ok'])
        self.assertTrue(self.api(a, 'login', login='9876500001', password='Test@1234')['ok'])
        journey = dict(source='SBC', destination='MAS', date=(date.today()+timedelta(days=4)).isoformat(), classCode='SL')
        result = self.api(a, 'search', **journey); self.assertTrue(result['ok'], result)
        train = result['data'][0]['number']
        quote = self.api(a, 'quote', **journey, train=train, count=1)['data']
        booking = dict(**journey, train=train, requestId='frontend-test-retry', paise=quote['paise'],
                       travellers=[dict(name='Web Passenger', age=25, gender='O', id='TEST123456')])
        created = self.api(a, 'book', **booking); self.assertTrue(created['ok'], created)
        pnr = created['data']['pnr']
        self.assertNotIn('TEST123456', json.dumps(created))
        self.assertEqual(self.api(a, 'book', **booking)['data']['pnr'], pnr)
        self.assertTrue(self.api(b, 'login', login='9876500002', password='Test@1234')['ok'])
        self.assertFalse(self.api(b, 'ticket', pnr=pnr)['ok'])
        self.assertFalse(self.api(b, 'cancel', pnr=pnr)['ok'])
        self.assertEqual(self.api(a, 'history')['data'][0]['pnr'], pnr)
        cancelled = self.api(a, 'cancel', pnr=pnr)
        self.assertTrue(cancelled['ok'], cancelled)
        self.assertEqual(cancelled['data']['status'], 'CANCELLED')
        self.assertIsNotNone(cancelled['data']['refund'])
        self.assertTrue(self.api(a, 'logout')['ok'])
        self.assertFalse(self.api(a, 'history')['ok'])

    def test_origin_assets_and_registration(self):
        client = self.client()
        for path in ('/', '/style.css', '/app.js'):
            with client.open(self.url+path) as response:
                self.assertEqual(response.status, 200)
                self.assertIn("frame-ancestors 'none'", response.headers['Content-Security-Policy'])
        request = Request(self.url+'/api', data=b'{"action":"history"}',
                          headers={'Content-Type':'application/json', 'Origin':'https://other.example'})
        with self.assertRaises(HTTPError) as caught:
            client.open(request)
        self.assertEqual(caught.exception.code, 403)
        self.assertTrue(self.api(client, 'register', name='New Passenger', mobile='9876599999', email='', password='Test@1234')['ok'])
        self.assertTrue(self.api(client, 'login', login='9876599999', password='Test@1234')['ok'])
        self.assertEqual(self.api(client, 'history')['data'], [])
        self.assertFalse(self.api(client, 'login', login='staff01', password='Test@1234')['ok'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--bridge', required=True)
    args, remaining = parser.parse_known_args()
    BRIDGE = str(Path(args.bridge).resolve())
    unittest.main(argv=[sys.argv[0]]+remaining)
