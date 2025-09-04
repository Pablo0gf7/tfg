// qim_audio_secure.c - QIM para audio con cifrado AEAD (XChaCha20-Poly1305) y cabecera BE
// Compilación:
//   gcc qim_audio_secure.c -o qim_audio -lsndfile -lfftw3 -lsodium -lm
//
// Uso:
//   ./qim_audio embed in.wav out.wav "mensaje a ocultar" "password"
//   ./qim_audio extract estego.wav "password"
//
// Notas:
// - Se cifra el mensaje y se empaqueta: [len_be(4)] + [salt(16) || nonce(24) || ciphertext(len+16)]
// - Así, el nonce y el salt VAN dentro del audio: no hay que guardarlos aparte.
// - La longitud (4 bytes BE) es la del BLOB CIFRADO, no del texto en claro.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sndfile.h>
#include <fftw3.h>
#include <sodium.h>

// ====== Parámetros ======
#define BLOCK_SIZE 1024 // tamaño de bloque DCT
#define DELTA 0.15      // paso de cuantización QIM

// libsodium
#define SALT_LEN crypto_pwhash_SALTBYTES                       // 16
#define NONCE_LEN crypto_aead_xchacha20poly1305_ietf_NPUBBYTES // 24
#define KEY_LEN crypto_aead_xchacha20poly1305_ietf_KEYBYTES    // 32
#define ABYTES crypto_aead_xchacha20poly1305_ietf_ABYTES       // 16

// ====== Prototipos ======
static inline double clamp_double(double x, double minv, double maxv);
static void u32_to_be(uint32_t v, unsigned char out[4]);
static uint32_t be_to_u32(const unsigned char in[4]);

static double qim_embed(double coef, int bit, double delta);
static int qim_extract(double coef, double delta);
static uint64_t compute_capacity_bits(sf_count_t total_frames, int channels);

static double mse_to_psnr(double mse, double peak);
static int read_embedded_length(const char *stego_file, uint32_t *out_len, int block_size, double delta);
static sf_count_t compute_modified_frames(sf_count_t total_frames, int channels, uint64_t total_bits, int block_size);

void calculate_metrics_audio(const char *orig_file, const char *stego_file, const char *output_path);

void embed_message(const char *infile, const char *outfile, const char *message, const char *password);
void extract_message(const char *infile, const char *password);

// --------- Utilidades ---------
static inline double clamp_double(double x, double minv, double maxv)
{
    return x < minv ? minv : (x > maxv ? maxv : x);
}

// Convierte una longitud (uint32) a 4 bytes big-endian
static void u32_to_be(uint32_t v, unsigned char out[4])
{
    out[0] = (unsigned char)((v >> 24) & 0xFF);
    out[1] = (unsigned char)((v >> 16) & 0xFF);
    out[2] = (unsigned char)((v >> 8) & 0xFF);
    out[3] = (unsigned char)(v & 0xFF);
}

// Reconstruye uint32 desde 4 bytes big-endian
static uint32_t be_to_u32(const unsigned char in[4])
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}

// --------- QIM básico ---------
static double qim_embed(double coef, int bit, double delta)
{
    double q = round(coef / delta);
    if (((int)q & 1) != bit)
    {
        q += (bit - ((int)q & 1)); // ajusta paridad
    }
    return q * delta;
}

static int qim_extract(double coef, double delta)
{
    int q = (int)round(coef / delta);
    return q & 1;
}

// --------- Cálculo de capacidad ---------
// Capacidad (en bits) = canales * sum_over_bloques( frames_bloque - 1 )
// (saltamos índice 0 de la DCT en cada bloque y canal)
static uint64_t compute_capacity_bits(sf_count_t total_frames, int channels)
{
    if (total_frames <= 0 || channels <= 0)
        return 0;
    sf_count_t full_blocks = total_frames / BLOCK_SIZE;
    sf_count_t last = total_frames % BLOCK_SIZE;
    uint64_t per_block_bits = (BLOCK_SIZE > 0 ? (uint64_t)(BLOCK_SIZE - 1) : 0);
    uint64_t capacity = (uint64_t)channels * (full_blocks * per_block_bits);
    if (last > 0)
    {
        if (last > 1)
            capacity += (uint64_t)channels * (uint64_t)(last - 1);
        // si last == 1, no hay coeficientes usables (solo DC)
    }
    return capacity;
}

// ============ EMBED (con cifrado) ============
void embed_message(const char *infile, const char *outfile, const char *message, const char *password)
{
    if (sodium_init() < 0)
    {
        fprintf(stderr, "Error iniciando libsodium\n");
        return;
    }

    SF_INFO sfinfo = {0};
    SNDFILE *in = sf_open(infile, SFM_READ, &sfinfo);
    if (!in)
    {
        fprintf(stderr, "Error abriendo entrada: %s\n", infile);
        return;
    }

    // Capacidad disponible
    uint64_t capacity_bits = compute_capacity_bits(sfinfo.frames, sfinfo.channels);

    // Estimar tamaño cifrado a ocultar
    uint32_t plain_len = (uint32_t)strlen(message);
    uint32_t cipher_len_est = plain_len + ABYTES;                 // AEAD añade 16 bytes
    uint32_t enc_len_est = SALT_LEN + NONCE_LEN + cipher_len_est; // salt + nonce + ciphertext

    uint64_t needed_bits = (uint64_t)(4 + enc_len_est) * 8ULL; // 4 bytes de cabecera + blob cifrado

    if (needed_bits > capacity_bits)
    {
        fprintf(stderr, "Mensaje demasiado grande para el audio.\n");
        fprintf(stderr, "Capacidad: %llu bits (%.2f KB)  Necesario: %llu bits (%.2f KB)\n",
                (unsigned long long)capacity_bits, capacity_bits / 8.0 / 1024.0,
                (unsigned long long)needed_bits, needed_bits / 8.0 / 1024.0);
        sf_close(in);
        return;
    }

    // Preparamos salida con el mismo formato
    SNDFILE *out = sf_open(outfile, SFM_WRITE, &sfinfo);
    if (!out)
    {
        fprintf(stderr, "Error creando salida: %s\n", outfile);
        sf_close(in);
        return;
    }

    // --- Cifrado ---
    unsigned char salt[SALT_LEN], nonce[NONCE_LEN], key[KEY_LEN];
    randombytes_buf(salt, SALT_LEN);
    randombytes_buf(nonce, NONCE_LEN);

    if (crypto_pwhash(key, KEY_LEN,
                      password, strlen(password), salt,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_DEFAULT) != 0)
    {
        fprintf(stderr, "Error derivando clave (memoria insuficiente)\n");
        sf_close(in);
        sf_close(out);
        return;
    }

    unsigned long long cipher_len = (unsigned long long)(plain_len + ABYTES);
    unsigned char *cipher = (unsigned char *)malloc((size_t)cipher_len);
    if (!cipher)
    {
        fprintf(stderr, "Memoria insuficiente.\n");
        sf_close(in);
        sf_close(out);
        return;
    }

    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            cipher, &cipher_len,
            (const unsigned char *)message, plain_len,
            NULL, 0,
            NULL,
            nonce, key) != 0)
    {
        fprintf(stderr, "Fallo cifrando.\n");
        free(cipher);
        sf_close(in);
        sf_close(out);
        return;
    }

    // Construye payload: [len(4 BE)] + salt(16) + nonce(24) + ciphertext(cipher_len)
    uint32_t enc_len = SALT_LEN + NONCE_LEN + (uint32_t)cipher_len;
    unsigned char *payload = (unsigned char *)malloc(4 + enc_len);
    if (!payload)
    {
        fprintf(stderr, "Memoria insuficiente.\n");
        free(cipher);
        sf_close(in);
        sf_close(out);
        return;
    }

    u32_to_be(enc_len, payload);
    memcpy(payload + 4, salt, SALT_LEN);
    memcpy(payload + 4 + SALT_LEN, nonce, NONCE_LEN);
    memcpy(payload + 4 + SALT_LEN + NONCE_LEN, cipher, (size_t)cipher_len);

    free(cipher);

    // Convertimos a bits (MSB-primero por byte)
    uint64_t total_bits = (uint64_t)(4 + enc_len) * 8ULL;
    int *bits = (int *)malloc((size_t)total_bits * sizeof(int));
    if (!bits)
    {
        fprintf(stderr, "Memoria insuficiente.\n");
        free(payload);
        sf_close(in);
        sf_close(out);
        return;
    }

    uint64_t bp = 0;
    for (uint64_t i = 0; i < (uint64_t)(4 + enc_len); ++i)
    {
        unsigned char byte = payload[i];
        for (int b = 7; b >= 0; --b)
        {
            bits[bp++] = (byte >> b) & 1;
        }
    }
    free(payload);

    // Buffers de trabajo
    size_t ch = (size_t)sfinfo.channels;
    float *framebuf = (float *)malloc(sizeof(float) * BLOCK_SIZE * ch);
    double *work = (double *)malloc(sizeof(double) * BLOCK_SIZE);
    if (!framebuf || !work)
    {
        fprintf(stderr, "Memoria insuficiente.\n");
        free(bits);
        if (framebuf)
            free(framebuf);
        if (work)
            free(work);
        sf_close(in);
        sf_close(out);
        return;
    }

    // Recorremos el audio por bloques de FRAMES; por cada canal aplicamos DCT y embebido
    sf_count_t frames;
    uint64_t bit_pos = 0;
    while ((frames = sf_readf_float(in, framebuf, BLOCK_SIZE)) > 0)
    {
        // Si ya no quedan bits por insertar, copiar el resto sin tocar
        if (bit_pos >= total_bits)
        {
            sf_writef_float(out, framebuf, frames);
            continue;
        }

        // Procesar canal a canal
        for (size_t c = 0; c < ch; ++c)
        {
            // Copia canal c -> work[]
            for (sf_count_t i = 0; i < frames; ++i)
            {
                work[i] = framebuf[i * ch + c];
            }

            // DCT-II
            fftw_plan p = fftw_plan_r2r_1d((int)frames, work, work, FFTW_REDFT10, FFTW_ESTIMATE);
            fftw_execute(p);
            fftw_destroy_plan(p);

            // Insertar bits en coeficientes 1..frames-1
            for (sf_count_t i = 1; i < frames && bit_pos < total_bits; ++i, ++bit_pos)
            {
                work[i] = qim_embed(work[i], bits[bit_pos], DELTA);
            }

            // IDCT-III
            fftw_plan ip = fftw_plan_r2r_1d((int)frames, work, work, FFTW_REDFT01, FFTW_ESTIMATE);
            fftw_execute(ip);
            fftw_destroy_plan(ip);

            // Normalizar (FFTW: DCT-II+III ~ 2N * identidad). Escalar y clamping a [-1,1].
            double scale = 1.0 / (2.0 * (double)frames);
            for (sf_count_t i = 0; i < frames; ++i)
            {
                double s = work[i] * scale;
                s = clamp_double(s, -1.0, 1.0);
                framebuf[i * ch + c] = (float)s;
            }
        }

        sf_writef_float(out, framebuf, frames);
    }

    if (bit_pos < total_bits)
    {
        fprintf(stderr, "Aviso: no se pudieron insertar todos los bits (%llu/%llu). (Revisa capacidad)\n",
                (unsigned long long)bit_pos, (unsigned long long)total_bits);
    }
    else
    {
        printf("Mensaje ocultado correctamente en: %s\n", outfile);
        calculate_metrics_audio(infile, outfile, "./out/audio_metrics.txt");
    }

    free(framebuf);
    free(work);
    free(bits);
    sf_close(in);
    sf_close(out);
}

// --------- EXTRACT (con descifrado) ---------
void extract_message(const char *infile, const char *password)
{
    if (sodium_init() < 0)
    {
        fprintf(stderr, "Error iniciando libsodium\n");
        return;
    }

    SF_INFO sfinfo = {0};
    SNDFILE *in = sf_open(infile, SFM_READ, &sfinfo);
    if (!in)
    {
        fprintf(stderr, "Error abriendo entrada: %s\n", infile);
        return;
    }

    // Calculamos capacidad para reservar exacto
    uint64_t capacity_bits = compute_capacity_bits(sfinfo.frames, sfinfo.channels);
    if (capacity_bits < 32)
    {
        fprintf(stderr, "El audio no contiene ni la cabecera completa.\n");
        sf_close(in);
        return;
    }

    int *bits = (int *)malloc((size_t)capacity_bits * sizeof(int));
    if (!bits)
    {
        fprintf(stderr, "Memoria insuficiente.\n");
        sf_close(in);
        return;
    }

    size_t ch = (size_t)sfinfo.channels;
    float *framebuf = (float *)malloc(sizeof(float) * BLOCK_SIZE * ch);
    double *work = (double *)malloc(sizeof(double) * BLOCK_SIZE);
    if (!framebuf || !work)
    {
        fprintf(stderr, "Memoria insuficiente.\n");
        free(bits);
        if (framebuf)
            free(framebuf);
        if (work)
            free(work);
        sf_close(in);
        return;
    }

    // Leemos todas las posiciones QIM en el mismo orden que en embed
    sf_count_t frames;
    uint64_t pos = 0;
    while ((frames = sf_readf_float(in, framebuf, BLOCK_SIZE)) > 0)
    {
        for (size_t c = 0; c < ch; ++c)
        {
            // Copiar canal c -> work
            for (sf_count_t i = 0; i < frames; ++i)
                work[i] = framebuf[i * ch + c];

            // DCT-II
            fftw_plan p = fftw_plan_r2r_1d((int)frames, work, work, FFTW_REDFT10, FFTW_ESTIMATE);
            fftw_execute(p);
            fftw_destroy_plan(p);

            // Extraer bits (coef 1..frames-1)
            for (sf_count_t i = 1; i < frames; ++i)
            {
                if (pos < capacity_bits)
                {
                    bits[pos++] = qim_extract(work[i], DELTA);
                }
            }
        }
    }

    // Primero 4 bytes -> longitud del blob cifrado (big-endian)
    if (pos < 32)
    {
        fprintf(stderr, "No hay suficientes bits para la longitud.\n");
        free(bits);
        free(framebuf);
        free(work);
        sf_close(in);
        return;
    }
    unsigned char len_bytes[4] = {0, 0, 0, 0};
    for (int bi = 0; bi < 4; ++bi)
    {
        unsigned char byte = 0;
        for (int b = 0; b < 8; ++b)
        {
            byte = (unsigned char)((byte << 1) | (bits[bi * 8 + b] & 1));
        }
        len_bytes[bi] = byte;
    }
    uint32_t enc_len = be_to_u32(len_bytes);
    uint64_t needed_bits = 32ULL + (uint64_t)enc_len * 8ULL;

    if (needed_bits > pos)
    {
        fprintf(stderr, "La longitud indica %u bytes, pero solo hay %llu bits útiles.\n",
                enc_len, (unsigned long long)pos);
        free(bits);
        free(framebuf);
        free(work);
        sf_close(in);
        return;
    }

    // Reconstruir blob cifrado
    unsigned char *enc = (unsigned char *)malloc(enc_len);
    if (!enc)
    {
        fprintf(stderr, "Memoria insuficiente.\n");
        free(bits);
        free(framebuf);
        free(work);
        sf_close(in);
        return;
    }
    for (uint32_t i = 0; i < enc_len; ++i)
    {
        unsigned char byte = 0;
        uint64_t base = 32ULL + (uint64_t)i * 8ULL;
        for (int b = 0; b < 8; ++b)
        {
            byte = (unsigned char)((byte << 1) | (bits[base + b] & 1));
        }
        enc[i] = byte;
    }

    if (enc_len < SALT_LEN + NONCE_LEN + ABYTES)
    {
        fprintf(stderr, "Payload demasiado corto para contener salt/nonce/cipher.\n");
        free(enc);
        free(bits);
        free(framebuf);
        free(work);
        sf_close(in);
        return;
    }

    unsigned char *salt = enc;
    unsigned char *nonce = enc + SALT_LEN;
    unsigned char *ct = enc + SALT_LEN + NONCE_LEN;
    uint32_t ct_len = enc_len - SALT_LEN - NONCE_LEN;

    unsigned char key[KEY_LEN];
    if (crypto_pwhash(key, KEY_LEN,
                      password, strlen(password), salt,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_DEFAULT) != 0)
    {
        fprintf(stderr, "Error derivando clave.\n");
        free(enc);
        free(bits);
        free(framebuf);
        free(work);
        sf_close(in);
        return;
    }

    unsigned char *plain = (unsigned char *)malloc(ct_len); // suficiente
    if (!plain)
    {
        fprintf(stderr, "Memoria insuficiente.\n");
        free(enc);
        free(bits);
        free(framebuf);
        free(work);
        sf_close(in);
        return;
    }

    unsigned long long plain_len = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            plain, &plain_len, NULL,
            ct, ct_len,
            NULL, 0,
            nonce, key) != 0)
    {
        fprintf(stderr, "Contraseña incorrecta o datos dañados (falló autenticación AEAD).\n");
        free(plain);
        free(enc);
        free(bits);
        free(framebuf);
        free(work);
        sf_close(in);
        return;
    }

    // Asegura terminación si era texto
    if (plain_len == 0 || plain[plain_len - 1] != '\0')
    {
        // Para imprimir seguro, añadimos '\0' (sin asumir que el mensaje original lo llevaba)
        unsigned char *tmp = realloc(plain, (size_t)plain_len + 1);
        if (tmp)
        {
            plain = tmp;
            plain[plain_len] = '\0';
        }
        else
        {
            plain[plain_len - 1] = '\0';
        } // peor caso
    }

    mkdir("out", 0777);

const char *out_path = "./out/audio_extract.bin";
if (escribirArchivoBin(out_path, plain, (size_t)plain_len) == 0) {
    printf("Mensaje descifrado guardado en: %s (%llu bytes)\n",
           out_path, (unsigned long long)plain_len);
} else {
    fprintf(stderr, "No se pudo guardar el mensaje en %s\n", out_path);
}


    free(plain);
    free(enc);
    free(bits);
    free(framebuf);
    free(work);
    sf_close(in);
}

// ============ MÉTRICAS: MSE / PSNR / SNR ============

static double mse_to_psnr(double mse, double peak)
{
    if (mse <= 0.0)
        return INFINITY; // idéntico => PSNR infinita
    return 10.0 * log10((peak * peak) / mse);
}

// Extrae SOLO la longitud (los 4 bytes de cabecera) del estego, reutilizando QIM.
// Devuelve 1 si OK y deja msg_len en out; 0 si error.
static int read_embedded_length(const char *stego_file, uint32_t *out_len, int block_size, double delta)
{
    SF_INFO sfinfo = {0};
    SNDFILE *in = sf_open(stego_file, SFM_READ, &sfinfo);
    if (!in)
        return 0;

    const int need_bits = 32;
    uint8_t len_bytes[4] = {0, 0, 0, 0};
    int *bits = (int *)malloc(need_bits * sizeof(int));
    if (!bits)
    {
        sf_close(in);
        return 0;
    }

    size_t ch = (size_t)sfinfo.channels;
    float *framebuf = (float *)malloc(sizeof(float) * block_size * ch);
    double *work = (double *)malloc(sizeof(double) * block_size);
    if (!framebuf || !work)
    {
        free(bits);
        if (framebuf)
            free(framebuf);
        if (work)
            free(work);
        sf_close(in);
        return 0;
    }

    sf_count_t frames;
    int pos = 0;
    while ((frames = sf_readf_float(in, framebuf, block_size)) > 0 && pos < need_bits)
    {
        for (size_t c = 0; c < ch && pos < need_bits; ++c)
        {
            for (sf_count_t i = 0; i < frames; ++i)
                work[i] = framebuf[i * ch + c];

            fftw_plan p = fftw_plan_r2r_1d((int)frames, work, work, FFTW_REDFT10, FFTW_ESTIMATE);
            fftw_execute(p);
            fftw_destroy_plan(p);

            for (sf_count_t i = 1; i < frames && pos < need_bits; ++i)
                bits[pos++] = qim_extract(work[i], delta);
        }
    }

    if (pos < need_bits)
    {
        free(bits);
        free(framebuf);
        free(work);
        sf_close(in);
        return 0;
    }

    for (int bi = 0; bi < 4; ++bi)
    {
        uint8_t byte = 0;
        for (int b = 0; b < 8; ++b)
            byte = (byte << 1) | (bits[bi * 8 + b] & 1);
        len_bytes[bi] = byte;
    }

    free(bits);
    free(framebuf);
    free(work);
    sf_close(in);

    *out_len = ((uint32_t)len_bytes[0] << 24) | ((uint32_t)len_bytes[1] << 16) |
               ((uint32_t)len_bytes[2] << 8) | (uint32_t)len_bytes[3];
    return 1;
}

static inline double psnr_c(double mse, double peak)
{
    return (mse <= 0.0) ? INFINITY : 10.0 * log10((peak * peak) / mse);
}

static inline double snr_c(double sig2, double err2)
{
    return (err2 > 0.0) ? 10.0 * log10(sig2 / err2) : INFINITY;
}

// Calcula cuántos FRAMES quedaron realmente “tocados” por DCT/IDCT (bloques hasta agotar bits)
static sf_count_t compute_modified_frames(sf_count_t total_frames, int channels, uint64_t total_bits, int block_size)
{
    if (total_bits == 0)
        return 0;
    sf_count_t full_blocks = total_frames / block_size;
    sf_count_t last = total_frames % block_size;

    uint64_t per_full_cap = (uint64_t)channels * (uint64_t)(block_size - 1);
    sf_count_t frames_mod = 0;

    uint64_t remaining = total_bits;

    for (sf_count_t b = 0; b < full_blocks && remaining > 0; ++b)
    {
        frames_mod += block_size;
        if (remaining > per_full_cap)
            remaining -= per_full_cap;
        else
            remaining = 0;
    }

    if (last > 0 && remaining > 0)
    {
        uint64_t last_cap = (last > 1) ? (uint64_t)channels * (uint64_t)(last - 1) : 0;
        if (last_cap > 0)
        {
            frames_mod += last;
        }
    }

    if (frames_mod > total_frames)
        frames_mod = total_frames;
    return frames_mod;
}

void calculate_metrics_audio(const char *orig_file, const char *stego_file, const char *output_path)
{

    SF_INFO a = {0}, b = {0};
    SNDFILE *orig = sf_open(orig_file, SFM_READ, &a);
    SNDFILE *stego = sf_open(stego_file, SFM_READ, &b);
    if (!orig || !stego)
    {
        fprintf(stderr, "No se pudieron abrir los archivos para métricas.\n");
        if (orig)
            sf_close(orig);
        if (stego)
            sf_close(stego);
        return;
    }

    int channels = (a.channels < b.channels) ? a.channels : b.channels;
    if (channels <= 0)
    {
        fprintf(stderr, "Canales inválidos.\n");
        sf_close(orig);
        sf_close(stego);
        return;
    }

    sf_count_t total_frames = (a.frames < b.frames) ? a.frames : b.frames;
    if (total_frames <= 0)
    {
        fprintf(stderr, "Sin frames para comparar.\n");
        sf_close(orig);
        sf_close(stego);
        return;
    }

    // Leer longitud embebida para estimar qué frames quedaron modificados
    uint32_t enc_len = 0;
    if (!read_embedded_length(stego_file, &enc_len, BLOCK_SIZE, DELTA))
    {
        fprintf(stderr, "No pude leer la longitud embebida del estego.\n");
        sf_close(orig);
        sf_close(stego);
        return;
    }
    uint64_t total_bits = 32ULL + (uint64_t)enc_len * 8ULL;
    sf_count_t modified_frames = compute_modified_frames(total_frames, channels, total_bits, BLOCK_SIZE);
    sf_count_t unmodified_frames = (modified_frames < total_frames) ? (total_frames - modified_frames) : 0;

    // Acumuladores
    size_t ch = (size_t)channels;
    double *sum_err2_all = (double *)calloc(ch, sizeof(double));
    double *sum_sig2_all = (double *)calloc(ch, sizeof(double));
    double *sum_err2_mod = (double *)calloc(ch, sizeof(double));
    double *sum_sig2_mod = (double *)calloc(ch, sizeof(double));
    if (!sum_err2_all || !sum_sig2_all || !sum_err2_mod || !sum_sig2_mod)
    {
        fprintf(stderr, "Memoria insuficiente (metrics).\n");
        if (sum_err2_all)
            free(sum_err2_all);
        if (sum_sig2_all)
            free(sum_sig2_all);
        if (sum_err2_mod)
            free(sum_err2_mod);
        if (sum_sig2_mod)
            free(sum_sig2_mod);
        sf_close(orig);
        sf_close(stego);
        return;
    }

    float *bufA = (float *)malloc(sizeof(float) * BLOCK_SIZE * ch);
    float *bufB = (float *)malloc(sizeof(float) * BLOCK_SIZE * ch);
    if (!bufA || !bufB)
    {
        fprintf(stderr, "Memoria insuficiente (buffers).\n");
        if (bufA)
            free(bufA);
        if (bufB)
            free(bufB);
        free(sum_err2_all);
        free(sum_sig2_all);
        free(sum_err2_mod);
        free(sum_sig2_mod);
        sf_close(orig);
        sf_close(stego);
        return;
    }

    // Recorrer y acumular
    sf_seek(orig, 0, SEEK_SET);
    sf_seek(stego, 0, SEEK_SET);
    sf_count_t frames_read, done_frames = 0;
    const double peak = 1.0; // libsndfile como float en [-1,1]

    while ((frames_read = sf_readf_float(orig, bufA, BLOCK_SIZE)) > 0)
    {
        sf_count_t frames_read_b = sf_readf_float(stego, bufB, frames_read);
        if (frames_read_b < frames_read)
            frames_read = frames_read_b;

        sf_count_t mod_left = (modified_frames > done_frames) ? (modified_frames - done_frames) : 0;
        sf_count_t mod_here = (mod_left > frames_read) ? frames_read : mod_left;

        for (sf_count_t i = 0; i < frames_read; ++i)
        {
            for (int c = 0; c < channels; ++c)
            {
                double x = bufA[i * channels + c];
                double y = bufB[i * channels + c];
                double e = x - y;
                sum_err2_all[c] += e * e;
                sum_sig2_all[c] += x * x;
                if (i < mod_here)
                {
                    sum_err2_mod[c] += e * e;
                    sum_sig2_mod[c] += x * x;
                }
            }
        }
        done_frames += frames_read;
        if (done_frames >= total_frames)
            break;
    }

    // Calcula también "no modificados" por diferencia
    double *sum_err2_unm = (double *)calloc(ch, sizeof(double));
    double *sum_sig2_unm = (double *)calloc(ch, sizeof(double));
    if (!sum_err2_unm || !sum_sig2_unm)
    {
        fprintf(stderr, "Memoria insuficiente (unmodified).\n");
        if (sum_err2_unm)
            free(sum_err2_unm);
        if (sum_sig2_unm)
            free(sum_sig2_unm);
        free(bufA);
        free(bufB);
        free(sum_err2_all);
        free(sum_sig2_all);
        free(sum_err2_mod);
        free(sum_sig2_mod);
        sf_close(orig);
        sf_close(stego);
        return;
    }
    for (int c = 0; c < channels; ++c)
    {
        sum_err2_unm[c] = sum_err2_all[c] - sum_err2_mod[c];
        sum_sig2_unm[c] = sum_sig2_all[c] - sum_sig2_mod[c];
    }

    // Abrir archivo de salida
    FILE *f = fopen(output_path, "w");
    if (!f)
    {
        perror("fopen(metrics)");
        free(sum_err2_unm);
        free(sum_sig2_unm);
        free(bufA);
        free(bufB);
        free(sum_err2_all);
        free(sum_sig2_all);
        free(sum_err2_mod);
        free(sum_sig2_mod);
        sf_close(orig);
        sf_close(stego);
        return;
    }

    // Encabezado
    fprintf(f, "== METRICAS AUDIO ==\n");
    fprintf(f, "Archivo original : %s\n", orig_file);
    fprintf(f, "Archivo estego   : %s\n", stego_file);
    fprintf(f, "Canales: %d   Frames comparados: %lld\n", channels, (long long)total_frames);
    fprintf(f, "Frames modificados: %lld   (no modificados: %lld)\n",
            (long long)modified_frames, (long long)unmodified_frames);
    fprintf(f, "Payload embebido (cifrado): %u bytes   Bits totales: %llu\n\n",
            enc_len, (unsigned long long)total_bits);

    // Sección: TODOS
    fprintf(f, "==== TODOS LOS FRAMES ====\n");
    double sum_err_all_tot = 0.0, sum_sig_all_tot = 0.0;
    for (int c = 0; c < channels; ++c)
    {
        double mse = sum_err2_all[c] / (double)total_frames;
        double ps = psnr_c(mse, peak);
        double sr = snr_c(sum_sig2_all[c], sum_err2_all[c]);
        fprintf(f, "Canal %d:  MSE=%.10g   PSNR=%.2f dB   SNR=%.2f dB\n", c, mse, ps, sr);
        sum_err_all_tot += sum_err2_all[c];
        sum_sig_all_tot += sum_sig2_all[c];
    }
    double mse_all_aggr = sum_err_all_tot / (double)(total_frames * channels);
    double psnr_all_aggr = psnr_c(mse_all_aggr, peak);
    double snr_all_aggr = snr_c(sum_sig_all_tot, sum_err_all_tot);
    fprintf(f, "TOTAL:    MSE=%.10g   PSNR=%.2f dB   SNR=%.2f dB\n\n", mse_all_aggr, psnr_all_aggr, snr_all_aggr);

    // Sección: SOLO MODIFICADOS
    fprintf(f, "==== SOLO FRAMES MODIFICADOS ====\n");
    double sum_err_mod_tot = 0.0, sum_sig_mod_tot = 0.0;
    for (int c = 0; c < channels; ++c)
    {
        double mse = (modified_frames > 0) ? (sum_err2_mod[c] / (double)modified_frames) : 0.0;
        double ps = psnr_c(mse, peak);
        double sr = snr_c(sum_sig2_mod[c], sum_err2_mod[c]);
        fprintf(f, "Canal %d:  MSE=%.10g   PSNR=%s%.2f dB   SNR=%s%.2f dB\n", c, mse,
                (modified_frames == 0 ? "(n/d) " : ""), ps,
                (modified_frames == 0 ? "(n/d) " : ""), sr);
        sum_err_mod_tot += sum_err2_mod[c];
        sum_sig_mod_tot += sum_sig2_mod[c];
    }
    if (modified_frames > 0)
    {
        double mse_mod_aggr = sum_err_mod_tot / (double)(modified_frames * channels);
        double psnr_mod_aggr = psnr_c(mse_mod_aggr, peak);
        double snr_mod_aggr = snr_c(sum_sig_mod_tot, sum_err_mod_tot);
        fprintf(f, "TOTAL:    MSE=%.10g   PSNR=%.2f dB   SNR=%.2f dB\n\n", mse_mod_aggr, psnr_mod_aggr, snr_mod_aggr);
    }
    else
    {
        fprintf(f, "TOTAL:    (sin frames modificados)\n\n");
    }

    // Sección: NO MODIFICADOS
    fprintf(f, "==== FRAMES NO MODIFICADOS ====\n");
    double sum_err_unm_tot = 0.0, sum_sig_unm_tot = 0.0;
    for (int c = 0; c < channels; ++c)
    {
        double mse = (unmodified_frames > 0) ? (sum_err2_unm[c] / (double)unmodified_frames) : 0.0;
        double ps = psnr_c(mse, peak); // normalmente infinito si mse≈0
        double sr = snr_c(sum_sig2_unm[c], sum_err2_unm[c]);
        fprintf(f, "Canal %d:  MSE=%.10g   PSNR=%.2f dB   SNR=%.2f dB   (frames=%lld)\n",
                c, mse, ps, sr, (long long)unmodified_frames);
        sum_err_unm_tot += sum_err2_unm[c];
        sum_sig_unm_tot += sum_sig2_unm[c];
    }
    if (unmodified_frames > 0)
    {
        double mse_unm_aggr = sum_err_unm_tot / (double)(unmodified_frames * channels);
        double psnr_unm_aggr = psnr_c(mse_unm_aggr, peak);
        double snr_unm_aggr = snr_c(sum_sig_unm_tot, sum_err_unm_tot);
        fprintf(f, "TOTAL:    MSE=%.10g   PSNR=%.2f dB   SNR=%.2f dB\n", mse_unm_aggr, psnr_unm_aggr, snr_unm_aggr);
    }
    else
    {
        fprintf(f, "TOTAL:    (sin frames no modificados)\n");
    }

    fclose(f);

    // liberar
    free(sum_err2_unm);
    free(sum_sig2_unm);
    free(bufA);
    free(bufB);
    free(sum_err2_all);
    free(sum_sig2_all);
    free(sum_err2_mod);
    free(sum_sig2_mod);
    sf_close(orig);
    sf_close(stego);
}