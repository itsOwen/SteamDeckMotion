#ifndef _KMICKI_MOTION_SIMPLEMOTION_H_
#define _KMICKI_MOTION_SIMPLEMOTION_H_

#include <cstdint>
#include <string>

namespace kmicki::motion
{
    struct SimpleMotionData
    {
        uint64_t timestamp;      // Microseconds since epoch
        float accel_x;          // Acceleration X-axis (G units)
        float accel_y;          // Acceleration Y-axis (G units)  
        float accel_z;          // Acceleration Z-axis (G units)
        float gyro_pitch;       // Gyroscope pitch (degrees/second)
        float gyro_yaw;         // Gyroscope yaw (degrees/second)
        float gyro_roll;        // Gyroscope roll (degrees/second)
        uint32_t frame_id;      // Frame counter
        float accel_magnitude;  // Total acceleration magnitude
        float gyro_magnitude;   // Total gyroscope magnitude
    };

    // Helper function to calculate magnitudes
    void CalculateMagnitudes(SimpleMotionData& data);
    
    // Convert to JSON string
    std::string ToJson(const SimpleMotionData& data);
}

#endif