# NextNews development

This workspace contains independent native platform tasks. HarmonyOS lives in
`apps/harmonyos`; keep future operating-system work in separate platform folders.

For HarmonyOS, read `docs/harmonyos/README.md` and the installed official
`.tools/deveco/node_modules/@deveco/deveco-cli/SKILL.md` when available. Use
`bash scripts/harmonyos/build.sh` for builds. Use the existing paired SDK/IDE;
do not silently upgrade or change the global shell, Java, Node or Xcode setup.

Run native parser/math tests with the per-command Xcode 27 override documented
in the README. A host test or successful HAP build is not proof of simulator or
phone rendering. Record device identity, API version and actual visual evidence.

Never commit signing materials, account details, generated build directories or
local installer/tool caches. Preserve user-supplied original splat files; prepare
new copies with scripts/harmonyos/prepare_model.py and retain provenance manifests.
