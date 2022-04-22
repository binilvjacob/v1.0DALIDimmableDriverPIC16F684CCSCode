#include <slave.h>					// Config file
#include<math.h>
#include <STDLIB.H>

																			// Code Version 1	v1.0

#use delay(clock=4000000)			// Internal clock of 4MHz

/////////////////////////////////////////////////////////////////////// CONFIGURATION PART START /////////////////////////////////////////////////////////////////////////////

#define Fixlampid   1 				// LAMP Address range 0-63
#define zoneid_init   210			// Zone Address range 207-223
#define device_type 4 				// Device type. 1 - normal led driver, 2 - fan driver, 3 - curtain driver, 4 - strip led driver, 5 - TRIAC MOSFET driver

#define G1 0b00000001
#define G2 0b00000000
#define rx pin_a1
#define tx pin_a0

#define MaxDuty  100 				// Maximum lux level
#define MinDuty  0					// Minimum lux level

			/// EEPROM locations ///

#define PowerOnLevelStore   0           // Latest lux level of the device	
#define MinimumLevelStore   2			// Minimum lux level storage
#define MaximumLevelStore  	3 			// Maximum lux level storage
#define FadeRateStore      	4  			// Faderate storage
#define ShortAddressStore  	6  			// Individual deviceID storage
#define Group_07Store    	7			// Group address first byte storage
#define Group_815Store  	8			// Group address second byte storage
#define SceneStore  		9			// Scenes storage starting location
#define ZoneIDStore 		32			// Zone ID storage location

/////////////////////////////////////////////////////////////////////// CONFIGURATION PART END ///////////////////////////////////////////////////////////////////////////////

#byte dutyreg = 0x15
 
#bit intf = 0x0b.1
#bit timerOnOff =0x10.0

int1 oddevenbit,a,atmp,b,error_flag,over_flowflag;
unsigned int8 dataCount;
char data[3],bitcount,tout;
unsigned char duty;

unsigned int16 SetPower,power;

char settling_time,i,dly=4,j;
int1 txmit_error=0;
char tx_buffer[3];
char r_a,currentSceen;
char l_st;
char command_st,RetryCount;
char FadeRateCount=1;

char zoneid=zoneid_init;


char stopBitCount,address ,command,databyte;
int1 dataready,forwrdFrameFlag,backwardFrameFlag ,masterFlag ;
int16 readDly=300;
int16 GroupSelectReg;
char gindex;

/////// new  //////
int txmit_count=0;
int error_value=0;
///////////////////

char MinimumLevel;
char MaximumLevel;
char FadeRate;
char PowerOnLevel;
char DTR,DwriteLocation,DTR_Ready;

char lampid  = Fixlampid;
int1 reset_flag=0;

void readData(void);
void init(void);
void handle(void );
void copyData(void);
void commands(void);
void txmit(char priority,char length);
void txmit1(void);
void txmit0(void);
void stopbit(void);
void lamp_on(void);
void lamp_off(void);
void startBit(void);
void init_from_eeprom(void);
void SetDimmLevel(unsigned int dimPesentage);

#rom  0x2100={MaxDuty,50,MinDuty,Maxduty,5,6,Fixlampid,G1,G2,6,7,8}				// Initial EEPROM values
#rom  0x2120={zoneid_init}

#int_EXT
EXT_isr() 
{ 
	output_toggle(pin_c3);
	clear_interrupt(int_ext);
	disable_interrupts(int_ext);
	disable_interrupts(INT_RTCC);
	bitcount=0;
	setup_timer_1(T1_internal|T1_div_by_1);
	set_timer1(0xffff-840); 					//858  880///old value 923
	enable_interrupts(int_timer1);
	stopBitCount = 0;
	oddevenbit=1;
	data[0]=0;
	data[1]=0;
	data[2]=0;
	tout=0 ;
	datacount = 0;   
	settling_time = 0; 
}

#int_TIMER1
TIMER1_isr()
{
	output_toggle(pin_c2);
	readDly=20;
	error_flag=0;
		if(oddevenbit==1)
		{
			a=input(rx); 
			atmp=a ;          
			oddevenbit=0 ;
			
					if(atmp)
					{
						while(atmp)
							{
								atmp=input(rx);
								if(readDly>0)
									readDly--;
								else
									atmp=0;
									
							}
					}         
					else
					{
						while(!atmp)
							{
								atmp=input(rx);
								readDly--;
									if(readdly==0)
										{
										atmp=1;
										}	
							}
					}
	
				setup_timer_1(T1_internal|T1_div_by_1);			//set timer1 with 1us least count
				set_timer1(0xffff-150);  						//374 355 350 old value 150	
		}
		else
		{ 
			b=input(rx) ; 										// store data line status in the second half
			oddevenbit=1;
			setup_timer_1(T1_internal|T1_div_by_1);
			set_timer1(0xffff-350);  							// delay  till the next call st to 73 us/////old value 350
			readData();  										// function  get the data from the conditions of a and b
						
		}
	return(0);
}

#int_RTCC
RTCC_isr()
{	
	reset_flag=1;
	if(FadeRateCount>0)
	{
		FadeRateCount--;
	}
	else
	{
		
		FadeRateCount=FadeRate;
		if(SetPower <Power)
		{	
				SetPower++;	
				set_pwm1_duty(SetPower );
		}
		else if(SetPower > Power)
		{	
				SetPower--;	
				set_pwm1_duty(SetPower );
		}
		else
		{
				set_pwm1_duty(SetPower );
		}
	}
	dly--;
  	if (dly == 0)
  	{	
      dly = 4;
      if(settling_time < 250)
      {
          settling_time++;
      }              
   }

}

void main(void)

{
	setup_wdt(WDT_ON);
	setup_wdt(WDT_72MS|WDT_TIMES_16);		//~1.1 s reset	
	
	init_from_eeprom();
	init();		
	GroupSelectReg = MAKE16(read_EEPROM (Group_815Store ),read_EEPROM (Group_07Store));	
	PowerOnLevel = read_EEPROM (PowerOnLevelStore);

	if(PowerOnLevel<= 2)
	{
		duty=0;	
		lamp_off();
		SetPower =0;
		set_pwm1_duty(SetPower );		
	}
	else
	{
		duty = PowerOnLevel;
		SetDimmLevel(duty);	
		set_pwm1_duty(Power );
		SetPower = Power;
		FadeRateCount=FadeRate;	
		lamp_on();		
	}

start:

	if(reset_flag==1)
	{
	restart_wdt(); 
	reset_flag=0;
	}		
	if (dataReady ==1)
	{
		if(address == 0xff)
		{
			handle(); 
		}		
		else if(address==lampid)		{
			
			handle(); 
		}		
    	else if(address == zoneid)
		{
			handle();	
		}
		else if(address>191 && address<208)
		{	
			gindex = address &0x0F;
			if ( bit_test (GroupSelectReg, gindex)==1)
			{ 				
				handle();
			}	
		}
		dataReady =0;
	}
	if(txmit_error==1 && txmit_count<5)
	{
		txmit_count++;
		txmit(2,2);
	}	
	else
	{
		txmit_count=0;
	}	
	goto start;
}

void init(void)
{
	setup_timer_2(T2_DIV_BY_16,124,1);  // 500 hz //old value T2_DIV_BY_16 //old value 124,10
 	setup_ccp1(CCP_PWM);	
	setup_timer_0(RTCC_INTERNAL|RTCC_DIV_1);
	setup_timer_1(T1_internal|T1_div_by_1);
	timerOnOff=0;
	clear_interrupt(int_ext);
	ext_int_edge( H_TO_L );	
	enable_interrupts(INT_EXT);
	enable_interrupts(INT_RTCC);
	disable_interrupts(INT_TIMER2);
	enable_interrupts(global);	
	settling_time =23;
	dataReady =0;
	SetPower=0;    
	return;
}

void handle(void )

{
	commands();
	delay_ms(2);
	RetryCount =0;
	return;
}


//				trnsmission of  bit 1			//
/*********************************************************************
 * Function:       void txmit0(void);
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          transmission of  bit 1 to the bus	
 *
 * Side Effects:    None
 *
 * Note:            None
**********************************************************************/

void txmit1(void)

{     
  	txmit_error = 0;
	if (input(rx)==1)
	{  
		output_bit(tx,0);
	}
	delay_us(79);
	if (input(rx)==1)
	{
		output_bit(tx,1);
		txmit_error = 1;
		return;
	}			  
	delay_us(290);		//345
	if (input(rx)==0)
	{
		output_bit(tx,1);
	}
	else
	{
		output_bit(tx,1);
		txmit_error = 1;
		return;
	}
	delay_us(79);
	if (input(rx)==0)
	{
		output_bit(tx,1);
		txmit_error = 1;
		return;
	}
    delay_us(290);
	if (input(rx)==0)
	{
		output_bit(tx,1);
		txmit_error = 1;
		return;
	}
    return;
}


//         transmission of 0 to the bus      //
/*********************************************************************
 * Function:       void txmit0(void);
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          transmission of  0 bit to the bus	
 *
 * Side Effects:    None
 *
 * Note:            None
**********************************************************************/

void txmit0(void)

{
	txmit_error = 0;	
	output_bit(tx,1);
	delay_us(79);
	if (input(rx)!=1)
	{		
		txmit_error = 1;
		return;
	}   
	delay_us(290);
	if (input(rx)==1)
	{
		output_bit(tx,0);
	}
    else
	{
		output_bit(tx,1);
		txmit_error = 1;
		return;
	}
    delay_us(79);
    if (input(rx)==1)
	{		
		txmit_error = 1;
		return;
	}
    delay_us(290);
	if (input(rx)==1)
	{		
		txmit_error = 1;
		return;
	}
    return;
}
//-----------------------------------------------------------------------------
                   // txmit2 bit
//-----------------------------------------------------------------------------

void txmit(char priority,char length)
{ 	
     j= 8*length;
	 while (settling_time < 12+Fixlampid);     // priority
     disable_interrupts(global);
     txmit1();        // start bit  
     for(i=0;i<j;i++)
         {
            if (shift_left(tx_buffer,3,1)==1)
            {
                 txmit1();
            }
            else
            {
                  txmit0();
            }
            if (txmit_error ==1)
            {
               goto rr;
            }		
         }        
	    stopbit();    
	    stopbit(); 
		stopbit(); 
		stopbit();
	rr: output_bit(tx,1);
		settling_time = 0;
	    intf =0;
	    enable_interrupts(global);	
		enable_interrupts(INT_RTCC);
	    return;
}

//--------------------------------------------------------------------------
          // stop bit function //
//--------------------------------------------------------------------------
void  stopbit(void)
{
      output_bit(tx,1);
      delay_us(830);
      return;
}
//--------------------------------------------------------------------------


void readData(void)
{ 
      error_flag=0;
      datacount++;
      forwrdFrameFlag = 0;
	  backwardFrameFlag =0;
      if(datacount< 27)
      {
         if((a==0 )&& (b==1))
         {
            shift_left(data,3,1);  // a one detected on bus 
         }
         else if((a==1)&&(b==0))
         {
            shift_left(data,3,0);  // a zero is detected on the bus 
         }
         else if ( a==1 && b==1)
         {
            switch (datacount)
            {
               case 17:
               {
                     stopBitCount ++;
                     break;
               }
               case 18:
               {
                  stopBitCount ++;
                  if(stopBitCount == 2)
                  {
                        r_a=1; 
                        copyData();
                        forwrdFrameFlag = 1;
                        masterflag = 0;
                        backwardFrameFlag =0;

                  }
                  else
                  {
						error_flag =1;
						clear_interrupt(int_ext);
						enable_interrupts(INT_EXT);
    				  disable_interrupts(int_timer1);
    		          enable_interrupts(INT_RTCC);
                  }
                  break;
               }
              	case 25:
				{
					stopBitCount ++;
					break;
				}
              	case 26: 
				{
					stopBitCount ++;
					if(stopBitCount == 2)
					{
						r_a=0; 
						copyData();
						forwrdFrameFlag =0;
						masterflag = 1;
						backwardFrameFlag =0;
					}
					else
					{
						error_flag =1;
						clear_interrupt(int_ext);
						enable_interrupts(INT_EXT);
    				  disable_interrupts(int_timer1);
    		          enable_interrupts(INT_RTCC);
					}
					break;
				}
                default:
                {
                      error_flag=1;
                      timerOnOff=0;
					  clear_interrupt(int_ext);
                      enable_interrupts(INT_EXT);
    				  disable_interrupts(int_timer1);
    		          enable_interrupts(INT_RTCC);
                      settling_time = 0;
                      break;
                }
             }   
          } 
		else
		{
			error_flag=1;    
			settling_time = 0;
			timerOnOff=0;    
			clear_interrupt(int_ext);   
			enable_interrupts(INT_EXT);
    		disable_interrupts(int_timer1);
    		enable_interrupts(INT_RTCC);         
		}
      }
	else  										// The  data count is greater than 27 
	{
		over_flowflag =1 ;
		settling_time = 0;
		timerOnOff=0;   
		clear_interrupt(int_ext);    
		enable_interrupts(INT_EXT);
        disable_interrupts(int_timer1);
        enable_interrupts(INT_RTCC);        
	}
    return;
}

void copyData(void)
{  
	dataReady =1;    
	if( r_a==1)
	{
		address = data[1];
		command =data[0];						
	}
	else if( r_a==0)
	{	
		address = data[2];
		command =data[1];
		databyte=data[0]; 
	}       
    timerOnOff=0;
    intf =0;
	clear_interrupt(int_ext);
    enable_interrupts(INT_EXT);
    disable_interrupts(int_timer1);
    enable_interrupts(INT_RTCC);
    settling_time = 0;
    return;
}


void commands(void)				// DALI commands decoder function
{ 
	command_st =0;	
	switch(command)
	{

	   	case 201:														// goto intermediate lux level ( range 0 to 100 )
		{  
			
			if(databyte > MaximumLevel )
			{
				duty = MaximumLevel;
			}
			else if(databyte< MinimumLevel )
		    {
			duty = MinimumLevel;
			}
			else
			{
				duty =databyte;
			} 		
			lamp_on();		
			SetDimmLevel(duty);									
			break;
		}
		case 208:														// Take the device  to full lux
		{  
			
			duty = MaximumLevel;
			lamp_on();	
							
			break;
		}
		case 212:														// Take the device to minimum lux
		{  
			
			duty =0;
			lamp_off();
			break;
		}
		case 216:														// Dim at individual device address			
		case 241:														// Dim at zone address
		{
			if(l_st==1)
			{				
				if(duty>MinimumLevel)
				{							
					duty--;
					SetDimmLevel(duty);					
				}
			}
			break;
		}
		case 220:														// Bright at individual device address
		case 240:  														// Bright at zone address
		{
			if(l_st==1)
			{			
				if(duty < MaximumLevel)
				{									
					duty++;
					SetDimmLevel(duty);			
				}
			}
			break;
		}	
	
		case 234: 														// Select a scene from 0-15 
		{
			
			if(databyte < 17)
			{				
				currentSceen = databyte;			
		        duty = read_EEPROM (currentSceen+SceneStore);	
			     	if(duty<=MinimumLevel)
					{
						duty=0;					
						lamp_off();		
					}
					else
					{								
						lamp_on();	
						SetDimmLevel(duty);		
					}			
			}
			break;
		 }
		case 231:  														// Store the current lux level of the device to a scene from 0-15
		{

			if(databyte < 17)
			{				
				disable_interrupts (global);
				write_eeprom(databyte+SceneStore,duty);
				delay_us(5);			
				enable_interrupts(global);	
			}
			break;
		}
		case 9:															// Add or Remove the device from a group from 0-15
		{		
				GroupSelectReg = MAKE16(read_EEPROM (Group_815Store ),read_EEPROM (Group_07Store));	
				gindex = databyte &0x0f;				
				switch (databyte & 0x10)
				{
					case 0:
						{
							bit_clear(GroupSelectReg,gindex);
							write_eeprom(Group_07Store  ,make8(GroupSelectReg,0));
							delay_us(10);
							write_eeprom(Group_815Store,make8(GroupSelectReg,1));
 							delay_us(10);
							break;
						}
					case 16:
						{
							bit_set(GroupSelectReg,gindex);
							write_eeprom(Group_07Store  ,make8(GroupSelectReg,0));
							delay_us(10);
							write_eeprom(Group_815Store,make8(GroupSelectReg,1));
 							delay_us(10);
							break;
						}
					
					default: break;

				}
				break ;
		}
		case 34:    													// Write short address alias lampid
		{
			if(databyte <64)
			{
					lampid = databyte;
					write_eeprom(ShortAddressStore ,lampid);
					delay_us(10);
			}
		
			break;		
		}
		case 35:    													// Write the desired data value to DTR register 
		{
					DTR = databyte;	
					DTR_Ready =1;
					break;
		}
		case 36:    													// Write the value stored in DTR to the desired address location 
		{
					
				DwriteLocation = databyte;	
				if(DTR_Ready ==1 && DwriteLocation<33 )
				{
					DTR_Ready =0;
					write_eeprom(DwriteLocation,DTR);
					DELAY_US(20);
				}
			init_from_eeprom();
			break;
		}
		case 37:    													// Read the value stored in DTR register of the device 
		{
				tx_buffer[2]=lampid;tx_buffer[1]=DTR; txmit(2,2); 
				break;			
		}
		case 38:    													// Store the desired EEPROM location value to DTR register
		{
				DwriteLocation = databyte;	
				if( DwriteLocation<33 )
				{
					DTR=Read_eeprom(DwriteLocation);
				}	
				break;				
		}
		case 39:														// Read the current lux level of the device
		{
			tx_buffer[2]=lampid;tx_buffer[1]=Read_eeprom(0); 
	    	txmit(2,2);		
			break;
		}
		case 49: 														// Write Zone ID of the device to corresponding EEPROM location
		{
			if(databyte >=208 && databyte <=223)
			{
					zoneid = databyte;
					write_eeprom(ZoneIDStore ,zoneid);
					delay_us(10);
			}		
			break;

		}
		case 50: 														// Read first group section byte status ( 0 -  absent in the grp, 1 - present in the group )
		{
			tx_buffer[2]=lampid;tx_buffer[1]=Read_eeprom(7); 
			txmit(2,2);			
			break;

		}
		case 51: 														// Read second group section byte status ( 0 -  absent in the grp, 1 - present in the group )
		{
			tx_buffer[2]=lampid;tx_buffer[1]=Read_eeprom(8); 
			txmit(2,2);			
			break;

		}
		case 42: 														// Write Maximum lux level of the device
		{
			MaximumLevel=databyte;
			write_eeprom(MaximumLevelStore,MaximumLevel);
			delay_us(10);			
			break;

		}
		case 43: 														// Write Minimum lux level of the device
		{
			MaximumLevel=databyte;
			write_eeprom(MinimumLevelStore,MinimumLevel);
			delay_us(10);		
			break;
		}
		case 47:														// Write FadeRate of the device
		{
			FadeRate=databyte;
			write_eeprom(FadeRateStore,FadeRate);
			delay_us(10);		
			break;
		}
		case 52:														// Read code version of the device
		{
			tx_buffer[2]=lampid;tx_buffer[1]=1; 
			txmit(2,2);			
			break;
		}
		default:
		{
			command_st=1;
			break;
		}
	}
	if(command_st==0)
	{ 		
		  write_eeprom(PowerOnLevelStore,duty);							// Write current lux level to EEPROM location
	}
return;
}

void lamp_on()															// Taking to corresponding lux level
{	
	SetDimmLevel(duty);
	l_st=1;
	return;
}
	
void lamp_off()															// Taking to Minimum lux level
{	
	SetDimmLevel(MinimumLevel);	
	l_st=0;
	return;
}
	
void SetDimmLevel(unsigned int dimPesentage)							// Mapping lux level of 0-100 to PWM levels of 0-1023
{
	if(dimPesentage > MaximumLevel)
		{
			dimPesentage = MaximumLevel;
		}	
	if(dimPesentage<10)											// Strip driver mapping
	{
		Power =dimPesentage;
	}
	else if(dimPesentage<30)
	{
		Power =10+_mul((dimPesentage-10),2);
	}
	else if(dimPesentage<60)
	{
		Power =50+_mul((dimPesentage-30),5);
	}
	else
	{
		Power =195+ _mul(dimPesentage-60,10);
	}	
	if(Power > 1020)
	{
		Power =1020;
	}	

/*
  	if(dimPesentage<10)											// Normal driver mapping
	{
		Power =_mul(dimPesentage,10);
	}
	else if(dimPesentage<90)
	{
		Power =_mul((dimPesentage-10),4)+100;
	}
	else if(dimPesentage<=100)
	{
		//Power =_mul(dimPesentage,6);
		Power =_mul(dimPesentage-89,45)+570;
	}	
	if(Power > 1020){Power =1020;}	
*/

	}


void init_from_eeprom(void)									// Reading values from EEPROM locations
{
	GroupSelectReg = MAKE16(read_EEPROM (Group_815Store ),read_EEPROM (Group_07Store));	
	delay_us(10);
	PowerOnLevel 		= 	read_EEPROM (PowerOnLevelStore);
	delay_us(10); 	    
	MinimumLevel		= 	read_EEPROM ( MinimumLevelStore );   
	delay_us(10);       	
	MaximumLevel 		= 	read_EEPROM ( MaximumLevelStore); 
	delay_us(10);  		
	FadeRate 			= 	read_EEPROM ( FadeRateStore);
	delay_us(10);
	lampid 				=	read_EEPROM ( ShortAddressStore );
	delay_us(10);
	zoneid				=	read_EEPROM(zoneidstore);
	delay_us(10); 
}


