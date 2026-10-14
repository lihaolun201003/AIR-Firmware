#include <Arduino.h>
#include <string.h>

// Temporary read-only UART probe. The host captures this SRAM through SWD
// and restores the saved air-monitor ELF in a finally block.
struct ProbeRecord {
  char command[32];
  char reply[192];
  uint32_t length;
  uint32_t overflow;
};

struct ProbeState {
  uint32_t magic;
  uint32_t done;
  uint32_t queryCount;
  uint32_t uptimeMs;
  ProbeRecord records[10];
};

extern "C" {
__attribute__((used)) volatile ProbeState hc06Probe;
}

static_assert(sizeof(ProbeRecord) == 232, "Host decoder layout mismatch");
static_assert(sizeof(ProbeState) == 2336, "Host decoder layout mismatch");

void setup() {
  const char *queries[] = {
      "AT\r\n", "AT+VERSION\r\n", "AT+LADDR\r\n", "AT+ADDR?\r\n",
      "AT+ADDR\r\n", "AT", "AT+VERSION", "AT+ADDR?", "AT+ADDR", "AT+LADDR"};
  hc06Probe.magic = 0x36435048;
  hc06Probe.queryCount = 10;
  Serial.setRx(PA3);
  Serial.setTx(PA2);
  Serial.begin(9600, SERIAL_8N1);
  delay(600);
  while (Serial.available()) Serial.read();

  for (uint32_t index = 0; index < 10; ++index) {
    volatile ProbeRecord &record = hc06Probe.records[index];
    const size_t count = strlen(queries[index]);
    for (size_t i = 0; i < count; ++i) record.command[i] = queries[index][i];
    record.command[count] = '\0';
    Serial.write(reinterpret_cast<const uint8_t *>(queries[index]), count);
    const uint32_t start = millis();
    while (static_cast<uint32_t>(millis() - start) < 1000) {
      while (Serial.available()) {
        const int byte = Serial.read();
        if (byte < 0) break;
        if (record.length < sizeof(record.reply) - 1) {
          record.reply[record.length++] = static_cast<char>(byte);
          record.reply[record.length] = '\0';
        } else {
          ++record.overflow;
        }
      }
      delay(1);
    }
  }
  hc06Probe.uptimeMs = millis();
  hc06Probe.done = 1;
}

void loop() { delay(10); }
