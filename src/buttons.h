#ifndef BUTTONS_H_
#define BUTTONS_H_

#include "freertos/queue.h"

// 버튼 정의
#define BTN_MODE     3
#define BTN_TOGGLE   1
#define BTN_BRIGHT   10

// 버튼 이벤트 유형
typedef enum {
    BTN_EVENT_MODE = 0,
    BTN_EVENT_TOGGLE,
    BTN_EVENT_BRIGHT
} button_event_t;

void buttons_init();
void setup_button_interrupt(int gpio, int btn_idx);
void button_task(void *pvParameters);
extern QueueHandle_t button_evt_queue; // 외부 변수 선언

// main_app.c에 정의된 함수를 사용하기 위한 선언
void turn_off_all_leds();

#endif