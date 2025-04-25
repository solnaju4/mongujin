#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" // QueueHandle_t 타입을 위해 포함
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "buttons.h"
#include "transitions.h"

static const char *TAG = "RGB_TRANS";
QueueHandle_t button_evt_queue;

// PCA9685 관련 정의
#define PCA9685_ADDR 0x40
#define PCA9685_MODE1 0x00
#define PCA9685_MODE2 0x01
#define PCA9685_PRESCALE 0xFE

// I2C 관련 정의
#define I2C_MASTER_SCL_IO 9       // SCL 핀
#define I2C_MASTER_SDA_IO 8       // SDA 핀
#define I2C_MASTER_NUM I2C_NUM_0  // I2C 포트 번호
#define I2C_MASTER_FREQ_HZ 100000 // I2C 주파수

// RGB LED 정의
#define NUM_LEDS 5
#define CHANNELS_PER_LED 3  // RGB 채널
#define TOTAL_CHANNELS (NUM_LEDS * CHANNELS_PER_LED)
#define PWM_RESOLUTION 4095  // PCA9685는 12비트 해상도

// 글로벌 변수
int brightness_level = 1;     // 0=밝음, 1=중간, 2=어두움
bool led_on = true;           // LED 상태
int transition_mode = 0;     // 트랜지션 모드 (0=개별 디졸브, 1=동시 디졸브, 2=순차적 디졸브)
uint16_t brightness_vals[3] = {4000, 2000, 800}; // 밝기 레벨 (애노드 타입이므로 값이 작을수록 밝음)

// LED 상태를 저장할 배열
rgb_color_t led_colors[NUM_LEDS];
rgb_color_t target_colors[NUM_LEDS];
transition_params_t led_params[NUM_LEDS]; // 각 LED의 전환 파라미터

// I2C 마스터 초기화
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) return ret;

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// PCA9685에 바이트 쓰기
static esp_err_t pca9685_write_byte(uint8_t reg_addr, uint8_t data) {
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, PCA9685_ADDR, write_buf, 2, 1000 / portTICK_PERIOD_MS);
}

// PCA9685 PWM 설정
void pca9685_set_pwm(uint8_t channel, uint16_t on, uint16_t off) {
    uint8_t data[5];
    data[0] = 0x06 + 4 * channel;
    data[1] = on & 0xFF;
    data[2] = (on >> 8) & 0xFF;
    data[3] = off & 0xFF;
    data[4] = (off >> 8) & 0xFF;
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM, PCA9685_ADDR, data, 5, 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ 채널 %d에 I2C 쓰기 실패! (%s)", channel, esp_err_to_name(err));
    }
}

// PWM duty 설정 (애노드 타입 LED 처리)
void set_pwm_duty(uint8_t channel, uint16_t duty) {
    // 밝기 레벨에 따라 duty 조정
    uint16_t adjusted_duty = duty * brightness_vals[brightness_level] / 4095;

    if (adjusted_duty == 0) {
        // 완전 OFF 모드 (full-off bit set)
        pca9685_set_pwm(channel, 4096, 0);
    } else {
        // 애노드 타입이므로 값을 반전 (4095 - duty)
        uint16_t off = 4095 - adjusted_duty;
        pca9685_set_pwm(channel, 0, off);
    }
}

// 단일 LED의 RGB 값 설정
void set_rgb_led(uint8_t led_index, uint16_t r, uint16_t g, uint16_t b) {
    // LED 인덱스 유효성 검사
    if (led_index >= NUM_LEDS) return;

    // 채널 매핑: LED 1은 채널 0,1,2, LED 2는 채널 3,4,5, ...
    uint8_t base_channel = led_index * CHANNELS_PER_LED;

    set_pwm_duty(base_channel, r);     // R
    set_pwm_duty(base_channel + 1, g); // G
    set_pwm_duty(base_channel + 2, b); // B
}

// 모든 LED 끄기
void turn_off_all_leds() {
    for (int i = 0; i < NUM_LEDS; i++) {
        set_rgb_led(i, 0, 0, 0);
    }
}

// PCA9685 초기화
void pca9685_init() {
    // Sleep 모드로 설정
    pca9685_write_byte(PCA9685_MODE1, 0x10); // SLEEP

    // 출력 모드 설정 (토템폴 출력)
    pca9685_write_byte(PCA9685_MODE2, 0x04); // OUTDRV

    // PWM 주파수 설정 (약 60Hz)
    //pca9685_write_byte(PCA9685_PRESCALE, 100);
    pca9685_write_byte(PCA9685_PRESCALE, 6);  // 800Hz 설정 (prescale = 6)
    // pca9685_write_byte(PCA9685_PRESCALE, 5); // 1000Hz 설정 (prescale = 5)
    vTaskDelay(pdMS_TO_TICKS(10));

    // Sleep 모드 해제 및 자동 증가 활성화
    pca9685_write_byte(PCA9685_MODE1, 0xA0); // AUTO_INCREMENT + RESTART
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "✅ PCA9685 초기화 완료");
}

// LED 애니메이션 태스크
void led_animation_task(void *pvParameters) {
    ESP_LOGI(TAG, "🌈 LED 애니메이션 태스크 시작");

    // 초기화: 모든 LED 끄기
    turn_off_all_leds();

   // 초기화 테스트: 모든 LED를 순차적으로 R, G, B 켜고 끄기
ESP_LOGI(TAG, "🧪 초기화 테스트 시작");
     for (int i = 0; i < NUM_LEDS; i++) {
    // 빨간색
    set_rgb_led(i, 4095, 0, 0);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    // 초록색
    set_rgb_led(i, 0, 4095, 0);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    // 파란색
    set_rgb_led(i, 0, 0, 4095);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    // 끄기
    set_rgb_led(i, 0, 0, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}
      // 모든 LED 화이트로
        for (int i = 0; i < NUM_LEDS; i++) {
            set_rgb_led(i, 4095, 4095, 4095);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);

    // 모든 LED 끄기
    turn_off_all_leds();
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "🧪 초기화 테스트 완료, 메인 애니메이션 시작");

    // 메인 애니메이션 루프
    while (1) {
        // LED가 켜져 있지 않으면 모든 LED를 끄고 대기
        if (!led_on) {
            turn_off_all_leds();
            vTaskDelay(500 / portTICK_PERIOD_MS);
            continue;
        }

        // 현재 모드에 따라 다른 트랜지션 실행

        apply_transition_by_mode(transition_mode);

       
        }
    }


void app_main(void) {
    ESP_LOGI(TAG, "===============================");
    ESP_LOGI(TAG, "🚀 ESP32 RGB 트랜지션 라이트 시작");
    ESP_LOGI(TAG, "===============================");

    // I2C 초기화
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "✅ I2C 초기화 완료");

    // PCA9685 초기화
    pca9685_init();
    // PCA9685 PWM 주파수 설정 (약 1000Hz)
    pca9685_write_byte(PCA9685_PRESCALE, 25);


    // 버튼 이벤트 큐 생성
    button_evt_queue = xQueueCreate(10, sizeof(button_event_t));
    if (button_evt_queue == NULL) {
        ESP_LOGE(TAG, "❌ 버튼 이벤트 큐 생성 실패");
        return;
    }

    // 버튼 인터럽트 설정
    setup_button_interrupt(BTN_MODE, 0);   // 모드 버튼
    setup_button_interrupt(BTN_TOGGLE, 1); // 전원 토글 버튼
    setup_button_interrupt(BTN_BRIGHT, 2); // 밝기 조절 버튼

    // 버튼 태스크 시작
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    // LED 애니메이션 태스크 시작
    xTaskCreate(led_animation_task, "led_animation_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "⚡ 모든 태스크 시작 완료");
}