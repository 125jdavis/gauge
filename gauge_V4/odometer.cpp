/*
 * ========================================
 * ODOMETER FUNCTIONS IMPLEMENTATION
 * ========================================
 */

#include "odometer.h"
#include "globals.h"

/**
 * updateOdometer - Calculate and update odometer based on speed and time
 */
float updateOdometer(float speedKmh, unsigned long timeIntervalMs) {
    float distanceTraveled = 0;
    
    // Only integrate if speed > 2 km/h (reduces GPS drift errors when stationary)
    if (speedKmh > 2) {
        // Calculate distance traveled: distance (km) = speed (km/h) * time (ms) * conversion factor
        // Conversion: 1 km/h = 1000m/3600s = 0.277778 m/s = 2.77778e-7 km/ms
        distanceTraveled = speedKmh * timeIntervalMs * 2.77778e-7;
    }
    
    // Update odometers
    odo = odo + distanceTraveled;
    odoTrip = odoTrip + distanceTraveled;
    
    return distanceTraveled;
}

/**
 * moveOdometerMotor - Move mechanical odometer motor by calculated distance
 */
void moveOdometerMotor(float distanceKm) {
    // Calculate steps required to move odometer motor
    // 
    // Formula breakdown:
    // 1. ODO_GEAR_TEETH / ODO_MOTOR_TEETH = gear ratio (how many motor revs per odo rev)
    // 2. distanceKm = distance traveled
    // 3. Assuming odometer advances 1 unit per full rotation (adjust based on actual mechanical design)
    // 4. steps = distanceKm * gearRatio * ODO_STEPS
    
    // For now, calculate the steps but don't move the motor yet
    // This is a placeholder for the actual motor movement logic
    
    // Example calculation (adjust based on actual mechanical odometer):
    // If odometer advances 0.1 km per revolution:
    // float odoRevsPerKm = 10.0;  // 10 revs = 1 km
    // float odoRevs = distanceKm * odoRevsPerKm;
    // float gearRatio = (float)ODO_GEAR_TEETH / (float)ODO_MOTOR_TEETH;
    // float motorRevs = odoRevs * gearRatio;
    // int steps = (int)(motorRevs * ODO_STEPS);
    
    // TODO: Add actual motor movement using odoMotor.step(steps)
    // Consider implementing a non-blocking approach if motor movement is slow
}
