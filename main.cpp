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
DigitalIn elevatorDoor(PE_0, PullUp);
DigitalIn elevatorPkgBmSns(PE_3, PullUp);
DigitalIn elevatorSysEn(PE_4, PullUp);
PwmOut pwmMotorCount(PA_5);
DigitalOut elevatorPosBlock(PE_6);
DigitalOut elevatorNegBlock(PE_7);
DigitalOut elevatorGrnWire(PE_8);
DigitalOut elevatorYlwWire(PE_9);
DigitalOut elevatorGryWire(PE_10);
DigitalOut elevatorRedWire(PE_11);
DigitalIn elevatorTopSwitch(PE_12, PullUp);
DigitalIn elevatorBotSwitch(PE_15, PullUp);

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



DigitalIn shaftButUp(PD_0);
DigitalIn shaftButDown(PD_1);
DigitalIn shaftBmSns(PD_2);
DigitalOut shaftMotorUp(PD_3);
DigitalOut shaftMotorDown(PD_4);
DigitalIn manual(PD_5);
void shaftMotor(void);
bool shaftPackPres = 0;
int shaftCnt = 0;

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
void rfidCardPres(void);
void buttonUnlock(void);

bool both=false;				
bool leaving= false;			
bool entering=false;			
bool inside=false;			
bool outside=false;			
bool armed=false;			
bool alarm=false;	
bool induct=false;		

bool cardPresent = false;
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

    button.rise(&unlock);  //FIXME replace flip with unlock
  //  rfidInt.rise(&rfidCardPres);

	int traveldown = 800;// number of pulses for stepper motor travel, 200 per revolution.			
	int travelup = 800;			
	int condition;// variable for present condition of door.			
				
				
				
	//setup stepper motor			
				
    while(1) {				
                    
        lightdisplay();				
        entrancedetection();
//      multiarm();
        if(buttonUnlockReq){
            unlock();
            buttonUnlockReq = false;
        }
            // Look for new cards
        if ( RfChip.PICC_IsNewCardPresent())
        {
            blinkLED(LedBlue);
        // ThisThread::sleep_for(500ms);
        // continue;
        // }
        // if(cardPresent){
        //     cardPresent = false;
        //     // Select one of the cards
            if ( RfChip.PICC_ReadCardSerial()) {
                // Print Card UID
                uint8_t ID[] = {0xe3, 0xdf, 0xa6, 0x2e};
                if(RfChip.uid.uidByte[0] == ID[0] && RfChip.uid.uidByte[1] == ID[1] && RfChip.uid.uidByte[2] == ID[2] && RfChip.uid.uidByte[3] == ID[3]){
                    printf("Card Match! \n");
                    unlock();
                    blinkLED(LedGreen);
                }
                else{
                    lock();
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
        

        if((!NFCReq) && (elevatorSysEn)){ // "NFC request (low) and chute mode not depressed/open.			
            lock();
            opendoor();
        }			
                        
        if(!(elevatorSysEn && elevatorDoor && elevatorPkgBmSns)){ //if package present, DOOR CLOSED and system enable switch is DEPRESSED/GROUND/0. 				
            delayMs(5000);// wait 5 seconds after door is closed				
            movedown(traveldown);			
            delayMs(5000);			
            moveup(travelup);			            
        }				        
    shaftMotor();

    }  				
} 				
				
							
void movedown(int c){				
	int i;			
	int r = 5; // on count time			
	int o = 4; // off count time =r-o			
				
        
    elevatorGrnWire = 1; //A8
    elevatorYlwWire = 1; //A9
    elevatorGryWire = 1; //A10
    elevatorRedWire	= 1; //A11	

    elevatorPosBlock = 1; //A6
    elevatorNegBlock = 1; //A7
			
				
				
	//c=c/2; // count is for each coil.			
	for(i = 0; i<= c; i++){			
		if (!elevatorTopSwitch) {	
            //active low A11, A10	
	        elevatorGryWire = 0; //A10
            elevatorRedWire	= 0; //A11 	

            //phase 1	
            //active low A11, A10	
	        elevatorGryWire = 0; //A10
            elevatorRedWire	= 0; //A11 	
            delayMs(r);			
	        elevatorGryWire = 1; 
            elevatorRedWire	= 1; 	
            delayMs(r-o);		
                    
				
            //phase 2		
            //active low A9, A10	
	        elevatorGryWire = 0; //A10
            elevatorYlwWire	= 0; //A9		
            delayMs(r);		
            elevatorGryWire = 1; //A10
            elevatorYlwWire	= 1; //A9			
            delayMs(r-o);		
                    
                    
            //phase 3		
            //active low A9, A8	
	        elevatorGrnWire = 0; //A8
            elevatorYlwWire	= 0; //A9		
            delayMs(r);		
            elevatorGrnWire = 1; //A8
            elevatorYlwWire	= 1; //A9			
            delayMs(r-o);

            //phase 4		
            //active low A11, A8	
	        elevatorRedWire = 0; //A11
            elevatorGrnWire	= 0; //A8		
            delayMs(r);		
            elevatorRedWire = 1; //A11
            elevatorGrnWire	= 1; //A8				
            delayMs(r-o);		
                    
                    
		}		
				
	}	
    // cut common power
    elevatorPosBlock = 0; //A6
    elevatorNegBlock = 0; //A7		
	// cut motor outputs	
    elevatorGrnWire = 0; //A8
    elevatorYlwWire = 0; //A9
    elevatorGryWire = 0; //A10
    elevatorRedWire	= 0; //A11		
}				
void moveup(int c){				
    int i;				
	int r = 5; // on count time			
	int o = 4; // off count time =r-o			
	//power control sides A8,A9,A10,A11				
    elevatorGrnWire = 1; //A8
    elevatorYlwWire = 1; //A9
    elevatorGryWire = 1; //A10
    elevatorRedWire	= 1; //A11	
    //power supply side pin A6,A7
    elevatorPosBlock = 1; //A6
    elevatorNegBlock = 1; //A7		
				
				
	//c=c/2; // count is for each coil.			
	for(i = 0; i<= c; i++){			
	if (!elevatorBotSwitch) {			
		//phase 1				
        //active low A9, A8	
	    elevatorGrnWire = 0; //A8
        elevatorYlwWire	= 0; //A9		
        delayMs(r);		
        elevatorGrnWire = 1; //A8
        elevatorYlwWire	= 1; //A9			
        delayMs(r-o);		
		//phase 2		
		 //active low A9, A10	
	    elevatorGryWire = 0; //A10
        elevatorYlwWire	= 0; //A9		
        delayMs(r);		
        elevatorGryWire = 1; //A10
        elevatorYlwWire	= 1; //A9	
		delayMs(r-o);		
				
				
		//phase 3		
		//active low A11, A10	
	    elevatorGryWire = 0; //A10
        elevatorRedWire	= 0; //A11 	
        delayMs(r);			
	    elevatorGryWire = 1; 
        elevatorRedWire	= 1; 			
		delayMs(r-o);		
				
				
		//phase 4		
        //active low A11, A8	
	    elevatorRedWire = 0; //A11
        elevatorGrnWire	= 0; //A8		
        delayMs(r);		
        elevatorRedWire = 1; //A11
        elevatorGrnWire	= 1; //A8			
		delayMs(r-o);		
				
				
	}			
				
	}			
    // cut common power
    elevatorPosBlock = 0; //A6
    elevatorNegBlock = 0; //A7		
	// cut motor outputs	
    elevatorGrnWire = 0; //A8
    elevatorYlwWire = 0; //A9
    elevatorGryWire = 0; //A10
    elevatorRedWire	= 0; //A11			
}				
void lightdisplay(void){				
	int doormode=elevatorSysEn;			
	if(lightDrSysEn && (!elevatorSysEn)){			
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
	lightAccessGrnt = 1; // turn on request to enter light. pb8			
	//power control sides A8,A9,A10,A11				
    elevatorGrnWire = 1; //A8
    elevatorYlwWire = 1; //A9
    elevatorGryWire = 1; //A10
    elevatorRedWire	= 1; //A11	
    //power supply side pin A6,A7
    elevatorPosBlock = 1; //A6
    elevatorNegBlock = 1; //A7				
	int timeout=0;			
				
				
	elevatorGrnWire = 0;//activate solenoid pa8			
				
	while ((!drClsdSwitch) && (timeout<=5)  ){//while door closed, before time runs out			
        delayMs(500);			
        timeout=timeout+1;			
	}			
				
				
    // cut common power
    elevatorPosBlock = 0; //A6
    elevatorNegBlock = 0; //A7		
	// cut motor outputs	
    elevatorGrnWire = 0; //A8
    elevatorYlwWire = 0; //A9
    elevatorGryWire = 0; //A10
    elevatorRedWire	= 0; //A11		
	lightAccessGrnt = 0; // turn off request to enter light. pb8			
}				
//****************************Driver Setup*******************************				
void setup(void){							
	//DISABLE RELAY POWER AND CONTROL SIGNALS ZERO			
    elevatorPosBlock = 0; elevatorNegBlock = 0; 	 // cut common power		
    elevatorGrnWire = 0; elevatorYlwWire = 0; elevatorGryWire = 0; elevatorRedWire	= 0;	// cut motor outputs
    // GPIOA->AFR[0] |= 0x00100000; /* PA5 pin for tim2 */ 		
    // GPIOA->MODER |= 0x00000800;  /* setup TIM2 */  				
    // RCC->APB1ENR |= 1; /* enable TIM2 clock */  				
    // TIM2->PSC = 10 - 1; /* divided by 10 */  				
    // TIM2->ARR = 32000 - 1; /* divided by 26667 */  				
    // TIM2->CNT = 0;  				
    // TIM2->CCMR1 = 0x0060; /* PWM mode */  				
    // TIM2->CCER = 1; /* enable PWM Ch1 */  				
    // TIM2->CCR1 = 2080 - 1; /* pulse width 1/3 of the period */  				
    // TIM2->CR1 = 1; /* enable timer */  

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
void shaftMotor(){
    if((!manual && shaftBmSns) | (!manual && shaftPackPres)){ // 
        shaftPackPres = 1;
    }
    else{
        shaftPackPres = 0;
        shaftCnt = 0;
    }
    if(manual){
        if(shaftButUp){
            shaftMotorUp = 1;
            shaftMotorDown = 0;
        }
        else if(shaftButDown){
            shaftMotorUp = 0;
            shaftMotorDown = 1;
        }
        else{ //else no movement
            shaftMotorUp = 0;
            shaftMotorDown = 0;
        }
    }
    else if(shaftPackPres){
        //move up to the top, then move all the way down
        shaftCnt++;
        if(shaftCnt <= 180){  //120*5ms should be 9s (give 9s to get all the way up)
            shaftMotorUp = 1;
            shaftMotorDown = 0;
            delayMs(5);
        }
        if(shaftCnt > 180 && shaftCnt <= 360){ //give another 9s to get back to the bottom
            shaftMotorUp = 0;
            shaftMotorDown = 1;
            delayMs(5);
        }
        if(shaftCnt > 360){
            shaftPackPres = 0;
            shaftCnt = 0;
        }

    }
}
