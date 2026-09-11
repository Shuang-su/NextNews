#!/usr/bin/env bash
# Source this file in bash. Does not change the user's shell profile.
NEXTNEWS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export NEXTNEWS_ROOT
export DEVECO_STUDIO_HOME="${DEVECO_STUDIO_HOME:-/Applications/DevEco-Studio.app}"
if [ ! -d "$DEVECO_STUDIO_HOME" ] && [ -d "$NEXTNEWS_ROOT/.tools/command-line-tools" ]; then
  export DEVECO_CLI_CLT_PATH="$NEXTNEWS_ROOT/.tools/command-line-tools"
  export PATH="$DEVECO_CLI_CLT_PATH/tool/node/bin:$DEVECO_CLI_CLT_PATH/bin:$PATH"
fi
export PATH="$NEXTNEWS_ROOT/.tools/deveco/node_modules/.bin:$DEVECO_STUDIO_HOME/Contents/tools/node/bin:$DEVECO_STUDIO_HOME/Contents/tools/ohpm/bin:$DEVECO_STUDIO_HOME/Contents/tools/hvigor/bin:$PATH"
if [ -d "$DEVECO_STUDIO_HOME/Contents/jbr/Contents/Home" ]; then
  export JAVA_HOME="$DEVECO_STUDIO_HOME/Contents/jbr/Contents/Home"
fi
