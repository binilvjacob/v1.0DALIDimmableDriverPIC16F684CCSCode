#include <16F684.h>

#device adc=8

#FUSES WDT            // Watch dog timer enabled
#FUSES INTRC_IO       // Internal crystal osc <= 4mhz for PCM/PCH , 3mhz to 10 mhz for PCD
#FUSES NOPROTECT      // Code not protected from reading
#FUSES BROWNOUT       // Brownout reset enabled
#FUSES MCLR           // Master Clear enabled
#FUSES NOCPD          // No EE protection
#FUSES PUT            // Power up timer enabled
#FUSES NOIESO         // Internal External Switch Over mode disabled
#FUSES NOFCMEN        // Fail-safe clock monitor disabled

#use delay(clock=4000000)