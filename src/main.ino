#include "Arduino.h"
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <myavrsleep.h>
#include <EpdDht22.h>
#include <math.h>

#define PIN_DHT 2 
//#define DHT_TYPE DHT22
#define INTERVAL 60000  // sensor read out interval

#define TRANSISTOR_SWITCH_PIN 5

const bool TURN_ON=1; 
const bool TURN_OFF=0;

//GxEPD2_AVR_BW display(GxEPD2::GDEP015OC1, /*CS=*/ SS, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7);
GxEPD2_AVR_BW *display;


/**
 * Cyklus po 8s
 *
 * kazdych 30 cyklov (5 min) --> vycitat senzor do buffer
 * kazdych 120 cyklov (20 min) --> vykreslit s priemerom za 5 min
 * kazdych 720 cyklov (2 hod) --> vypocitat priemer za 2h a vykreslit graf
 */

#ifdef DBG
    #define FIVE_MIN 3  // 30 number of cycles during 5 minutes
    #define TWENTY_MIN 12 // 120 number of cycles during 20 min
    #define TWO_HOUR 72  // 720 number of cycles during 2 hours
#else
    #define FIVE_MIN 30  // 30 number of cycles during 5 minutes
    #define TWENTY_MIN 120 // 120 number of cycles during 20 min
    #define TWO_HOUR 720  // 720 number of cycles during 2 hours
#endif

volatile char sleepCnt = 0;

uint8_t numberOfWakes = 0;

Settings settings {
    PIN_DHT,
    TRANSISTOR_SWITCH_PIN, 
#ifdef DBG
    development
#else
    production
#endif
};


static EpdDht22 *epdDht22;


void setup(){
    Serial.begin(115200);
    Serial.println(F("setup"));

    epdDht22 = new EpdDht22(&settings);
    epdDht22->powerDown();
}


void loop(){

    Serial.print("Number of wakes: ");
    Serial.println(numberOfWakes);

    Serial.println("Read sensor .... ");
    readout();

    if((numberOfWakes % (uint16_t)(TWENTY_MIN / FIVE_MIN)) == 0) {
        Serial.println("Once in 20min: 5 min average and draw screen ...");
        epdDht22->twentyMinuteAverage();
    }

    if((numberOfWakes % (uint16_t)(TWO_HOUR / FIVE_MIN) == 0)){
        Serial.println("Once in 2h: 20 min average and draw history ...");
        epdDht22->twoHourAverage();
        numberOfWakes = 0;
    }

    if((numberOfWakes % (uint16_t)(TWENTY_MIN / FIVE_MIN)) == 0) {
        printScreen();
    }


   // Disable the ADC (Analog to digital converter, pins A0 [14] to A5 [19])
   //static byte prevADCSRA = ADCSRA;
   //ADCSRA = 0; 
   //set_sleep_mode(SLEEP_MODE_PWR_DOWN);
   //sleep_enable();

   while (sleepCnt <= FIVE_MIN ) {

        // Turn of Brown Out Detection (low voltage). This is automatically re-enabled upon timer interrupt
        //sleep_bod_disable();

        // Ensure we can wake up again by first disabling interrupts (temporarily) so
        // the wakeISR does not run before we are asleep and then prevent interrupts,
        // and then defining the ISR (Interrupt Service Routine) to run when poked awake by the timer
        noInterrupts();

        // clear various "reset" flags
        MCUSR = 0; 	// allow changes, disable reset
        WDTCSR = bit (WDCE) | bit(WDE); // set interrupt mode and an interval
        WDTCSR = bit (WDIE) | bit(WDP3) | bit(WDP2) | bit(WDP1) | bit(WDP0);    // set WDIE, and 1 second delay
        wdt_reset();

        // Send a message just to show we are about to sleep
        //Serial.println("Good night!");
        Serial.flush();

        // Allow interrupts now
        interrupts();

        // And enter sleep mode as set above
        //sleep_cpu();

        avrDeepSleep();
   }

    // --------------------------------------------------------
    // Controller is now asleep until woken up by an interrupt
    // --------------------------------------------------------

    // Prevent sleep mode, so we don't enter it again, except deliberately, by code
    //sleep_disable();

    // Wakes up at this point when timer wakes up C
    Serial.println("I'm awake!");
    numberOfWakes++;


    // Reset sleep counter
    sleepCnt = 0;

    // Re-enable ADC if it was previously running
    //ADCSRA = prevADCSRA;
    avrEnableAdc();
    delay(1000);
}


void readout(){
    //epdDht22->powerUp();

    Dht22Data _tmp = epdDht22->readDht22();
    Serial.print("Temperature: ");
    Serial.print(_tmp.temperature);
    Serial.print(" Humidity:: ");
    Serial.println(_tmp.humidity);
}


void printScreen(){
    epdDht22->powerUp();
    epdDht22->printScreen();
    epdDht22->powerDown();
}

// When WatchDog timer causes microcontroller to wake it comes here
ISR (WDT_vect) {

	// Turn off watchdog, we don't want it to do anything (like resetting this sketch)
	wdt_disable();

	// Increment the WDT interrupt count
	sleepCnt++;

	// Now we continue running the main Loop() just after we went to sleep
}
