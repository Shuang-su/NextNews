#!/usr/bin/env bash
# Build the pinned upstream viewer beside a generated native SOG stream.
set -euo pipefail
source "$(dirname "$0")/env.sh"
reference="$NEXTNEWS_ROOT/.local/supersplat-viewer-reference"
output="${1:-$NEXTNEWS_ROOT/.local/sog-huafa}"
revision=96f62515b99a28a20579041a656f7b1911c2964c
[[ -f "$output/provenance.json" && -f "$output/lod-meta.json" ]] || { echo 'Expected a generated SOG stream directory'; exit 1; }
if [[ ! -d "$reference/.git" ]]; then
  git clone https://github.com/playcanvas/supersplat-viewer.git "$reference"
  git -C "$reference" checkout --detach "$revision"
fi
[[ "$(git -C "$reference" rev-parse HEAD)" = "$revision" ]] || { echo 'Reference checkout does not match the pinned revision'; exit 1; }
(cd "$reference" && npm ci --cache "$NEXTNEWS_ROOT/.local/npm-cache" && npm run build)
node --input-type=module - "$reference" "$output" <<'JS'
import fs from 'node:fs';import path from 'node:path';import {pathToFileURL} from 'node:url';
const [reference, output] = process.argv.slice(2);
const {defaultSettings}=await import(pathToFileURL(path.join(reference,'dist/settings.js')));
for(const file of ['index.html','index.css','index.js'])fs.copyFileSync(path.join(reference,'public',file),path.join(output,file));
const settings=defaultSettings('object'),b=JSON.parse(fs.readFileSync(path.join(output,'scene.json'),'utf8')).bounds;
const center=[-b[0],-b[1],b[2]],d=3*b[3];settings.cameras[0].initial={position:[center[0],center[1]+Math.sin(.35)*d,center[2]+Math.cos(.35)*d],target:center,fov:45};settings.background.color=[.035,.045,.065];
fs.writeFileSync(path.join(output,'settings.json'),JSON.stringify(settings));
const html=path.join(output,'index.html');fs.writeFileSync(html,fs.readFileSync(html,'utf8').replace('<head>',`<head><script>const u=new URL(location.href);if(!u.searchParams.has('content')||u.searchParams.get('content')==='scene.json'){u.searchParams.set('content','lod-meta.json');location.replace(u.href);}</script>`));
JS
printf 'Browser viewer prepared in %s\n' "$output"
