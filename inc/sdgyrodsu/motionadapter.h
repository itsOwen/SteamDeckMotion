#ifndef _KMICKI_SDGYRODSU_MOTIONADAPTER_H_
#define _KMICKI_SDGYRODSU_MOTIONADAPTER_H_

#include <cstdint>
#include <string>
#include "sdhidframe.h"
#include "motion/simplemotion.h"
#include "hiddev/hiddevreader.h"
#include "pipeline/serve.h"
#include "pipeline/signalout.h"

namespace kmicki::sdgyrodsu
{
    class MotionAdapter
    {
        public:
        MotionAdapter() = delete;
        MotionAdapter(hiddev::HidDevReader & _reader, bool persistent = true);

        void StartFrameGrab();
        
        // Get new motion data frame
        // Returns true if new data is available
        bool GetMotionData(kmicki::motion::SimpleMotionData &motionData);
        
        void StopFrameGrab();
        bool IsControllerConnected();

        // Static helper function for motion data conversion
        static void ConvertMotionData(const SdHidFrame& frame, kmicki::motion::SimpleMotionData &data, 
                                    float &lastAccelRtL, float &lastAccelFtB, float &lastAccelTtB,
                                    uint32_t frameId);

        pipeline::SignalOut NoGyro;

        private:
        bool ignoreFirst;
        bool isPersistent;

        kmicki::motion::SimpleMotionData data;
        hiddev::HidDevReader & reader;

        uint32_t lastInc;
        uint64_t lastTimestamp;
        uint32_t frameCounter;
        
        float lastAccelRtL;
        float lastAccelFtB;
        float lastAccelTtB;

        int toReplicate;
        int noGyroCooldown;

        pipeline::Serve<hiddev::HidDevReader::frame_t> * frameServe;
        
        // Helper function
        void ProcessFrame(const SdHidFrame& frame, kmicki::motion::SimpleMotionData &motionData);
    };
}

#endif