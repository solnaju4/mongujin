// transitions.c – 모드 1, 2 개선 후 숨쉬기 효과 수정

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <math.h>
#include "transitions.h"

static const char *TAG = "RGB_TRANS";
#define NUM_LEDS 5
#define FADE_STEPS 100  // 단계 수 증가로 더 부드럽게
#define FADE_DELAY_MS 40  // 숨쉬기 페이드 속도 느리게
#define HOLD_TIME_MIN_MS 1000
#define DEBOUNCE_TIME_MS 300  // 버튼 디바운스 간격 증가로 과잉 반응 방지  // 버튼 디바운스 간격 설정
#define HOLD_TIME_MAX_MS 3000

extern rgb_color_t led_colors[NUM_LEDS];
extern bool led_on;
extern void set_rgb_led(uint8_t led_index, uint16_t r, uint16_t g, uint16_t b);
extern void turn_off_all_leds();
extern int transition_mode;
extern volatile bool mode_changed;  // set to true by button interrupt when mode changes

rgb_color_t base_colors[12] = {
    {4095, 0, 0}, {4095, 1650, 0}, {4095, 4095, 0}, {0, 4095, 0},
    {0, 0, 4095}, {75, 0, 130}, {148, 0, 211}, {4095, 2048, 0},
    {4095, 0, 2048}, {2048, 4095, 0}, {0, 4095, 2048}, {0, 2048, 4095}
};

rgb_color_t get_random_color() {
    return base_colors[esp_random() % 12];
}

rgb_color_t interpolate_easeInOut(rgb_color_t start, rgb_color_t end, float progress) {
    float p = progress < 0.5 ? 2 * progress * progress : -1 + (4 - 2 * progress) * progress;
    rgb_color_t result = {
        .r = start.r + (int)((end.r - start.r) * p),
        .g = start.g + (int)((end.g - start.g) * p),
        .b = start.b + (int)((end.b - start.b) * p)
    };
    return result;
}

// mode 2: 동일 컬러 숨쉬기 개선
void breathing_loop_transition() {
    static int color_idx = 0;
        rgb_color_t color;

    // 초기 OFF 제거 → 컬러로 바로 페이드인 시작

    while (led_on && transition_mode == 2) {
        if (!led_on || mode_changed || transition_mode != 2) return;

        color = base_colors[color_idx];

        // turn_off_all_leds();  // 잔불 방지용 OFF 제거
        // vTaskDelay(600 / portTICK_PERIOD_MS);  // 숨쉬기 루프 사이 텀 증가
        // 중복 선언 제거됨

        // 모든 LED OFF → 동일한 컬러로 페이드 인/아웃
        for (int step = 0; step <= FADE_STEPS; step++) {
            if (mode_changed) return;
            float min_brightness = 0.1f;
            float range = 0.9f;
            float b = min_brightness + range * (0.5f - 0.5f * cosf(M_PI * step / FADE_STEPS));
            uint16_t r = (uint16_t)(powf((color.r * b) / 4095.0f, 2.2f) * 4095);
            uint16_t g = (uint16_t)(powf((color.g * b) / 4095.0f, 2.2f) * 4095);
            uint16_t b_val = (uint16_t)(powf((color.b * b) / 4095.0f, 2.2f) * 4095);
            for (int i = 0; i < NUM_LEDS; i++) {
                set_rgb_led(i, r, g, b_val);
            }
            vTaskDelay(FADE_DELAY_MS / portTICK_PERIOD_MS);
        }

        for (int step = FADE_STEPS; step >= 0; step--) {
            if (mode_changed) return;
            float min_brightness = 0.1f;
            float range = 0.9f;
            float b = min_brightness + range * (0.5f - 0.5f * cosf(M_PI * step / FADE_STEPS));
            uint16_t r = (uint16_t)(color.r * b);
            uint16_t g = (uint16_t)(color.g * b);
            uint16_t b_val = (uint16_t)(color.b * b);
            for (int i = 0; i < NUM_LEDS; i++) {
                set_rgb_led(i, r, g, b_val);
            }
            vTaskDelay(FADE_DELAY_MS / portTICK_PERIOD_MS);
        }

        vTaskDelay(300 / portTICK_PERIOD_MS);
        color_idx = (color_idx + 1) % 12;
    }
}

// mode 1 그대로 유지
void rainbow_transition_mode() {
    static float hue = 0;
    while (led_on && transition_mode == 1) {
        for (int i = 0; i < NUM_LEDS; i++) {
            // HSV to RGB 변환: 360도 중 빨주노초파남보 컬러 강조
            int h = ((int)(hue + i * 60) % 360);
            float s = 1.0f, v = 1.0f;
            float c = v * s;
            float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
            float m = v - c;
            float r, g, b;
            if (h < 60) { r = c; g = x; b = 0; }
            else if (h < 120) { r = x; g = c; b = 0; }
            else if (h < 180) { r = 0; g = c; b = x; }
            else if (h < 240) { r = 0; g = x; b = c; }
            else if (h < 300) { r = x; g = 0; b = c; }
            else { r = c; g = 0; b = x; }
            set_rgb_led(i, (uint16_t)((r + m) * 4095), (uint16_t)((g + m) * 4095), (uint16_t)((b + m) * 4095));
        }
        hue += 3;
        if (hue >= 360) hue -= 360;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// mode 0 기존 유지
void individual_dissolve_transition() {
    rgb_color_t white = {4095, 4095, 4095};
    rgb_color_t current_color[NUM_LEDS];
    rgb_color_t next_color[NUM_LEDS];

    rgb_color_t shared_color = get_random_color();
    for (int step = 0; step <= FADE_STEPS; step++) {
        float progress = (float)step / FADE_STEPS;
        rgb_color_t c = interpolate_easeInOut((rgb_color_t){4095, 4095, 4095}, shared_color, progress);
        for (int i = 0; i < NUM_LEDS; i++) {
            set_rgb_led(i, c.r, c.g, c.b);
        }
        vTaskDelay(FADE_DELAY_MS / portTICK_PERIOD_MS);
    }
    for (int i = 0; i < NUM_LEDS; i++) {
        current_color[i] = shared_color;
    }

    while (led_on && transition_mode == 0) {
        for (int i = 0; i < NUM_LEDS; i++) {
            if (!led_on) return;
            next_color[i] = get_random_color();
            for (int step = 0; step <= FADE_STEPS; step++) {
                float progress = (float)step / FADE_STEPS;
                rgb_color_t c = interpolate_easeInOut(current_color[i], white, progress);
                set_rgb_led(i, c.r, c.g, c.b);
                vTaskDelay(FADE_DELAY_MS / portTICK_PERIOD_MS);
            }
            for (int step = 0; step <= FADE_STEPS; step++) {
                float progress = (float)step / FADE_STEPS;
                rgb_color_t c = interpolate_easeInOut(white, next_color[i], progress);
                set_rgb_led(i, c.r, c.g, c.b);
                vTaskDelay(FADE_DELAY_MS / portTICK_PERIOD_MS);
            }
            current_color[i] = next_color[i];
            int hold = HOLD_TIME_MIN_MS + (esp_random() % (HOLD_TIME_MAX_MS - HOLD_TIME_MIN_MS));
            vTaskDelay(hold / portTICK_PERIOD_MS);
        }
    }
}

void apply_transition_by_mode(int mode) {
    switch (mode) {
        case 0: individual_dissolve_transition(); break;
        case 1: rainbow_transition_mode(); break;
        case 2: breathing_loop_transition(); break;
        default: individual_dissolve_transition(); break;
    }
    mode_changed = false;  // ✅ 모드 변경 후 플래그 초기화
}
