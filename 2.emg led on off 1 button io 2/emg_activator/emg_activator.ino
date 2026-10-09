#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// B: 08:A6:F7:BD:D3:E4 -- GPIO5 LED receiver
// Arduino-ESP32 3.x receive callback signature.
constexpr uint8_t LED_PIN = 5;
constexpr bool LED_ACTIVE_LOW = true;
constexpr uint8_t WIFI_CHANNEL = 1;
constexpr uint32_t RX_TIMEOUT_MS = 500;
const uint8_t A_MAC[] = {0x08, 0xA6, 0xF7, 0xBF, 0x2F, 0x3C};

struct ReceivedState {
  uint32_t receivedAt;
  uint8_t level;
};
QueueHandle_t stateQueue = nullptr;

void setLed(bool on) {
  digitalWrite(LED_PIN, LED_ACTIVE_LOW ? (on ? LOW : HIGH)
                                            : (on ? HIGH : LOW));
}

void check(esp_err_t result) {
  if (result == ESP_OK) return;
  Serial.printf("Setup failed: %s\n", esp_err_to_name(result));
  while (true) delay(1000);
}

void onReceive(const esp_now_recv_info_t *info,
               const uint8_t *data, int len) {
  // Only accept a one-byte 0/1 value from A.
  if (info == nullptr || data == nullptr || len != 1) return;
  if (memcmp(info->src_addr, A_MAC, sizeof(A_MAC)) != 0) return;
  if (data[0] > 1) return;

  ReceivedState state = {millis(), data[0]};
  // The Wi-Fi callback only hands the latest value to loop().
  xQueueOverwrite(stateQueue, &state);
}

void setup() {
  Serial.begin(115200);
  setLed(false);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);

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
  // Receiving unencrypted unicast needs no peer registration on B.
  Serial.println("B ready: waiting for A");
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
  setLed(fresh && latest.level == 1);
  delay(1);
}
