async function boot() {
  const outEl = document.createElement('pre'); outEl.id = 'out'; document.body.appendChild(outEl);
  const OUT = { files: {}, data: {}, log: [] };
  try {
    await Promise.all(['700 64px Oswald', '64px Anton', '32px "Special Elite"'].map(f => document.fonts.load(f)));
    buildShared(); SH.weapons = null;
    const r = new T.WebGLRenderer({ antialias: false }); r.setSize(64, 36);
    const card = { aspect: 16 / 9, r, env: makeEnv(r) };
    const b64 = buf => { let s = ''; const u = new Uint8Array(buf); for (let i = 0; i < u.length; i += 0x8000) s += String.fromCharCode.apply(null, u.subarray(i, i + 0x8000)); return btoa(s); };
    const glb = obj => new Promise((res, rej) => new T.GLTFExporter().parse(obj, b => res(b), e => rej(e), { binary: true, onlyVisible: true, embedImages: true, maxTextureSize: 4096 }));
    const png = cv => cv.toDataURL('image/png').split(',')[1];
    const flipV = cv => { const o = mkCanvas(cv.width, cv.height), c = o.getContext('2d'); c.translate(0, cv.height); c.scale(1, -1); c.drawImage(cv, 0, 0); return o; };
    // glTF has no alpha maps: fold an alpha map into an RGBA colour map; BackSide becomes DoubleSide
    const fixMaterials = root => root.traverse(m => {
      if (!m.isMesh) return;
      let mat = m.material;
      if (mat.alphaMap) {
        const src = mat.alphaMap.image, w = src.width, h = src.height, cv = mkCanvas(w, h), c = cv.getContext('2d');
        c.drawImage(src, 0, 0); const d = c.getImageData(0, 0, w, h), col = mat.color, op = mat.opacity;
        for (let i = 0; i < d.data.length; i += 4) { const a = d.data[i + 1] / 255 * op; d.data[i] = col.r * 255; d.data[i + 1] = col.g * 255; d.data[i + 2] = col.b * 255; d.data[i + 3] = a * 255; }
        c.putImageData(d, 0, 0); const t = new T.CanvasTexture(cv); t.encoding = T.sRGBEncoding;
        mat = mat.clone(); mat.alphaMap = null; mat.map = t; mat.color = new T.Color(1, 1, 1); mat.opacity = 1; mat.transparent = true; m.material = mat;
      }
      if (mat.side === T.BackSide) { mat = mat.clone(); mat.side = T.DoubleSide; m.material = mat; }
    });
    // displacement does not survive glTF: move the vertices instead
    const bakeDisp = root => root.traverse(m => {
      if (!m.isMesh || !m.material.displacementMap) return;
      const mat = m.material, img = mat.displacementMap.image, w = img.width, h = img.height, d = img.getContext('2d').getImageData(0, 0, w, h).data;
      const g = m.geometry = m.geometry.clone(), pos = g.attributes.position, nrm = g.attributes.normal, uv = g.attributes.uv;
      const H = (x, y) => d[(Math.min(h - 1, Math.max(0, y)) * w + Math.min(w - 1, Math.max(0, x))) * 4] / 255;
      for (let i = 0; i < pos.count; i++) {
        const x = uv.getX(i) * (w - 1), y = (1 - uv.getY(i)) * (h - 1), x0 = Math.floor(x), y0 = Math.floor(y), fx = x - x0, fy = y - y0;
        const v = lerp(lerp(H(x0, y0), H(x0 + 1, y0), fx), lerp(H(x0, y0 + 1), H(x0 + 1, y0 + 1), fx), fy) * mat.displacementScale + mat.displacementBias;
        pos.setXYZ(i, pos.getX(i) + nrm.getX(i) * v, pos.getY(i) + nrm.getY(i) * v, pos.getZ(i) + nrm.getZ(i) * v);
      }
      pos.needsUpdate = true; g.computeVertexNormals();
      const nm = mat.clone(); nm.displacementMap = null; nm.displacementScale = 0; nm.displacementBias = 0; m.material = nm;
    });
    // instanced rivets -> plain meshes
    const unInstance = root => { const add = []; root.traverse(m => { if (!m.isInstancedMesh) return; const mt = new T.Matrix4();
      for (let i = 0; i < m.count; i++) { m.getMatrixAt(i, mt); const c = new T.Mesh(m.geometry, m.material); mt.decompose(c.position, c.quaternion, c.scale); add.push([m.parent, c]); } m.visible = false; });
      add.forEach(([p, c]) => p.add(c)); };
    // a copy of src placed at its world transform under a fresh root (units -> metres: 1 unit = 10 cm)
    const worldCopy = (src, filter) => { src.updateMatrixWorld(true); const c = src.clone(true); src.matrixWorld.decompose(c.position, c.quaternion, c.scale);
      if (filter) { const rm = []; c.traverse(o => { if (o !== c && !filter(o)) rm.push(o); }); rm.forEach(o => o.parent && o.parent.remove(o)); } return c; };
    const exportRoot = kids => { const root = new T.Scene(), g = new T.Group(); g.scale.setScalar(.1); kids.forEach(k => g.add(k)); root.add(g); unInstance(root); bakeDisp(root); fixMaterials(root); return root; };
    const v3 = v => [v.x, v.y, v.z];

    for (const st of ['victory', 'defeat']) {
      buildScene(card, st); const E = window.__exp;
      E.scene.updateMatrixWorld(true);
      if (st === 'victory') {
        // everything static and shared by both outcomes
        const keepOut = new Set([E.stamp, E.paper, E.soot]);
        const kids = [];
        for (const o of E.scene.children) {
          if (keepOut.has(o) || o.isLight || o.isPoints || o.type === 'Object3D' || (!o.isMesh && !o.isGroup && !o.isInstancedMesh)) continue;
          if (o.isGroup && o.children.length === 0) continue;
          kids.push(worldCopy(o, x => !x.userData.statePart && !x.isLight));
        }
        OUT.files['set_common.glb'] = b64(await glb(exportRoot(kids)));
        // stamp body in its own frame, without faces
        const body = E.stamp.clone(true); body.position.set(0, 0, 0); body.rotation.set(0, 0, 0); body.scale.set(1, 1, 1);
        const faces = E.stamp.userData.faces; body.children.slice().forEach((c, i) => { if (i < 2) body.remove(c); });
        OUT.files['stamp_body.glb'] = b64(await glb(exportRoot([body])));
        OUT.files['paper.glb'] = b64(await glb(exportRoot([worldCopy(E.paper)])));
        OUT.files['soot.glb'] = b64(await glb(exportRoot([(() => { const c = worldCopy(E.soot); c.material = c.material.clone(); c.material.opacity = 1; return c; })()])));
        for (const [nm, cv] of [['noise', SH.noise.cv], ['soot_alpha', SH.soot], ['sprite_dot', SH.dot], ['sprite_flame', SH.flame], ['sprite_smoke', SH.smoke]]) OUT.files[nm + '.png'] = png(cv);
      }
      // per-outcome parts
      const parts = []; E.tinG.updateMatrixWorld(true);
      E.tinG.traverse(o => { if (o.userData.statePart) parts.push(worldCopy(o)); });
      OUT.files[`tin_${st}.glb`] = b64(await glb(exportRoot(parts)));
      const faces = E.stamp.userData.faces;
      for (const [k, f] of [['mirrored', faces[0]], ['readable', faces[1]]]) { const c = f.clone(); c.visible = true; OUT.files[`face_${st}_${k}.glb`] = b64(await glb(exportRoot([c]))); }
      // paper textures in the same (glTF) orientation as the paper's own texture
      OUT.files[`ink_${st}.png`] = png(flipV(E.ink)); OUT.files[`inkheight_${st}.png`] = png(flipV(E.inkH));
      if (E.keep) OUT.files[`keep_${st}.png`] = png(flipV(E.keep.cv));
      // burn field without the burn value: CPU copy for spawning flames, 128 x 181
      const GW = 128, GH = 181, grid = [];
      for (let y = 0; y < GH; y++) for (let x = 0; x < GW; x++) grid.push(+E.burnF(x / (GW - 1), y / (GH - 1)).toFixed(3));
      OUT.data[st] = {
        keys: E.keys, slam: E.slam, spinA: E.spinA, spinB: E.spinB, holdEnd: E.holdEnd, exitEnd: E.exitEnd, dur: E.dur, bar: E.bar,
        burnA: E.burnA, burnB: E.burnB, burnTo: E.burnTo, wordBox: E.wordBox, inkColor: E.inkColor, paper: { py: E.py, yaw: CFG.paperYaw, W, H },
        cam: { base: v3(E.baseCam), end: v3(E.endCam), look0: v3(E.look0), look1: v3(E.look1), fov: CFG.cam.fov },
        lights: Object.fromEntries(['spot', 'wash', 'glow', 'fill', 'rim', 'fire'].map(n => { const l = E[n]; const o = { pos: v3(l.position), color: l.color.getHexString(), intensity: l.intensity, distance: l.distance || 0, decay: l.decay || 0 };
          if (l.isSpotLight) { o.target = v3(l.target.position); o.angle = l.angle; o.penumbra = l.penumbra; } if (l.isDirectionalLight) o.target = v3(l.target.position); return [n, o]; })),
        hemi: { sky: E.hemi.color.getHexString(), ground: E.hemi.groundColor.getHexString(), intensity: E.hemi.intensity },
        weapons: WPOS, burnGrid: { w: GW, h: GH, v: grid }
      };
    }
    OUT.log.push('ok');
  } catch (e) { OUT.log.push('ERROR ' + e + ' ' + (e.stack || '')); }
  outEl.textContent = JSON.stringify(OUT);
}
