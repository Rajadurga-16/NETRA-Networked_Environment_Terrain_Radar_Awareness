/*
  NETRA - Vehicle A Pin Assignment (v3: HC-SR04 ultrasonic + L298N motor driver module)
  ESP32 DevKit V1

  Rules followed:
  - Avoid strapping pins for anything critical: GPIO0, GPIO2, GPIO5, GPIO12, GPIO15
  - Avoid input-only pins (GPIO34-39) for outputs
  - Keep UART0 (GPIO1/GPIO3) completely free -> this is the USB link to the laptop
  - GPS gets its own hardware UART (UART1). No radar UART needed -
    HC-SR04 is a simple trigger/echo digital sensor, not a UART device.
*/

#ifndef NETRA_PINS_H
#define NETRA_PINS_H

// ---------- 16x2 Character LCD (I2C backpack, HD44780-compatible) ----------
#define LCD_SDA         21
#define LCD_SCL         22
#define LCD_COLS        16
#define LCD_ROWS        2
#define LCD_I2C_ADDR    0x27   // most common I2C-backpack address; some boards use 0x3F.
                                // Run firmware/i2c_scanner/i2c_scanner.ino if unsure.

// ---------- GPS (NEO-6M) on UART1 ----------
#define GPS_RX_PIN      16
#define GPS_TX_PIN      17
#define GPS_BAUD        9600

// ---------- HC-SR04 Ultrasonic obstacle sensor ----------
#define ULTRASONIC_TRIG_PIN   4    // ESP32 output -> HC-SR04 TRIG (direct, no divider needed)
#define ULTRASONIC_ECHO_PIN   32   // HC-SR04 ECHO -> voltage divider -> ESP32 input (SEE NOTE 2)

// ---------- Buzzer & LEDs ----------
#define BUZZER_PIN      25
#define LED_RED_PIN     26
#define LED_GREEN_PIN   33

// ---------- L298N Motor Driver MODULE (the red board with screw terminals,
// NOT the bare L293D chip - your board already has its own onboard logic,
// so wiring is just 4 direction pins, nothing else from the ESP32 side) ----
#define MOTOR_LEFT_IN1   13   // L298N IN1
#define MOTOR_LEFT_IN2   14   // L298N IN2
#define MOTOR_RIGHT_IN3  18   // L298N IN3
#define MOTOR_RIGHT_IN4  19   // L298N IN4
// ENA and ENB stay on the L298N board's own jumper caps (left ON) - this
// keeps both motors permanently enabled at full speed; the ESP32 does not
// wire to ENA/ENB at all in this MVP. See note 5 below to add PWM speed later.

/*
  WIRING NOTES (read ALL of these before powering anything on):

  1. 16x2 I2C LCD backpack: needs 5V for a readable backlight; its backpack
     handles level-shifting so SDA/SCL are safe straight to GPIO21/GPIO22.
     If you see a blank row of black boxes, turn the small blue potentiometer
     on the back of the backpack (that's contrast, not a wiring fault).

  2. HC-SR04 voltage divider (IMPORTANT - skipping this can damage your
     ESP32's GPIO32 pin):
       HC-SR04 ECHO pin outputs a 5V pulse. ESP32 GPIOs are only 3.3V
       tolerant. Build this simple divider using THREE of your 220-ohm
       resistors (no need to buy 1k/2k - this ratio does the same job):

         HC-SR04 ECHO ---[220 ohm]---+---[220 ohm]---[220 ohm]--- GND
                                      |
                                      +--- ESP32 GPIO32 (ULTRASONIC_ECHO_PIN)

       One 220-ohm resistor between ECHO and the junction node, then TWO
       220-ohm resistors in series (440 ohm total) from that node to GND.
       This divides 5V down to ~3.3V (5V x 440/660 = 3.33V) - safe for the
       ESP32 input.
       HC-SR04 TRIG: wire directly from ESP32 GPIO4, no divider needed (it's
       an ESP32 OUTPUT going INTO the sensor; HC-SR04 reliably reads a 3.3V
       pulse as logic HIGH).
       HC-SR04 VCC -> 5V, GND -> GND (shared ground with everything else).

  3. POWER DOMAINS - the #1 rule for "no brownout, no damage":
       - ESP32 + LCD + GPS + HC-SR04 + buzzer + LEDs ALL run off the ESP32's
         own 5V/3.3V rails, powered via the USB cable (laptop or a power
         bank). This is a completely separate, clean power domain.
       - The two DC motors run on the SEPARATE motor battery pack (your two
         18650 cells), wired to the L298N module's own "+12V"/motor-power
         screw terminal - NOT through the ESP32 at all.
       - L298N IN1-IN4 are simple 3.3V-logic-compatible control inputs on
         the module - wire them straight to the 4 ESP32 GPIOs above. No
         extra logic-supply pin to wire (that's only a concern with the
         bare L293D chip, not this L298N module).
       - ALL grounds (ESP32 GND, L298N GND, motor battery GND) MUST be tied
         together on the breadboard's ground rail. Skipping this "common
         ground" is the single most common reason a two-power-supply robot
         behaves randomly or not at all.
       - NEVER wire the motor battery's raw voltage into the ESP32's 5V/VIN
         pin. Power the ESP32 only from USB during testing and demo.
       - Leave the L298N's own "+5V output" pin (and its 5V-EN jumper)
         alone / disconnected - don't use it to power the ESP32. Motor
         current spikes would ride straight into it and defeat the whole
         point of separating the power domains.

  4. 100uF electrolytic capacitor: connect it directly across the L298N
     module's motor-power screw terminal (+ and -), as close to the module
     as possible, on the battery side. Mind polarity - the capacitor's
     longer leg (or the side NOT marked with a stripe) is positive, goes to
     the + terminal. This absorbs voltage spikes when the motors start/stop
     so they don't glitch the shared ground / reset the ESP32.

  5. ENA/ENB left on the board's jumpers (full speed always) is a deliberate
     MVP simplification. If you later want variable speed, remove those two
     jumpers and wire ENA/ENB to two more ESP32 GPIOs (e.g. GPIO23, GPIO27)
     driven with ledcWrite() PWM - optional upgrade, not needed for the demo.

  6. Buzzer: active buzzer (built-in oscillator), direct digitalWrite
     HIGH/LOW straight from GPIO25, no transistor needed for this MVP. The
     2N2222/BC547 you bought are NOT required here - keep them as spares;
     they'd matter if you later swap to a passive/louder siren buzzer that
     draws more current than a GPIO can safely source.

  7. LEDs: each through a 220-ohm resistor to GND.
*/

#endif
