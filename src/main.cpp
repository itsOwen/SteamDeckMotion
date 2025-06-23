#include "hiddev/hiddevreader.h"
#include "hiddev/hiddevfinder.h"
#include "sdgyrodsu/sdhidframe.h"
#include "sdgyrodsu/motionadapter.h"
#include "motion/jsonserver.h"
#include "log/log.h"
#include <iostream>
#include <future>
#include <thread>
#include <csignal>

using namespace kmicki::sdgyrodsu;
using namespace kmicki::hiddev;
using namespace kmicki::log;
using namespace kmicki::motion;

const LogLevel cLogLevel = LogLevelDebug;
const bool cUseHiddevFile = false;

const int cFrameLen = 64;       // Steam Deck Controls' custom HID report length in bytes
const int cScanTimeUs = 4000;   // Steam Deck Controls' period between received report data in microseconds
const uint16_t cVID = 0x28de;   // Steam Deck Controls' USB Vendor-ID
const uint16_t cPID = 0x1205;   // Steam Deck Controls' USB Product-ID
const int cInterfaceNumber = 2; // Steam Deck Controls' USB Interface Number

const std::string cVersion = "3.0-motion";   // Release version

bool stop = false;
std::mutex stopMutex = std::mutex();
std::condition_variable stopCV = std::condition_variable();

void SignalHandler(int signal)
{
    {
        LogF msg;
        msg << "Incoming signal: ";
        bool stopCmd = true;
        switch(signal)
        {
            case SIGINT:
                msg << "SIGINT";
                break;
            case SIGTERM:
                msg << "SIGTERM";
                break;
            default:
                msg << "Other";
                stopCmd = false;
                break;
        }
        if(!stopCmd)
        {
            msg << ". Unhandled, ignoring...";
            return;
        }
        msg << ". Stopping...";
    }

    {
        std::lock_guard lock(stopMutex);
        stop = true;
    }
    stopCV.notify_all();
}

int main()
{
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    stop = false;
    SetLogLevel(cLogLevel);

    { LogF() << "SteamDeck Motion Service Version: " << cVersion; }
    { LogF() << "Serving JSON motion data over UDP"; }

    std::unique_ptr<HidDevReader> readerPtr;

    if(cUseHiddevFile)
    {
        int hidno = FindHidDevNo(cVID, cPID);
        if(hidno < 0) 
        {
            Log("Steam Deck Controls' HID device not found.");
            return 1;
        }

        { LogF() << "Found Steam Deck Controls' HID device at /dev/usb/hiddev" << hidno; }
        
        readerPtr.reset(new HidDevReader(hidno, cFrameLen, cScanTimeUs));
    }
    else
    {
        Log("Using HIDAPI for Steam Deck Controls access.");
        readerPtr.reset(new HidDevReader(cVID, cPID, cInterfaceNumber, cFrameLen, cScanTimeUs));
    }

    HidDevReader &reader = *readerPtr;

    // Set frame start marker for Steam Deck HID frames
    reader.SetStartMarker({ 0x01, 0x00, 0x09, 0x40 });

    // Create motion adapter and server
    MotionAdapter adapter(reader);
    reader.SetNoGyro(adapter.NoGyro);
    
    JsonServer server(adapter);

    Log("Motion service started. Press Ctrl+C to stop.");

    // Wait for stop signal
    {
        std::unique_lock lock(stopMutex);
        stopCV.wait(lock, []{ return stop; });
    }

    Log("SteamDeck Motion Service exiting.");

    return 0;
}