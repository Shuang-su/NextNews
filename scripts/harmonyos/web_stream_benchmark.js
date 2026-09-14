// Instrument the pinned local upstream viewer without changing its LOD/sort implementation.
// frame:ready is an engine readiness signal, not equality with NextNews's selection set.
// Different selection algorithms are reported with their actual draw counts.
window.nextnewsWeb = {
  busy: false,
  async measure() {
    if (this.busy) return;
    const app = window.app;
    if (!app) throw new Error('SuperSplat first frame is not ready');
    const profile = await (await fetch('viewer-settings.json')).json();
    const bounds = (await (await fetch('scene.json')).json()).bounds;
    const camera = app.root.findComponent('camera'), initial = profile.cameras[0].initial;
    const d = initial.position.map((v, i) => v - initial.target[i]), length = Math.hypot(...d);
    const yaw = Math.atan2(d[0], d[2]), pitch = Math.asin(d[1] / length);
    this.busy = true;
    window.nextnewsResolution = [1320, 2623];
    let angle = yaw, tilt = pitch, rendered = 0, readyAtFrame = -1, targetVersion = 0, readyVersion = -1;
    const apply = () => {

      camera.fov = initial.fov; camera.nearClip = bounds[3] * .001; camera.farClip = bounds[3] * 100;
      camera.entity.setPosition(...initial.position);
      camera.entity.lookAt(initial.position[0] - Math.sin(angle) * Math.cos(tilt) * length,
        initial.position[1] - Math.sin(tilt) * length,
        initial.position[2] - Math.cos(angle) * Math.cos(tilt) * length);
    };
    const ready = (c, layer, value, loading) => {
      if (c !== camera) return;
      if (value && loading === 0) { readyAtFrame = rendered; readyVersion = targetVersion; }
      else { readyAtFrame = -1; readyVersion = -1; }
    };
    const post = () => { rendered++; };
    const oldAuto = app.autoRender; app.autoRender = true;
    app.on('update', apply); app.on('postrender', post); app.systems.gsplat.on('frame:ready', ready);
    const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
    const percentile = (values, q) => values.slice().sort((a, b) => a - b)[Math.ceil(values.length * q) - 1];
    try {
      const results = [];
      for (let trial = -2; trial < 20; trial++) {
        targetVersion++; readyVersion = -1; readyAtFrame = -1;
        angle = yaw + (trial % 2 === 0 ? -.65 : .65);
        const start = performance.now(), first = rendered;
        while (!(rendered > first + 2 && readyVersion === targetVersion && rendered > readyAtFrame)) {
          if (performance.now() - start > 45000) throw new Error('Upstream ready timeout');
          await pause(10);
        }
        const r = { trial, version: targetVersion, elapsedMs: performance.now() - start,
          drawn: app.stats.frame.gsplats, budget: app.scene.gsplat.splatBudget,
          width: app.graphicsDevice.width, height: app.graphicsDevice.height };
        console.info('WebTrial ' + JSON.stringify(r)); if (trial >= 0) results.push(r);
        await pause(300);
      }
      const frames = []; let last = performance.now();
      const record = () => { const now = performance.now(); frames.push(now - last); last = now; };
      app.on('postrender', record); const start = performance.now();
      while (performance.now() - start < 20000) {
        const t = (performance.now() - start) / 1000;
        angle = yaw + .65 * Math.sin(t * 1.2); tilt = pitch + .08 * Math.sin(t * .7); await pause(16);
      }
      app.off('postrender', record);
      const result = { renderer: app.graphicsDevice.deviceType, userAgent: navigator.userAgent,
        trials: results.length, readyP95Ms: percentile(results.map(r => r.elapsedMs), .95),
        frameP95Ms: percentile(frames.slice(1), .95), frames: frames.length,
        width: app.graphicsDevice.width, height: app.graphicsDevice.height,
        budget: app.scene.gsplat.splatBudget, drawn: app.stats.frame.gsplats,
        camera: initial, trajectory: 'yaw + 0.65 sin(1.2t), pitch + 0.08 sin(0.7t)',
        note: 'Engine frame:ready; selection algorithm differs; not a NextNews coverage gate' };
      this.result = result; console.info('WebBenchmark ' + JSON.stringify(result));
      return result;
    } catch (error) { console.error('WebBenchmark failure=' + String(error)); throw error; }
    finally { app.off('update', apply); app.off('postrender', post); app.systems.gsplat.off('frame:ready', ready); app.autoRender = oldAuto; this.busy = false; }
  }
};
