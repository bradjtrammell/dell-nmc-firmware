"""End-to-end local-only tests. Never connects to an NMC or UPS."""
import http.client
import json
from pathlib import Path
import socket
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SimulatorHttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        cls.port = probe.getsockname()[1]
        probe.close()
        cls.process = subprocess.Popen(
            [str(ROOT / "build/openh910p-sim"), "--port", str(cls.port)],
            cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
        )
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", cls.port), 0.05).close()
                break
            except OSError:
                time.sleep(0.04)
        else:
            cls.process.kill()
            raise RuntimeError("Simulator failed to bind loopback port")

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()
        try:
            cls.process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            cls.process.kill()
            cls.process.communicate(timeout=3)

    def request(self, method, path, body=None, headers=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=3)
        try:
            connection.request(method, path, body=body, headers=headers or {})
            response = connection.getresponse()
            data = response.read()
            return response.status, dict(response.getheaders()), data
        finally:
            connection.close()

    def test_native_json_endpoints(self):
        for path in (
            "/api/v1/health", "/api/v1/device", "/api/v1/capabilities",
            "/api/v1/ups/status", "/api/v1/ups/measurements",
            "/api/v1/ups/outlets", "/api/v1/events"
        ):
            with self.subTest(path=path):
                code, headers, data = self.request("GET", path)
                self.assertEqual(code, 200)
                self.assertTrue(headers["Content-Type"].startswith("application/json"))
                parsed = json.loads(data)
                self.assertEqual(parsed["schemaVersion"], 1)
                self.assertEqual(parsed["source"], "simulator")
                self.assertIs(parsed["simulated"], True)
                self.assertEqual(headers["X-OpenH910P-Mode"], "simulator")
                self.assertNotIn("Access-Control-Allow-Origin", headers)
                self.assertIn("default-src 'self'", headers["Content-Security-Policy"])

    def test_model_readings_have_units_and_power_actions_disabled(self):
        _, _, data = self.request("GET", "/api/v1/ups/measurements")
        self.assertEqual(json.loads(data)["inputVoltage"], {"valueDecivolts": 1190, "unit": "dV"})
        _, _, data = self.request("GET", "/api/v1/ups/outlets")
        for outlet in json.loads(data)["outlets"]:
            self.assertIs(outlet["controlEnabled"], False)

    def test_complete_native_json_configuration_catalog(self):
        code, _, data = self.request("GET", "/api/v1/configuration/fields")
        self.assertEqual(code, 200)
        info = json.loads(data)
        self.assertEqual((info["total"], info["pageSize"], info["pages"]), (302, 16, 19))
        self.assertIs(info["configurationWritable"], False)
        catalog = []
        for index in range(19):
            code, _, data = self.request("GET", f"/api/v1/configuration/fields/{index}")
            self.assertEqual(code, 200)
            page = json.loads(data)
            self.assertEqual(page["page"], index)
            self.assertIs(page["configurationWritable"], False)
            catalog += page["items"]
        self.assertEqual(len(catalog), 302)
        self.assertEqual(len({item["name"] for item in catalog}), 302)
        self.assertEqual({a: sum(item["access"] == a for item in catalog) for a in ("RW", "RO", "WO")},
                         {"RW": 290, "RO": 8, "WO": 4})
        self.assertNotIn("value", catalog[0])
        self.assertNotIn("default", catalog[0])
        self.assertTrue(all("type" in item and "group" in item for item in catalog))
        for path in ("/api/v1/configuration/fields/19", "/api/v1/configuration/fields/01"):
            code, _, _ = self.request("GET", path)
            self.assertEqual(code, 404)

    def test_no_mutations_can_be_sent(self):
        for method in ("POST", "PUT", "PATCH", "DELETE"):
            with self.subTest(method=method):
                code, _, body = self.request(method, "/api/v1/ups/outlets", "{}")
                self.assertEqual(code, 405)
                self.assertEqual(json.loads(body)["error"], "READ_ONLY")

    def test_no_legacy_xml_routes(self):
        for path in ("/get_object.xml", "/configuration_descriptor.xml", "/Forms/set_obj_1"):
            code, _, _ = self.request("GET", path)
            self.assertEqual(code, 404)

    def test_static_website_and_security(self):
        for path, expected in (
            ("/", b"OpenH910P"),
            ("/style.css", b"--ink:"),
            ("/app.js", b"AbortController"),
            ("/favicon.svg", b"<svg"),
        ):
            with self.subTest(path=path):
                code, headers, body = self.request("GET", path)
                self.assertEqual(code, 200)
                self.assertIn(expected, body)
                self.assertIn("frame-ancestors 'none'", headers["Content-Security-Policy"])
                self.assertNotIn(b"dell-white", body.lower())

    def test_head_and_bad_routes(self):
        code, headers, body = self.request("HEAD", "/api/v1/device")
        self.assertEqual(code, 200)
        self.assertEqual(body, b"")
        self.assertGreater(int(headers["Content-Length"]), 0)
        for path in ("/does-not-exist", "/api/v1/nonexistent", "/.git/config"):
            code, _, _ = self.request("GET", path)
            self.assertEqual(code, 404)

    def test_invalid_port_rejected(self):
        result = subprocess.run(
            [str(ROOT / "build/openh910p-sim"), "--port", "65536"],
            cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=3,
        )
        self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
