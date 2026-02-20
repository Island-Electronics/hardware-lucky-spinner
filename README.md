# Kypruino Hardware Lucky Spinner

Standalone firmware for an Arduino UNO-compatible board that performs fair offline random selection.

# Key Features

- **Hardware-based entropy**  
  Randomness is seeded using real-world timing jitter from human interaction (button presses) combined with internal timers.

- **Fair and unbiased selection**  
  Uniform random distribution across the full participant range using rejection sampling (no modulo bias).

- **No repeated winners**  
  Each selected item is guaranteed to be unique within a single session.

- **Deterministic after selection**  
  Once a winner is selected, the result cannot be influenced by timing, button press duration, or animation state.

- **Clear visual feedback**  
  Uses connected display, NeoPixel LEDs, and buzzer for intuitive user interaction.

- **Session-limited by design**  
  The number of selectable results is limited by the display capacity.  
  Restarting requires a hardware reset, preventing accidental re-runs.

## How It Works

### 1. Participant Range Setup
- Use **UP / DOWN** buttons to set the total number of selectable items (`N`).
- Use **LEFT** to decrease value faster (`-10`).
- Press **RIGHT** to move to winner count setup.

### 2. Winner Count Setup
- Set how many winners should be selected in this session.
- Use the same controls: **UP / DOWN** (`+/-1`), **LEFT** (`-10`), **RIGHT** (start).
- Winner count is automatically constrained to `1..min(N, 120)`.

### 3. Random Selection Process
- The firmware continuously animates a fast-changing numeric "runner" across the full range `1…N`.
- Internally, the actual winner is **preselected** using a fair random algorithm.
- The animation is purely visual and does not affect the outcome.

### 4. Result Confirmation
- Press **RIGHT** to stop the animation and reveal the next selected value.
- The result is added to the on-screen list.

### 5. Multiple Selections
- Repeat the process to select additional unique values.
- Selection stops automatically once the target winner count is reached.

### 6. Reset
- A hardware reset is required to start a new session.

## Hardware Configuration (Current Board)

- Buttons:
  - `D6` -> `UP`
  - `D7` -> `LEFT`
  - `D4` -> `RIGHT`
  - `D2` -> `DOWN`
- NeoPixel strip: `D8`, `3` RGB LEDs
- OLED display: `128x32 SSD1306` on standard I2C pins (`SDA/SCL`), no reset pin
- Buzzer: `D9`

Full pin map: `docs/PINOUT.md`

## Controls

- Setup screen 1 (participants):
  - `UP`: increase participants by `+1`
  - `DOWN`: decrease participants by `-1`
  - `LEFT`: decrease participants by `-10`
  - `RIGHT`: next setup step
- Setup screen 2 (winner count):
  - `UP`: increase winners by `+1`
  - `DOWN`: decrease winners by `-1`
  - `LEFT`: decrease winners by `-10`
  - `RIGHT`: start spinning
- Spin screen:
  - `RIGHT`: catch and reveal the preselected winner
- Winner list screen:
  - `RIGHT`: move to the next selection round
- After target winner count is reached:
  - Use hardware reset to start a new session

## Display and LED Tuning

- The menu layout is optimized for `128x32` displays.
- NeoPixel brightness can be reduced with `LED_BRIGHTNESS_DIVIDER` in `firmware/lucky_spinner/lucky_spinner.ino`:
  - `1` = full brightness
  - `2..18...` = progressively dimmer output

## Fairness Model

- PRNG: `xorshift32`
- Mapping to `1..N`: rejection sampling (no modulo bias)
- The next winner is chosen before the user presses catch, so button timing only affects reveal timing, not winner identity

## Repository Structure

```
.
├── firmware/
│   └── lucky_spinner/
│       └── lucky_spinner.ino
├── docs/
│   └── PINOUT.md
├── LICENSE
└── README.md
```

## Build and Upload

### Arduino IDE

1. Open sketch: `firmware/lucky_spinner/lucky_spinner.ino`
2. Select your UNO-compatible board and serial port
3. Install required libraries if needed:
   - `Adafruit GFX Library`
   - `Adafruit SSD1306`
   - `Adafruit NeoPixel`
4. Compile and upload

## Notes

- Maximum participant value is `9999`.
- Maximum storable winners is `120` (RAM-safe limit), and actual per-session winner target is constrained to `1..min(N, 120)`.
- This project is intentionally simple and transparent for demos, classrooms, and small giveaways.

## License

See `LICENSE`.
