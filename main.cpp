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
DigitalIn elevatorManual (PF_7, PullUp); //elevatorSysEn --> elevatorManual || 1 == auto :: 0 == manual
DigitalIn mArmManual (PF_9, PullUp); // mArmManual ||  inputs : 1 == auto :: 0 == manual (active LOW)
InterruptIn button(PG_0);
DigitalIn motorUpBut(PD_0);
DigitalIn motorDownBut(PD_1);

//Control Box Light Outputs
DigitalOut lightDrSysEn(PB_6);
DigitalOut armedLight(PC_3);
DigitalOut lightAccessGrnt(PA_3);
DigitalOut lightOccupantIn(PD_7);
DigitalOut lightOccupantOut(PD_6);
DigitalOut faultLightAlarm(PC_0);
DigitalOut faultLightDrOpen(PD_5);

//RFID Harness
#define MF_RESET    D8
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
void delayMs(int n);
void TIM2_Config(void);		
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
//void firebaseRead(void);
//void firebaseWrite(char[] command);


//controls for automatic elevator
bool down = false;
bool up = false;
int elevatorCnt = 0;
int traveldown = 800;// number of pulses for stepper motor travel, 200 per revolution.			
int travelup = 800;	

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
uint8_t ID[] = {0xe3, 0xdf, 0xa6, 0x2e};


// Firebase-related variables
//microcontroller WRITES to firebase
//const char firebaseWriteVars[][] = {"alarm" "buttonREQ" "packageCycles"};
//microcontroller READS from firebase
//const char firebaseReadVars[][] = {"id_card" "RFID" "buttonALLOW" "packageCycles" "alarmEn"};

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
    button.fall(&buttonUnlock);  		
				
    while(1) {				      
        lightdisplay();				
        entrancedetection();
      multiarmCtrl();

        rfidCtrl();        

//        if((buttonUnlockReq||rfidUnlockReq) && elevatorManual && mArmManual){ // "valid unlock Req from button OR rfid, AND elevator system AND multiArm are automatic	(1)	
        if(buttonUnlockReq||rfidUnlockReq){
            opendoor();
            buttonUnlockReq = 0;
            rfidUnlockReq = 0;
        }			
                        				        
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
    
    if(elevatorManual && !elevatorPkgBmSns && !elevatorDoor){ // automatic mode (1) and package present(0) and elevator's door closed(0)
        delayMs(50000); //approx 9s 
        movedown(traveldown);
        delayMs(50000);
        moveup(travelup);

    }
    if(!elevatorManual){ //if elevator in Manual mode, buttons control movement
        if(!motorUpBut && motorDownBut){
            if(down){ //checks to stop motor between direction VERY IMPORTANT!!
                elevatorUp = 0;
                elevatorDown = 0;
                delayMs(1000);
                down = false;
            }
            elevatorUp = 1;
            elevatorDown = 0;
            up = true;
            elevatorCnt = 0;
        }
        else if(!motorDownBut && motorUpBut){
            if(up){ //checks to stop motor between directions VERY IMPORTANT!!
                elevatorUp = 0;
                elevatorDown = 0;            
                delayMs(1000);
                up = false;
            }
            elevatorUp = 0;
            elevatorDown = 1;
            down = true;
            elevatorCnt = 0;
        }
        else{ //else no movement
            elevatorUp = 0;
            elevatorDown = 0;
            delayMs(1);
            elevatorCnt++;
            if(elevatorCnt > 1000){ //if motor hasn't been active in at lease 500ms- reset up/down booleans (no longer causes delay before manual up/down) 
                up = false;
                down = false;
                elevatorCnt = 1000;
            }
        }
    }
}

void multiarmCtrl(){
    if(mArmManual){ //if system is automatic call function for automatic control
        multiarmAuto();
    }
    else if(!mArmManual){
        if(motorUpBut){ //move forward (CW)
            pwmMotorCount.pulsewidth(0.0013);   //TIM2->CCR1 = cw;
            mArmMoving = true;
        }
        if(motorDownBut){ //move backwards (CCW)
            pwmMotorCount.pulsewidth(0.0017);   //TIM2->CCR1 = ccw;
            mArmMoving = true;
        } 
        else{ //neither or both buttons pressed > stop motor
            pwmMotorCount.pulsewidth(0.0015);   //TIM2->CCR1 = stopped;
            mArmMoving = false;
        }
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
        delayMs(1000);		
	}	
    //motor off
    elevatorUp = 0; //A6
    elevatorDown = 0; //A7		
}				
void moveup(int c){				
    int i;				
		
   //ensure solenoid is off before proceeding
    greenSolenoid = 0;

    //motor off delay for 1000ms (ensure no fuse blown)
    elevatorUp = 0; //A6
    elevatorDown = 0; //A7
	delayMs(1000);					
				
	//c=c/2; // count is for each coil.			
	for(i = 0; i<= c; i++){		//at least 800 cycle * 10ms OR stop when motor has reached the top
        if(!elevatorTopSwitch){ //break from the for loops section once the elevator has reached the Top
            break;
        }				
        elevatorUp = 1; //A6
        elevatorDown = 0; //A7
	    delayMs(1000);							
	}			
    // motor off
    elevatorUp = 0; //A6
    elevatorDown = 0; //A7				
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
	if(lightDrSysEn && (!elevatorManual)){		//LIGHTS UP WHEN SYS IS MANUAL	
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
        delayMs(1000);				
		faultLightAlarm = 0; 		
	}	
    else{
        faultLightAlarm = 0; 
    }		
    if(elevatorUp || elevatorDown || mArmMoving){
        armedLight = 1;
    }
    else{
        armedLight = 0;
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
        delayMs(500);			
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
    pwmMotorCount.period(0.020);		//period 20ms (50Hz)	
    pwmMotorCount.pulsewidth(0.0015);   //pulse width varies between 1.3, 1.5, and 1.7 ms 

    // Init. RC522 Chip
    RfChip.PCD_Init();
    pc.set_format(
        /* bits */ 8,
        /* parity */ BufferedSerial::None,
        /* stop bit */ 1
    );
    TIM2_Config();

//    ethernetInit();

    // THIS MUST BE DONE AFTER ETHERNET IS INITIALIZED!
 //   firebaseRead();

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
    buttonUnlockReq = true;
}

void delayMs(int n){
	while(n > 0){
		while(!(TIM2->SR & 1)) {};
		TIM2->SR &= ~1;
		n--;
	}
}
void TIM2_Config(){
	//configure TIM2 to wrap around at 1Hz
	RCC->APB1ENR |= 1;			//enable TIM2 clock
	TIM2->PSC = 1600 - 1;   //divided by 1600
	TIM2->ARR = 10 - 1;			//divided by 10 = 1kHz
	TIM2->CNT = 0;					//clear counter
	TIM2->CR1 = 1;					//enable TIM2
}



void multiarmAuto(void){
   // int cw = 2080-1; (1.3ms)
  //  int stopped = 2400-1; (1.5ms)
   // int ccw = 2720-1; (1.7ms)
    int c=0;
    
    if(!(mArmCasePres)){//motion sensor quiet, package present
        induct=true;
        pwmMotorCount.pulsewidth(0.0013);   //TIM2->CCR1 = cw;
        mArmMoving = true;
        delayMs(5000);//engage motor for 2.5 seconds
        pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
        mArmMoving = false;
    }
    //jamming suspected.
    for(int i=0; i<2;i++){
        if(!(mArmCasePres)){
            pwmMotorCount.pulsewidth(0.0017); //TIM2->CCR1 = ccw;
            mArmMoving = true;
            delayMs(2500);
            pwmMotorCount.pulsewidth(0.0013); //TIM2->CCR1 = cw;
            mArmMoving = true;
            delayMs(5000);//engage motor for 2.5 seconds
            pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
            mArmMoving = false;
        }
    }
    
    if((mArmCasePres&&mArmBmSwitch)&& induct ){//both beams cleared, induction started:
        while((mArmBmSwitch) && (c<100)){
            pwmMotorCount.pulsewidth(0.0013); //TIM2->CCR1 = cw;
            mArmMoving = true;
            delayMs(100);
            pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
            mArmMoving = false;
            c=c+1;
        }
        c=0;
    }
    pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
    mArmMoving = false;
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