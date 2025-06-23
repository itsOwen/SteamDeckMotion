#include "motion/simplemotion.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace kmicki::motion
{
    void CalculateMagnitudes(SimpleMotionData& data)
    {
        // Calculate acceleration magnitude
        data.accel_magnitude = std::sqrt(
            data.accel_x * data.accel_x + 
            data.accel_y * data.accel_y + 
            data.accel_z * data.accel_z
        );
        
        // Calculate gyroscope magnitude  
        data.gyro_magnitude = std::sqrt(
            data.gyro_pitch * data.gyro_pitch + 
            data.gyro_yaw * data.gyro_yaw + 
            data.gyro_roll * data.gyro_roll
        );
    }

    std::string ToJson(const SimpleMotionData& data)
    {
        std::ostringstream json;
        json << std::fixed << std::setprecision(4);
        
        json << "{"
             << "\"timestamp\":" << data.timestamp << ","
             << "\"accel\":{"
             << "\"x\":" << data.accel_x << ","
             << "\"y\":" << data.accel_y << ","
             << "\"z\":" << data.accel_z
             << "},"
             << "\"gyro\":{"
             << "\"pitch\":" << data.gyro_pitch << ","
             << "\"yaw\":" << data.gyro_yaw << ","
             << "\"roll\":" << data.gyro_roll
             << "},"
             << "\"frameId\":" << data.frame_id << ","
             << "\"magnitude\":{"
             << "\"accel\":" << data.accel_magnitude << ","
             << "\"gyro\":" << data.gyro_magnitude
             << "}"
             << "}";
             
        return json.str();
    }
}