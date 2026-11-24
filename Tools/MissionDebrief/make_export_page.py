"""Builds export_scene.html from the approved round-6 mock-up: same scene code, plus an exporter that writes the
static set, the state-specific tin parts, the stamp body and faces, the paper and soot planes as .glb, the paper /
ink / burn textures as PNG, and the timeline numbers as JSON, all into <pre id="out"> for --dump-dom."""
import sys

src, dst = sys.argv[1:]
s = open(src, encoding='utf-8').read()


def rep(a, b, count=1):
    global s
    assert s.count(a) == count, (s.count(a), a[:80])
    s = s.replace(a, b)


rep('<script src="https://cdn.jsdelivr.net/npm/three@0.149.0/build/three.min.js"></script>',
    '<script src="https://cdn.jsdelivr.net/npm/three@0.147.0/build/three.min.js"></script>\n'
    '<script src="https://cdn.jsdelivr.net/npm/three@0.147.0/examples/js/exporters/GLTFExporter.js"></script>')
# tag the state-specific tin parts
rep("const pad = feltPad(win, design); pad.position.y = .0835; g.add(pad);",
    "const pad = feltPad(win, design); pad.position.y = .0835; pad.userData.statePart = true; g.add(pad);")
rep("feltSide.position.y = .048; g.add(feltSide);", "feltSide.position.y = .048; feltSide.userData.statePart = true; g.add(feltSide);")
rep("label.position.set(0, .047, .475); label.receiveShadow = true; hinge.add(label);",
    "label.position.set(0, .047, .475); label.receiveShadow = true; label.userData.statePart = true; hinge.add(label);")
# expose the scene internals
rep("  return {\n    scene, cam, dur, bar,",
    "  window.__exp = { scene, stamp, paper, soot, tinG, keys, slam, spinA, spinB, holdEnd, exitEnd, tip, tilt, yawTip, wx, wz, py, yawS, top,\n"
    "    baseCam, endCam, look0, look1, dur, bar, burnA, burnB, burnTo, wordBox, burnF, toWorld, spot, wash, glow, fill, rim, hemi, fire, L, ink, inkH, keep, inkColor };\n"
    "  return {\n    scene, cam, dur, bar,")
# replace boot with the exporter
i = s.index("async function boot() {")
j = s.index("boot();\n})();", i)
s = s[:i] + open(sys.argv[0].replace('make_export_page.py', 'export_boot.js'), encoding='utf-8').read() + "\n" + s[j:]
open(dst, 'w', encoding='utf-8').write(s)
print('ok')
