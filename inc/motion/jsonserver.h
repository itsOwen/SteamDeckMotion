#ifndef _KMICKI_MOTION_JSONSERVER_H_
#define _KMICKI_MOTION_JSONSERVER_H_

#include "motion/simplemotion.h"
#include "sdgyrodsu/motionadapter.h"
#include <thread>
#include <netinet/in.h>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <chrono>

namespace kmicki::motion
{
    class JsonServer
    {
        public:
        JsonServer() = delete;
        JsonServer(kmicki::sdgyrodsu::MotionAdapter & _motionSource);
        ~JsonServer();

        private:
        
        struct Client
        {
            sockaddr_in address;
            std::chrono::steady_clock::time_point lastSeen;
            
            bool operator==(sockaddr_in const& other) const;
            bool operator!=(sockaddr_in const& other) const;
        };

        std::mutex mainMutex;
        std::mutex stopSendMutex;
        std::mutex socketSendMutex;
        std::shared_mutex clientsMutex;

        bool stop;
        bool stopSending;

        int socketFd;
        int broadcastPort;

        kmicki::sdgyrodsu::MotionAdapter & motionSource;
        std::unique_ptr<std::thread> serverThread;

        void serverTask();
        void sendTask();
        void Start();
        
        std::vector<Client> clients;
        
        void AddClient(const sockaddr_in& clientAddr);
        void RemoveStaleClients();
        void BroadcastMotionData(const SimpleMotionData& data);
        
        static const int cDefaultPort = 27760;
        static const int cSendRateHz = 60;  // 60Hz output (down from 250Hz input)
        static const std::chrono::seconds cClientTimeout;
    };
}

#endif