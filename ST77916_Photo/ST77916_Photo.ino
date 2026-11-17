#include <Arduino_GFX_Library.h>
#include "image_rgb565.h"

// Based on SuiSuiren's Raspberry Pi test in TFT_eSPI issue #3915.
constexpr uint16_t SCREEN_SIZE = 360;
constexpr uint32_t QSPI_CLOCK_HZ = 40000000;
constexpr uint32_t TE_TIMEOUT_US = 50000;
constexpr int PANEL_RESET_GPIO = 9;
constexpr int PANEL_BACKLIGHT_GPIO = 11;
constexpr int PANEL_TE_GPIO = 18;

// The pin order is CS, SCK, IO0, IO1, IO2, IO3.
Arduino_DataBus *lcd = new Arduino_ESP32QSPI(10, 17, 4, 5, 6, 7);
static_assert(IMAGE_WIDTH == SCREEN_SIZE && IMAGE_HEIGHT == SCREEN_SIZE,
              "Convert the image to 360x360 RGB565 first.");
static_assert(sizeof(IMAGE_PIXELS) / sizeof(IMAGE_PIXELS[0]) ==
                  static_cast<uint32_t>(SCREEN_SIZE) * SCREEN_SIZE,
              "Incorrect RGB565 pixel count.");
portMUX_TYPE teLock = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t teEdgeCount = 0;
volatile uint32_t teLastEdgeUs = 0;
volatile uint32_t tePeriodUs = 0;

// HD18004C18 register values and delays from the Raspberry Pi test.
// Multi-byte parameters share one SPI transaction with their command.
static constexpr uint8_t panelInit[] = {
  BEGIN_WRITE,
  WRITE_COMMAND_8, 0x01,
  END_WRITE,
  DELAY, 120,

  BEGIN_WRITE,
  WRITE_C8_D8, 0xF0, 0x28,
  WRITE_C8_D8, 0xF2, 0x28,
  WRITE_C8_D8, 0x73, 0xF0,
  WRITE_C8_D8, 0x76, 0x0F,
  WRITE_C8_D8, 0x7C, 0xD1,
  WRITE_C8_D8, 0x83, 0xE0,
  WRITE_C8_D8, 0x84, 0x61,
  WRITE_C8_D8, 0xF2, 0x82,
  WRITE_C8_D8, 0xF0, 0x00,
  WRITE_C8_D8, 0xF0, 0x01,
  WRITE_C8_D8, 0xF1, 0x01,
  WRITE_C8_D8, 0xB0, 0x52,
  WRITE_C8_D8, 0xB1, 0x49,
  WRITE_C8_D8, 0xB2, 0x24,
  WRITE_C8_D8, 0xB3, 0x01,
  WRITE_C8_D8, 0xB4, 0x66,
  WRITE_C8_D8, 0xB5, 0x44,
  WRITE_C8_D8, 0xB6, 0xC5,
  WRITE_C8_D8, 0xB7, 0x40,
  WRITE_C8_D8, 0xB8, 0x86,
  WRITE_C8_D8, 0xB9, 0x15,
  WRITE_C8_D8, 0xBA, 0x00,
  WRITE_C8_D8, 0xBB, 0x08,
  WRITE_C8_D8, 0xBC, 0x08,
  WRITE_C8_D8, 0xBD, 0x00,
  WRITE_C8_D8, 0xBE, 0x00,
  WRITE_C8_D8, 0xBF, 0x07,
  WRITE_C8_D8, 0xC0, 0x80,
  WRITE_C8_D8, 0xC1, 0x10,
  WRITE_C8_D8, 0xC2, 0x37,
  WRITE_C8_D8, 0xC3, 0x80,
  WRITE_C8_D8, 0xC4, 0x10,
  WRITE_C8_D8, 0xC5, 0x37,
  WRITE_C8_D8, 0xC6, 0xA9,
  WRITE_C8_D8, 0xC7, 0x41,
  WRITE_C8_D8, 0xC8, 0x01,
  WRITE_C8_D8, 0xC9, 0xA9,
  WRITE_C8_D8, 0xCA, 0x41,
  WRITE_C8_D8, 0xCB, 0x01,
  WRITE_C8_D8, 0xCC, 0x7F,
  WRITE_C8_D8, 0xCD, 0x7F,
  WRITE_C8_D8, 0xCE, 0xFF,
  WRITE_C8_D8, 0xD0, 0x91,
  WRITE_C8_D8, 0xD1, 0x68,
  WRITE_C8_D8, 0xD2, 0x68,
  WRITE_C8_D16, 0xF5, 0x00, 0xA5,
  WRITE_C8_D8, 0xF1, 0x10,
  WRITE_C8_D8, 0xF0, 0x00,
  WRITE_C8_D8, 0xF0, 0x02,
  WRITE_C8_BYTES, 0xE0, 14,
    0xF0, 0x0E, 0x15, 0x0A, 0x0B, 0x07, 0x3E,
    0x43, 0x53, 0x39, 0x17, 0x17, 0x35, 0x38,
  WRITE_C8_BYTES, 0xE1, 14,
    0xF0, 0x0D, 0x15, 0x0A, 0x0A, 0x06, 0x3D,
    0x33, 0x52, 0x39, 0x16, 0x16, 0x35, 0x37,
  WRITE_C8_D8, 0xF0, 0x10,
  WRITE_C8_D8, 0xF3, 0x10,
  WRITE_C8_D8, 0xE0, 0x09,
  WRITE_C8_D8, 0xE1, 0x00,
  WRITE_C8_D8, 0xE2, 0x03,
  WRITE_C8_D8, 0xE3, 0x00,
  WRITE_C8_D8, 0xE4, 0xE0,
  WRITE_C8_D8, 0xE5, 0x06,
  WRITE_C8_D8, 0xE6, 0x21,
  WRITE_C8_D8, 0xE7, 0x00,
  WRITE_C8_D8, 0xE8, 0x05,
  WRITE_C8_D8, 0xE9, 0x82,
  WRITE_C8_D8, 0xEA, 0xDE,
  WRITE_C8_D8, 0xEB, 0xC0,
  WRITE_C8_D8, 0xEC, 0x40,
  WRITE_C8_D8, 0xED, 0x84,
  WRITE_C8_D8, 0xEE, 0xFF,
  WRITE_C8_D8, 0xEF, 0x71,
  WRITE_C8_D8, 0xF8, 0xFF,
  WRITE_C8_D8, 0xF9, 0x50,
  WRITE_C8_D8, 0xFA, 0xFF,
  WRITE_C8_D8, 0xFB, 0xF3,
  WRITE_C8_D8, 0xFC, 0x00,
  WRITE_C8_D8, 0xFD, 0x00,
  WRITE_C8_D8, 0xFE, 0x00,
  WRITE_C8_D8, 0xFF, 0x00,
  WRITE_C8_D8, 0x60, 0x42,
  WRITE_C8_D8, 0x61, 0xDF,
  WRITE_C8_D8, 0x62, 0x40,
  WRITE_C8_D8, 0x63, 0x40,
  WRITE_C8_D8, 0x64, 0x02,
  WRITE_C8_D8, 0x65, 0x00,
  WRITE_C8_D8, 0x66, 0x00,
  WRITE_C8_D8, 0x67, 0x00,
  WRITE_C8_D8, 0x68, 0x00,
  WRITE_C8_D8, 0x69, 0x00,
  WRITE_C8_D8, 0x6A, 0x00,
  WRITE_C8_D8, 0x6B, 0x00,
  WRITE_C8_D8, 0x70, 0x42,
  WRITE_C8_D8, 0x71, 0xDF,
  WRITE_C8_D8, 0x72, 0x40,
  WRITE_C8_D8, 0x73, 0x40,
  WRITE_C8_D8, 0x74, 0x01,
  WRITE_C8_D8, 0x75, 0x00,
  WRITE_C8_D8, 0x76, 0x00,
  WRITE_C8_D8, 0x77, 0x00,
  WRITE_C8_D8, 0x78, 0x00,
  WRITE_C8_D8, 0x79, 0x00,
  WRITE_C8_D8, 0x7A, 0x00,
  WRITE_C8_D8, 0x7B, 0x00,
  WRITE_C8_D8, 0x80, 0x48,
  WRITE_C8_D8, 0x81, 0x00,
  WRITE_C8_D8, 0x82, 0x04,
  WRITE_C8_D8, 0x83, 0x02,
  WRITE_C8_D8, 0x84, 0xDC,
  WRITE_C8_D8, 0x85, 0x00,
  WRITE_C8_D8, 0x86, 0x00,
  WRITE_C8_D8, 0x87, 0x00,
  WRITE_C8_D8, 0x88, 0x48,
  WRITE_C8_D8, 0x89, 0x00,
  WRITE_C8_D8, 0x8A, 0x06,
  WRITE_C8_D8, 0x8B, 0x02,
  WRITE_C8_D8, 0x8C, 0xDE,
  WRITE_C8_D8, 0x8D, 0x00,
  WRITE_C8_D8, 0x8E, 0x00,
  WRITE_C8_D8, 0x8F, 0x00,
  WRITE_C8_D8, 0x90, 0x48,
  WRITE_C8_D8, 0x91, 0x00,
  WRITE_C8_D8, 0x92, 0x08,
  WRITE_C8_D8, 0x93, 0x02,
  WRITE_C8_D8, 0x94, 0xE0,
  WRITE_C8_D8, 0x95, 0x00,
  WRITE_C8_D8, 0x96, 0x00,
  WRITE_C8_D8, 0x97, 0x00,
  WRITE_C8_D8, 0x98, 0x48,
  WRITE_C8_D8, 0x99, 0x00,
  WRITE_C8_D8, 0x9A, 0x0A,
  WRITE_C8_D8, 0x9B, 0x02,
  WRITE_C8_D8, 0x9C, 0xE2,
  WRITE_C8_D8, 0x9D, 0x00,
  WRITE_C8_D8, 0x9E, 0x00,
  WRITE_C8_D8, 0x9F, 0x00,
  WRITE_C8_D8, 0xA0, 0x48,
  WRITE_C8_D8, 0xA1, 0x00,
  WRITE_C8_D8, 0xA2, 0x03,
  WRITE_C8_D8, 0xA3, 0x02,
  WRITE_C8_D8, 0xA4, 0xDB,
  WRITE_C8_D8, 0xA5, 0x00,
  WRITE_C8_D8, 0xA6, 0x00,
  WRITE_C8_D8, 0xA7, 0x00,
  WRITE_C8_D8, 0xA8, 0x48,
  WRITE_C8_D8, 0xA9, 0x00,
  WRITE_C8_D8, 0xAA, 0x05,
  WRITE_C8_D8, 0xAB, 0x02,
  WRITE_C8_D8, 0xAC, 0xDD,
  WRITE_C8_D8, 0xAD, 0x00,
  WRITE_C8_D8, 0xAE, 0x00,
  WRITE_C8_D8, 0xAF, 0x00,
  WRITE_C8_D8, 0xB0, 0x48,
  WRITE_C8_D8, 0xB1, 0x00,
  WRITE_C8_D8, 0xB2, 0x07,
  WRITE_C8_D8, 0xB3, 0x02,
  WRITE_C8_D8, 0xB4, 0xDF,
  WRITE_C8_D8, 0xB5, 0x00,
  WRITE_C8_D8, 0xB6, 0x00,
  WRITE_C8_D8, 0xB7, 0x00,
  WRITE_C8_D8, 0xB8, 0x48,
  WRITE_C8_D8, 0xB9, 0x00,
  WRITE_C8_D8, 0xBA, 0x09,
  WRITE_C8_D8, 0xBB, 0x02,
  WRITE_C8_D8, 0xBC, 0xE1,
  WRITE_C8_D8, 0xBD, 0x00,
  WRITE_C8_D8, 0xBE, 0x00,
  WRITE_C8_D8, 0xBF, 0x00,
  WRITE_C8_D8, 0xC0, 0x65,
  WRITE_C8_D8, 0xC1, 0x74,
  WRITE_C8_D8, 0xC2, 0x47,
  WRITE_C8_D8, 0xC3, 0x56,
  WRITE_C8_D8, 0xC4, 0xAA,
  WRITE_C8_D8, 0xC5, 0x11,
  WRITE_C8_D8, 0xC6, 0x00,
  WRITE_C8_D8, 0xC7, 0x2A,
  WRITE_C8_D8, 0xC8, 0xA2,
  WRITE_C8_D8, 0xC9, 0x33,
  WRITE_C8_D8, 0xD0, 0x65,
  WRITE_C8_D8, 0xD1, 0x74,
  WRITE_C8_D8, 0xD2, 0x47,
  WRITE_C8_D8, 0xD3, 0x56,
  WRITE_C8_D8, 0xD4, 0xAA,
  WRITE_C8_D8, 0xD5, 0x11,
  WRITE_C8_D8, 0xD6, 0x00,
  WRITE_C8_D8, 0xD7, 0x2A,
  WRITE_C8_D8, 0xD8, 0xA2,
  WRITE_C8_D8, 0xD9, 0x33,
  WRITE_C8_D8, 0xF3, 0x01,
  WRITE_C8_D8, 0xF0, 0x00,
  WRITE_C8_D8, 0x4D, 0x00,
  WRITE_C8_D8, 0x4E, 0x00,
  WRITE_C8_D8, 0x4F, 0x00,
  WRITE_C8_D8, 0x4C, 0x01,
  END_WRITE,
  DELAY, 10,

  BEGIN_WRITE,
  WRITE_C8_D8, 0x4C, 0x00,
  END_WRITE,
  DELAY, 10,

  BEGIN_WRITE,
  WRITE_C8_D8, 0x3A, 0x55,
  WRITE_C8_D8, 0xF0, 0x00,
  WRITE_C8_BYTES, 0x2A, 4, 0x00, 0x00, 0x01, 0x67,
  WRITE_C8_BYTES, 0x2B, 4, 0x00, 0x00, 0x01, 0x67,
  WRITE_COMMAND_8, 0x21,
  WRITE_COMMAND_8, 0x11,
  END_WRITE,
  DELAY, 120,

  BEGIN_WRITE,
  WRITE_C8_D8, 0x3A, 0x55,
  WRITE_C8_D8, 0x36, 0x00, // MADCTL: RGB channel order for IMAGE_PIXELS.
  WRITE_C8_D8, 0x35, 0x00, // TEON: one pulse per frame.
  WRITE_COMMAND_8, 0x29,
  END_WRITE,
  DELAY, 20,

};

void ARDUINO_ISR_ATTR onTeRising() {
  const uint32_t nowUs = micros();
  portENTER_CRITICAL_ISR(&teLock);
  if (teEdgeCount != 0) {
    tePeriodUs = nowUs - teLastEdgeUs;
  }
  teLastEdgeUs = nowUs;
  ++teEdgeCount;
  portEXIT_CRITICAL_ISR(&teLock);
}

bool waitForNextTe(uint32_t &periodUs) {
  const uint32_t startUs = micros();
  portENTER_CRITICAL(&teLock);
  const uint32_t previousEdgeCount = teEdgeCount;
  portEXIT_CRITICAL(&teLock);

  while (micros() - startUs < TE_TIMEOUT_US) {
    portENTER_CRITICAL(&teLock);
    const bool newEdge = teEdgeCount != previousEdgeCount;
    periodUs = tePeriodUs;
    portEXIT_CRITICAL(&teLock);

    if (newEdge) {
      return true;
    }
    delayMicroseconds(25);
  }
  return false;
}

void writePhoto() {
  lcd->beginWrite();
  lcd->writeC8D16D16(0x2A, 0, SCREEN_SIZE - 1);
  lcd->writeC8D16D16(0x2B, 0, SCREEN_SIZE - 1);
  lcd->writeCommand(0x2C);
  // On ESP32, flash is memory-mapped. Arduino_GFX's writePixels reads its input.
  lcd->writePixels(const_cast<uint16_t *>(IMAGE_PIXELS),
                   static_cast<uint32_t>(IMAGE_WIDTH) * IMAGE_HEIGHT);
  lcd->endWrite();
}

void setup() {
  Serial.begin(115200);
  if (!lcd->begin(QSPI_CLOCK_HZ, 0)) {
    Serial.println("LCD setup failed");
    return;
  }

  pinMode(PANEL_RESET_GPIO, OUTPUT);
  digitalWrite(PANEL_RESET_GPIO, HIGH);
  delay(20);
  digitalWrite(PANEL_RESET_GPIO, LOW);
  delay(120);
  digitalWrite(PANEL_RESET_GPIO, HIGH);
  delay(120);

  pinMode(PANEL_BACKLIGHT_GPIO, OUTPUT);
  digitalWrite(PANEL_BACKLIGHT_GPIO, LOW);
  pinMode(PANEL_TE_GPIO, INPUT_PULLDOWN);
  attachInterrupt(PANEL_TE_GPIO, onTeRising, RISING);
  lcd->batchOperation(panelInit, sizeof(panelInit));

  uint32_t tePeriod = 0;
  const bool synced = waitForNextTe(tePeriod);

  const uint32_t startUs = micros();
  writePhoto();
  const uint32_t frameUs = micros() - startUs;

  digitalWrite(PANEL_BACKLIGHT_GPIO, HIGH);

  if (!synced) {
    Serial.printf("TE timeout on GPIO%d. Photo sent without sync.\n", PANEL_TE_GPIO);
  } else if (tePeriod != 0) {
    Serial.printf("TE sync: %.2f Hz\n", 1000000.0 / tePeriod);
  } else {
    Serial.println("TE sync");
  }
  Serial.printf("Photo write: %.2f ms\n", frameUs / 1000.0);
}

void loop() {
  delay(1000);
}
