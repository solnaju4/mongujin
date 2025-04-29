## 프로젝트 개요 

(Project Overview)
Mongujin 2025는
ESP32-C3 MCU와 PCA9685 PWM 컨트롤러를 기반으로,
5개의 RGB 조명(총 15채널)을 부드럽고 다양한 방식으로 트랜지션(전환) 제어하는
고급 조명 시스템을 만드는 프로젝트입니다.

##주요 목표

### 버튼 제어,

부드러운 색상 전환 효과,

동적 트랜지션 패턴,

### 웹 기반 설정 관리
를 하나의 일관된 시스템으로 통합하는 것입니다.

### 프로그램 사양 (Specifications)
MCU: ESP32-C3 SuperMini (4MB Flash)

PWM 드라이버: PCA9685 (16채널 PWM 확장 모듈, 15채널 사용)

LED: 5개 RGB LED (각 LED당 R,G,B → 3채널 사용)

I2C 통신: SDA(GPIO 7), SCL(GPIO 6)

버튼: 총 3개 (GPIO 3, GPIO 1, GPIO 10 사용)

WebServer: ESP32 내장 SoftAP + 웹 서버 (HTTP)

전원: 5V 어댑터 사용

## 개발환경: 
PlatformIO + ESP-IDF Framework

## 주요 기능 (Features)
### 1. 트랜지션 모드 (Transition Modes)
Mode 1: 랜덤 5색 선택 후 화이트 → 컬러 → 화이트 부드럽게 반복

Mode 2: 레인보우 컬러 체이싱 (Chasing) 패턴

Mode 3: 무지개색 부드러운 페이드 루프

Mode 4: 12색 순차 전환 (고정 시간)

Mode 5: 촛불처럼 은은하게 숨쉬는 조명 효과

※ 현재 Mode 1, 2, 3까지 정상 작동. 4, 5는 기본 구조만 마련.
### 2. 버튼 제어 (Button Functions)
버튼 1 (GPIO 3): 밝기 단계 증가 (Low → Mid → High → 순환)

버튼 2 (GPIO 1): 밝기 단계 감소

버튼 3 (GPIO 10): 트랜지션 모드 변경

※ 버튼은 인터럽트(Interrupt) 기반으로 즉각 반응 처리.

### 3. 밝기 제어 (Brightness Control)
3단계 밝기 레벨 지원 (약 30%, 60%, 100%)

PWM 출력 범위를 소프트웨어로 스케일링

### 4. 전원 제어 (Power Control)
모든 LED ON/OFF 토글 가능

5. 웹 기반 설정 (In Progress 🚧)
SoftAP로 스마트폰/PC 연결

웹페이지 접속 후 트랜지션 모드, 밝기, 전원 설정 가능

차후 설정 저장 기능 추가 예정
  - 
## 📌 개발 환경

- **보드**: ESP32-C3 (esp32-c3-devkitm-1)
- **PWM 모듈**: PCA9685 (I2C, 0x40)
- **프레임워크**: ESP-IDF (via PlatformIO)
- **컴파일 환경**: VSCode + PlatformIO Extension
- **전압**: 5V (공급에 따라 RGB LED 개수 조절 필요)

---

## 🔮 향후 계획

- 웹서버 + 설정 UI 추가 (SPIFFS 기반)
- 색상/밝기/모드 설정값 저장 및 복원
- OTA 업데이트 연동

---

## 📌 참고 사항

- LED 채널 매핑: RGB 3채널 × 5등 = 15채널 (0~14)
- PWM 범위: 0~4095 (12비트 정밀도)
- 색상 배열: 12컬러 프리셋 사용 (`base_colors[]`)

