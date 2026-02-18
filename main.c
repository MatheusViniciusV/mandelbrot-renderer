#include "platform.h"

#include <math.h>
#include <stdbool.h>
#include <omp.h> // Paralelismo
#include <immintrin.h> // AVX2

#define MAX_ITERATIONS 256

static u32 ColorPalette[MAX_ITERATIONS + 1];
static bool IsPaletteInitialized = false;

void InitColorPalette(void)
{
    if (IsPaletteInitialized) return;

    for (int i = 0; i < MAX_ITERATIONS; ++i)
    {
        // Truque matemático para criar um gradiente suave
        float t = (float)i / (float)MAX_ITERATIONS;
        
        // Usando ondas senoidais para gerar RGB baseados na iteração
        u8 r = (u8)(9 * (1 - t) * t * t * 255);
        u8 g = (u8)(15 * (1 - t) * (1 - t) * t * 255);
        u8 b = (u8)(8.5 * (1 - t) * (1 - t) * (1 - t) * 255);
        ColorPalette[i] = ((u32)0xFF << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
    }

    // Os pontos do conjunto são pretos
    ColorPalette[MAX_ITERATIONS] = 0xFF000000;
    IsPaletteInitialized = true;
}

// Função principal de renderização usando AVX2
void RenderMandelbrotAVX2(OffscreenBuffer *buffer, f32 center_x, f32 center_y, f32 zoom)
{
    if (!IsPaletteInitialized) InitColorPalette();

    int width = buffer->width;
    int height = buffer->height;
    u32 *pixels = (u32 *)buffer->memory;

    f32 start_x = center_x - (width / 2.0f) * zoom;
    f32 start_y = center_y - (height / 2.0f) * zoom;

    const __m256 v_threshold = _mm256_set1_ps(4.0f);
    const __m256 v_two = _mm256_set1_ps(2.0f);
    const __m256 v_zoom_x = _mm256_set1_ps(zoom);
    const __m256 v_x_offsets = _mm256_mul_ps(_mm256_setr_ps(0, 1, 2, 3, 4, 5, 6, 7), v_zoom_x);

    // OpenMP divide as linhas entre os núcleos
    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < height; ++y) {
        u32 *row_pixel = pixels + (y * (buffer->pitch / 4));
        f32 c_im_scalar = start_y + y * zoom;
        __m256 v_c_im = _mm256_set1_ps(c_im_scalar);
        
        int iter_counts[8] __attribute__((aligned(32)));

        int x = 0;
        // Loop principal AVX
        for (; x <= width - 8; x += 8)
        {
            f32 base_c_re = start_x + x * zoom;
            __m256 v_c_re = _mm256_add_ps(_mm256_set1_ps(base_c_re), v_x_offsets);
            __m256 v_z_re = _mm256_setzero_ps();
            __m256 v_z_im = _mm256_setzero_ps();
            __m256i v_iterations = _mm256_setzero_si256();

            // Loop de iteração do Mandelbrot
            for (int i = 0; i < MAX_ITERATIONS; ++i)
            {
                /*
                A fórmula do Mandelbrot é:
                    Z_{n+1} = Z_n^2 + C
                Expandindo Z = x + yi:
                    Z^2 = (x + yi)(x + yi) = x^2 - y^2 + 2xyi
                A verificação de escape (para otimização) é:
                    |Z| <= 2, isto é, raiz(x^2 + y^2) <= 2,
                ou melhor ainda:
                    x^2 + y^2 <= 4
                */

                __m256 v_z_re2 = _mm256_mul_ps(v_z_re, v_z_re); // Z_re^2
                __m256 v_z_im2 = _mm256_mul_ps(v_z_im, v_z_im); // Z_im^2
                __m256 v_mag2 = _mm256_add_ps(v_z_re2, v_z_im2); // mag^2 = re^2 + im^2

                // Cria uma máscara
                __m256 v_mask_active = _mm256_cmp_ps(v_mag2, v_threshold, _CMP_LE_OQ);

                // Se a máscara for toda zero, todos os pixels escaparam
                int mask_bits = _mm256_movemask_ps(v_mask_active);
                if (mask_bits == 0) break;

                // A máscara 'v_mask_active' tem -1 para pixels ativos
                v_iterations = _mm256_sub_epi32(v_iterations, _mm256_castps_si256(v_mask_active));

                __m256 v_new_re = _mm256_add_ps(_mm256_sub_ps(v_z_re2, v_z_im2), v_c_re);
                __m256 v_new_im = _mm256_add_ps(_mm256_mul_ps(v_two, _mm256_mul_ps(v_z_re, v_z_im)), v_c_im);

                // _mm256_blendv_ps seleciona o segundo argumento se a máscara for true
                v_z_re = _mm256_blendv_ps(v_z_re, v_new_re, v_mask_active);
                v_z_im = _mm256_blendv_ps(v_z_im, v_new_im, v_mask_active);
            }

            // Uso do 'storeu' previne crash se o GCC falhar no alinhamento da thread
            _mm256_storeu_si256((__m256i*)iter_counts, v_iterations);

            // // Loop escalar curto para aplicar as cores nos 8 pixels
            for (int k = 0; k < 8; ++k)
            {
                row_pixel[x + k] = ColorPalette[iter_counts[k]];
            }
        }

        // Processa os pixels restantes se a largura não for múltipla de 8
        for (; x < width; ++x)
        {
            f32 c_re = start_x + x * zoom;
            f32 z_re = 0, z_im = 0, z_re2 = 0, z_im2 = 0;
            int iteration = 0;
            while (z_re2 + z_im2 <= 4.0f && iteration < MAX_ITERATIONS)
            {
                z_im = 2 * z_re * z_im + c_im_scalar;
                z_re = z_re2 - z_im2 + c_re;
                z_re2 = z_re * z_re;
                z_im2 = z_im * z_im;
                iteration++;
            }
            row_pixel[x] = ColorPalette[iteration];
        }
    }
}

void UpdateAndRender(Input *input, OffscreenBuffer *buffer)
{
    static f32 center_x = -0.75f;
    static f32 center_y = 0.0f;
    static f32 zoom = 0.004f;

    f32 zoom_factor = 1.0f;
    if (input->keys[KEY_PLUS].is_ended_down) zoom_factor = 0.92f; // Zoom in
    if (input->keys[KEY_MINUS].is_ended_down) zoom_factor = 1.087f; // Zoom out
    zoom *= zoom_factor;

    f32 move_speed = 10.0f * zoom; 
    if (input->keys[KEY_RIGHT].is_ended_down) center_x += move_speed;
    if (input->keys[KEY_LEFT].is_ended_down)  center_x -= move_speed;
    if (input->keys[KEY_DOWN].is_ended_down)  center_y += move_speed;
    if (input->keys[KEY_UP].is_ended_down)    center_y -= move_speed;

    RenderMandelbrotAVX2(buffer, center_x, center_y, zoom);
}