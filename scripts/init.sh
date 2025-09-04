#!/usr/bin/env bash
set -euo pipefail

PLUGINDIR="$HOME/.msf4/plugins"
if [[ ! -f "$PLUGINDIR/stego.rb" ]]; then
  echo "[!] No se encuentra $PLUGINDIR/stego.rb. Ejecuta scripts/install.sh"
  exit 1
fi

echo "[*] Abriendo msfconsole. Prueba dentro:"
echo "    msf > load $PLUGINDIR/stego.rb"
echo "    msf > stego_debug on"
echo "    msf > stego_bin"
echo "    msf > stego --help   # (tu binario debería mostrar su ayuda)"

## Configurar tu path a msfconsole
/home/pvbb/metaploit/msfconsole
