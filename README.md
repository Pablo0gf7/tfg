# stego-metasploit

Proyecto para **ocultar y extraer mensajes** en **texto, imagen y audio** mediante algoritmos esteganográficos escritos en C, invocados desde **Metasploit** a través de un plugin sencillo (`stego.rb`).

> Pensado para **uso académico/demostrativo** (laboratorio). No está orientado a fines maliciosos.

---

**Trabajo de Fin de Grado (TFG)**
*Desarrollo de un plugin para Metasploit utilizando
algoritmos esteganográficos (texto, imagen y audio) con cifrado e integración en Metasploit*

---

## ¿Para qué sirve?

* Probar y demostrar técnicas de esteganografía:

  * Texto (capitalización de letras)
  * Imagen (LSB2 con cifrado)
  * Audio (QIM con AEAD)
* Integrar estas técnicas en `msfconsole` sin reescribir la lógica en Ruby (el plugin orquesta binarios C externos).

---

