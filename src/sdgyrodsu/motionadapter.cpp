#include "sdgyrodsu/motionadapter.h"
#include "sdgyrodsu/sdhidframe.h"
#include "log/log.h"

#include <iostream>
#include <iomanip>
#include <chrono>

using namespace kmicki::motion;
using namespace kmicki::log;

#define SD_SCANTIME_US 4000
#define ACC_1G 0x4000
#define GYRO_1DEGPERSEC 16
#define GYRO_DEADZONE 8
#define ACCEL_SMOOTH 0x1FF

namespace kmicki::sdgyrodsu
{
    float SmoothAccel(float &last, int16_t curr)
    {
        static const float acc1G = (float)ACC_1G;
        if(abs(curr - last) < ACCEL_SMOOTH)
        {
            last = ((float)last*0.95+(float)curr*0.05);
        }
        else
        {
            last = (float)curr;
        }
        return last/acc1G;
    }

    uint64_t GetCurrentTimestamp()
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }

    uint64_t ToTimestamp(uint32_t const& increment)
    {
        return (uint64_t)increment * SD_SCANTIME_US;
    }

    void MotionAdapter::ConvertMotionData(const SdHidFrame& frame, SimpleMotionData &data, 
                                        float &lastAccelRtL, float &lastAccelFtB, float &lastAccelTtB,
                                        uint32_t frameId)
    {
        static const float gyro1dps = (float)GYRO_1DEGPERSEC;

        data.timestamp = GetCurrentTimestamp();
        data.frame_id = frameId;
        
        // Convert accelerometer data (with smoothing)
        data.accel_x = -SmoothAccel(lastAccelRtL, frame.AccelAxisRightToLeft);
        data.accel_y = -SmoothAccel(lastAccelFtB, frame.AccelAxisFrontToBack);
        data.accel_z = SmoothAccel(lastAccelTtB, frame.AccelAxisTopToBottom);
        
        // Convert gyroscope data
        if(frame.Header & 0xFF == 0xDD)
        {
            // No gyro data available
            data.gyro_pitch = 0.0f;
            data.gyro_yaw = 0.0f;
            data.gyro_roll = 0.0f;
        }
        else 
        {
            auto gyroRtL = frame.GyroAxisRightToLeft;
            auto gyroFtB = frame.GyroAxisFrontToBack;
            auto gyroTtB = frame.GyroAxisTopToBottom;

            // Apply deadzone
            if(gyroRtL < GYRO_DEADZONE && gyroRtL > -GYRO_DEADZONE)
                gyroRtL = 0;
            if(gyroFtB < GYRO_DEADZONE && gyroFtB > -GYRO_DEADZONE)
                gyroFtB = 0;
            if(gyroTtB < GYRO_DEADZONE && gyroTtB > -GYRO_DEADZONE)
                gyroTtB = 0;

            data.gyro_pitch = (float)gyroRtL / gyro1dps;
            data.gyro_yaw = -(float)gyroFtB / gyro1dps;
            data.gyro_roll = (float)gyroTtB / gyro1dps;
        }
        
        // Calculate magnitudes
        CalculateMagnitudes(data);
    }

    MotionAdapter::MotionAdapter(hiddev::HidDevReader & _reader, bool persistent)
    : reader(_reader),
      lastInc(0), frameCounter(0),
      lastAccelRtL(0.0), lastAccelFtB(0.0), lastAccelTtB(0.0),
      isPersistent(persistent), toReplicate(0), noGyroCooldown(0),
      frameServe(nullptr)
    {
        Log("MotionAdapter: Initialized. Waiting for start of frame grab.", LogLevelDebug);
    }

    void MotionAdapter::StartFrameGrab()
    {
        lastInc = 0;
        frameCounter = 0;
        ignoreFirst = true;
        Log("MotionAdapter: Starting frame grab.", LogLevelDebug);
        reader.Start();
        frameServe = &reader.GetServe();
    }

    bool MotionAdapter::GetMotionData(SimpleMotionData &motionData)
    {
        static const int64_t cMaxDiffReplicate = 100;
        static const int cNoGyroCooldownFrames = 1000;
        static const int cMaxRepeatedLoop = 1000;

        if(frameServe == nullptr)
            return false;

        if(noGyroCooldown > 0) --noGyroCooldown;

        auto const& dataFrame = frameServe->GetPointer();

        if(ignoreFirst)
        {
            auto lock = frameServe->GetConsumeLock();
            ignoreFirst = false;
        }

        int repeatedLoop = cMaxRepeatedLoop;

        while(true)
        {
            if(toReplicate == 0)
            {
                auto lock = frameServe->GetConsumeLock();
                auto const& frame = GetSdFrame(*dataFrame);

                // Check for gyro malfunction (all zeros)
                if( noGyroCooldown <= 0
                    &&  frame.AccelAxisFrontToBack == 0 && frame.AccelAxisRightToLeft == 0 
                    &&  frame.AccelAxisTopToBottom == 0 && frame.GyroAxisFrontToBack == 0 
                    &&  frame.GyroAxisRightToLeft == 0 && frame.GyroAxisTopToBottom == 0)
                {
                    NoGyro.SendSignal();
                    noGyroCooldown = cNoGyroCooldownFrames;
                }

                int64_t diff = (int64_t)frame.Increment - (int64_t)lastInc;

                if(lastInc != 0 && diff < 1 && diff > -100)
                {
                    if(repeatedLoop == cMaxRepeatedLoop)
                    {
                        Log("MotionAdapter: Frame was repeated. Ignoring...", LogLevelDebug);
                        { LogF(LogLevelTrace) << std::setw(8) << std::setfill('0') << std::setbase(16)
                                        << "Current increment: 0x" << frame.Increment << ". Last: 0x" << lastInc << "."; }
                    }
                    if(repeatedLoop <= 0)
                    {
                        Log("MotionAdapter: Frame is repeated continuously...");
                        return false;
                    }
                    --repeatedLoop;
                }
                else
                {
                    if(lastInc != 0 && diff > 1)
                    {
                        LogF logMsg((diff > 6)?LogLevelDefault:LogLevelDebug);
                        logMsg << "MotionAdapter: Missed " << (diff-1) << " frames.";
                        if(diff > 1000)
                            { LogF(LogLevelTrace) << std::setw(8) << std::setfill('0') << std::setbase(16)
                                     << "Current increment: 0x" << frame.Increment << ". Last: 0x" << lastInc << "."; }
                        if(diff <= cMaxDiffReplicate)
                        {
                            logMsg << " Replicating...";
                            toReplicate = diff-1;
                        }
                    }

                    ProcessFrame(frame, motionData);
                    
                    if(toReplicate > 0)
                    {
                        lastTimestamp = ToTimestamp(lastInc+1);
                        motionData.timestamp = lastTimestamp;
                        if(!isPersistent)
                            data = motionData;
                    }
                        
                    lastInc = frame.Increment;
                    return true;
                }
            }
            else
            {
                // Replicated frame
                --toReplicate;
                lastTimestamp += SD_SCANTIME_US;
                if(!isPersistent)
                {
                    motionData = data;
                    motionData.timestamp = lastTimestamp;
                }
                else
                {
                    motionData.timestamp = lastTimestamp;
                }

                return true;
            }
        }
    }

    void MotionAdapter::ProcessFrame(const SdHidFrame& frame, SimpleMotionData &motionData)
    {
        ConvertMotionData(frame, motionData, lastAccelRtL, lastAccelFtB, lastAccelTtB, ++frameCounter);
    }

    void MotionAdapter::StopFrameGrab()
    {
        Log("MotionAdapter: Stopping frame grab.", LogLevelDebug);
        if(frameServe != nullptr)
        {
            reader.StopServe(*frameServe);
            frameServe = nullptr;
        }
        reader.Stop();
    }

    bool MotionAdapter::IsControllerConnected()
    {
        return true;
    }
}