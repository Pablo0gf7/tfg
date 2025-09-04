#!/usr/bin/env bash
set -euo pipefail

PLUGINDIR="$HOME/.msf4/plugins"
PLUGIN="$PLUGINDIR/stego.rb"

if [[ -f "$PLUGIN" ]]; then
  echo "[*] Eliminando $PLUGIN ..."
  rm -f "$PLUGIN"
else
  echo "[*] No se encontró $PLUGIN (ya estaba desinstalado)."
fi

echo "[*] Listo."
