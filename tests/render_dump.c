// Renderiza a cena de teste no host e grava PPMs (valida o renderizador sem PS2).
#include <stdio.h>
#include <stdlib.h>
#include "render.h"
#include "scene_test.h"

static void Dump(const char *path, const uint32_t *fb, int stride)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n240 160\n255\n");
    for (int y = 0; y < 160; y++) for (int x = 0; x < 240; x++) {
        uint32_t p = fb[y * stride + x];
        fputc(p & 255, f); fputc((p >> 8) & 255, f); fputc((p >> 16) & 255, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    static uint32_t fb[256 * 256];
    SceneInit();
    int want[] = { 0, 120, 400 };
    int next = 0;
    for (int t = 0; t <= 400; t++) {
        SceneUpdate(t);
        if (t == want[next]) {
            char name[64]; snprintf(name, sizeof name, "/tmp/frame_%03d.ppm", t);
            RenderFrame(fb, 256); Dump(name, fb, 256); next++;
        }
    }
    puts("ok");
    return 0;
}
