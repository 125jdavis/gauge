# GitHub Copilot Instructions
# Automotive Instrument Panel Controller

## Project Overview
Arduino-style C++ (.ino files) targeting Arduino and STM32 boards. Reads sensors, controls
stepper motors, communicates over CAN/SPI/Serial, drives displays, and uses hardware timers
for smooth motor motion.

---

## Top Priorities
- **Smooth motor motion is the highest priority.** Never compromise timer-driven motor
  control. Flag anything that could introduce jitter or interrupt conflicts.
- **Non-blocking code everywhere.** Never use `delay()` or any blocking call unless there
  is absolutely no alternative — and if so, ask first. Use `millis()`/`micros()` patterns.
- **When in doubt, ask.** Stop and ask clarifying questions rather than assume. If an
  assumption must be made, state it explicitly before proceeding.

---

## Platform (Arduino vs STM32)
- Use Arduino-style C++ and Arduino IDE conventions throughout.
- When writing hardware-specific code (timers, CAN, SPI, GPIO), always ask which platform
  before proceeding.
- Keep platform-specific code in separate files (e.g. `motor_arduino.ino` / `motor_stm32.ino`).
  Shared logic goes in common files with no platform dependencies.

---

## Code Style
- Clean, simple, readable code for a beginner-to-intermediate developer.
- Descriptive names for everything — no cryptic abbreviations.
- `#define` or `const` for all pin numbers, timing values, and magic numbers.
- Short, single-purpose functions. Avoid dynamic memory allocation.

---

## Comments
- Comments are welcome and encouraged, but keep them **brief** — one line where possible.
- Explain *why*, not just *what*. Focus on non-obvious logic and hardware interactions.
- At the top of any hardware-related file or function, note pin assignments and wiring assumptions.
- Flag timing/interrupt risks and significant memory usage inline with a short `// WARNING:` or `// NOTE:`.

---

## Repo Organization
- Keep the repo clean and organized.
- All documentation goes in the `/documentation` folder — not scattered in the root or source files.
- Tests should be written automatically when new code is added, but keep them **brief** —
  cover core behavior and key edge cases only, no exhaustive suites.

---

## Error Handling
- Do not add error handling by default. When a potential failure mode is spotted, flag it
  and ask how to handle it before adding any code.

---

## Communication (CAN, SPI, Serial)
- Note baud rate, message format, and protocol details in a brief comment at the top of
  any communication file or function.
- Flag any call that could block and warn if it risks disrupting motor control timing.
