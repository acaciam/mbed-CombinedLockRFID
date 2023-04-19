//Test of cheap 13.56 Mhz RFID-RC522 module from eBay
//This code is based on Martin Olejar's MFRC522 library. Minimal changes
//Adapted for Nucleo STM32 F401RE. Should work on other Nucleos too

//Connect as follows:
//RFID pins        ->  Nucleo header CN5 (Arduino-compatible header)
//----------------------------------------
//RFID IRQ=pin5    ->   Not used. Leave open
//RFID MISO=pin4   ->   Nucleo SPI_MISO=PA_6=D12
//RFID MOSI=pin3   ->   Nucleo SPI_MOSI=PA_7=D11
//RFID SCK=pin2    ->   Nucleo SPI_SCK =PA_5=D13
//RFID SDA=pin1    ->   Nucleo SPI_CS  =PB_6=D10
//RFID RST=pin7    ->   Nucleo         =PA_9=D8
//3.3V and Gnd to the respective pins                              
                              
#include "mbed.h"
#include "MFRC522.h"
#include "stm32f4xx.h"  				
#include <stdbool.h>	
#include "Firebase.h"
#include "trng_api.h"
#include "NTPclient.h"
#include <string>
#include <LowPowerTimer.h>
// Nucleo Pin for MFRC522 reset (pick another D pin if you need D8)


#define UART_RX     PD_2
#define UART_TX     PC_12
BufferedSerial     pc(UART_TX, UART_RX, 9600);

DigitalOut LedGreen(LED1);  //PB0
DigitalOut LedBlue(LED2);   //PB7
DigitalOut LedRed(LED3);    //PB14

//Internal Harness
DigitalOut elevatorUp(PE_4); //elevatorPosBlock-->elevatorUp
DigitalOut elevatorDown(PE_5); //elevatorNegBlock --> elevatorDown
DigitalOut greenSolenoid(PE_6); //elevatorGrnWire --> greenSolenoid


//Control Box Inputs
DigitalIn elevatorAutomatic (PF_7, PullUp); //elevatorSysEn --> elevatorAutomatic || 1 == auto :: 0 == manual
DigitalIn mArmAutomatic (PF_9, PullUp); // mArmAutomatic ||  inputs : 1 == auto :: 0 == manual (active LOW)
InterruptIn button(PG_0, PullUp);
DigitalIn motorUpBut(PD_0, PullUp);
DigitalIn motorDownBut(PD_1, PullUp);

//Control Box Light Outputs
DigitalOut lightDrSysEn(PB_6);
DigitalOut movingLight(PC_3);
DigitalOut lightAccessGrnt(PA_3);
DigitalOut lightOccupantIn(PD_7);
DigitalOut lightOccupantOut(PD_6);
DigitalOut faultLightAlarm(PC_0);
DigitalOut faultLightDrOpen(PD_5);

//RFID Harness
#define MF_RESET    PC_7
#define SPI_MOSI PB_5
#define SPI_MISO PB_4
#define SPI_SCK PB_3
MFRC522    RfChip   (SPI_MOSI, SPI_MISO, SPI_SCK, PA_4, MF_RESET);

//Front Door Harness
DigitalIn inBmSns(PF_4, PullUp); 
DigitalIn outBmSns(PC_2, PullUp);
DigitalIn drClsdSwitch(PB_6, PullUp);
DigitalIn interiorMotion(PB_1);

//Elevator Harness
DigitalIn elevatorTopSwitch(PE_14, PullUp);
DigitalIn elevatorBotSwitch(PE_15, PullUp);
DigitalIn elevatorDoor(PB_10, PullUp);
DigitalIn elevatorPkgBmSns(PB_11, PullUp);

//Multi Arm Machine Harness
PwmOut pwmMotorCount(PA_5);
DigitalIn mArmOutMotionDetector(PA_6, PullUp);
DigitalIn mArmCasePres(PD_14, PullUp); //package switch (package has arrived)
DigitalIn mArmBmSwitch(PD_15, PullUp); //arm beam sensor (package has been inducted)

//------------------------------------------------

//setup/base functions
void setup(void);	
void ethernetInit(void);	
void PWM_Config(void);		
void blinkLED(DigitalOut led);
			
//main control functions			
void lightdisplay(void);				
void entrancedetection(void);	
void rfidCtrl(void);
void elevatorCtrl(void);
void multiarmCtrl(void);

//function calls from main controls
void multiarmAuto(void);		
void movedown(int c);				
void moveup(int d);	
void opendoor(void);
void buttonUnlock(void);


//Firebase function calls
void firebaseRead(void);
//void firebaseWrite(char[] command);


//controls for automatic elevator
bool down = false;
bool up = false;
int elevatorCnt = 0;
int traveldown = 1600;// 50ms * c cycle = 20s
int travelup = 1600;

//bools for tracking
bool both=false;				
bool leaving= false;			
bool entering=false;						
bool armed=false;				
bool induct=false;	
bool inside=false;			
bool outside=false;
bool mArmMoving = false;
bool rfidUnlockReq = false;

//bools to write to firebase
bool buttonUnlockReq = false;
bool alarm=false;	

//bools to read from from database
bool alarmEn = true;
bool buttonUnlockAllow = false;


// ID card
//uint8_t ID[] = {0xe3, 0xdf, 0xa6, 0x2e};
uint8_t ID[] = {0x73, 0x3b, 0xbc, 0x1d};

// Firebase-related variables
//microcontroller WRITES to firebase
const char firebaseWriteVars[][14] = {"alarm", "buttonREQ", "packageCycles"};
//microcontroller READS from firebase
const char firebaseReadVars[][14] = {"id_card", "RFID", "buttonALLOW", "packageCycles", "alarmEn"};

// This should be read prior to being set by any code, and as such firebaseRead should be
// called in initialization to avoid overwriting.
int packageCycles = -1;

// Which ID card do we want to read from Firebase.
// This is only really useful for demonstration purposes of gathering different
// ID cards, which is configured by Firebase and set here before gathering the ID card.
// This is why "id_card" is read from Firebase before "RFID".
int id_card = 0 ;


int main(void) {	
    int condition;// variable for present condition of door.	

	setup();

    LowPowerTimer timer;	
    timer.start();

    button.fall(&buttonUnlock);  		
				
    while(1) {				      
        lightdisplay();				
        entrancedetection();
        if(timer.read() > 30){ //if rfid timer is > 30s
            //reset the RFID board
            timer.stop();
            RfChip.PCD_Init();
            timer.reset(); //reset and start timer
            timer.start();

        }
        if(elevatorAutomatic && mArmAutomatic){
            rfidCtrl();
        }        

        if((buttonUnlockReq||rfidUnlockReq) && elevatorAutomatic && mArmAutomatic){ //"valid unlock Req from button OR rfid, AND elevator system AND multiArm are automatic	(1)	
            opendoor();
            buttonUnlockReq = 0;
            rfidUnlockReq = 0;
        }			
        multiarmCtrl();               				        
        elevatorCtrl();
    }  				
} 
void rfidCtrl(){
    // Look for new cards
    if ( RfChip.PICC_IsNewCardPresent())
    {
        blinkLED(LedBlue);
        if ( RfChip.PICC_ReadCardSerial()) {
            // Print Card UID
            if(RfChip.uid.uidByte[0] == ID[0] && RfChip.uid.uidByte[1] == ID[1] && RfChip.uid.uidByte[2] == ID[2] && RfChip.uid.uidByte[3] == ID[3]){
                printf("Card Match! \n");
                rfidUnlockReq = true;
                blinkLED(LedGreen);
            }
            else{
                rfidUnlockReq = false;
                printf("Not Matching Card \n");
                blinkLED(LedRed);
            }
            printf("Card UID: ");
            for (uint8_t i = 0; i < RfChip.uid.size; i++){
                printf(" %X", RfChip.uid.uidByte[i]);
            }
            printf("\n\n\r");
        }

    }
}				
void elevatorCtrl(){
    
    if(elevatorAutomatic && !elevatorPkgBmSns && !elevatorDoor){ // automatic mode (1) and package present(0) and elevator's door closed(0)
        bool run = true;
        for(int i = 0; i < 10; i++){
            if(elevatorPkgBmSns){
                run = false;
                break;
            }
            ThisThread::sleep_for(500ms);
        }
        if(run){
            movedown(traveldown);
            ThisThread::sleep_for(5s);
            moveup(travelup);
        }
    }
    if(!elevatorAutomatic){ //if elevator in Manual mode, buttons control movement
        greenSolenoid = 0; //turn off solenoid when we enter the manual motor mode SAFETY
        //safety OFF before running
        elevatorUp = 0;
        elevatorDown = 0;       
        ThisThread::sleep_for(500ms);
        while(!motorUpBut && motorDownBut){ //go UP
            elevatorUp = 1;
            elevatorDown = 0;
            movingLight = 1;
        }
        while(!motorDownBut && motorUpBut){ //go DOWN
            elevatorUp = 0;
            elevatorDown = 1;
            movingLight = 1;
        }
        elevatorUp = 0;
        elevatorDown = 0;
        movingLight = 0;
    }
}

void multiarmCtrl(){
    if(mArmAutomatic){ //if system is automatic call function for automatic control
        multiarmAuto();
    }
    else if(!mArmAutomatic){ //manual mode for multiarm
        while(!motorUpBut && motorDownBut){ //move forward (CW)
            pwmMotorCount.pulsewidth(0.0013);   //TIM2->CCR1 = cw;
            movingLight = 1;
        }
        while(!motorDownBut && motorUpBut){ //move backwards (CCW)
            pwmMotorCount.pulsewidth(0.0017);   //TIM2->CCR1 = ccw;
            movingLight = 1;
        } 
        //neither or both buttons pressed > stop motor
        pwmMotorCount.pulsewidth(0.0015);   //TIM2->CCR1 = stopped;
        movingLight = 0;
        
    }
}				
							
void movedown(int c){		
    int i = 0;		
    //ensure solenoid is off before proceeding
    greenSolenoid = 0;
		
				
	//c=c/2; // count is for each coil.			
	for(i = 0; i<= c; i++){		
        if(!elevatorBotSwitch){ //break from the for loops section once the elevator has reached the bottom
            break;
        }	
        elevatorUp = 0;
        elevatorDown = 1;
        movingLight = 1;
        ThisThread::sleep_for(50ms);		
	}	
    //motor off
    elevatorUp = 0; //A6
    elevatorDown = 0; //A7	
    movingLight = 0;	
}				
void moveup(int c){				
    int i;				
		
   //ensure solenoid is off before proceeding
    greenSolenoid = 0;				
				
	//c=c/2; // count is for each coil.			
	for(i = 0; i<= c; i++){		//at least 800 cycle * 10ms OR stop when motor has reached the top
        if(!elevatorTopSwitch){ //break from the for loops section once the elevator has reached the Top
            break;
        }				
        elevatorUp = 1; //A6
        elevatorDown = 0; //A7
        movingLight = 1;
	    ThisThread::sleep_for(50ms);							
	}			
    // motor off
    elevatorUp = 0; //A6
    elevatorDown = 0; //A7
    movingLight = 0;				
}				
void lightdisplay(void){	
    if(armed && (!interiorMotion && inside)){
        //FIXME ADD write read for alarm and alarm enable
        if(alarmEn){
            alarm = 1;
        }
        else{
            alarm = 0;
        }
    }
    //FIXME remove when the app is set up so only app change change enabled status
    if(!alarmEn && (armed && (outside))){ //reset the alarm enable when the person leaves
        alarmEn = true;
    }				
	if(lightDrSysEn && (!elevatorAutomatic)){		//LIGHTS UP WHEN SYS IS MANUAL	
	    lightDrSysEn = 0;
    } 					
	// Door left open fault: Delivery person walked away leaving the door open			
	if(outside && drClsdSwitch){ //person left and door left open: Door open fault		
        faultLightDrOpen = 1; 				
	}else{
        faultLightDrOpen = 0; 
    }		
	if(alarm){ //armed, no motion, entrant left: Door fault			
        faultLightAlarm = 1;				
        ThisThread::sleep_for(1s);				
		faultLightAlarm = 0; 		
	}	
    else{
        faultLightAlarm = 0; 
    }		
    if(elevatorUp || elevatorDown || mArmMoving){
        movingLight = 1;
    }
    else{
        movingLight = 0;
    }		
				
}				
				
//This function determines if a person is on the inside or outside of the residence with the dual beam sensors.  				
void entrancedetection(void){				
				
	if((!outBmSns && !inBmSns)){//Both lasers tripped	was: if(!(outBmSns && inBmSns)){		
	    both= true;			
		lightOccupantOut = 0;		
		lightOccupantIn = 0;		
	}			
	if((both) && (outBmSns && !inBmSns)){//entering			
		entering=true;		
	    leaving=false;
    }			
				
	if((both) && (!outBmSns && inBmSns)){//leaving			
		leaving=true;		
	    entering= false;
    }			
				
	if((leaving) && (inBmSns && outBmSns)){//outside			
	    inside=false;			
	    outside= true;			
		lightOccupantOut = 1;		
		lightOccupantIn = 0;		
	    both=false;			
	    leaving=false;
    }
			
	if((entering) && (inBmSns && outBmSns)){//inside			
	    inside=true;
        outside = false;			
		lightOccupantOut = 0;		
		lightOccupantIn = 1;		
    	leaving=false;			
    	both=false;			
    	entering=false;
    }			
				
	if (inside && interiorMotion){//Occupant detected and motion sensor active, alarm is armed.			
		armed=true;		
	}			
	if (outside && armed && (!drClsdSwitch)){//Occupant has left and closed the door, disarm alarm.			
		armed=false;		
		outside=false;		
		lightOccupantOut = 0;	//turn off outside indicator	
		lightOccupantIn = 0;	// turn off door open fault.
	}			

    // Since we want to know what the alarm state should be at any time, 
    // we want to write the alarm here. As a potential TODO, an alarm disable variable
    // can be set to trigger alarm off, after validation.
    // firebaseWrite("alarm");

}				
				
				
				
void opendoor(void){	
    int timeout=0;				
	lightAccessGrnt = 1; // turn on request to enter light. pb8			
    //ensure the elevator motor will be inactive
    elevatorUp = 0;
    elevatorDown = 0;
				
				
	greenSolenoid = 1;//activate solenoid pa8			
				
	while ((!drClsdSwitch) && (timeout<=5)  ){//while door closed, before time runs out			
        ThisThread::sleep_for(500ms);			
        timeout++;			
	}			
				
						
	// cut motor outputs	
    greenSolenoid = 0; //A8
		
	lightAccessGrnt = 0; // turn off request to enter light. pb8			
}				
//****************************Driver Setup*******************************				
void setup(void){							
	//DISABLE RELAY POWER AND CONTROL SIGNALS ZERO			
    elevatorUp = 0; elevatorDown = 0; 	 // elevator motor OFF	
    greenSolenoid = 0; // solenoid off (door locked)
    pwmMotorCount.period(.020);		//period 20ms (50Hz)	
    pwmMotorCount.pulsewidth(.0015);   //pulse width varies between 1.3, 1.5, and 1.7 ms 

    // Init. RC522 Chip
    RfChip.PCD_Init();
    pc.set_format(
        /* bits */ 8,
        /* parity */ BufferedSerial::None,
        /* stop bit */ 1
    );

    //ethernetInit();

    // THIS MUST BE DONE AFTER ETHERNET IS INITIALIZED!
  //  firebaseRead();

}				

void ethernetInit(){
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
    else{printf("No NTP could not set RTC !!\n\n");}  
    
    printf("\nConnecting TLS re-use socket...!\n\n");
    TLSSocket* socket = new TLSSocket();
    startTLSreusesocket((char*)FirebaseID);
     
    printf("\nReady...!\n\n");	
}				

void blinkLED(DigitalOut led){
    led = 1;
    ThisThread::sleep_for(500ms);
    led = 0;
}
void buttonUnlock(void){
    if(elevatorAutomatic && mArmAutomatic){
        buttonUnlockReq = true;
    }
}


void multiarmAuto(void){
    int c=0;
    
    if(!mArmCasePres){//motion sensor quiet, package present
        induct=true;
        pwmMotorCount.pulsewidth(0.0013);   //TIM2->CCR1 = cw;
        movingLight = 1;
        ThisThread::sleep_for(5s);//engage motor for 2.5 seconds
        pwmMotorCount.pulsewidth(.0015); //TIM2->CCR1 = stopped;
        movingLight = 0;
    }
    //jamming suspected.
    for(int i=0; i<2;i++){
        if(!(mArmCasePres)){
            pwmMotorCount.pulsewidth(.0017); //TIM2->CCR1 = ccw;
            movingLight = 1;
            ThisThread::sleep_for(2500ms);
            pwmMotorCount.pulsewidth(.0013); //TIM2->CCR1 = cw;
            movingLight = 1;
            ThisThread::sleep_for(5s);//engage motor for 2.5 seconds
            pwmMotorCount.pulsewidth(.0015); //TIM2->CCR1 = stopped;
            movingLight = 0;
        }
    }
    
    if(mArmCasePres&&mArmBmSwitch&& induct ){//button not pressed AND beam sensor connected, induction started:
        while(mArmBmSwitch && (c<100)){ //go until beam sensor is broken
            pwmMotorCount.pulsewidth(0.0013); //TIM2->CCR1 = cw;
            movingLight = 1;
            ThisThread::sleep_for(100ms);
            pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
            movingLight = 0;
            c=c+1;
        }
        c=0;
    }
    pwmMotorCount.pulsewidth(.0015); //TIM2->CCR1 = stopped;
    movingLight = 0;
    induct=false;
}




/*************FIREBASE RELATED CODE***************
void firebaseRead(void)
{
    for(i = begin(firebaseReadVars); i < end(firebaseReadVars); i++)
    {
        char* FirebaseReader;
        switch(firebaseRead[i])
        {

            case "RFID"
            {
                if(id_card == 0);
                { 
                    strcat(FirebaseReader, FirebaseUrl);
                    strcat(FirebaseReader, "id0.json?auth=");
                    strcat(FirebaseReader, FirebaseAuth);
                    ID[0] = (uint8_t)stoi(str(getFirebase((char*)FirebaseReader)));
                    FirebaseReader = "";
                    strcat(FirebaseReader, FirebaseUrl);
                    strcat(FirebaseReader, "id1.json?auth=");
                    strcat(FirebaseReader, FirebaseAuth);
                    ID[1] = (uint8_t)stoi(str(getFirebase((char*)FirebaseReader)));
                    FirebaseReader = "";
                    strcat(FirebaseReader, FirebaseUrl);
                    strcat(FirebaseReader, "id2.json?auth=");
                    strcat(FirebaseReader, FirebaseAuth);
                    ID[2] = (uint8_t)stoi(str(getFirebase((char*)FirebaseReader)));
                    FirebaseReader = "";
                    strcat(FirebaseReader, FirebaseUrl);
                    strcat(FirebaseReader, "id3.json?auth=");
                    strcat(FirebaseReader, FirebaseAuth);
                    ID[3] = (uint8_t)stoi(str(getFirebase((char*)FirebaseReader)));
                }
                else
                {
                    strcat(FirebaseReader, FirebaseUrl);
                    strcat(FirebaseReader, "alt_id0.json?auth=");
                    strcat(FirebaseReader, FirebaseAuth);
                    ID[0] = (uint8_t)stoi(str(getFirebase((char*)FirebaseReader)));
                    FirebaseReader = "";
                    strcat(FirebaseReader, FirebaseUrl);
                    strcat(FirebaseReader, "alt_id1.json?auth=");
                    strcat(FirebaseReader, FirebaseAuth);
                    ID[1] = (uint8_t)stoi(str(getFirebase((char*)FirebaseReader)));
                    FirebaseReader = "";
                    strcat(FirebaseReader, FirebaseUrl);
                    strcat(FirebaseReader, "alt_id2.json?auth=");
                    strcat(FirebaseReader, FirebaseAuth);
                    ID[2] = (uint8_t)stoi(str(getFirebase((char*)FirebaseReader)));
                    FirebaseReader = "";
                    strcat(FirebaseReader, FirebaseUrl);
                    strcat(FirebaseReader, "alt_id3.json?auth=");
                    strcat(FirebaseReader, FirebaseAuth);
                    ID[3] = (uint8_t)stoi(str(getFirebase((char*)FirebaseReader)));
                }
            }

            case "buttonALLOW"
            {
                strcat(FirebaseReader, FirebaseUrl);
                strcat(FirebaseReader, "buttonAllow.json?auth=");
                strcat(FirebaseReader, FirebaseAuth);
                char* buttonAllow = getFirebase((char*)FirebaseReader);
                if(buttonAllow == "true")
                {
                    buttonUnlockAllow = true;
                }
                else
                {
                    buttonUnlockAllow = false;
                }
            }

            case "id_card"
            {
                strcat(FirebaseReader, FirebaseUrl);
                strcat(FirebaseReader, "id_card.json?auth=");
                strcat(FirebaseReader, FirebaseAuth);
                id_card = stoi(str(getFirebase((char*)FirebaseReader)));
            }

            case "packageCycles"
            {
                strcat(FirebaseReader, FirebaseUrl);
                strcat(FirebaseReader, "packageCycles.json?auth=");
                strcat(FirebaseReader, FirebaseAuth);
                packageCycles = stoi(str(getFirebase((char*)FirebaseReader)));
            }
        }
    }
}

void firebaseWrite(char[] command)
{
    switch(command)
    {
        char* FirebaseReader;
        case "alarm":
        {
            strcat(FirebaseReader, FirebaseUrl);
            strcat(FirebaseReader, "alarm.json?auth=");
            strcat(FirebaseReader, FirebaseAuth);

            char* alarm_write = alarm?"true":"false";

            putFirebase(FirebaseReader,alarm_write);
        }
        case "buttonREQ":
        {
            strcat(FirebaseReader, FirebaseUrl);
            strcat(FirebaseReader, "buttonREQ.json?auth=");
            strcat(FirebaseReader, FirebaseAuth);

            char* button_write = buttonUnlockReq?"true":"false";

            putFirebase(FirebaseReader,button_write);
        }
        case "packageCycles":
        {
            strcat(FirebaseReader, FirebaseUrl);
            strcat(FirebaseReader, "packageCycles.json?auth=");
            strcat(FirebaseReader, FirebaseAuth);

            char* cycles_write;

            itoa(packageCycles, cycles_write, 10);

            putFirebase(FirebaseReader,cycles_write);
        }
    }
}*/