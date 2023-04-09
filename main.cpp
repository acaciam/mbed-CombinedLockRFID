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
// #include "Firebase.h"
// #include "trng_api.h"
// #include "NTPclient.h"

// Nucleo Pin for MFRC522 reset (pick another D pin if you need D8)
#define MF_RESET    D8
#define SPI_MOSI PB_5
#define SPI_MISO PB_4
#define SPI_SCK PB_3

#define UART_RX     PA_3
#define UART_TX     PA_2

DigitalOut LedGreen(LED1);  //PB0
DigitalOut LedBlue(LED2);   //PB7
DigitalOut LedRed(LED3);    //PB14
DigitalOut Lock(PG_9);
InterruptIn button(PD_7);
InterruptIn rfidInt(PG_14);

BufferedSerial     pc(UART_TX, UART_RX, 9600);
MFRC522    RfChip   (SPI_MOSI, SPI_MISO, SPI_SCK, PG_2, MF_RESET);


//PA assigns --> used PE instead (Kept PWM as PA_5) 
//redone to account for new solenoid and elevator motor that require less wires
DigitalIn elevatorDoor(PE_0, PullUp);
DigitalIn elevatorPkgBmSns(PE_3, PullUp);
DigitalIn elevatorAuto (PE_4, PullUp); //elevatorSysEn --> elevatorAuto || 1 == auto :: 0 == manual
PwmOut pwmMotorCount(PA_5);
DigitalOut elevatorUp(PE_6); //elevatorPosBlock-->elevatorUp
DigitalOut elevatorDown(PE_7); //elevatorNegBlock --> elevatorDown
DigitalOut greenSolenoid(PE_8); //elevatorGrnWire --> greenSolenoid
DigitalIn elevatorTopSwitch(PE_12, PullUp);
DigitalIn elevatorBotSwitch(PE_15, PullUp);

DigitalIn motorUpBut(PD_0);
DigitalIn motorDownBut(PD_1);
void elevatorCtrl(void);
bool down = false;
bool up = false;
int elevatorCnt = 0;
int traveldown = 800;// number of pulses for stepper motor travel, 200 per revolution.			
int travelup = 800;	


//PB assigns -> switched around PB assignments to fit f429 board
DigitalIn outBmSns(PB_1, PullUp);
DigitalIn inBmSns(PB_2, PullUp);
DigitalOut lightDrSysEn(PB_6);
DigitalOut lightAccessGrnt(PB_8);
DigitalOut lightOccupantIn(PB_9);
DigitalOut lightOccupantOut(PB_10);
#define pb11 PB_11
//REPLACE PB3,4,5 -> PG0,1,3
DigitalIn pg0(PG_0);
DigitalIn drClsdSwitch(PG_1, PullUp);
DigitalIn NFCReq(PG_3, PullUp);

//PC assigns -> switched around pin assignments on PC to fit with our new board
DigitalOut faultLightAlarm(PC_0);
DigitalOut faultLightDrOpen(PC_2);
DigitalOut armedLight(PC_3);
DigitalIn interiorMotion(PC_6);
DigitalIn mArmCasePres(PC_7);
DigitalIn mArmHome(PC_8);
#define mArmOutMotionDetector PC_9




void setup(void);				
void movedown(int c);				
void moveup(int d);				
void delayMs(int n);
void TIM2_Config(void);				
void lightdisplay(void);				
void entrancedetection(void);				
void opendoor(void);
void multiarm(void);		

void blinkLED(DigitalOut led);
void flip(void);
void unlock(void);
void lock(void);
void rfidCtrl();
void buttonUnlock(void);

bool both=false;				
bool leaving= false;			
bool entering=false;			
bool inside=false;			
bool outside=false;			
bool armed=false;			
bool alarm=false;	
bool induct=false;		

bool rfidUnlockReq = false;
bool buttonUnlockReq = false;

int main(void) {	
    
    
  // Init. RC522 Chip
    RfChip.PCD_Init();
    pc.set_format(
        /* bits */ 8,
        /* parity */ BufferedSerial::None,
        /* stop bit */ 1
    );
    TIM2_Config();
    lock();

	setup();	

    button.rise(&unlock);  
		
	int condition;// variable for present condition of door.			
				
				
				
	//setup stepper motor			
				
    while(1) {				      
        lightdisplay();				
        entrancedetection();
    //  multiarm();

        rfidCtrl();        

        if((buttonUnlockReq||rfidUnlockReq) && elevatorAuto){ // "NFC request (low) and elevator system is automatic	(1)	
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
            uint8_t ID[] = {0xe3, 0xdf, 0xa6, 0x2e};
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
    
    if(elevatorAuto && !elevatorPkgBmSns && !elevatorDoor){ // automatic mode and package present and elevator's door closed (all active LOW)
        delayMs(5000);
        movedown(traveldown);
        delayMs(5000);
        moveup(travelup);

    }
    if(!elevatorAuto){ //if elevator in Manual mode, buttons control movement
        if(motorUpBut){
            if(down){ //checks to stop motor between direction VERY IMPORTANT!!
                delayMs(500);
                down = false;
            }
            elevatorUp = 1;
            elevatorDown = 0;
            up = true;
            elevatorCnt = 0;
        }
        else if(motorDownBut){
            if(up){ //checks to stop motor between directions VERY IMPORTANT!!
                delayMs(500);
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
            if(elevatorCnt > 500){ //if motor hasn't been active in at lease 500ms- reset up/down booleans (no longer causes delay before manual up/down) 
                up = false;
                down = false;
                elevatorCnt = 500;
            }
        }
    }
}
				
							
void movedown(int c){		
    int i = 0;		
    //ensure solenoid is off before proceeding
    greenSolenoid = 0;

    //motor off delay for 1000ms (ensure no fuse blown)
    elevatorUp = 0; //A6
    elevatorDown = 0; //A7
	delayMs(1000);		
				
	//c=c/2; // count is for each coil.			
	for(i = 0; i<= c; i++){		
        if(elevatorBotSwitch){ //break from the for loops section once the elevator has reached the bottom
            break;
        }	
        elevatorUp = 0;
        elevatorDown = 1;
        delayMs(10);		
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
        if(elevatorTopSwitch){ //break from the for loops section once the elevator has reached the Top
            break;
        }				
        elevatorUp = 1; //A6
        elevatorDown = 0; //A7
	    delayMs(10);							
	}			
    // motor off
    elevatorUp = 0; //A6
    elevatorDown = 0; //A7				
}				
void lightdisplay(void){						
	if(lightDrSysEn && (!elevatorAuto)){		//LIGHTS UP WHEN SYS IS MANUAL	
	    lightDrSysEn = 0;
    } 			
//	GPIOB->ODR |= (doormode << 3);// activate led for door mode			
 //   inBmSns = doormode;
	// Door left open fault: Delivery person walked away leaving the door open			
	if(outside && drClsdSwitch){ //person left and door left open: Door open fault		
        faultLightDrOpen = 1; 				
	}else{
        faultLightDrOpen = 0; 
    }		
	if(armed && (!interiorMotion && inside)){ //armed, no motion, entrant left: Door fault			
        faultLightAlarm = 1;				
        delayMs(1000);				
		faultLightAlarm = 0; 		
	}			
	if(armed){			
		armedLight = 1;
    }		
	if(!(armed)){			
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
    pwmMotorCount.pulsewidth(0.0013);   //pulse width varies between 1.3, 1.5, and 1.7 ms 	
}				
				

void blinkLED(DigitalOut led){
    led = 1;
    ThisThread::sleep_for(500ms);
    led = 0;
}
void buttonUnlock(void){
    buttonUnlockReq = true;
}
void unlock(void){
    Lock = 0;
 //   delayMs(1000);
   // Lock = 1;
}
void lock(void){
    Lock = 1;
    ThisThread::sleep_for(50ms);
}

void rfidCardPres(void){
    cardPresent = true;
    blinkLED(LedBlue);
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



void multiarm(void){
   // int cw = 2080-1; (1.3ms)
  //  int stopped = 2400-1; (1.5ms)
   // int ccw = 2720-1; (1.7ms)
    int c=0;
    
    if(!(mArmCasePres)){//motion sensor quiet, package present
        induct=true;
        pwmMotorCount.pulsewidth(0.0013);   //TIM2->CCR1 = cw;
        delayMs(5000);//engage motor for 2.5 seconds
        pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
    }
    //jamming suspected.
    for(int i=0; i<2;i++){
        if(!(mArmCasePres)){
            pwmMotorCount.pulsewidth(0.0017); //TIM2->CCR1 = ccw;
            delayMs(2500);
            pwmMotorCount.pulsewidth(0.0013); //TIM2->CCR1 = cw;
            delayMs(5000);//engage motor for 2.5 seconds
            pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
        }
    }
    
    if((mArmCasePres&&mArmHome)&& induct ){//both beams cleared, induction started:
        while((mArmHome) && (c<100)){
            pwmMotorCount.pulsewidth(0.0013); //TIM2->CCR1 = cw;
            delayMs(100);
            pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
            c=c+1;
        }
        c=0;
    }
    pwmMotorCount.pulsewidth(0.0015); //TIM2->CCR1 = stopped;
    induct=false;
}
