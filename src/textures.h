#ifndef TEXTURES_H
#define TEXTURES_H

#include <gsKit.h>

// Texturas 16x16 detalhadas para corredores, salas de aula com cartazes/lousa e portas escolares
static inline u64 get_wall_texel(int tile, int u, int v, int shade) {
    u &= 15;
    v &= 15;
    if (tile == 1) {
        // Corredor: parede branca de tijolos com faixa azul no rodape e cartaz educativo ocasional
        if (v >= 13) {
            return GS_SETREG_RGBAQ((int)(shade * 0.14f), (int)(shade * 0.34f), shade, 0x80, 0x00);
        }
        if (v == 12) {
            return GS_SETREG_RGBAQ((int)(shade * 0.10f), (int)(shade * 0.22f), (int)(shade * 0.70f), 0x80, 0x00);
        }
        int brick_edge = ((v % 4 == 0) || (((u + (v / 4) * 4) % 8) == 0));
        int s = brick_edge ? (int)(shade * 0.80f) : shade;
        return GS_SETREG_RGBAQ(s, s, s, 0x80, 0x00);
    } else if (tile == 2) {
        // Sala de aula: parede amarela com lousa verde escolar e giz branco
        if (u >= 2 && u <= 13 && v >= 3 && v <= 11) {
            if (u == 2 || u == 13 || v == 3 || v == 11) {
                return GS_SETREG_RGBAQ((int)(shade * 0.55f), (int)(shade * 0.32f), (int)(shade * 0.12f), 0x80, 0x00);
            }
            // Rabiscos de giz branco na lousa
            if ((v == 5 && (u == 4 || u == 5 || u == 7 || u == 9)) ||
                (v == 7 && (u >= 4 && u <= 10 && (u % 2 == 0)))) {
                return GS_SETREG_RGBAQ(shade, shade, shade, 0x80, 0x00);
            }
            return GS_SETREG_RGBAQ((int)(shade * 0.10f), (int)(shade * 0.48f), (int)(shade * 0.18f), 0x80, 0x00);
        }
        return GS_SETREG_RGBAQ(shade, (int)(shade * 0.90f), (int)(shade * 0.38f), 0x80, 0x00);
    } else if (tile == 3) {
        // Porta azul escolar com visor de vidro na parte superior e macaneta dourada
        if (u >= 5 && u <= 10 && v >= 2 && v <= 5) {
            return GS_SETREG_RGBAQ((int)(shade * 0.65f), (int)(shade * 0.85f), shade, 0x80, 0x00);
        }
        if (u >= 11 && u <= 13 && v >= 8 && v <= 9) {
            return GS_SETREG_RGBAQ(shade, (int)(shade * 0.85f), 0x10, 0x80, 0x00);
        }
        int panel = (u >= 3 && u <= 12 && v >= 7 && v <= 13);
        int s = panel ? (int)(shade * 0.82f) : shade;
        return GS_SETREG_RGBAQ((int)(s * 0.16f), (int)(s * 0.40f), s, 0x80, 0x00);
    }
    return GS_SETREG_RGBAQ(shade, 0x20, 0x20, 0x80, 0x00);
}

#endif
