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

    def test_shutdown_agent_capabilities_and_host_unavailability(self):
        code, hdr, raw = self.request("GET","/api/v1/agents/capabilities")
        self.assertEqual(code,200)
        data=json.loads(raw)
        self.assertEqual(data["resource"],"agents.capabilities")
        self.assertIs(data["coordinatorCompiled"],True)
        self.assertEqual(data["maximumClients"],12)
        self.assertEqual(data["minimumLeaseSeconds"],10)
        self.assertEqual(data["maximumLeaseSeconds"],300)
        self.assertEqual(data["minimumWarningSeconds"],30)
        self.assertEqual(data["maximumWarningSeconds"],1800)
        for key in ("registrationWritable","notificationTransportQualified",
                    "mutualTlsQualified","trustedShutdownIntentAvailable",
                    "physicalPowerControl"):
            self.assertIs(data[key],False)
        code,_,raw=self.request("GET","/api/v1/agents/status")
        self.assertEqual(code,503)
        self.assertEqual(json.loads(raw)["code"],"AGENT_COORDINATOR_UNAVAILABLE")
        code,_,raw=self.request("HEAD","/api/v1/agents/status")
        self.assertEqual(code,503)
        self.assertEqual(raw,b"")
        for endpoint in ("/api/v1/agents/status","/api/v1/agents/capabilities"):
            for method in ("POST","PUT","PATCH","DELETE"):
                code,_,raw=self.request(method,endpoint,"{}")
                self.assertEqual(code,405)
                self.assertEqual(json.loads(raw)["error"],"READ_ONLY")

    def test_snmp_monitoring_is_bounded_and_unavailable_for_network(self):
        status, headers, raw = self.request("GET", "/api/v1/snmp/capabilities")
        self.assertEqual(status, 200)
        body = json.loads(raw)
        self.assertEqual(body["resource"], "snmp.capabilities")
        self.assertEqual(body["supportedOids"], 7)
        self.assertEqual(len(body["oids"]), 7)
        self.assertEqual(body["maximumVarbinds"], 8)
        self.assertIs(body["readOnlyAgentCompiled"], True)
        self.assertIs(body["v2cPacketCodec"], True)
        self.assertIs(body["setOperationsAllowed"], False)
        self.assertIs(body["listenerEnabled"], False)
        self.assertIs(body["v3UsmAuthPrivQualified"], False)
        self.assertIs(body["sourceAclQualified"], False)
        self.assertIs(body["communityProvisioned"], False)
        self.assertEqual(headers["X-OpenH910P-Mode"], "simulator")
        self.assertEqual(len(set(o["oid"] for o in body["oids"])), 7)
        status, _, data = self.request("HEAD", "/api/v1/snmp/capabilities")
        self.assertEqual((status,data),(200,b""))
        for verb in ("POST","PUT","PATCH","DELETE"):
            code, _, packet = self.request(verb,"/api/v1/snmp/capabilities","{}")
            self.assertEqual(code,405)
            self.assertEqual(json.loads(packet)["error"],"READ_ONLY")

    def test_custom_github_update_policy_and_read_only_release_check(self):
        status, headers, raw = self.request("GET", "/api/v1/firmware/update-policy")
        self.assertEqual(status, 200)
        data = json.loads(raw)
        self.assertEqual(data["resource"], "firmware.update-policy")
        self.assertEqual(data["releaseProvider"], "github")
        self.assertEqual(data["repository"], "bradjtrammell/dell-nmc-firmware")
        self.assertEqual(data["hardwareTarget"], "H910P")
        self.assertEqual(data["runningVersion"], "0.6.0-alpha.1")
        self.assertIs(data["releaseCheckEnabled"], True)
        self.assertIs(data["nativeSha256Compiled"], True)
        self.assertIs(data["signedManifestGateCompiled"], True)
        for key in ("signatureKeysQualified","flashLayoutQualified",
                    "recoveryQualified","deviceHttpsQualified",
                    "installEnabled","rollbackEnabled","automaticUpdateEnabled"):
            self.assertIs(data[key], False)
        self.assertNotIn("dell.com", raw.decode().lower())
        self.assertIn("connect-src 'self' https://api.github.com",
                      headers["Content-Security-Policy"])
        code, _, raw = self.request("HEAD", "/api/v1/firmware/update-policy")
        self.assertEqual(code, 200)
        self.assertEqual(raw, b"")
        for method in ("POST", "PUT", "PATCH", "DELETE"):
            code, _, body = self.request(method, "/api/v1/firmware/update-policy","{}")
            self.assertEqual(code, 405)
            self.assertEqual(json.loads(body)["error"], "READ_ONLY")

    def test_notification_router_status_and_read_only_contract(self):
        status, headers, raw = self.request("GET", "/api/v1/notifications/policy")
        self.assertEqual(status, 200)
        data = json.loads(raw)
        self.assertEqual(data["resource"], "notifications.policy")
        self.assertIs(data["routerCompiled"], True)
        self.assertEqual(data["maximumRules"], 8)
        self.assertEqual(data["queueCapacity"], 24)
        self.assertEqual(data["maximumAttempts"], 5)
        self.assertEqual(data["maximumCooldownSeconds"], 3600)
        self.assertEqual(len(data["events"]), 8)
        self.assertEqual(data["transports"], ["email","webhook","snmp-trap"])
        for key in ("deliveryAvailable","rulesWritable",
                    "destinationsProvisioned","transportQualified",
                    "credentialsConfigured"):
            self.assertIs(data[key], False)
        self.assertEqual(headers["X-OpenH910P-Mode"], "simulator")
        status, _, raw = self.request("GET", "/api/v1/notifications/status")
        self.assertEqual(status, 503)
        self.assertEqual(json.loads(raw)["code"], "NOTIFICATION_ENGINE_UNAVAILABLE")
        for route in ("/api/v1/notifications/policy",
                      "/api/v1/notifications/status"):
            status, _, raw = self.request("HEAD", route)
            self.assertEqual(status, 200 if route.endswith("policy") else 503)
            self.assertEqual(raw, b"")
            for method in ("POST","PUT","PATCH","DELETE"):
                status, _, raw = self.request(method, route, "{}")
                self.assertEqual(status, 405)
                self.assertEqual(json.loads(raw)["error"], "READ_ONLY")

    def test_weekly_schedule_contract_is_draft_only(self):
        code, headers, payload = self.request("GET", "/api/v1/schedule/schema")
        self.assertEqual(code, 200)
        body = json.loads(payload)
        self.assertEqual(body["resource"], "schedule.schema")
        self.assertEqual(body["dayCount"], 7)
        self.assertEqual(body["slotSeconds"], 1800)
        self.assertEqual(body["slotsPerDay"], 48)
        self.assertEqual(body["disabledValue"], "-1")
        self.assertEqual(body["mode"], "local-draft-export-only")
        self.assertIs(body["weeklyPlannerCompiled"], True)
        for key in ("scheduleWritable", "activeScheduleAvailable",
                    "rtcQualified", "powerControlQualified", "persistent"):
            self.assertIs(body[key], False)
        self.assertNotIn("activeSchedule", body)
        self.assertEqual(headers["X-OpenH910P-Mode"], "simulator")
        code, _, payload = self.request("HEAD", "/api/v1/schedule/schema")
        self.assertEqual(code, 200)
        self.assertEqual(payload, b"")
        for method in ("POST", "PUT", "PATCH", "DELETE"):
            with self.subTest(method=method):
                code, _, payload = self.request(method, "/api/v1/schedule/schema", "{}")
                self.assertEqual(code, 405)
                self.assertEqual(json.loads(payload)["error"], "READ_ONLY")
        code, _, payload = self.request("GET", "/api/v1/schedule/execute")
        self.assertEqual(code, 404)
        self.assertEqual(json.loads(payload)["code"], "NOT_FOUND")

    def test_authentication_status_is_truthful_and_login_is_disabled(self):
        code, headers, raw = self.request("GET", "/api/v1/security/capabilities")
        self.assertEqual(code, 200)
        data = json.loads(raw)
        self.assertEqual(data["resource"], "security.capabilities")
        self.assertIs(data["sessionEngineCompiled"], True)
        self.assertEqual(data["maximumSessions"], 4)
        self.assertEqual(data["idleTimeoutSeconds"], 900)
        self.assertEqual(data["absoluteTimeoutSeconds"], 28800)
        self.assertEqual(data["maximumFailedAttempts"], 5)
        self.assertEqual(data["lockoutSeconds"], 60)
        self.assertIs(data["csrfRequiredForWrites"], True)
        self.assertIs(data["increasingRequestIdRequired"], True)
        self.assertEqual(data["status"], "provider-unqualified")
        for key in ("authenticationAvailable", "loginEnabled",
                    "tlsQualified", "entropyQualified",
                    "credentialStoreQualified", "mutationsAuthorized"):
            self.assertIs(data[key], False)
        self.assertNotIn("password", raw.decode().lower())
        self.assertNotIn("sessiontoken", raw.decode().lower())
        self.assertEqual(headers["X-OpenH910P-Mode"], "simulator")
        code, _, data = self.request("HEAD", "/api/v1/security/capabilities")
        self.assertEqual(code, 200)
        self.assertEqual(data, b"")
        for path in ("/api/v1/security/capabilities", "/api/v1/security/login",
                     "/api/v1/security/sessions"):
            for method in ("POST", "PUT", "PATCH", "DELETE"):
                with self.subTest(path=path, method=method):
                    code, _, data = self.request(method, path, "{}")
                    self.assertEqual(code, 405)
                    self.assertEqual(json.loads(data)["error"], "READ_ONLY")
        code, _, body = self.request("GET", "/api/v1/security/login")
        self.assertEqual(code, 404)
        self.assertEqual(json.loads(body)["code"], "NOT_FOUND")

    def test_configuration_staging_is_metadata_only_and_read_only(self):
        code, headers, raw = self.request("GET", "/api/v1/configuration/staging")
        self.assertEqual(code, 200)
        data = json.loads(raw)
        self.assertEqual(data["resource"], "configuration.staging")
        self.assertEqual(data["supportedFields"], 9)
        self.assertEqual(data["mode"], "local-draft-export-only")
        for capability in ("configurationWritable", "authorizationAvailable",
                           "storageQualified", "activeValuesAvailable"):
            self.assertIs(data[capability], False)
        fields = data["fields"]
        self.assertEqual(len(fields), 9)
        self.assertEqual(len({field["name"] for field in fields}), 9)
        self.assertNotIn("value", raw.decode())
        self.assertFalse(any("Password" in field["name"] or "Toggle" in field["name"]
                             for field in fields))
        self.assertEqual(headers["X-OpenH910P-Mode"], "simulator")
        for method in ("POST", "PUT", "PATCH", "DELETE"):
            with self.subTest(method=method):
                status, _, body = self.request(method, "/api/v1/configuration/staging", "{}")
                self.assertEqual(status, 405)
                self.assertEqual(json.loads(body)["error"], "READ_ONLY")
        code, _, body = self.request("HEAD", "/api/v1/configuration/staging")
        self.assertEqual(code, 200)
        self.assertEqual(body, b"")

    def test_host_simulator_does_not_invent_native_event_statistics(self):
        code, _, body = self.request("GET", "/api/v1/events/stats")
        self.assertEqual(code, 404)
        self.assertEqual(json.loads(body)["resource"], "error")
        code, _, body = self.request("POST", "/api/v1/events/stats", "{}")
        self.assertEqual(code, 405)
        self.assertEqual(json.loads(body)["error"], "READ_ONLY")

    def test_simulator_never_fabricates_native_measurement_history(self):
        for route in ("/api/v1/ups/history", "/api/v1/ups/history/0"):
            code, _, raw = self.request("GET", route)
            self.assertEqual(code, 503)
            parsed = json.loads(raw)
            self.assertEqual(parsed["code"], "HISTORY_UNAVAILABLE")
            self.assertEqual(parsed["source"], "simulator")
        code, _, raw = self.request("GET", "/api/v1/ups/history/8")
        self.assertEqual(code, 404)
        self.assertEqual(json.loads(raw)["code"], "PAGE_NOT_FOUND")
        code, _, raw = self.request("POST", "/api/v1/ups/history", "{}")
        self.assertEqual(code, 405)
        self.assertEqual(json.loads(raw)["error"], "READ_ONLY")

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

    def test_complete_management_interface_and_security_headers(self):
        expected = {
            "/": ("text/html", b'<nav id="navigation"', b'/app.js'),
            "/index.html": ("text/html", b'<main id="main"', b'OpenH910P'),
            "/app.css": ("text/css", b'grid-template-columns', b'prefers-reduced-motion'),
            "/app.js": ("text/javascript", b'/api/v1/', b'UPS Properties'),
        }
        for path, (mime, first, second) in expected.items():
            with self.subTest(path=path):
                code, headers, data = self.request("GET", path)
                self.assertEqual(code, 200)
                self.assertTrue(headers["Content-Type"].startswith(mime))
                self.assertEqual(len(data), int(headers["Content-Length"]))
                self.assertIn(first, data)
                self.assertIn(second, data)
                self.assertEqual(headers["X-OpenH910P-Mode"], "simulator")
                self.assertEqual(headers["Cache-Control"], "no-store")
                self.assertEqual(headers["X-Content-Type-Options"], "nosniff")
                self.assertEqual(headers["X-Frame-Options"], "DENY")
                self.assertIn("script-src 'self'", headers["Content-Security-Policy"])
                self.assertNotIn("Access-Control-Allow-Origin", headers)
                code, headers, data = self.request("HEAD", path)
                self.assertEqual(code, 200)
                self.assertEqual(data, b"")
                self.assertGreater(int(headers["Content-Length"]), 0)
        for path in ("/style.css", "/favicon.svg", "/app.js.map", "/web/app.js",
                     "/.git/config", "/..%2fetc%2fpasswd", "/docs/current-ui-baseline.md"):
            with self.subTest(path=path):
                code, _, data = self.request("GET", path)
                self.assertIn(code, (400, 404))
                self.assertIn("error", json.loads(data))
        for path in ("/", "/app.js", "/app.css", "/index.html"):
            for method in ("POST", "PUT", "DELETE"):
                with self.subTest(path=path, method=method):
                    code, _, data = self.request(method, path, b"{}")
                    self.assertEqual(code, 405)
                    self.assertEqual(json.loads(data)["error"], "READ_ONLY")

    def test_ui_navigation_scope_and_read_only_state(self):
        script = (ROOT / "web/app.js").read_text(encoding="utf-8")
        html = (ROOT / "web/index.html").read_text(encoding="utf-8")
        for label in ("UPS Properties", "UPS Control", "Weekly Schedule",
                      "Shutdown Parameters", "Measurements", "Event Log",
                      "System Log", "Email Notification", "Network", "System",
                      "Notified Applications", "Access Control", "SNMP", "Time",
                      "Firmware Upload"):
            self.assertIn(label, script)
        for path in ("health", "device", "capabilities", "ups/status",
                     "ups/measurements", "ups/outlets", "events",
                     "configuration/fields"):
            self.assertIn(path, script)
        self.assertIn("disabled=!!disabled", script)
        self.assertIn("textContent", script)
        self.assertIn("aria-live", html)
        self.assertIn("app.js", html)
        self.assertNotIn("<script>", html)
        self.assertNotIn("http://", script)
        self.assertEqual(script.count("https://"), 2)
        self.assertIn("https://api.github.com/repos/bradjtrammell/dell-nmc-firmware/releases?per_page=12", script)
        self.assertIn("https://github.com/bradjtrammell/dell-nmc-firmware/releases/tag/", script)

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
