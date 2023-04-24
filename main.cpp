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
DigitalIn mArmOutMotionDetector(PA_6, PullUp); //action on 1
DigitalIn mArmCasePres(PD_14, PullUp); //package switch (package has arrived)
DigitalIn mArmBmSwitch(PD_15, PullUp); //arm beam sensor (package has been inducted)

//Bluetooth Defines, sets
const int MAXBUFFSIZE = 32;
#define UART5_TX PC_12
#define UART5_RX PD_2
char buf[MAXBUFFSIZE] = {' '};
BufferedSerial blue(UART5_TX, UART5_RX, 9600);

//Precalculated values are better for speed.
//Rather than needing to calculate, this will be a
//simple lookup operation.
const char hexNumbers[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};


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

//function calls for bluetooth controls
void bluetoothProcess(char c[]);
bool idSet(char c[]);
char* uInt8toChar(uint8_t i);
uint8_t chartoUInt8(char c);

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

int packageCycles = -1;


int main(void) {	
    int condition;// variable for present condition of door.	

	setup();

    LowPowerTimer timer;	
    timer.start();

    button.fall(&buttonUnlock);  		
				
    while(1) {				      
        //only run system ops when motors are in automatic modes
        if(elevatorAutomatic && mArmAutomatic){
            lightdisplay();				
            entrancedetection();
            if(timer.read() > 30){ //if rfid timer is > 30s
                //reset the RFID board
                timer.stop();
                RfChip.PCD_Init();
                timer.reset(); //reset and start timer
                timer.start();
            }
            rfidCtrl();
            if((buttonUnlockReq||rfidUnlockReq)){ //"valid unlock Req from button OR rfid, AND elevator system AND multiArm are automatic	(1)	
                opendoor();
                buttonUnlockReq = 0;
                rfidUnlockReq = 0;
            }
        }      	
        multiarmCtrl();               				        
        elevatorCtrl();

        //Bluetooth reading
        char buf[MAXBUFFSIZE] = {0};
        if (blue.readable()) 
        {
          blue.read(buf, sizeof(buf));
          printf(buf);
          bluetoothProcess(buf);
        }
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

                //13 is sizeof("Card Match! \n")-1
                blue.write("Card Match! \n", 13);

                rfidUnlockReq = true;
                blinkLED(LedGreen);
            }
            else{
                rfidUnlockReq = false;
                printf("Not Matching Card \n");

                //13 is sizeof("Not Matching Card \n")-1
                blue.write("Not Matching Card \n", 20);

                blinkLED(LedRed);
            }
            printf("Card UID: ");
            
            // 10 is sizeof("Card UID: ")-1
            blue.write("Card UID: ", 10);

            for (uint8_t i = 0; i < RfChip.uid.size; i++){
                printf(" %X", RfChip.uid.uidByte[i]);

                //Tell the bluetooth the UID of the card just read
                blue.write(uInt8toChar(RfChip.uid.uidByte[i]), 1);
            }
            printf("\n\n\r");

            // 1 is sizeof("\n")-1
            blue.write("\n", 1);
        }

    }
}

//-------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------ELEVATOR CONTROLS-----------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------
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
            buttonUnlockReq = 0; //clear button unlock request if it was triggered
            packageCycles++;
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
//-------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------MULTIARM CONTROLS-----------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------
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
    packageCycles++;
}							
//-------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------	
//-------------------------------------------------------------------------------------------------------------------------------------------			
void lightdisplay(void){	
    if(armed && (!interiorMotion && inside)){
        if(alarmEn){
            alarm = 1;

            //15 is the sizeof("Alarm set off!\n")-1
            blue.write("Alarm set off!\n", 15);
        }
        else{
            alarm = 0;
        }
    }
    /*
    //FIXME remove when the app is set up so only app change change enabled status
    if(!alarmEn && (armed && (outside))){ //reset the alarm enable when the person leaves
        alarmEn = true;
    }*/
	// if(lightDrSysEn && (!elevatorAutomatic)){		//LIGHTS UP WHEN SYS IS MANUAL	
	//     lightDrSysEn = 0;
    // } 					
	// Door left open fault: Delivery person walked away leaving the door open			
	if(outside && drClsdSwitch){ //person left and door left open: Door open fault		
        faultLightDrOpen = 1; 				
	}else{
        faultLightDrOpen = 0; 
    }		
	if(alarm && alarmEn){ //armed, no motion, entrant left: Door fault			
        faultLightAlarm = 1;	
        ThisThread::sleep_for(1s);
        faultLightAlarm = 0; 
        alarm = 0;	
	}	
    else{
        alarm = 0;
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
	if (outside && (!drClsdSwitch)){//Occupant has left and closed the door, disarm alarm.			
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
				
	while ((!drClsdSwitch) && (timeout<10)  ){//while door closed, before time runs out			
        ThisThread::sleep_for(500ms);			
        timeout++;			
	}			
				
						
	// cut motor outputs	
    greenSolenoid = 0; //A8
		
	lightAccessGrnt = 0; // turn off request to enter light. pb8			
}		
void blinkLED(DigitalOut led){
    led = 1;
    ThisThread::sleep_for(500ms);
    led = 0;
}
void buttonUnlock(void){
    if(elevatorAutomatic && mArmAutomatic && !movingLight){
        buttonUnlockReq = true;

        //17 is the sizeof("[button:Allow] ?\n")-1
        blue.write("[button:Allow] ?\n", 17);
    }
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
    blue.set_format(
        /* bits */ 8,
        /* parity */ BufferedSerial::None,
        /* stop bit */ 1
    );

}				

// Bluetooth-focused Function Calls

char* uInt8toChar(uint8_t i)
{
    int hi = i>>4;
    int lo = i&0x0f;
    char c[] = {hexNumbers[hi],hexNumbers[lo]};
    return (char*)c;
}

void bluetoothProcess(char c[])
{
    //Pre-processed input for speed.
    for(int i = 0; i < sizeof(c); i++)
    {
        char ca = c[i];
        c[i] = toupper(ca);
    }

    // only checking the first two characters for speed's sake.

    // Alarm Processing
    // Assumed input is "ALARM:True" / "ALARM:False"
    // This will
    if(c[0] == 'A' && c[1] =='L')
    {
        if(c[6] == 'T')
        {
            alarmEn = true;
            return;
        }
        else if(c[6] == 'F')
        {
            alarmEn = false;
            return;
        }

        // 15 is sizeof("Invalid input.\n")
        blue.write("Invalid input.\n", 15);
        return;
    }

    // Unlock Request Processing
    // Assumed input is "UNLOCK:ALLOW" / "UNLOCK:DENY" / "UNLOCK:NO" / "UNLOCK:YES"
    if(c[0] == 'U' && c[1] =='N')
    {
        if(c[7] == 'A' || c[7] == 'Y')
        {
            buttonUnlockAllow = true;
            buttonUnlockReq = false;
            return;
        }

        if(c[7] == 'D' || c[7] == 'N')
        {
            buttonUnlockAllow = false;
            buttonUnlockReq = true;
            return;
        }

        // 15 is sizeof("Invalid input.\n")
        blue.write("Invalid input.\n", 15);
        return;
    }

    // Package Cycles Report Processing
    // Assumed input is "PC"
    if(c[0] == 'P' && c[1] =='C')
    {
        string s = to_string(packageCycles);
        blue.write(s.c_str(), sizeof(s.length()));
        
        // New Line
        blue.write("\n", 1);
        return;
    }

    // ID Card Change Processing
    // Assumed input is "ID:{UID[0]}{UID[1]}{UID[2]}{UID[3}}"
    // With UID numbers in HEX format, without 0x.
    // 0xe3 would be put in "E3".
    if(c[0] == 'I' && c[1] =='D')
    {
        if(idSet(c))
        {
            return;
        }
        // 15 is sizeof("Invalid input.\n")
        blue.write("Invalid input.\n", 15);
        return;
    }
};


bool idSet(char c[])
{
    // This was chosen for speed.
    // Much like the pre-processing of the writes
    // for static printing to bluetooth terminal.
    // was done for speed.
    bool err = false;
    uint8_t futureID[8] = {0};
    // Process the characters into INTs and sanitizes them.
    for(int i = 0; i < 8; i++)
    {
        futureID[i] = chartoUInt8(c[3+i]);
        if(futureID[i] > (uint8_t)15)
        {
            return false;
        }
    }

    //16 is sizeOf("Writing ID Card ")-1
    blue.write("Writing ID Card ", 16);
    for(int i = 0; i<4; i++)
    {
        ID[i] = (futureID[2*i]<<4) | futureID[(2*i)+1];
        blue.write(uInt8toChar(ID[i]),2);
    }
    blue.write("\n",0);
    return true;
};


uint8_t chartoUInt8(char c)
{
    // Simple, works, and also has a fail case 
    // to make sure that errors on input are caught.
    uint8_t i;
    switch (c){
        case '0':
            i = 0;
            break;
        case '1':
            i = 1;
            break;
        case '2':
            i = 2;
            break;
        case '3':
            i = 3;
            break;
        case '4':
            i = 4;
            break;
        case '5':
            i = 5;
            break;
        case '6':
            i = 6;
            break;
        case '7':
            i = 7;
            break;
        case '8':
            i = 8;
            break;
        case '9':
            i = 9;
            break;
        case 'A':
            i = 0x0A;
            break;
        case 'B':
            i = 0x0B;
            break;
        case 'C':
            i = 0x0C;
            break;
        case 'D':
            i = 0x0D;
            break;
        case 'E':
            i = 0x0E;
            break;
        case 'F':
            i = 0x0F;
            break;
        default:
            i = 22;
    }
    return (uint8_t)i;
};