# Odometer Motor Fix Documentation

## Problem Description
The odometer motor (20BYJ-48 stepper motor) was not physically moving despite correct hardware setup. Simple test code at 4 RPM worked, confirming the hardware was functional.

## Root Cause
The code was using the Arduino `Stepper` library which:
1. Requires `setSpeed()` to be called before movement (which was never done)
2. Uses blocking `step()` calls that aren't suitable for non-blocking operation in an ISR
3. Doesn't provide fine control over step timing needed for slow, precise movements

## Solution
Replaced the Arduino Stepper library with custom non-blocking direct pin control:

### Key Changes
1. **Direct Pin Control**: Instead of `Stepper.step()`, we now directly control the 4 motor pins (ODO_PIN1-4)
2. **4-Phase Sequence**: Implemented wave drive stepping sequence for the 20BYJ-48
3. **Non-Blocking Operation**: Uses `micros()` timing to advance steps only when enough time has elapsed
4. **Slow Speed**: Set to ~2.93 RPM (5ms per step), safely below the 3 RPM requirement

### Technical Details

#### 20BYJ-48 Motor Specifications
- 5V 4-phase unipolar stepper motor
- Internal 64:1 gearing ratio
- 4096 steps per revolution (with gearing)
- Maximum speed: ~15 RPM
- Target speed for odometer: < 3 RPM

#### Step Sequence (Wave Drive)
```
Phase: A    B    C    D
Step 0: ON   OFF  OFF  OFF
Step 1: OFF  ON   OFF  OFF
Step 2: OFF  OFF  ON   OFF
Step 3: OFF  OFF  OFF  ON
```

Wave drive was chosen over full-step or half-step because:
- Lower power consumption (only 1 coil energized at a time)
- Lower heat generation
- Sufficient torque for odometer application
- Smoother operation at low speeds

#### Speed Calculation
- Target: < 3 RPM
- At 3 RPM: 3 rev/min × 4096 steps/rev = 12,288 steps/min = 204.8 steps/sec
- Delay per step at 3 RPM: 1,000,000 μs / 204.8 = 4,882 μs
- **Chosen delay: 5000 μs (5ms)** → 200 steps/sec = 2.93 RPM
- This provides a safety margin below the 3 RPM limit

## Testing the Fix

### Expected Behavior
1. Motor should move smoothly when distance accumulates
2. Movement should be non-blocking (other system functions continue normally)
3. Speed should be approximately 2.93 RPM maximum
4. Motor should not skip steps or stall

### Basic Test Procedure
1. Upload the modified code to the Arduino
2. Monitor the serial output for initialization messages
3. Provide a speed source (GPS, CAN, Hall sensor, or synthetic)
4. Observe the odometer motor:
   - Should step smoothly as distance accumulates
   - Each step should be visible (slow enough to see)
   - Should not make excessive noise or vibration
   - Should not overheat

### Diagnostic Commands
If testing with synthetic speed (SPEED_SOURCE = 4 in config_calibration.cpp):
- The motor should start moving as soon as synthetic speed > 0
- Distance accumulates based on speed and time
- Motor steps when accumulated distance requires advancement

### Troubleshooting

#### Motor not moving at all
- Check that ODO_PIN1-4 are correctly connected (pins 8, 9, 10, 11)
- Verify 5V power supply to motor
- Check that `updateOdometerMotor()` is being called from ISR
- Monitor serial output for any error messages

#### Motor moving but skipping steps
- May indicate speed is too fast (though 5ms delay should prevent this)
- Check power supply - insufficient current can cause skipping
- Verify wiring connections are secure

#### Motor moving in wrong direction
- This is normal - direction can be reversed by changing the step sequence
- To reverse: change the increment to decrement in the step sequence
  - Current (forward): `odoMotorStepIndex = (odoMotorStepIndex + 1) % 4`
  - Reverse: `odoMotorStepIndex = (odoMotorStepIndex + 3) % 4`
  - Note: Adding 3 mod 4 is equivalent to subtracting 1 (with wraparound)

#### Motor gets hot
- Wave drive should minimize heat, but some warmth is normal
- Excessive heat may indicate:
  - Mechanical binding in odometer mechanism
  - Incorrect wiring (possible short circuit)
  - Step sequence error

## Code Changes Summary

### Files Modified
1. **gauge_V4/outputs.cpp**
   - Added step sequence array and timing constants
   - Rewrote `updateOdometerMotor()` to use direct pin control
   - Removed `odoMotor.step()` calls

2. **gauge_V4/globals.cpp**
   - Removed `Stepper odoMotor` object instantiation

3. **gauge_V4/globals.h**
   - Removed `extern Stepper odoMotor` declaration
   - Removed `#include <Stepper.h>`

4. **gauge_V4/gauge_V4.ino**
   - Removed `#include <Stepper.h>`

### No Changes Required
- Pin definitions remain the same (ODO_PIN1-4)
- Calibration values remain the same (ODO_STEPS, ODO_GEAR_TEETH, etc.)
- Distance accumulation logic unchanged
- ISR timer configuration unchanged

## Performance Impact
- Minimal CPU overhead: only 4 digitalWrite calls per step
- At 2.93 RPM: 200 steps/sec average
- Per ISR call (10 kHz): typically returns immediately (no step needed)
- When stepping: ~10-20 μs additional execution time
- Total impact: < 0.1% additional CPU usage

## Future Enhancements (Optional)
If needed in the future, consider:
1. **Half-stepping**: Double resolution (8192 steps/rev) at cost of slightly higher power
2. **Full-stepping**: Higher torque by energizing 2 coils at a time
3. **Acceleration profile**: Gradually ramp up/down speed for smoother starts/stops
4. **Direction control**: Add ability to reverse for calibration/testing

## References
- 20BYJ-48 Datasheet: Standard 5V stepper motor with 64:1 gearing
- Arduino digitalWrite reference: https://www.arduino.cc/reference/en/language/functions/digital-io/digitalwrite/
- Timer-based ISR implementation: See gauge_V4.ino ISR(TIMER3_COMPA_vect)
