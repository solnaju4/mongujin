#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "buttons.h"
// #include "main_app.h" // 더 이상 직접 포함하지 않음

static const char *TAG = "RGB_TRANS";
volatile bool mode_changed = false;


// 버튼 정의
#define BTN_MODE     3
#define BTN_TOGGLE   1
#define BTN_BRIGHT   10

// 인터럽트 디바운스 시간 (ms)
#define DEBOUNCE_TIME 400

// 버튼 마지막 눌림 시간 (디바운싱용)
static int64_t last_btn_press_time[3] = {0, 0, 0};


// 버튼 ISR - 인터럽트 서비스 루틴
static void IRAM_ATTR button_isr_handler(void* arg) {
    int64_t current_time = esp_timer_get_time() / 1000; // microseconds -> milliseconds
    int btn_idx = (int)arg;

    // 디바운싱: 마지막 이벤트 이후 일정 시간이 지났는지 확인
    if ((current_time - last_btn_press_time[btn_idx]) > DEBOUNCE_TIME) {
        button_event_t evt = (button_event_t)btn_idx;
        xQueueSendFromISR(button_evt_queue, &evt, NULL);
        last_btn_press_time[btn_idx] = current_time;
    }
}

// 버튼 설정 (인터럽트 모드)
void setup_button_interrupt(int gpio, int btn_idx) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // 하강 에지 인터럽트
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << gpio),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };

    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio, button_isr_handler, (void*)btn_idx);

    ESP_LOGI(TAG, "버튼 %d 인터럽트 설정 완료 (GPIO: %d)", btn_idx, gpio);
}

void button_task(void *pvParameters) {
    button_event_t evt;

    while (1) {
        if (xQueueReceive(button_evt_queue, &evt, portMAX_DELAY)) {
            switch (evt) {
                case BTN_EVENT_MODE: {
                    extern int transition_mode;
                    extern volatile bool mode_changed;
                    transition_mode = (transition_mode + 1) % 3;
                    mode_changed = true;
                    ESP_LOGI(TAG, "🎨 트랜지션 모드 변경: %d", transition_mode);
                    break;
                }

                case BTN_EVENT_TOGGLE: {
                    extern bool led_on;
                    led_on = !led_on;
                    if (!led_on) turn_off_all_leds();
                    ESP_LOGI(TAG, "💡 전체 LED %s", led_on ? "ON" : "OFF");
                    break;
                }

                case BTN_EVENT_BRIGHT: {
                    extern int brightness_level;
                    brightness_level = (brightness_level + 1) % 3;
                    ESP_LOGI(TAG, "🌕 밝기 단계 변경: %d", brightness_level);
                    break;
                }

                default:
                    ESP_LOGW(TAG, "알 수 없는 버튼 이벤트: %d", evt);
                    break;
            }
        }
    }
}

void buttons_init() {
    // 버튼 이벤트 큐는 main_app.c에서 생성됨
    // 버튼 인터럽트 설정
    setup_button_interrupt(BTN_MODE, 0);   // 모드 버튼
    setup_button_interrupt(BTN_TOGGLE, 1); // 전원 토글 버튼
    setup_button_interrupt(BTN_BRIGHT, 2); // 밝기 조절 버튼
    // 버튼 태스크는 main_app.c에서 시작됨
}

