#include <stdio.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

#define WS2812_PWM_0 64
#define WS2812_PWM_1 128
#define NUM_LEDS 3
#define RESET_SLOTS 60
#define WS2812_BUF_LEN (NUM_LEDS * 24 + RESET_SLOTS)
#define T 3000

static u16 dma_pwm_buffer[WS2812_BUF_LEN];

/* ---- copied from main.c ---- */
void set_led(u8 idx, u8 r, u8 g, u8 b) {
    u32 grb = ((u32)g << 16) | ((u32)r << 8) | (u32)b;
    int offset = idx * 24;
    for (int i = 0; i < 24; i++) {
        u8 bit = (grb >> (23 - i)) & 1;
        dma_pwm_buffer[i + offset] = bit ? WS2812_PWM_1 : WS2812_PWM_0;
    }
}

/* ---- test helpers ---- */
// Decode the buffer back into bytes, the way the LED would read it
static void decode(int led, u8 *g, u8 *r, u8 *b) {
    u8 v[3] = {0};
    for (int i = 0; i < 24; i++) {
        int bit = dma_pwm_buffer[led * 24 + i] == WS2812_PWM_1;
        v[i / 8] = (u8)((v[i / 8] << 1) | bit);
    }
    *g = v[0]; *r = v[1]; *b = v[2];
}

// Fake button: pulled up (1), pressed (0) from 1000 to 1050 ms
static u8 fake_button(u32 now) { return (now >= 1000 && now < 1050) ? 0 : 1; }

int main(void) {
    u8 prev_btn = 1, blink_active = 0;
    u32 blink_start = 0, last_update = 0;

    printf("  ms |   R   G   B  (decoded from DMA buffer)\n");

    for (u32 now = 0; now <= 3200; now++) {   // one loop iteration per ms
        u32 cycle = now % T;
        u8 btn = fake_button(now);

        if (prev_btn == 1 && btn == 0 && !blink_active) {
            blink_active = 1;
            blink_start = now;
        }
        prev_btn = btn;

        u8 r = (cycle <= 1500) ? 255 : 0;
        u8 g = (u8)((cycle * 255) / T);
        u8 b = 0;

        if (blink_active) {
            u32 elapsed = now - blink_start;
            if (elapsed < 900) b = ((elapsed / 150) % 2 == 0) ? 255 : 0;
            else { blink_active = 0; b = 0; }
        }

        if (now - last_update >= 16) {
            set_led(0, r, 0, 0);
            set_led(1, 0, g, 0);
            set_led(2, 0, 0, b);
            last_update = now;

            if (now % 80 == 0) {               // print every 80 ms
                u8 g0, r0, b0, g1, r1, b1, g2, r2, b2;
                decode(0, &g0, &r0, &b0);
                decode(1, &g1, &r1, &b1);
                decode(2, &g2, &r2, &b2);
                printf("%4u | %3u %3u %3u\n", now, r0, g1, b2);
            }
        }
    }
    return 0;
}