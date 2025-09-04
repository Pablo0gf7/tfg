# stego-metasploit

Plugin de Metasploit (`stego.rb`) para invocar **algoritmos esteganográficos en C** (imagen, audio, texto) desde `msfconsole`.  
No se porta la lógica a Ruby: la capa Ruby **orquesta** un binario externo, preservando rendimiento y control.

> **Estado**: estable para uso local/lab. Se aceptan PRs.

---

## ✨ Características

- `load stego.rb` en `msfconsole` y tendrás:
  - `stego <args...>`: ejecuta el binario configurado con tus argumentos.
  - `stego_set_bin /ruta/al/binario`
  - `stego_bin`: muestra y verifica la ruta
  - `stego_debug on|off|toggle`: trazas de depuración
- Redacción automática de secretos al imprimir la línea ejecutada (flags `-p`, `-k`, `-m`).
- Compatible con:
  - **Binario monolítico** (`dist/steg`): modos `-t` (texto), `-i` (imagen), `-a` (audio).
  - **Binarios específicos** (`dist/qim_audio`, etc.).

---

## 📦 Instalación

### Requisitos
- Metasploit Framework (msfconsole)
- (Opcional) Toolchain C y librerías:
  - `libsndfile`, `fftw3`, `libsodium`, `libm` (audio)
  - `stb_image.h`, `stb_image_write.h` (imagen)

### 1) Clonar
```bash
git clone https://github.com/<tu-usuario>/stego-metasploit.git
cd stego-metasploit
