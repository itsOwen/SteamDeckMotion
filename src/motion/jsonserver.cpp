#include "motion/jsonserver.h"
#include "log/log.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <cstdlib>

using namespace kmicki::sdgyrodsu;
using namespace kmicki::log;

namespace kmicki::motion
{
    const std::chrono::seconds JsonServer::cClientTimeout(30);

    const char * GetIP(sockaddr_in const& addr, char *buf)
    {
        return inet_ntop(addr.sin_family, &(addr.sin_addr.s_addr), buf, INET6_ADDRSTRLEN);
    }

    JsonServer::JsonServer(MotionAdapter & _motionSource)
        : motionSource(_motionSource), stop(false), serverThread(), stopSending(false),
          mainMutex(), stopSendMutex(), socketSendMutex(), socketFd(-1)
    {
        // Check for custom port
        if (const char* customPort = std::getenv("SDMOTION_SERVER_PORT")) {
            broadcastPort = std::atoi(customPort);
        } else {
            broadcastPort = cDefaultPort;
        }
        
        Start();
    }
    
    JsonServer::~JsonServer()
    {
        if(serverThread.get() != nullptr)
        {
            {
                std::lock_guard lock(mainMutex);
                stop = true;
            }
            serverThread.get()->join();
        }
        if(socketFd > -1)
            close(socketFd);
    }

    void JsonServer::Start() 
    {
        Log("JsonServer: Initializing.");
        if(serverThread.get() != nullptr)
        {
            {
                std::lock_guard lock(mainMutex);
                stop = true;
            }
            serverThread.get()->join();
            serverThread.reset();
        }

        socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        timeval read_timeout;
        read_timeout.tv_sec = 2;
        read_timeout.tv_usec = 0;
        setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));

        if(socketFd == -1)
            throw std::runtime_error("JsonServer: Socket could not be created.");
        
        sockaddr_in sockInServer;
        sockInServer = sockaddr_in();
        sockInServer.sin_family = AF_INET;
        sockInServer.sin_port = htons(broadcastPort);
        sockInServer.sin_addr.s_addr = INADDR_ANY;

        if(bind(socketFd, (sockaddr*)&sockInServer, sizeof(sockInServer)) < 0)
            throw std::runtime_error("JsonServer: Bind failed.");

        char ipStr[INET6_ADDRSTRLEN];
        ipStr[0] = 0;
        { LogF() << "JsonServer: Socket created at IP: " << GetIP(sockInServer, ipStr) 
                 << " Port: " << ntohs(sockInServer.sin_port) << "."; }

        stop = false;
        serverThread.reset(new std::thread(&JsonServer::serverTask, this));
        Log("JsonServer: Initialized.", LogLevelDebug);
    }

    void JsonServer::serverTask()
    {
        char buf[512];
        sockaddr_in sockInClient;
        socklen_t sockInLen = sizeof(sockInClient);

        std::unique_ptr<std::thread> sendThread;
        char ipStr[INET6_ADDRSTRLEN];

        Log("JsonServer: Start listening for clients.");
        
        std::unique_lock mainLock(mainMutex);
        while(!stop)
        {
            mainLock.unlock();
            
            // Listen for any UDP packet to register clients
            auto recvLen = recvfrom(socketFd, buf, sizeof(buf), 0, (sockaddr*)&sockInClient, &sockInLen);
            
            if(recvLen > 0)
            {
                std::ostringstream addressTextStream;
                addressTextStream << "IP: " << GetIP(sockInClient, ipStr) << " Port: " << ntohs(sockInClient.sin_port);
                auto addressText = addressTextStream.str();
                
                { LogF(LogLevelTrace) << "JsonServer: Client registration from " << addressText; }
                
                AddClient(sockInClient);
                
                // Start sending thread if not already running
                if(sendThread.get() == nullptr)
                {
                    stopSending = false;
                    sendThread.reset(new std::thread(&JsonServer::sendTask, this));
                    Log("JsonServer: Started sending motion data.");
                }
            }
            
            // Periodic cleanup of stale clients
            RemoveStaleClients();
            
            mainLock.lock();
        }

        if(sendThread.get() != nullptr)
        {
            Log("JsonServer: Stopping send thread...", LogLevelDebug);
            {
                std::lock_guard lock(stopSendMutex);
                stopSending = true;
            }
            sendThread.get()->join();
        }
        Log("JsonServer: Stopped.");
    }

    void JsonServer::sendTask()
    {
        Log("JsonServer: Initiating motion data streaming.", LogLevelDebug);
        motionSource.StartFrameGrab();

        const auto sendInterval = std::chrono::microseconds(1000000 / cSendRateHz); // 60Hz
        auto nextSend = std::chrono::steady_clock::now();

        Log("JsonServer: Start broadcasting motion data.", LogLevelDebug);

        std::unique_lock mainLock(stopSendMutex);

        while(!stopSending)
        {
            mainLock.unlock();
            
            SimpleMotionData motionData;
            if(motionSource.GetMotionData(motionData))
            {
                BroadcastMotionData(motionData);
            }
            
            // Rate limiting to 60Hz
            nextSend += sendInterval;
            std::this_thread::sleep_until(nextSend);
            
            mainLock.lock();
        }

        Log("JsonServer: Stopping motion data streaming.", LogLevelDebug);
        motionSource.StopFrameGrab();
        Log("JsonServer: Stop broadcasting motion data.", LogLevelDebug);
    }

    void JsonServer::AddClient(const sockaddr_in& clientAddr)
    {
        std::lock_guard lock(clientsMutex);
        
        // Check if client already exists
        auto client = std::find(clients.begin(), clients.end(), clientAddr);
        if(client != clients.end())
        {
            // Update last seen time
            client->lastSeen = std::chrono::steady_clock::now();
        }
        else
        {
            // Add new client
            Client newClient;
            newClient.address = clientAddr;
            newClient.lastSeen = std::chrono::steady_clock::now();
            clients.push_back(newClient);
            
            char ipStr[INET6_ADDRSTRLEN];
            { LogF() << "JsonServer: New client registered: " 
                     << GetIP(clientAddr, ipStr) << ":" << ntohs(clientAddr.sin_port); }
        }
    }

    void JsonServer::RemoveStaleClients()
    {
        std::lock_guard lock(clientsMutex);
        auto now = std::chrono::steady_clock::now();
        
        clients.erase(
            std::remove_if(clients.begin(), clients.end(),
                [now](const Client& client) {
                    return (now - client.lastSeen) > cClientTimeout;
                }),
            clients.end()
        );
    }

    void JsonServer::BroadcastMotionData(const SimpleMotionData& data)
    {
        std::string jsonData = ToJson(data);
        
        std::shared_lock lock(clientsMutex);
        for(const auto& client : clients)
        {
            std::lock_guard socketLock(socketSendMutex);
            sendto(socketFd, jsonData.c_str(), jsonData.length(), 0, 
                   (sockaddr*)&client.address, sizeof(client.address));
        }
    }

    bool JsonServer::Client::operator==(sockaddr_in const& other) const
    {
        return address.sin_addr.s_addr == other.sin_addr.s_addr
            && address.sin_port == other.sin_port;
    }

    bool JsonServer::Client::operator!=(sockaddr_in const& other) const
    {
        return !(*this == other);
    }
}