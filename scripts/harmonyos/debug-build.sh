#!/usr/bin/env bash
# Build/sign in an ignored copy, leaving tracked signing configuration untouched.
set -euo pipefail
source "$(dirname "$0")/env.sh"
source_app="$NEXTNEWS_ROOT/apps/harmonyos"
debug_app="$NEXTNEWS_ROOT/.local/signed-harmonyos/apps/harmonyos"
mkdir -p "$debug_app"
rsync -a --exclude='build-profile.json5' --exclude='.idea' --exclude='build' --exclude='oh_modules' --exclude='.hvigor' --exclude='.cxx' --exclude='local.properties' "$source_app/" "$debug_app/"
# The module build profile has no signing material and should follow source.
cp "$source_app/entry/build-profile.json5" "$debug_app/entry/build-profile.json5"
node - "$source_app/build-profile.json5" "$debug_app/build-profile.json5" <<'JS'
const fs = require('fs');
const json5 = require(process.env.NEXTNEWS_ROOT + '/.tools/deveco/node_modules/json5');
const [source, target] = process.argv.slice(2);
const base = json5.parse(fs.readFileSync(source, 'utf8'));
if (fs.existsSync(target)) {
  const previous = json5.parse(fs.readFileSync(target, 'utf8'));
  base.app.signingConfigs = previous.app.signingConfigs || [];
  for (const product of base.app.products) {
    const old = previous.app.products.find(p => p.name === product.name);
    if (old?.signingConfig) product.signingConfig = old.signingConfig;
  }
}
fs.writeFileSync(target, JSON.stringify(base, null, 2) + '\n', {mode: 0o600});
JS
cd "$debug_app"
devecocli signature generate
devecocli build "$@"
mkdir -p "$NEXTNEWS_ROOT/artifacts/harmonyos"
cp entry/build/default/outputs/default/entry-default-signed.hap "$NEXTNEWS_ROOT/artifacts/harmonyos/NextNews-debug.hap"
printf 'Debug HAP: %s\n' "$NEXTNEWS_ROOT/artifacts/harmonyos/NextNews-debug.hap"
