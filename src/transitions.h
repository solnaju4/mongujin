#ifndef TRANSITIONS_H_
#define TRANSITIONS_H_

#include <stdint.h>
#include <stdbool.h>

// RGB 색상 구조체
typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} rgb_color_t;

// 트랜지션 파라미터 구조체
typedef struct {
    int fade_steps;
    int fade_delay_ms;
    int hold_time_ms;
} transition_params_t;

rgb_color_t get_random_color();
rgb_color_t interpolate_easeInOut(rgb_color_t start, rgb_color_t end, float progress);

void individual_dissolve_transition();  // mode 0
void rainbow_transition_mode();         // mode 1
void breathing_loop_transition();       // mode 2
void apply_transition_by_mode(int mode); // mode 선택 기반 트리거

#endif
