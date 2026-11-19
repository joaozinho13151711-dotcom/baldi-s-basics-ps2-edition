#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "map.h"
#include "font.h"
#include "audio.h"
#include "textures.h"
#include "sprites.h"

#define SCREEN_W 640
#define SCREEN_H 448
#define TOTAL_NOTEBOOKS 7
#define PI 3.14159265f

typedef enum {
    STATE_TITLE = 0,
    STATE_PLAYING,
    STATE_MATH_PAD,
    STATE_JUMPROPE,
    STATE_GAMEOVER,
    STATE_VICTORY
} GameState;

typedef struct {
    float x, y, angle;
    float stamina;
    int notebooks;
    int anger;
    int item_slot; // 0 = vazio, 1 = BSODA, 2 = TESOURA, 3 = BARRA ZESTY
    int jump_count;
    float rope_pos;
    float jump_z;
    float jump_vz;
    int detention_timer;
} Player;

typedef struct {
    float x, y;
    float step_timer;
    float stun_timer;
} Baldi;

typedef struct {
    float x, y;
    float vx, vy;
    float cooldown;
} Playtime;

typedef struct {
    float x, y;
    float vx, vy;
    float cooldown;
} Principal;

typedef struct {
    float x, y;
    float vx, vy;
    int active;
} SodaWave;

typedef struct {
    int problem_num;
    int a, b, op;
    int options[3];
    int correct_idx;
    int selected_idx;
    int impossible;
} MathQuestion;

static int school_map[MAP_H][MAP_W];
static float z_buffer[SCREEN_W];
static char padBuf[256] __attribute__((aligned(64)));

static int is_solid(int tile) {
    return (tile == 1 || tile == 2);
}

static int can_move_to(float nx, float ny, float radius) {
    int min_x = (int)(nx - radius);
    int max_x = (int)(nx + radius);
    int min_y = (int)(ny - radius);
    int max_y = (int)(ny + radius);
    if (min_x < 0 || max_x >= MAP_W || min_y < 0 || max_y >= MAP_H) return 0;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            if (is_solid(school_map[y][x])) return 0;
        }
    }
    return 1;
}

static void generate_math_question(MathQuestion *q, int prob_num, int notebook_count) {
    q->problem_num = prob_num;
    q->selected_idx = 0;
    if (prob_num == 3 && notebook_count >= 1) {
        q->impossible = 1;
        q->a = 999;
        q->b = 777;
        q->op = 0;
        q->options[0] = 42;
        q->options[1] = 108;
        q->options[2] = 0;
        q->correct_idx = -1;
        return;
    }
    q->impossible = 0;
    q->a = (rand() % 9) + 1;
    q->b = (rand() % 9) + 1;
    q->op = rand() % 2;
    int ans = (q->op == 0) ? (q->a + q->b) : (q->a - q->b);
    q->correct_idx = rand() % 3;
    for (int i = 0; i < 3; i++) {
        if (i == q->correct_idx) q->options[i] = ans;
        else q->options[i] = ans + ((i == 0) ? -2 : 3);
    }
}

static void start_new_game(Player *p, Baldi *b, Playtime *pt, Principal *pr, SodaWave *soda) {
    memcpy(school_map, INITIAL_SCHOOL_MAP, sizeof(school_map));
    p->x = 2.5f;
    p->y = 2.5f;
    p->angle = 0.0f;
    p->stamina = 100.0f;
    p->notebooks = 0;
    p->anger = 0;
    p->item_slot = 0;
    p->jump_count = 0;
    p->rope_pos = 0.0f;
    p->jump_z = 0.0f;
    p->jump_vz = 0.0f;
    p->detention_timer = 0;

    b->x = 17.5f;
    b->y = 17.5f;
    b->step_timer = 0.0f;
    b->stun_timer = 0.0f;

    pt->x = 12.5f;
    pt->y = 6.5f;
    pt->vx = 0.03f;
    pt->vy = 0.0f;
    pt->cooldown = 0.0f;

    pr->x = 7.5f;
    pr->y = 11.5f;
    pr->vx = 0.0f;
    pr->vy = 0.028f;
    pr->cooldown = 0.0f;

    soda->active = 0;
}

static void render_char_pixel_art(GSGLOBAL *gs, Player *p, float sx, float sy, const unsigned char sprite[24][16]) {
    float dx = sx - p->x;
    float dy = sy - p->y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.25f || dist > 16.0f) return;

    float fov = PI / 3.0f;
    float ang = atan2f(dy, dx) - p->angle;
    while (ang >  PI) ang -= 2.0f * PI;
    while (ang < -PI) ang += 2.0f * PI;
    if (fabsf(ang) > fov * 0.65f) return;

    int screen_x = (int)((0.5f + tanf(ang) / (2.0f * tanf(fov / 2.0f))) * SCREEN_W);
    if (screen_x < 0 || screen_x >= SCREEN_W || dist >= z_buffer[screen_x]) return;

    int total_h = (int)((SCREEN_H / dist) * 0.88f);
    int total_w = (int)((SCREEN_H / dist) * 0.56f);
    int x0 = screen_x - total_w / 2;
    int y0 = (SCREEN_H / 2) - total_h / 2;

    int shade_pct = (int)(100.0f / (1.0f + dist * 0.10f));
    if (shade_pct < 35) shade_pct = 35;

    for (int r = 0; r < 24; r++) {
        int py0 = y0 + (total_h * r) / 24;
        int py1 = y0 + (total_h * (r + 1)) / 24;
        for (int c = 0; c < 16; c++) {
            unsigned char idx = sprite[r][c];
            if (idx == 0) continue;
            int px0 = x0 + (total_w * c) / 16;
            int px1 = x0 + (total_w * (c + 1)) / 16;
            gsKit_prim_sprite(gs, px0, py0, px1, py1, 5, palette_color(idx, shade_pct));
        }
    }
}

static void render_item_pixel_art(GSGLOBAL *gs, Player *p, float sx, float sy, const unsigned char sprite[12][12]) {
    float dx = sx - p->x;
    float dy = sy - p->y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.25f || dist > 16.0f) return;

    float fov = PI / 3.0f;
    float ang = atan2f(dy, dx) - p->angle;
    while (ang >  PI) ang -= 2.0f * PI;
    while (ang < -PI) ang += 2.0f * PI;
    if (fabsf(ang) > fov * 0.65f) return;

    int screen_x = (int)((0.5f + tanf(ang) / (2.0f * tanf(fov / 2.0f))) * SCREEN_W);
    if (screen_x < 0 || screen_x >= SCREEN_W || dist >= z_buffer[screen_x]) return;

    int sz = (int)((SCREEN_H / dist) * 0.36f);
    int x0 = screen_x - sz / 2;
    int y0 = (SCREEN_H / 2) - sz / 2 + (int)((SCREEN_H / dist) * 0.12f);

    int shade_pct = (int)(100.0f / (1.0f + dist * 0.10f));
    if (shade_pct < 40) shade_pct = 40;

    for (int r = 0; r < 12; r++) {
        int py0 = y0 + (sz * r) / 12;
        int py1 = y0 + (sz * (r + 1)) / 12;
        for (int c = 0; c < 12; c++) {
            unsigned char idx = sprite[r][c];
            if (idx == 0) continue;
            int px0 = x0 + (sz * c) / 12;
            int px1 = x0 + (sz * (c + 1)) / 12;
            gsKit_prim_sprite(gs, px0, py0, px1, py1, 4, palette_color(idx, shade_pct));
        }
    }
}

int main(int argc, char *argv[]) {
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);

    padInit(0);
    padPortOpen(0, 0, padBuf);

    audio_init();

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    GSGLOBAL *gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Width = SCREEN_W;
    gsGlobal->Height = SCREEN_H;
    gsGlobal->PSM = GS_PSM_CT32;
    gsGlobal->DoubleBuffering = GS_SETTING_ON;

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    GameState state = STATE_TITLE;
    Player player;
    Baldi baldi;
    Playtime playtime;
    Principal principal;
    SodaWave soda;
    MathQuestion math_q;

    start_new_game(&player, &baldi, &playtime, &principal, &soda);

    struct padButtonStatus buttons;
    u32 paddata = 0, old_pad = 0, new_pad = 0;

    while (1) {
        if (padGetState(0, 0) == PAD_STATE_STABLE) {
            if (padRead(0, 0, &buttons) != 0) {
                paddata = 0xffff ^ buttons.btns;
                new_pad = paddata & ~old_pad;
                old_pad = paddata;
            }
        }

        if (state == STATE_TITLE) {
            gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x18, 0x7A, 0x3A, 0x80, 0x00));
            gsKit_prim_sprite(gsGlobal, 30, 30, 610, 418, 1, GS_SETREG_RGBAQ(0xF2, 0xEE, 0xD8, 0x80, 0x00));

            // Desenha o Baldi em pixel art grande na tela titulo
            for (int r = 0; r < 24; r++) {
                for (int c = 0; c < 16; c++) {
                    unsigned char idx = SPRITE_BALDI[r][c];
                    if (idx != 0) {
                        gsKit_prim_sprite(gsGlobal, 430 + c * 9, 105 + r * 9, 430 + (c + 1) * 9, 105 + (r + 1) * 9, 3, palette_color(idx, 100));
                    }
                }
            }

            draw_text(gsGlobal, 55, 58, 3, GS_SETREG_RGBAQ(0x10, 0x60, 0x20, 0x80, 0x00), "BALDIS BASICS PS2");
            draw_text(gsGlobal, 55, 100, 2, GS_SETREG_RGBAQ(0x20, 0x20, 0x20, 0x80, 0x00), "COLETE OS 7 CADERNOS E ESCAPE");
            draw_text(gsGlobal, 55, 150, 2, GS_SETREG_RGBAQ(0x30, 0x30, 0x30, 0x80, 0x00), "DIRECIONAL - ANDAR E GIRAR");
            draw_text(gsGlobal, 55, 180, 2, GS_SETREG_RGBAQ(0x30, 0x30, 0x30, 0x80, 0x00), "QUADRADO   - CORRER NAO CORRA PERTO DO DIRETOR");
            draw_text(gsGlobal, 55, 210, 2, GS_SETREG_RGBAQ(0x30, 0x30, 0x30, 0x80, 0x00), "CIRCULO    - USAR ITEM BSODA TESOURA ZESTY");
            draw_text(gsGlobal, 55, 240, 2, GS_SETREG_RGBAQ(0x30, 0x30, 0x30, 0x80, 0x00), "TRIANGULO  - PULAR CORDA PLAYTIME");
            draw_text(gsGlobal, 75, 340, 3, GS_SETREG_RGBAQ(0xC0, 0x15, 0x15, 0x80, 0x00), "APERTE X PARA JOGAR");

            if (new_pad & PAD_CROSS) {
                start_new_game(&player, &baldi, &playtime, &principal, &soda);
                state = STATE_PLAYING;
            }
        }
        else if (state == STATE_MATH_PAD) {
            gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x10, 0x35, 0x18, 0x80, 0x00));
            gsKit_prim_sprite(gsGlobal, 45, 35, 595, 412, 1, GS_SETREG_RGBAQ(0xE8, 0xEC, 0xE8, 0x80, 0x00));

            // Mini Baldi no canto do Pad YCTP
            for (int r = 0; r < 24; r++) {
                for (int c = 0; c < 16; c++) {
                    unsigned char idx = SPRITE_BALDI[r][c];
                    if (idx != 0) {
                        gsKit_prim_sprite(gsGlobal, 495 + c * 4, 55 + r * 4, 495 + (c + 1) * 4, 55 + (r + 1) * 4, 3, palette_color(idx, 100));
                    }
                }
            }

            char header[48];
            sprintf(header, "CADERNO %d - PERGUNTA %d DE 3", player.notebooks + 1, math_q.problem_num);
            draw_text(gsGlobal, 70, 65, 2, GS_SETREG_RGBAQ(0x10, 0x50, 0x15, 0x80, 0x00), header);

            char eq[48];
            if (math_q.impossible) sprintf(eq, "999 + 777 - 404 = ?");
            else sprintf(eq, "%d %c %d = ?", math_q.a, (math_q.op == 0) ? '+' : '-', math_q.b);
            draw_text(gsGlobal, 120, 145, 4, GS_SETREG_RGBAQ(0x15, 0x15, 0x15, 0x80, 0x00), eq);

            if (new_pad & PAD_LEFT)  math_q.selected_idx = (math_q.selected_idx + 2) % 3;
            if (new_pad & PAD_RIGHT) math_q.selected_idx = (math_q.selected_idx + 1) % 3;

            for (int i = 0; i < 3; i++) {
                int bx = 110 + i * 150;
                u64 box_col = (i == math_q.selected_idx)
                    ? GS_SETREG_RGBAQ(0x20, 0xB0, 0x40, 0x80, 0x00)
                    : GS_SETREG_RGBAQ(0x90, 0x95, 0x90, 0x80, 0x00);
                gsKit_prim_sprite(gsGlobal, bx, 235, bx + 110, 295, 2, box_col);

                char opt[16];
                sprintf(opt, "%d", math_q.options[i]);
                draw_text(gsGlobal, bx + 25, 253, 3, GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00), opt);
            }

            draw_text(gsGlobal, 110, 345, 2, GS_SETREG_RGBAQ(0x30, 0x30, 0x30, 0x80, 0x00), "ESQ/DIR SELECIONA  X CONFIRMA");

            if (new_pad & PAD_CROSS) {
                if (math_q.selected_idx == math_q.correct_idx) {
                    audio_play_ding(1);
                } else {
                    audio_play_ding(0);
                    player.anger++;
                }
                if (math_q.problem_num < 3) {
                    generate_math_question(&math_q, math_q.problem_num + 1, player.notebooks);
                } else {
                    player.notebooks++;
                    state = STATE_PLAYING;
                }
            }
        }
        else if (state == STATE_JUMPROPE) {
            gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x90, 0x18, 0x25, 0x80, 0x00));
            gsKit_prim_sprite(gsGlobal, 50, 40, 590, 408, 1, GS_SETREG_RGBAQ(0xF5, 0xE5, 0xE5, 0x80, 0x00));

            for (int r = 0; r < 24; r++) {
                for (int c = 0; c < 16; c++) {
                    unsigned char idx = SPRITE_PLAYTIME[r][c];
                    if (idx != 0) {
                        gsKit_prim_sprite(gsGlobal, 470 + c * 6, 175 + r * 6, 470 + (c + 1) * 6, 175 + (r + 1) * 6, 3, palette_color(idx, 100));
                    }
                }
            }

            char jtxt[48];
            sprintf(jtxt, "VAMOS PULAR CORDA  %d DE 5", player.jump_count);
            draw_text(gsGlobal, 80, 70, 3, GS_SETREG_RGBAQ(0x90, 0x10, 0x15, 0x80, 0x00), jtxt);
            draw_text(gsGlobal, 80, 125, 2, GS_SETREG_RGBAQ(0x20, 0x20, 0x20, 0x80, 0x00), "APERTE TRIANGULO OU X PARA PULAR");
            if (player.item_slot == 2) {
                draw_text(gsGlobal, 80, 155, 2, GS_SETREG_RGBAQ(0x10, 0x65, 0x20, 0x80, 0x00), "APERTE CIRCULO PARA CORTAR COM TESOURA");
                if (new_pad & PAD_CIRCLE) {
                    player.item_slot = 0;
                    playtime.cooldown = 360.0f;
                    audio_play_ding(1);
                    state = STATE_PLAYING;
                }
            }

            if ((new_pad & (PAD_TRIANGLE | PAD_CROSS)) && player.jump_z <= 0.0f) {
                player.jump_vz = 9.5f;
                audio_play_jump();
            }

            player.jump_z += player.jump_vz;
            player.jump_vz -= 0.65f;
            if (player.jump_z < 0.0f) { player.jump_z = 0.0f; player.jump_vz = 0.0f; }

            float prev_rope = player.rope_pos;
            player.rope_pos += 0.055f;
            if (player.rope_pos >= 1.0f) player.rope_pos -= 1.0f;

            if (prev_rope < 0.5f && player.rope_pos >= 0.5f) {
                if (player.jump_z > 12.0f) {
                    player.jump_count++;
                    audio_play_ding(1);
                    if (player.jump_count >= 5) {
                        playtime.cooldown = 300.0f;
                        state = STATE_PLAYING;
                    }
                } else {
                    player.jump_count = 0;
                    audio_play_ding(0);
                }
            }

            int py = 320 - (int)player.jump_z;
            gsKit_prim_sprite(gsGlobal, 260, py - 50, 300, py, 3, GS_SETREG_RGBAQ(0x20, 0x60, 0xD0, 0x80, 0x00));
            int rope_y = 215 + (int)(sinf(player.rope_pos * PI) * 115.0f);
            gsKit_prim_sprite(gsGlobal, 120, rope_y, 450, rope_y + 6, 4, GS_SETREG_RGBAQ(0x20, 0x20, 0x20, 0x80, 0x00));
        }
        else if (state == STATE_PLAYING) {
            if (player.detention_timer > 0) {
                player.detention_timer--;
            }

            float rot_speed = 0.048f;
            if (paddata & PAD_LEFT)  player.angle -= rot_speed;
            if (paddata & PAD_RIGHT) player.angle += rot_speed;

            int running = (paddata & PAD_SQUARE) && (player.stamina > 2.0f) && (player.detention_timer == 0);
            float move_speed = running ? 0.098f : 0.054f;

            float dir_x = cosf(player.angle);
            float dir_y = sinf(player.angle);
            float step_x = 0.0f, step_y = 0.0f;

            if (player.detention_timer == 0) {
                if (paddata & PAD_UP) {
                    step_x = dir_x * move_speed;
                    step_y = dir_y * move_speed;
                } else if (paddata & PAD_DOWN) {
                    step_x = -dir_x * move_speed * 0.7f;
                    step_y = -dir_y * move_speed * 0.7f;
                }
            }

            if (step_x != 0.0f || step_y != 0.0f) {
                if (can_move_to(player.x + step_x, player.y, 0.24f)) player.x += step_x;
                if (can_move_to(player.x, player.y + step_y, 0.24f)) player.y += step_y;

                if (running) {
                    player.stamina -= 0.78f;
                    if (player.stamina < 0.0f) player.stamina = 0.0f;
                }
            } else if (player.stamina < 100.0f) {
                player.stamina += 0.45f;
                if (player.stamina > 100.0f) player.stamina = 100.0f;
            }

            // Uso de itens (Circulo)
            if (new_pad & PAD_CIRCLE) {
                if (player.item_slot == 1) {
                    // BSODA
                    player.item_slot = 0;
                    soda.x = player.x + dir_x * 0.6f;
                    soda.y = player.y + dir_y * 0.6f;
                    soda.vx = dir_x * 0.22f;
                    soda.vy = dir_y * 0.22f;
                    soda.active = 1;
                    audio_play_soda();
                } else if (player.item_slot == 3) {
                    // Barra Zesty (recupera 100% da estamina)
                    player.item_slot = 0;
                    player.stamina = 100.0f;
                    audio_play_ding(1);
                }
            }

            if (soda.active) {
                soda.x += soda.vx;
                soda.y += soda.vy;
                if (!can_move_to(soda.x, soda.y, 0.2f)) {
                    soda.active = 0;
                } else {
                    float sdx = soda.x - baldi.x;
                    float sdy = soda.y - baldi.y;
                    if (sqrtf(sdx * sdx + sdy * sdy) < 0.9f) {
                        if (can_move_to(baldi.x + soda.vx * 3.0f, baldi.y + soda.vy * 3.0f, 0.25f)) {
                            baldi.x += soda.vx * 3.0f;
                            baldi.y += soda.vy * 3.0f;
                        }
                        baldi.stun_timer = 95.0f;
                    }
                }
            }

            // Coleta de cadernos, itens e saida
            int gx = (int)player.x;
            int gy = (int)player.y;
            if (gx >= 0 && gx < MAP_W && gy >= 0 && gy < MAP_H) {
                int cell = school_map[gy][gx];
                if (cell == 4) {
                    school_map[gy][gx] = 0;
                    generate_math_question(&math_q, 1, player.notebooks);
                    state = STATE_MATH_PAD;
                } else if (cell == 5) {
                    school_map[gy][gx] = 0;
                    player.item_slot = 1;
                    audio_play_ding(1);
                } else if (cell == 6) {
                    school_map[gy][gx] = 0;
                    player.item_slot = 2;
                    audio_play_ding(1);
                } else if (cell == 7) {
                    school_map[gy][gx] = 0;
                    player.item_slot = 3;
                    audio_play_ding(1);
                } else if (cell == 9 && player.notebooks >= TOTAL_NOTEBOOKS) {
                    state = STATE_VICTORY;
                }
            }

            // Playtime patrulhando
            if (playtime.cooldown > 0.0f) {
                playtime.cooldown -= 1.0f;
            } else {
                if (can_move_to(playtime.x + playtime.vx, playtime.y, 0.25f)) playtime.x += playtime.vx;
                else playtime.vx = -playtime.vx;

                float ptx = player.x - playtime.x;
                float pty = player.y - playtime.y;
                if (sqrtf(ptx * ptx + pty * pty) < 0.65f) {
                    player.jump_count = 0;
                    player.rope_pos = 0.0f;
                    state = STATE_JUMPROPE;
                }
            }

            // Diretor da Escola (pune se correr perto dele!)
            if (principal.cooldown > 0.0f) {
                principal.cooldown -= 1.0f;
            } else {
                if (can_move_to(principal.x, principal.y + principal.vy, 0.25f)) principal.y += principal.vy;
                else principal.vy = -principal.vy;

                float prx = player.x - principal.x;
                float pry = player.y - principal.y;
                if (running && sqrtf(prx * prx + pry * pry) < 3.5f) {
                    // Coloca de detencao na sala central por 5 segundos
                    player.x = 9.5f;
                    player.y = 8.5f;
                    player.detention_timer = 300;
                    principal.cooldown = 400.0f;
                    audio_play_detention();
                }
            }

            // IA do Baldi
            if (baldi.stun_timer > 0.0f) {
                baldi.stun_timer -= 1.0f;
            } else if (player.notebooks > 0) {
                float interval = 38.0f - (player.anger * 3.2f);
                if (interval < 12.0f) interval = 12.0f;

                baldi.step_timer += 1.0f;
                if (baldi.step_timer >= interval) {
                    baldi.step_timer = 0.0f;
                    audio_play_slap(player.anger);

                    float dx = player.x - baldi.x;
                    float dy = player.y - baldi.y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist > 0.001f) {
                        float step_len = 0.48f + player.anger * 0.05f;
                        float bx = (dx / dist) * step_len;
                        float by = (dy / dist) * step_len;
                        if (can_move_to(baldi.x + bx, baldi.y, 0.24f)) baldi.x += bx;
                        if (can_move_to(baldi.x, baldi.y + by, 0.24f)) baldi.y += by;
                    }
                }
            }

            float pdx = player.x - baldi.x;
            float pdy = player.y - baldi.y;
            if (player.notebooks > 0 && sqrtf(pdx * pdx + pdy * pdy) < 0.52f) {
                state = STATE_GAMEOVER;
            }

            // Renderizacao 3D Texturizada em 1a Pessoa
            gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0xDC, 0xD8, 0xC0, 0x80, 0x00));
            gsKit_prim_sprite(gsGlobal, 0, SCREEN_H / 2, SCREEN_W, SCREEN_H, 1, GS_SETREG_RGBAQ(0x68, 0x6A, 0x72, 0x80, 0x00));

            float fov = PI / 3.0f;
            for (int sx = 0; sx < SCREEN_W; sx += 4) {
                float cam_x = (2.0f * sx / (float)SCREEN_W) - 1.0f;
                float ray_ang = player.angle + atanf(cam_x * tanf(fov / 2.0f));
                float rdx = cosf(ray_ang);
                float rdy = sinf(ray_ang);

                float dist = 0.0f, hit_x = 0.0f, hit_y = 0.0f;
                int hit = 0, tile = 1;
                while (!hit && dist < 16.0f) {
                    dist += 0.05f;
                    hit_x = player.x + rdx * dist;
                    hit_y = player.y + rdy * dist;
                    int mx = (int)hit_x;
                    int my = (int)hit_y;
                    if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H) {
                        hit = 1;
                    } else {
                        tile = school_map[my][mx];
                        if (is_solid(tile) || tile == 3 || tile == 9) hit = 1;
                    }
                }

                float cdist = dist * cosf(ray_ang - player.angle);
                if (cdist < 0.1f) cdist = 0.1f;
                for (int k = 0; k < 4 && (sx + k) < SCREEN_W; k++) z_buffer[sx + k] = cdist;

                int line_h = (int)(SCREEN_H / cdist);
                int y0 = (SCREEN_H / 2) - (line_h / 2);
                int y1 = (SCREEN_H / 2) + (line_h / 2);

                int shade = (int)(225.0f / (1.0f + cdist * 0.15f));
                if (shade < 40) shade = 40;
                if (shade > 235) shade = 235;

                float wall_u = (hit_x - floorf(hit_x)) + (hit_y - floorf(hit_y));
                int tex_u = (int)(wall_u * 16.0f) & 15;

                if (tile == 9) {
                    u64 exit_col = (player.notebooks >= TOTAL_NOTEBOOKS)
                        ? GS_SETREG_RGBAQ(0x20, shade, 0x30, 0x80, 0x00)
                        : GS_SETREG_RGBAQ(shade, 0x20, 0x20, 0x80, 0x00);
                    gsKit_prim_sprite(gsGlobal, sx, y0, sx + 4, y1, 2, exit_col);
                } else {
                    for (int band = 0; band < 16; band++) {
                        int by0 = y0 + (line_h * band) / 16;
                        int by1 = y0 + (line_h * (band + 1)) / 16;
                        u64 tcol = get_wall_texel(tile, tex_u, band, shade);
                        gsKit_prim_sprite(gsGlobal, sx, by0, sx + 4, by1, 2, tcol);
                    }
                }
            }

            // Sprites Pixel-Art dos Itens no Mapa
            for (int y = 0; y < MAP_H; y++) {
                for (int x = 0; x < MAP_W; x++) {
                    if (school_map[y][x] == 4) render_item_pixel_art(gsGlobal, &player, x + 0.5f, y + 0.5f, ITEM_NOTEBOOK);
                    else if (school_map[y][x] == 5) render_item_pixel_art(gsGlobal, &player, x + 0.5f, y + 0.5f, ITEM_BSODA);
                    else if (school_map[y][x] == 6) render_item_pixel_art(gsGlobal, &player, x + 0.5f, y + 0.5f, ITEM_SCISSORS);
                    else if (school_map[y][x] == 7) render_item_pixel_art(gsGlobal, &player, x + 0.5f, y + 0.5f, ITEM_ZESTY);
                }
            }

            if (soda.active) {
                render_item_pixel_art(gsGlobal, &player, soda.x, soda.y, ITEM_BSODA);
            }

            // Sprites Pixel-Art dos Personagens
            render_char_pixel_art(gsGlobal, &player, baldi.x, baldi.y, SPRITE_BALDI);
            render_char_pixel_art(gsGlobal, &player, playtime.x, playtime.y, SPRITE_PLAYTIME);
            render_char_pixel_art(gsGlobal, &player, principal.x, principal.y, SPRITE_PRINCIPAL);

            // HUD Superior e Slot de Item com icone pixel-art
            char hud_nb[32];
            sprintf(hud_nb, "CADERNOS %d DE %d", player.notebooks, TOTAL_NOTEBOOKS);
            draw_text(gsGlobal, 20, 18, 2, GS_SETREG_RGBAQ(0x10, 0x10, 0x10, 0x80, 0x00), hud_nb);

            gsKit_prim_sprite(gsGlobal, 560, 12, 620, 68, 6, GS_SETREG_RGBAQ(0x22, 0x25, 0x28, 0x80, 0x00));
            if (player.item_slot == 1) draw_pixel_art_2d(gsGlobal, 566, 16, 4, ITEM_BSODA);
            else if (player.item_slot == 2) draw_pixel_art_2d(gsGlobal, 566, 16, 4, ITEM_SCISSORS);
            else if (player.item_slot == 3) draw_pixel_art_2d(gsGlobal, 566, 16, 4, ITEM_ZESTY);

            if (player.detention_timer > 0) {
                char det_txt[40];
                sprintf(det_txt, "DETENCAO  %d SEGUNDOS", (player.detention_timer / 60) + 1);
                draw_text(gsGlobal, 165, 190, 3, GS_SETREG_RGBAQ(0xD5, 0x15, 0x15, 0x80, 0x00), det_txt);
            }

            int stam_w = (int)((player.stamina / 100.0f) * 180.0f);
            gsKit_prim_sprite(gsGlobal, 20, SCREEN_H - 36, 200, SCREEN_H - 18, 6, GS_SETREG_RGBAQ(0x25, 0x25, 0x25, 0x80, 0x00));
            gsKit_prim_sprite(gsGlobal, 20, SCREEN_H - 36, 20 + stam_w, SCREEN_H - 18, 7, GS_SETREG_RGBAQ(0x20, 0xC8, 0x55, 0x80, 0x00));
            draw_text(gsGlobal, 24, SCREEN_H - 54, 2, GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00), "ESTAMINA");
        }
        else if (state == STATE_GAMEOVER) {
            gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x70, 0x08, 0x08, 0x80, 0x00));
            for (int r = 0; r < 24; r++) {
                for (int c = 0; c < 16; c++) {
                    unsigned char idx = SPRITE_BALDI[r][c];
                    if (idx != 0) {
                        gsKit_prim_sprite(gsGlobal, 248 + c * 9, 60 + r * 9, 248 + (c + 1) * 9, 60 + (r + 1) * 9, 3, palette_color(idx, 100));
                    }
                }
            }
            draw_text(gsGlobal, 130, 295, 4, GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00), "BALDI PEGOU VOCE");
            draw_text(gsGlobal, 135, 360, 2, GS_SETREG_RGBAQ(0xFF, 0xD0, 0xD0, 0x80, 0x00), "APERTE X PARA TENTAR DE NOVO");
            if (new_pad & PAD_CROSS) state = STATE_TITLE;
        }
        else if (state == STATE_VICTORY) {
            gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x10, 0x65, 0x25, 0x80, 0x00));
            draw_text(gsGlobal, 110, 150, 4, GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00), "VOCE ESCAPOU DA ESCOLA");
            draw_text(gsGlobal, 140, 250, 2, GS_SETREG_RGBAQ(0xD0, 0xFF, 0xD0, 0x80, 0x00), "APERTE X PARA VOLTAR AO MENU");
            if (new_pad & PAD_CROSS) state = STATE_TITLE;
        }

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }
    return 0;
}
