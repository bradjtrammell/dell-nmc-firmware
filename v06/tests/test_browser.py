"""Optional screenshot/layout test of the new UI using live native-C fixture output.

Browser networking is not used: captured local JSON is supplied by a fetch shim,
so restricted browser environments can still test layout and client behavior.
"""
import base64
import json
from pathlib import Path
import subprocess
import unittest
from urllib.request import urlopen

try:
    from playwright.sync_api import sync_playwright
except ImportError:
    sync_playwright = None

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(sync_playwright is None, "Install Playwright and Chromium for browser tests")
class VisualTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import socket
        import time
        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
        probe.close()
        cls.process = subprocess.Popen(
            [str(ROOT / "build/openh910p-sim"), "--port", str(port)],
            cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        for attempt in range(50):
            try:
                urlopen(f"http://127.0.0.1:{port}/api/v1/health", timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.04)
        else:
            cls.process.terminate()
            raise RuntimeError("Local simulator startup failed")
        cls.fixtures = {}
        for path in (
            "health", "device", "capabilities", "ups/status", "ups/measurements",
            "ups/outlets", "events",
        ):
            url = f"http://127.0.0.1:{port}/api/v1/{path}"
            with urlopen(url, timeout=2) as response:
                cls.fixtures[f"/api/v1/{path}"] = json.loads(response.read())
        for path in ["configuration/fields"] + [f"configuration/fields/{p}" for p in range(19)]:
            with urlopen(f"http://127.0.0.1:{port}/api/v1/{path}", timeout=2) as response:
                cls.fixtures[f"/api/v1/{path}"] = json.loads(response.read())
        cls.playwright = sync_playwright().start()
        cls.browser = cls.playwright.chromium.launch(
            headless=True, executable_path="/usr/bin/chromium", args=["--no-sandbox"]
        )

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.playwright.stop()
        cls.process.terminate()
        cls.process.wait(timeout=3)

    def test_desktop_and_mobile_layout_and_client(self):
        html = (ROOT / "web/index.html").read_text()
        html = html.replace('<link rel="stylesheet" href="/style.css">', "")
        html = html.replace('<script src="/app.js" defer></script>', "")
        icon = base64.b64encode((ROOT / "web/favicon.svg").read_bytes()).decode("ascii")
        html = html.replace('src="/favicon.svg"', f'src="data:image/svg+xml;base64,{icon}"')
        stylesheet = (ROOT / "web/style.css").read_text()
        source = (ROOT / "web/app.js").read_text()
        for width in (1440, 768, 390, 360):
            with self.subTest(width=width):
                page = self.browser.new_page(viewport={"width": width, "height": 900})
                errors = []
                page.on("pageerror", lambda error: errors.append(str(error)))
                page.set_content(html)
                page.add_style_tag(content=stylesheet)
                page.evaluate(
                    "fixtures => { window.fetch = async (path) => { const item = fixtures[path]; return item ? new Response(JSON.stringify(item), { status:200, headers:{ 'Content-Type':'application/json' }}) : new Response('', {status:404}); }; }",
                    self.fixtures,
                )
                page.add_script_tag(content=source)
                page.wait_for_function("document.getElementById('battery-value').textContent === '97'", timeout=3000)
                result = page.evaluate("""() => ({
                    scrollWidth:document.documentElement.scrollWidth,
                    windowWidth:innerWidth,
                    connected:document.getElementById('api-state').textContent,
                    controls:[...document.querySelectorAll('#outlets-body button')].map(e=>e.disabled),
                    input:document.getElementById('input-voltage').textContent,
                    title:document.title
                })""")
                self.assertEqual(errors, [])
                self.assertLessEqual(result["scrollWidth"], result["windowWidth"])
                self.assertEqual(result["connected"], "Local simulator connected")
                self.assertEqual(result["controls"], [True, True, True])
                page.wait_for_function("document.getElementById('schema-count').textContent.includes('302 total')", timeout=3000)
                self.assertEqual(page.locator("#schema-body tr").count(), 40)
                page.locator("#schema-search").fill("FirmwareUpgrade")
                self.assertGreater(page.locator("#schema-body tr").count(), 0)
                self.assertIn("matching definitions", page.locator("#schema-count").inner_text())
                page.locator("#schema-search").fill("")
                self.assertEqual(result["input"], "119.0")
                self.assertIn("OpenH910P", result["title"])
                if width == 1440 or width == 390:
                    page.screenshot(path=str(ROOT / f"build/preview-{width}.png"), full_page=True)
                page.close()


if __name__ == "__main__":
    unittest.main()
