
#include "NTPclient.h"

#define NTP_PORT 123
#define NTP_CLIENT_PORT 0 //Random port
#define timeout 3000
#define NTP_TIMESTAMP_DELTA 2208988800ull 
// Diff btw a UNIX timestamp (Starting Jan, 1st 1970)
// and a NTP timestamp (Starting Jan, 1st 1900)

NTPclient::NTPclient(NetworkInterface & _m_intf) : m_intf(_m_intf)
{
}
#ifdef htons
#undef htons
#endif /* htons */
#ifdef htonl
#undef htonl
#endif /* htonl */
#ifdef ntohs
#undef ntohs
#endif /* ntohs */
#ifdef ntohl
#undef ntohl
#endif /* ntohl */


#if ((__BYTE_ORDER__) == (__ORDER_LITTLE_ENDIAN__))

#define htons(x) ((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8))
#define ntohs(x) htons(x)
#define htonl(x) ((((x) & 0xff) << 24) | \
                     (((x) & 0xff00) << 8) | \
                     (((x) & 0xff0000UL) >> 8) | \
                     (((x) & 0xff000000UL) >> 24))
#define ntohl(x) htonl(x)

#else

#define htons(x)  (x)
#define htonl(x)  (x)
#define ntohl(x)  (x)
#define ntohs(x)  (x)

#endif 

uint32_t NTPclient::getNTP(const char * NTPpool, uint32_t tzoffset, bool dst, bool setRTC)

{
  SocketAddress address(0, NTP_PORT);
  int r = m_intf.gethostbyname(NTPpool, &address);  
  if (r) {
        printf("error: 'gethostbyname(\"%s\")' failed with code %d\r\n", NTPpool, r);
    } else if (!address) {
        printf("error: 'gethostbyname(\"%s\")' returned null IP address\r\n", NTPpool);
    }  
   
  //Create & bind socket
  if (m_sock.open(&m_intf) < 0) printf ("ERROR sock open \n\r");  
  m_sock.set_timeout(timeout);  

  struct NTPPacket pkt;  
  memset (&pkt, 0, sizeof(NTPPacket));   

  //Now ping the server and wait for response
  //Prepare NTP Packet:
  pkt.li = 0;           //Leap Indicator : No warning
  pkt.vn = 4;           //Version Number : 4
  pkt.mode = 3;         //Client mode
  pkt.stratum = 0;      //Not relevant here
  pkt.poll = 0;         //Not significant as well
  pkt.precision = 0;    //Neither this one is

    int ret = m_sock.sendto(address, (char*)&pkt, sizeof(NTPPacket) ); 
    if (ret < 0 ){
        m_sock.close();
        return 0;
    }
    //Read response
    ret = m_sock.recvfrom(&address, (char*)&pkt, sizeof(NTPPacket) );
    if(ret < 0){
        m_sock.close();
        return 0;
        }
    if(ret < sizeof(NTPPacket)){
        m_sock.close();
        return 0;
        }
    //Correct Endianness
    pkt.refTm_s = ntohl( pkt.refTm_s ); 
    pkt.refTm_f = ntohl( pkt.refTm_f );
    pkt.origTm_s = ntohl( pkt.origTm_s );
    pkt.origTm_f = ntohl( pkt.origTm_f );
    pkt.rxTm_s = ntohl( pkt.rxTm_s );
    pkt.rxTm_f = ntohl( pkt.rxTm_f );
    pkt.txTm_s = ntohl( pkt.txTm_s );
    pkt.txTm_f = ntohl( pkt.txTm_f );
    
    
    uint32_t CETtime = (time_t)(pkt.txTm_s - NTP_TIMESTAMP_DELTA + tzoffset); 
    // check for DST time change, only valid for europe!!!
    uint32_t DST=0;  
    if(dst){
        uint32_t dow,hour,day,month;             
        char buffer[10];        
        strftime(buffer, 2,"%H", localtime(&CETtime));                                
        hour = atoi(buffer);
        strftime(buffer, 2,"%w", localtime(&CETtime));                                
        dow = atoi(buffer);
        strftime(buffer, 2,"%e", localtime(&CETtime));                
        day = atoi(buffer);
        strftime(buffer, 2,"%m", localtime(&CETtime));
        month = atoi(buffer);       
        
        uint32_t previousSunday = day - dow;
        if (month > 2 && month < 9){DST=3600;}
        // DST starts 2nd Sunday of March;  2am            
        if (month == 2 && previousSunday >= 25 && hour >= 2){DST=3600;}
        // DST ends 1st Sunday of November; 2am
        if (month == 9 && previousSunday < 25 && hour >= 2){DST=0;}
    }            
    if(setRTC){set_time(CETtime+DST+1);} // add extra second here for processing         
    m_sock.close();
    return (CETtime+DST+1);
}
