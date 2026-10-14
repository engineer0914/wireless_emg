#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// A: 08:A6:F7:BF:2F:3C
// Each button connects its input pin to A's 3V3.
constexpr uint8_t INPUT_1_PIN = 14;
constexpr uint8_t INPUT_2_PIN = 27;
constexpr uint8_t STATUS_LED_PIN = 5;
constexpr uint32_t LED_TOGGLE_INTERVAL_MS = 500;
constexpr uint8_t WIFI_CHANNEL = 1;
constexpr uint32_t SEND_INTERVAL_MS = 50;
const uint8_t B_MAC[] = {0x08, 0xA6, 0xF7, 0xBD, 0xD3, 0xE4};

uint32_t lastSendMs = 0;
uint32_t lastLedToggleMs = 0;
bool statusLedOn = false;

void check(esp_err_t result) {
  if (result == ESP_OK) return;
  Serial.printf("Setup failed: %s\n", esp_err_to_name(result));
  while (true) delay(1000);
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);  // LOLIN D32 built-in LED: LOW = on.
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
  lastLedToggleMs = millis();
  lastSendMs = millis() - SEND_INTERVAL_MS;  // Send immediately on entering loop.
  Serial.println("A ready: GPIO14 + GPIO27 -> B, every 50 ms");
}

void loop() {
  const uint32_t now = millis();

  // Running indicator only; blinking does NOT confirm reception by B.
  // Independent timer: 500 ms on + 500 ms off, with no blocking LED delay.
  if (uint32_t(now - lastLedToggleMs) >= LED_TOGGLE_INTERVAL_MS) {
    lastLedToggleMs = now;
    statusLedOn = !statusLedOn;
    digitalWrite(STATUS_LED_PIN, statusLedOn ? LOW : HIGH);
  }

  if (uint32_t(now - lastSendMs) >= SEND_INTERVAL_MS) {
    lastSendMs = now;
    // Two bytes: [GPIO14 state, GPIO27 state]. Packet format is unchanged.
    uint8_t levels[2];
    levels[0] = digitalRead(INPUT_1_PIN) == HIGH ? 1 : 0;
    levels[1] = digitalRead(INPUT_2_PIN) == HIGH ? 1 : 0;
    const esp_err_t result = esp_now_send(B_MAC, levels, sizeof(levels));
    if (result != ESP_OK) {
      Serial.printf("Send request failed: %s\n", esp_err_to_name(result));
    }
  }
  delay(1);  // Briefly yield to other tasks; does not wait for the LED cycle.
}
