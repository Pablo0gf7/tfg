#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGINDIR="$HOME/.msf4/plugins"
BIN_DEFAULT="$ROOT/dist/stego"   

mkdir -p "$PLUGINDIR"

echo "[*] Copiando plugin a $PLUGINDIR ..."
install -m 0644 "$ROOT/plugin/stego.rb" "$PLUGINDIR/"

if [[ -x "$BIN_DEFAULT" ]]; then
  echo "[*] Detectado binario por defecto: $BIN_DEFAULT"
  echo "export STEGO_BIN=\"$BIN_DEFAULT\"" > "$ROOT/.stego.env"
  echo "[*] Para usarlo en esta sesión: source $ROOT/.stego.env"
else
  echo "[!] No se encontró $BIN_DEFAULT ejecutable."
  echo "    Compila primero (scripts/build.sh) o edita este fichero para apuntar a tu binario."
fi

echo "[*] Instalación completa.
- Carga el plugin en msfconsole:  load $PLUGINDIR/stego.rb
- Configura manualmente (si no usas .stego.env): stego_set_bin /ruta/a/tu/binario"
