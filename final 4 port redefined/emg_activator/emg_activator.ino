#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// B: 08:A6:F7:BD:D3:E4 -- Arduino-ESP32 3.x
constexpr uint8_t OUTPUT_1_PIN = 23;  // A GPIO14 -> B GPIO23
constexpr uint8_t OUTPUT_2_PIN = 22;  // A GPIO27 -> B GPIO22
constexpr bool OUTPUT_1_ACTIVE_LOW = false;  // HIGH input -> HIGH output
constexpr bool OUTPUT_2_ACTIVE_LOW = false;
constexpr uint8_t STATUS_LED_PIN = 5;
constexpr uint32_t LED_TOGGLE_INTERVAL_MS = 500;
constexpr uint8_t WIFI_CHANNEL = 1;
constexpr uint32_t RX_TIMEOUT_MS = 500;
const uint8_t A_MAC[] = {0x08, 0xA6, 0xF7, 0xBF, 0x2F, 0x3C};

struct ReceivedState {
  uint32_t receivedAt;
  uint8_t level1;
  uint8_t level2;
};
QueueHandle_t stateQueue = nullptr;
uint32_t lastLedToggleMs = 0;
bool statusLedOn = false;

void setOutputs(bool active1, bool active2) {
  digitalWrite(OUTPUT_1_PIN, (active1 != OUTPUT_1_ACTIVE_LOW) ? HIGH : LOW);
  digitalWrite(OUTPUT_2_PIN, (active2 != OUTPUT_2_ACTIVE_LOW) ? HIGH : LOW);
}

void check(esp_err_t result) {
  if (result == ESP_OK) return;
  Serial.printf("Setup failed: %s\n", esp_err_to_name(result));
  while (true) delay(1000);
}

void onReceive(const esp_now_recv_info_t *info,
               const uint8_t *data, int len) {
  // Reject other senders, old one-byte packets, and invalid levels.
  if (info == nullptr || data == nullptr || len != 2) return;
  if (memcmp(info->src_addr, A_MAC, sizeof(A_MAC)) != 0) return;
  if (data[0] > 1 || data[1] > 1) return;

  ReceivedState state = {millis(), data[0], data[1]};
  xQueueOverwrite(stateQueue, &state);
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);  // Built-in LED: LOW = on.
  pinMode(OUTPUT_1_PIN, OUTPUT);
  pinMode(OUTPUT_2_PIN, OUTPUT);
  setOutputs(false, false);

  stateQueue = xQueueCreate(1, sizeof(ReceivedState));
  if (stateQueue == nullptr) {
    Serial.println("Queue allocation failed");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  check(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
  check(esp_wifi_set_ps(WIFI_PS_NONE));
  check(esp_now_init());
  check(esp_now_register_recv_cb(onReceive));
  lastLedToggleMs = millis();
  Serial.println("B ready: GPIO14 -> GPIO23, GPIO27 -> GPIO22");
}

void loop() {
  static ReceivedState latest = {};
  static bool received = false;
  ReceivedState incoming;
  if (xQueueReceive(stateQueue, &incoming, 0) == pdTRUE) {
    latest = incoming;
    received = true;
  }
  const bool fresh = received &&
                     uint32_t(millis() - latest.receivedAt) < RX_TIMEOUT_MS;
  setOutputs(fresh && latest.level1 == 1, fresh && latest.level2 == 1);

  // Independent running indicator. Blinks even if A is off or disconnected.
  // No 500 ms delay: reception and output updates continue during blinking.
  const uint32_t now = millis();
  if (uint32_t(now - lastLedToggleMs) >= LED_TOGGLE_INTERVAL_MS) {
    lastLedToggleMs = now;
    statusLedOn = !statusLedOn;
    digitalWrite(STATUS_LED_PIN, statusLedOn ? LOW : HIGH);
  }
  delay(1);
}
