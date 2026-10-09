#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// A: 08:A6:F7:BF:2F:3C
// Each button connects its input pin to A's 3V3.
constexpr uint8_t INPUT_1_PIN = 2;
constexpr uint8_t INPUT_2_PIN = 4;
constexpr uint8_t WIFI_CHANNEL = 1;
constexpr uint32_t SEND_INTERVAL_MS = 50;
const uint8_t B_MAC[] = {0x08, 0xA6, 0xF7, 0xBD, 0xD3, 0xE4};

void check(esp_err_t result) {
  if (result == ESP_OK) return;
  Serial.printf("Setup failed: %s\n", esp_err_to_name(result));
  while (true) delay(1000);
}

void setup() {
  Serial.begin(115200);
  pinMode(INPUT_1_PIN, INPUT_PULLDOWN);
  pinMode(INPUT_2_PIN, INPUT_PULLDOWN);

  WiFi.mode(WIFI_STA);
  check(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
  check(esp_wifi_set_ps(WIFI_PS_NONE));
  check(esp_now_init());

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, B_MAC, sizeof(B_MAC));
  peer.channel = WIFI_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  check(esp_now_add_peer(&peer));
  Serial.println("A ready: GPIO2 + GPIO4 -> B, every 50 ms");
}

void loop() {
  // Two bytes: [GPIO2 state, GPIO4 state]. Both channels are independent.
  uint8_t levels[2];
  levels[0] = digitalRead(INPUT_1_PIN) == HIGH ? 1 : 0;
  levels[1] = digitalRead(INPUT_2_PIN) == HIGH ? 1 : 0;
  const esp_err_t result = esp_now_send(B_MAC, levels, sizeof(levels));
  if (result != ESP_OK) {
    Serial.printf("Send request failed: %s\n", esp_err_to_name(result));
  }
  delay(SEND_INTERVAL_MS);
}
