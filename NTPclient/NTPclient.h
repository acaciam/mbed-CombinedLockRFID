
#include "mbed.h"
 
#ifndef NTPCLIENT_H
#define NTPCLIENT_H

#include <stdint.h>
#include "UDPSocket.h"
#include "NetworkInterface.h"

#define NTP_DEFAULT_PORT 123
#define NTP_DEFAULT_TIMEOUT 4000
#define NTP_DEFAULT_TIMEZONE_OFFSET 0

class NTPclient
{
public:  
    NTPclient(NetworkInterface & _m_intf);
    // Returns current time in seconds (blocking)
    // Update the time using the server host
    // Blocks until completion
    // NTPpool, NTP server IPv4 address or hostname (will be resolved via DNS)
    // tzoffset, offset in seconds (3600 add 1 hour, -3600 subtract 1 hour)
    // dst, adjust for DST 1= enabled, 0=dissabled 
    // setRTC, set system RTC 1= enabled, 0=dissabled 

    uint32_t getNTP(const char* NTPpool, uint32_t tzoffset, bool dst, bool setRTC); 

private:
    struct NTPPacket
    {
    //We are in LE Network is BE
    //LSb first
    unsigned mode : 3;
    unsigned vn : 3;
    unsigned li : 2;

    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    
    //32 bits header
    uint32_t rootDelay;
    uint32_t rootDispersion;
    uint32_t refId;
    uint32_t refTm_s;
    uint32_t refTm_f;
    uint32_t origTm_s;
    uint32_t origTm_f;
    uint32_t rxTm_s;
    uint32_t rxTm_f;
    uint32_t txTm_s;
    uint32_t txTm_f;
  } __attribute__ ((packed));
  
  NetworkInterface & m_intf;  // WiFi interface  
  UDPSocket m_sock;
};
#endif /* NTPCLIENT_H_ */
