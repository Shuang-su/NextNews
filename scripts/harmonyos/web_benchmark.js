// Run through playwright-cli eval on the locally built upstream SuperSplat Viewer.
// PUBLIC_SCENE is replaced with bounds measured from the identical input PLY.
async () => {
  const spec = PUBLIC_SCENE;
  const app = window.app, camera = app.root.findComponent('camera');
  if (!app || !camera) throw new Error('Viewer has not finished loading');
  const resource=app.assets.get(app.root.findComponent('gsplat').asset).resource;
  if (resource.numSplats !== spec.count) throw new Error('Reference Gaussian count mismatch');
  camera.fov = 45; camera.nearClip = spec.radius * .001; camera.farClip = spec.radius * 100;
  camera.clearColor.set(.035,.045,.065,1);
  const start = performance.now(); let frames = 0;
  return await new Promise(resolve => {
    const move = () => {
      const elapsed = performance.now() - start, yaw = Math.min(elapsed, 10000) * .0003, pitch = .2, d = spec.radius * 3;
      camera.entity.setPosition(spec.center[0] + Math.sin(yaw)*Math.cos(pitch)*d,
        spec.center[1]+Math.sin(pitch)*d, spec.center[2]+Math.cos(yaw)*Math.cos(pitch)*d);
      camera.entity.lookAt(spec.center[0],spec.center[1],spec.center[2]);app.renderNextFrame = true;
    };
    const count = () => {
      frames++; const elapsed = performance.now()-start;
      if (elapsed >= 10000) {
        app.off('update', move);app.off('postrender',count);
        const gl=app.graphicsDevice.gl, ext=gl?.getExtension('WEBGL_debug_renderer_info');
        const result={frames,elapsedMs:elapsed,fps:frames*1000/elapsed,width:app.graphicsDevice.width,height:app.graphicsDevice.height,
          count:spec.count,renderer:ext?gl.getParameter(ext.UNMASKED_RENDERER_WEBGL):app.graphicsDevice.deviceType};
        window.nextnewsBenchmark=result;resolve(result);
      }
    };
    app.on('update',move);app.on('postrender',count);
  });
}
