/***************************************************************************************************/
// SISInterruptService.cpp
//  The routines to handle interrupt servicing and decoding of the protocol.
//  Communicates with main code:
//      codeAvailable - True when a new code is available
//      receivedSensorCode - The value of the new code
//  When the main code has finished processing the receivedSensorCode it should call
//  resetCodeAvailable.
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  This software uses code extracted from the Arduino RC-Switch library:
//    https://github.com/sui77/rc-switch
// Portions of this software that have been extracted from RC-Switch are subject to the RC-Switch
// license, as well as the the SIS Terms of Use.
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/
#include <SISInterruptService.h>
#include <SISGlobals.h>
#include "application.h"


const int INTERRUPT_315 = 3;   // the 315 MHz receiver is attached to interrupt 3, which is D3 on an Spark
const int INTERRUPT_433 = 4;   // the 433 MHz receiver is attached to interrupt 4, which is D4 on an Spark
const int WINDOW = 200;    	// timing window (+/- microseconds) within which to accept a bit as a valid code
const int TOLERANCE = 60;  	// % timing tolerance on HIGH or LOW values for codes

volatile boolean codeAvailable = false;  // set to true when a valid code is received and confirmed
volatile unsigned long receivedSensorCode; // decoded 24 bit value for a received and confirmed code from a wireless sensor
const int MAX_CODE_TIMES = 52;
volatile unsigned int codeTimes315[MAX_CODE_TIMES];  // array to hold times (microseconds) between interrupts from transitions in the received data
                         	//  times[0] holds the value of the sync interval (LOW).  HIGH and LOW times for codes are
                         	//  stored in times[1] - times[49] (HIGH and LOW pairs for 24 bits).  A few extra elements
                         	//  are provided for overflow.
volatile unsigned int codeTimes433[MAX_CODE_TIMES];  // array to hold times (microseconds) between interrupts from transitions in the received data
                         	//  times[0] holds the value of the sync interval (LOW).  HIGH and LOW times for codes are
                         	//  stored in times[1] - times[49] (HIGH and LOW pairs for 24 bits).  A few extra elements
                         	//  are provided for overflow.
volatile unsigned int *codeTimes;  // pointer to 315 MHz or 433 MHz codeTimes array based upon the interrupt.

/*********************** Declare aheads ******************************/
// ISR for the two recievers
void isr315();
void isr433();
// routines called by the ISRs
void process315();
void process433();
// routine called by the processXYZ routine when it has enough data to try a decode
void decode(unsigned int changeCount);
/*********************** end of Declare aheads ******************************/



/************************************** initISR() ***********************************************/
// Called once to initialize ISR routines
void initISR() {

    pinMode(INTERRUPT_315, INPUT);
    pinMode(INTERRUPT_433, INPUT);

    attachInterrupt(INTERRUPT_315, isr315, CHANGE);   // 315 MHz receiver on interrupt 3 => that is pin #D3
    attachInterrupt(INTERRUPT_433, isr433, CHANGE);   // 433 MHz receiver on interrupt 4 => that is pin #D4

}

// Call to get a new code if one is available. Once called the next call will
// return 0 unless another code has been received.
unsigned long getNewSensorCode() {

    if (codeAvailable) {

        codeAvailable = !codeAvailable;

        return receivedSensorCode;

    } else {

        return 0;
    }

}


/************************************** isr315() ***********************************************/
//This is the interrupt service routine for interrupt 3 (315 MHz receiver)
void isr315()
{
  codeTimes = codeTimes315;	// set pointer to 315 MHz array
  process315();
  return;
}

/***********************************end of isr315() ********************************************/

/************************************** isr433() ***********************************************/
//This is the interrupt service routine for interrupt 4 (433 MHz receiver)
void isr433()
{
  codeTimes = codeTimes433;	// set pointer to 433 MHz array
  process433();
  return;
}

/***********************************end of isr433() ********************************************/

/************************************* process315() ***********************************************/
//This is the code to process and store timing data for interrupt 3 (315 MHz)
//This is identical to the process433 routine

void process315()
{
    //this is right out of RC-SWITCH
    static unsigned int duration;
    static unsigned int changeCount;
    static unsigned long lastTime = 0L;
    static unsigned int repeatCount = 0;

    long time = micros();
    duration = time - lastTime;

    if (duration > 5000
        && duration > codeTimes[0] - 200
        && duration < codeTimes[0] + 200)
    {
        // we found a second sync
        repeatCount++;
        changeCount--;

	    if (repeatCount == 2)  // two successive code words found
	    {
            decode(changeCount); // decode the protocol from the codeTimes array
            repeatCount = 0;
        }
        changeCount = 0; // reset so we're ready to start a new sequence
    }
    else if (duration > 5000)
    {
        // If the duration is this long, then it could be a sync
        changeCount = 0;
    }

    if (changeCount >= MAX_CODE_TIMES) // too many bits before sync
    {
        // reset, we just had a blast of noise
        changeCount = 0;
        repeatCount = 0;
    }

    codeTimes[changeCount++] = duration;
    lastTime = time;

    return;
}
/***********************************end of process315() ********************************************/

/************************************** process433() ***********************************************/
//This is the code to process and store timing data for interrupt 4 (433 MHz)

void process433()
{
    //this is right out of RC-SWITCH
    static unsigned int duration;
    static unsigned int changeCount;
    static unsigned long lastTime = 0L;
    static unsigned int repeatCount = 0;

    long time = micros();
    duration = time - lastTime;

/*
    A pulse for a bit is between 300 and 500 microseconds. A bit always contains either
    three high or three low pulse intervals in a row. So if we have a duration that is
    longer than 1500, it is not part of the bit stream.
    When we are just processing noise, then codeTimes[0] could be anything.
    If this interrupt is:
        - the first rise of the pulse that starts the very first sync of
          the first code transmission, then duration
          could be anything. It will be stored in codeTimes at the current
          index position since we don't know it is special.
        - the fall of the first pulse that starts a sync, then duration
          will be one pulse. It will be stored in codeTimes at the current
          index position.
        - the rise at the end of the sync, then duration will be over 5000 microseconds
          and changeCount will be set to 0. The duration of this sync low time
          will be stored in changeCount[0]. The duration is 31 pulse times long.
        - the fall of the first part of a bit sequence then duration will be either
          one pulse if a 0 or 3 pulses which is the start of a 1.
        - if this is a valid code sequence, then as soon as this code transmission
          is over, a new sync and sequence will start. The new sequence will start
          with a pulse high and then 31 pulses low. This will trigger us that the
          previous sequence of interrupts was a valid code sequence. To trigger
          us this new sync low time must be longer than 5000 (of course) and be within
          +/- 200 microseconds of the sync low that we saw previously. If these
          conditions are met, then we bump repeatCount because we have now seen
          two valid sync low times within 52 changes.
        - when this all happens successfully a second time (we get a third sync
          low that is +/- 200 microseconds of the first one we saw) then we
          call the decoder for it to decide if the sequence of interrupt times
          is a valid code sequence.

    Q: Why don't we calculate codeTimes[0]/31 and check that a new duration is
       at that value +/- some tolerance? That would allow us to ignore noise.
 */
    if (duration > 5000
        && duration > codeTimes[0] - 200
        && duration < codeTimes[0] + 200)
    {
        // we found a second sync
        repeatCount++;
        changeCount--;

	    if (repeatCount == 2)  // two successive code words found
	    {
            decode(changeCount); // decode the protocol from the codeTimes array
            repeatCount = 0;
        }
        changeCount = 0; // reset so we're ready to start a new sequence
    }
    else if (duration > 5000)
    {
        // If the duration is this long, then it could be a sync
        changeCount = 0;
    }

    if (changeCount >= MAX_CODE_TIMES) // too many bits before sync
    {
        // reset, we just had a blast of noise
        changeCount = 0;
        repeatCount = 0;
    }

    codeTimes[changeCount++] = duration;
    lastTime = time;

    return;
}
/***********************************end of isr433() ********************************************/

/*************************************** decode() ************************************************/
// decode():  Function to decode data in the appropriate codeTimes array for the data that was
//  just processed.  This function supports PT2262 and EV1527 codes -- 24 bits of data where
//  0 = 3 low and 1 high, 1 = 3 high and 1 low.
// Arguments:
//  changeCount: the number of timings recorded in the codeTimes[] buffer.

void decode(unsigned int changeCount)
{

    unsigned long code = 0L;
    unsigned long pulseTime;
    float pulseTimeThree;
    unsigned long pulseTolerance;

    pulseTime = codeTimes[0] / 31L;
    pulseTimeThree = pulseTime * 3;

    pulseTolerance = pulseTime * TOLERANCE * 0.01;

    for (int i = 1; i < changeCount ; i=i+2)
    {

	    if (codeTimes[i] > pulseTime - pulseTolerance
            && codeTimes[i] < pulseTime + pulseTolerance
            && codeTimes[i+1] > pulseTimeThree - pulseTolerance
            && codeTimes[i+1] < pulseTimeThree + pulseTolerance)
	    {
            // we have a 0 shift left one
            code = code << 1;

	    }
        else if (codeTimes[i] > pulseTimeThree - pulseTolerance
                && codeTimes[i] < pulseTimeThree + pulseTolerance
                && codeTimes[i+1] > pulseTime - pulseTolerance
                && codeTimes[i+1] < pulseTime + pulseTolerance)
  	          {
                  // we have a 1, add one to code
                  code = code + 1;
                  // shift left one
                  code = code << 1;
  	           }
               else
  	            {
                    // Failed, this sequence of interrupts did not indicate a 1 or 0
                    // so abort the decoding process.
                    i = changeCount;
                    code = 0;
                }
    }
    // in decoding we shift one too many, so shift right one
    code = code >> 1;

    if (changeCount > 6) // ignore < 4bit values as there are no devices sending 4bit values => noise
    {
        receivedSensorCode = code;
        if (code == 0)
        {
            codeAvailable = false;
        }
        else
        {
            codeAvailable = true;
        }

    }
    else	// too short -- noise
    {
        codeAvailable = false;
        receivedSensorCode = 0L;
    }

    return;
}

/************************************ end of decode() ********************************************/



/****************************************** simulateSensor() ***************************************/
// simulateSensor(): this funtion is used to simulate sensors - for testing only.
#ifdef TESTRUN

//This routine is called every iteration of the main loop. Based on the current
//time it decides if a sensor trip should be made.
void simulateSensor() {
  struct simulationEvent {
	unsigned long simTime; 	// time of a fake trip in msec
	int simPosition;       	// the sensor config position to trip
  } ;
  const int SIMULATE_EVENTS_MAX = 5;
  const simulationEvent simEvents[SIMULATE_EVENTS_MAX] = {
	{5000, 1},
	{6000, 2},
	{20000, 3},
	{21000, 2},
	{50000, 2}
  };
  static unsigned long simStartTime = 0;
  static int simLastEventFired = -1;

  if (simStartTime == 0) {
	simStartTime = millis();  // first time through we get the time of the start of the simulation
  }

  unsigned long simCurrentTime = millis() - simStartTime; // what is the current simulation time?

  int i = simLastEventFired + 1;
  while (i < SIMULATE_EVENTS_MAX)
  {   // check each simulation event after the last one we tripped

	if (simCurrentTime > simEvents[i].simTime){  // if we are later in simulation than time of an event, trip it

  	receivedSensorCode = sensor_info[simEvents[i].simPosition].activateCode;  // setting global
  	codeAvailable = true;   // setting global
  	simLastEventFired = i;  // next time we will check only after this event
  	break;              	// only trip the next event

	}
	i++;
  }
}

#endif  /* TESTRUN */

/********************************** end of simulateSensor() ***********************************/
