#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// B: 08:A6:F7:BD:D3:E4 -- Arduino-ESP32 3.x
constexpr uint8_t OUTPUT_1_PIN = 5;   // A GPIO2 -> built-in LED
constexpr uint8_t OUTPUT_2_PIN = 18;  // A GPIO4 -> external output
constexpr bool OUTPUT_1_ACTIVE_LOW = true;   // Built-in LED: LOW = on
constexpr bool OUTPUT_2_ACTIVE_LOW = false;  // GPIO18: HIGH = active
constexpr uint8_t WIFI_CHANNEL = 1;
constexpr uint32_t RX_TIMEOUT_MS = 500;
const uint8_t A_MAC[] = {0x08, 0xA6, 0xF7, 0xBF, 0x2F, 0x3C};

struct ReceivedState {
  uint32_t receivedAt;
  uint8_t level1;
  uint8_t level2;
};
QueueHandle_t stateQueue = nullptr;

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
  Serial.println("B ready: GPIO2 -> GPIO5 LED, GPIO4 -> GPIO18");
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
  delay(1);
}
