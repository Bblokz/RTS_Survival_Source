"""Run export_scene.html in Chrome via Playwright and unpack the exported files."""
import base64
import json
import os
import sys

from playwright.sync_api import sync_playwright

page_path, out_dir = sys.argv[1:]
os.makedirs(out_dir, exist_ok=True)
with sync_playwright() as p:
    b = p.chromium.launch(executable_path=r"C:\Program Files\Google\Chrome\Application\chrome.exe", headless=True,
                          args=["--allow-file-access-from-files", "--enable-unsafe-swiftshader", "--use-angle=swiftshader"])
    pg = b.new_page()
    pg.on("console", lambda m: print("console:", m.text[:300]) if m.type in ("error", "warning") else None)
    pg.goto("file:///" + page_path.replace("\\", "/"))
    pg.wait_for_function("document.getElementById('out') && document.getElementById('out').textContent.length > 0", timeout=900000)
    txt = pg.eval_on_selector("#out", "e => e.textContent")
    b.close()
o = json.loads(txt)
print(o["log"])
for k, v in o["files"].items():
    open(os.path.join(out_dir, k), "wb").write(base64.b64decode(v))
    print(k, len(v) * 3 // 4)
json.dump(o["data"], open(os.path.join(out_dir, "timeline.json"), "w"), indent=1)
