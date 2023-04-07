#include "mbed.h"
#include "Firebase.h"
#include "trng_api.h"
#include "NTPclient.h"

DigitalOut led1(LED1,0);
DigitalOut led2(LED1,0);
DigitalOut led3(LED1,0);

char    dataPUT[200];       // PUT data container
char    dataPOST[200];      // POST data container
char    timebuff[100];      // timestamp container

int     PUTdata,POSTdata,GETdata,RTCsecond,Lastsecond,RTCminute,Lastminute,RTChour,Lasthour;

void    getRTC(),getSENSORS();
/*
int main()
{    
    ThisThread::sleep_for(100);  // allow time to reset
    printf("\033[0m\033[2J\033[H\n  ----- Firebase Example -----\r\n\n\n");
    printf("Initialise!\r\n\n");  
     
    // connect to the default connection access point
    net = connect_to_default_network_interface();    
   
    // get NTP time and set RTC
    NTPclient           ntp(*net);
    printf("\nConnecting to NTP server..\n");    
    //  NTP server address, timezone offset in seconds +/-, enable DST, set RTC 
    if(ntp.getNTP("0.pool.ntp.org",0,1,1)){
        time_t seconds = time(NULL);
        printf("System time set by NTP: %s\n\n", ctime(&seconds));
        }
        else{printf("No NTP could not set RTC !!\n\n");
        }  
    
    printf("\nConnecting TLS re-use socket...!\n\n");
    TLSSocket* socket = new TLSSocket();
    startTLSreusesocket((char*)FirebaseID);
     
    printf("\nReady...!\n\n");        
    
    while(1){ 
        led1=1;       
        while(Lastsecond==RTCsecond){   // synchronise data upload with RTC             
            getRTC();       
        }
        Lastsecond=RTCsecond;
        
        getSENSORS();   // get sensor data.
        led1=0;
                       
        if(RTCsecond%10==0){PUTdata=1;}                 // PUT 10 second counter
        if(RTCsecond==0){GETdata=1;}                    // GET minute counter
        if(RTCminute==0 && RTCsecond==0){POSTdata=1;}   // POST hour counter                   
        
        if(PUTdata){    // PUT data to Firebase, initialise a data block the updates it on subsiquent PUT's
            led3=1;            
            strcpy(FirebaseUrl, FirebaseID);                    // Firebase account ID
            strcat(FirebaseUrl, "/Test/Current/.json?auth=");   // bit in the middle to send .json data and authority
            strcat(FirebaseUrl, FirebaseAuth);                  // Firebase account authorisation key                   
            if(putFirebase((char*)FirebaseUrl,(char*)dataPUT)){
                printf("PUT Current okay, time: %s\n",timebuff);
                }                
            PUTdata=0;
            led3=0;
        }                           
        if(POSTdata){   // POST data to Firebase, adds records           
            led3=1;            
            strcpy(FirebaseUrl, FirebaseID);                    // Firebase account ID
            strcat(FirebaseUrl, "/Test/History/.json?auth=");   // bit in the middle to send .json data and authority
            strcat(FirebaseUrl, FirebaseAuth);                  // Firebase account authorisation key                
            if(postFirebase((char*)FirebaseUrl,(char*)dataPOST)){
                 printf("POST History okay, time: %s\n",timebuff);
                 }     
            POSTdata=0;
            led3=0;
        }             
        if(GETdata){    // retrieve data from Firebase
            led2=1;            
            strcpy(FirebaseUrl, FirebaseID);                    // Firebase account ID
            strcat(FirebaseUrl, "/Mill/Current/.json?auth=");   // bit in the middle to send .json data and authority
            strcat(FirebaseUrl, FirebaseAuth);                  // Firebase account authorisation key                                     
            printf("\nGET current...  time: %s\n",timebuff);
            printf("%s \n\n",getFirebase((char*)FirebaseUrl));
            GETdata=0;
            led2=0;
        }                                                 
    }       
}
*/
void getSENSORS()
{
    //  dummy sensor values, change these if you add real sensors eg, BME280, DS1820 etc. 
    // STM32F7 trng   
    //RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;  // Enable RNG clock source 
    //RNG->CR |= RNG_CR_RNGEN;            // RNG Peripheral enable 
    //while (!(RNG->SR & (RNG_SR_DRDY))); // Wait until one RNG number is ready 
    //int num = RNG->DR;
    //num = abs(num%100);
        
    int num = abs(rand()%100);    
    //printf("RNG: %d\n",num);
    
    char    Temp[5][7] = {"23.80","72.50","65.00","45.30","30.60"};
    float temp1 = (num/1.3), temp2 = (num*1.13), humidity = (num), pressure = (985+num);
    // POST data
    sprintf(dataPOST,"{\"Temp0\" : \"%s\",\"Temp1\" : \"%s\",\"Temp2\" : \"%s\",\"Temp3\" : \"%s\",\"Temp4\" : \"%s\",\"Time\" : \"%s\"}",
    Temp[0],Temp[1],Temp[2],Temp[3],Temp[4],timebuff);
    // PUT data
    sprintf(dataPUT,"{\"TempIn\" : \"%3.2f\", \"TempOut\" : \"%3.2f\",\"Humidity\" : \"%2.2f\",\"Pressure\" : \"%4.2f\", \"Time\" : \"%s\"}",
    temp1,temp2,humidity,pressure,timebuff);
}

void getRTC()
{
    time_t seconds = time(NULL);                                    
    strftime(timebuff, 3,"%H", localtime(&seconds));
    RTChour = atoi(timebuff);
    strftime(timebuff, 3,"%M", localtime(&seconds));
    RTCminute = atoi(timebuff);
    strftime(timebuff, 3,"%S", localtime(&seconds));
    RTCsecond = atoi(timebuff);    
    strftime(timebuff,50,"%H:%M:%S %m/%d/%Y ", localtime(&seconds));           
}