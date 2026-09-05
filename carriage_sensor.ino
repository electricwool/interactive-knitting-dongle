/*
 * DesignaKnit Carriage-Pass Sensor — Arduino Nano v3
 *
 * Emulates the FT230XS carriage-pass sensor described in ../protocol.md.
 *
 * The Arduino Nano enumerates as an FTDI FT232RL USB-serial port. Its TXD
 * pin (PD1) is wired to the FTDI chip's RXD input. Holding that line low
 * for longer than one character frame is reported by the FTDI to the host
 * as a UART "break" plus a received 0x00 byte — the same event DesignaKnit
 * uses to increment the row counter (protocol.md §5–6, "and/or" rule).
 *
 * No serial data is ever transmitted; the ATmega only drives the line.
 *
 * Sensor wiring (analog pin A1):
 *   - internal pull-up enabled
 *   - reed switch (or equivalent) from A1 to GND
 *   - the carriage magnet closes the switch, pulling A1 below 1.0 V
 */

// ---- Pin definitions -------------------------------------------------------
const int SENSOR_PIN = A1;  // analog input A1, internal pull-up active
const int TXD_PIN    = 1;   // ATmega TXD (PD1) -> FTDI RXD input

// ---- Timing / thresholds ---------------------------------------------------
// 1.0 V threshold on a 5 V / 10-bit ADC: 1.0 / 5.0 * 1023 ≈ 205 counts.
// A reading below this means the sensor line is pulled low.
const int THRESHOLD_COUNTS = 205;

const unsigned long DEBOUNCE_MS    = 10;   // low condition must hold this long
const unsigned long REARM_DELAY_MS = 100;  // re-arm 100 ms after the line goes high
const unsigned long BREAK_MS       = 5;    // TX line held low (1 frame @57600 ≈ 174 µs)

enum State : uint8_t {
  ARMED,  // waiting for the next carriage pass
  FIRED   // signal sent; waiting for release + re-arm delay (monostable)
};

State state = ARMED;

unsigned long lowStart  = 0;  // when the line first went low (debounce)
unsigned long highStart = 0;  // when the line first went high (re-arm)

void setup() {
  // TX line idle-high: no break, no 0x00 reported.
  pinMode(TXD_PIN, OUTPUT);
  digitalWrite(TXD_PIN, HIGH);

  // Sensor input with internal pull-up.
  pinMode(SENSOR_PIN, INPUT_PULLUP);
}

void loop() {
  bool isLow = analogRead(SENSOR_PIN) < THRESHOLD_COUNTS;
  unsigned long now = millis();

  switch (state) {
    case ARMED:
      if (isLow) {
        if (lowStart == 0) {
          lowStart = now;
        } else if (now - lowStart >= DEBOUNCE_MS) {
          fireCarriagePass();
          state = FIRED;
          lowStart = 0;
          highStart = 0;
        }
      } else {
        lowStart = 0;  // not a stable low yet
      }
      break;

    case FIRED:
      // Monostable: ignore the sensor until the line has gone back high and
      // stayed high for REARM_DELAY_MS. A carriage parked at the sensor
      // produces exactly one signal, not repeated ones.
      if (isLow) {
        highStart = 0;  // still parked at the sensor; keep waiting
      } else {
        if (highStart == 0) {
          highStart = now;
        } else if (now - highStart >= REARM_DELAY_MS) {
          state = ARMED;
          highStart = 0;
        }
      }
      break;
  }
}

// Emit one carriage-pass event: hold the FTDI RXD line low for BREAK_MS.
// The FTDI reports this to DesignaKnit as break + 0x00 (one row increment).
void fireCarriagePass() {
  digitalWrite(TXD_PIN, LOW);
  delay(BREAK_MS);
  digitalWrite(TXD_PIN, HIGH);
}
