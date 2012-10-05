#ifndef WIN32
# include <errno.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <ifaddrs.h>
#else
# include <windows.h>
# include <winsock.h>
# pragma comment(lib, "wsock32.lib")
#endif
#include <cstdio>
#include <cstring>
#include <iostream>
#include "logging.h"
#include "udp_broadcast.h"



#ifndef IPPROTO_UDP
#define IPPROTO_UDP 0
#endif

UDP_Broadcast::UDP_Broadcast(Logging* log) throw (UDP_Broadcast::IOError)
    : log(log)
{
#ifdef WIN32
    WORD sockVersion;
    WSADATA wsaData;
    sockVersion = MAKEWORD(1,1);
    WSAStartup(sockVersion, &wsaData);
#endif

    sock = -1;
    
#ifndef WIN32
    //get pointer to list of network interfaces (does not work with WIN32)
    ifacenum=0;
    struct ifaddrs *ifap = 0;
    if (-1 == getifaddrs(&ifap))
    {
        log->add("could not get list of network interfaces");
        throw UDP_Broadcast::IOError("Could not get list of network interfaces", errno);
    }
    struct ifaddrs *it = ifap;
    int ifacecounter = 0;
    while(ifacenum < 32 && it)
    {
        if(it->ifa_addr
           && AF_INET==it->ifa_addr->sa_family
           && std::string("lo") != it->ifa_name)
        {
            ifaddr[ifacenum]=((struct sockaddr_in*)(it->ifa_addr))->sin_addr;
            log->add("IPv4_Multicast: Interface found: %s (%s)", it->ifa_name, inet_ntoa(ifaddr[ifacenum]));
            ++ifacenum;
        }

        ++ifacecounter;
        it = it->ifa_next;
    }
    
#else
     log->add("ERROR(WIN32): Can not get list of network interfaces. "
            "NOT IMPLEMENTED YET!");
#endif

    // create socket
    sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( sock == -1 )
    {
        perror("could not create socket\n");
        throw UDP_Broadcast::IOError("Could not create socket", errno);
    }
}

UDP_Broadcast::~UDP_Broadcast()
{
#ifndef WIN32
    close(sock);
#endif
}


void UDP_Broadcast::set_destination(const std::string& host,
                                    const uint16_t port) throw (UDP_Broadcast::IOError)
{
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#ifndef WIN32
    if (! inet_aton(host.c_str(), &addr.sin_addr))
    {
        throw IOError("inet_aton failed. Invalid address: " + host);
    }
#else
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    if (addr.sin_addr.s_addr == 0)
    {
        throw IOError("Invalid address: " + host);
    } 
#endif
    int yes = 1;

    // allow packets to be received on this host        
#ifndef WIN32
    if (-1 == setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, 
                         (const char*)&yes, sizeof(yes)))
    {
        throw IOError("could not set multicast loop", errno);
    }
#endif
}

void UDP_Broadcast::send(const void* buffer, const size_t buflen) throw (IOError)
{   
#ifndef WIN32
    for(int i=0; i<ifacenum; ++i)
    {
        //select network interface
        struct ip_mreqn mreqn;
        mreqn.imr_ifindex=0;
        mreqn.imr_multiaddr=addr.sin_addr;
        mreqn.imr_address=ifaddr[i];

        if (-1 == setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof(mreqn)))
        {
            throw UDP_Broadcast::IOError(
                std::string("setsockopt() failed: ") + strerror(errno) );

        }

        //send data
        ssize_t result = ::sendto(sock, (const char*)buffer, buflen, 0,
                              (struct sockaddr*) &addr, sizeof(addr));
        if (result == -1)
        {
            throw UDP_Broadcast::IOError(
                std::string("send() failed for iface : ") + strerror(errno));
        }
    }
    if (!ifacenum)
    {
        //no usable interfaces; the user is probably using loopback and has no network connection
        ssize_t result = ::sendto(sock, (const char *)buffer, buflen, 0,
                              (struct sockaddr*) &addr, sizeof(addr));
        if (result == -1)
        {
            throw UDP_Broadcast::IOError(
                std::string("send() failed : ") + strerror(errno));
        }
    }
#else
    //send data
    ssize_t result = ::sendto(sock, (const char*)buffer, buflen, 0,
                              (struct sockaddr*) &addr, sizeof(addr));
    if (result == -1)
    {
        throw UDP_Broadcast::IOError(
            std::string("send() failed for iface : ") + strerror(errno));
    }
#endif //WIN32
}

void UDP_Broadcast::send(const std::string& buffer) throw (IOError)
{
    send(buffer.c_str(), buffer.size());
}

