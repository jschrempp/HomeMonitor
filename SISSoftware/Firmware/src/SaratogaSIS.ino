//#define SEND_EVENTS_TO_WEBPAGE // if on will send the events needed for the
                                 // config web page to display live events
//#define TESTRUN
//#define DEBUG                   // turns on Serial port messages
#define DEBUG_LED                 // enables LED toggling in toggleD7LED()
#define D7LED_DELAY 200           // how long to wait when toggling LED for debugging
//#define DEBUG_TRIP
//#define DEBUG_EVENT
//#define DEBUG_ADVISORY
//#define DEBUG_COMMANDS
#define photon044                 // when present, enables functions that only work with 0.4.4 or higher
#define CLOUD_LOG
// ZZZ blynk
#include "blynk.h"
#include <SISGlobals.h>
#include <SISConfigStore.h>
#include <TPPCircularBuff.h>
#include <TPPInterruptService.h>
#include <TPPUtils.h>

/***************************************************************************************************/
// saratogaSIS: SIS firmware - this is the software that is upoaded to the SIS Hub and performs
// sensor protocol decoding, sensor trip code processing, logging and cloud communication.  Compile
// this code and upload (flash) it to your Photon or other particle.io device.
//
//  This software has been extensively tested with the particle.io Photon.  It will run on the
// particle.io Core; however, the Core may run out of RAM unless the circular buffer size is
// reduced from 100 to 25.  This software is also intended ro run on the partcile.io Electron;
// however, the Electron is not available for testing as of this release.
//
//  Use of this software is subject to the Terms of Use, which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  This software uses code extracted from the Arduino RC-Switch library:
//    https://github.com/sui77/rc-switch
// Portions of this software that have been extracted from RC-Switch are subject to the RC-Switch
// license, as well as the the SIS Terms of Use.
//
//  Version 1.01.  12/30/15.
const String VERSION = "2.00";   	// current firmware version
//  v 2.0 removed global arrays and their associated constants. Added structures to handle the
//      SIS sensor config. Changes read/write config to handle new structures and to be backwards
//      compatible.
//
//  (c) 2015 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/



/************************************* Global Constants ****************************************************/

const unsigned long FILTER_TIME = 5000L;  // Once a sensor trip has been detected, it requires 5 seconds before a new detection is valid

const byte NUM_BLINKS = 2;  	// number of times to blink the D7 LED when a sensor trip is received
const unsigned long BLINK_TIME = 300L; // number of milliseconds to turn D7 LED on and off for a blink

// additional global constants for Saratoga SIS Test
const unsigned long MULTI_TIME = 2ul;   	// 2 second interval to detect multiple persons
const unsigned long AWAY = 600ul;       	// 10 * 60 = 600; ten minutes for declaring away
const unsigned long COMATOSE = 3600ul;  	// 60*60 = 3600; 1 hour for for declaring no movement
//XXX new
typedef enum  enum_messageIndex_type  {                    // used to select the correct message from the string array messages[]
    emsgNoOneIsHome = 0,
    emsgPersonIsHome,
    emsgNoMovement,
    emsgMultiplePersons
}  ;
//XXX end new
const String messages[] = {             	// additional log messages for this application. Use enum_messageIndex to access these.
                        	"No one is home",
                        	"Person is home",
                        	"No movement",
                        	"Multiple persons"
                      	};
const byte UKN = 0;     	// unknown
const byte HOME = 1;    	// person is home
const byte NOT_HOME = 2;	// person is not home

/************************************* Global Variables ****************************************************/
// ZZZ blynk
char auth[] = "0ac2d3236cc249b282e9d99df086c958";
#define BLYNK_PIN_CIRBUFLEN V5


// Strings to publish data to the cloud
String sensorCode = String("");


char cloudMsg[80];  	// buffer to hold last sensor tripped message
char cloudBuf[90];  	// buffer to hold message read out from circular buffer
char registrationInfo[80]; // buffer to hold information about registered sensors



char config[120];    	// buffer to hold local configuration information
long eventNumber = 0;   // grows monotonically to make each event unique


#if defined(DEBUG_EVENT) || defined(DEBUG_ADVISORY) || defined(DEBUG_COMMANDS)
	const unsigned long FILTER_TIME_UNREGISTERED = 5000L; // same as above, but for unregistered sensors
	unsigned long lastUnregisteredTripTime = 0;
	String debugLogMessage = String("junk"); // used by define DEBUG sections
#endif

// Additional globals for Saratoga SIS Test
int lastPIR = -1;   	// last PIR to trip - initialize to invalid value
time_t lastPIRTime = 0; 	// trip time of the last PIR to trip
time_t lastDoorTime = 0;	// trip time of the last Door sensor to trip
boolean lastSensorIsDoor = false;   // for door then PIR detection
boolean supress = false;	// multiple person suppression flag
byte personHome = UKN;  // initialize to unknown status
boolean comatose = false;   // patient is not moving


const unsigned long ONE_DAY_IN_MILLIS = 86400000L;	// 24*60*60*1000
time_t resetTime;       	// variable to hold the time of last reset

unsigned long upcount = 0L; // sequence number added to the circular buffer log entries


// ZZZ
/**************** BLYNK *****************/
// This function tells Arduino what to do if there is a Widget
// which is requesting data for Virtual Pin (5)
BLYNK_READ(BLYNK_PIN_CIRBUFLEN)
{
    int numberToReport = upcount;
    Blynk.virtualWrite(BLYNK_PIN_CIRBUFLEN, numberToReport); // getCircularBufLen());
}

bool g_blynkNotifyNow = false;
int g_blynkNotifyCount = 0;
// call this within main loop
void Blynk_Notify_EverySoOften() {

    static time_t lastNotify = 0;
    time_t now = millis();

//    if (now - lastNotify > 120000)
    if (g_blynkNotifyNow) {
        g_blynkNotifyNow = false;
        Blynk.notify("Notify from photon");
        g_blynkNotifyCount++;
        lastNotify = now;
    }

}

int blynkNow(String param) {
    g_blynkNotifyNow = true;
    return g_blynkNotifyCount;
}

/************* BLYNK *****************/


/**************************************** setup() ***********************************************/
void setup()
{

// ZZZ blynk
  Blynk.begin(auth);

  // Use D7 LED as a test indicator.  Light it for the time spent in setup()
  pinMode(D7, OUTPUT);
  digitalWrite(D7, HIGH);

  configStoreInit();

  toggleD7LED();

	// initialize the I2C comunication
  Wire.setSpeed(CLOCK_SPEED_100KHZ);
  Wire.stretchClock(false);
  Wire.begin();

  toggleD7LED();

  #ifdef DEBUG
	 Serial.begin(9600);
  #endif

  initISR();

  toggleD7LED();

  // restore the saved configuration from non-volatile memory
  restoreConfig();

  toggleD7LED();

  // wait for the Core to synchronise time with the Internet
  while(Time.year() <= 1970 && millis() < 30000)
  {
  	delay(100);
    Spark.process();
  }

  if (Time.year() <= 1970)
  {
    reportFatalError(3);
    //never returns from here
  }

  toggleD7LED();

  // Publish local configuration information in config[]
  resetTime = Time.now();    	// the current time = time of last reset
  publishConfig();

  // Make sensorCode and cBufreadout strings available to the cloud
  Spark.variable("Config", config, STRING);
  Spark.variable("sensorTrip", cloudMsg, STRING);
  Spark.function("ReadBuffer", readBuffer);
  Spark.variable("circularBuff", cloudBuf, STRING);
  Spark.variable("registration", registrationInfo, STRING);
  Spark.function("Register", registrar);
  Spark.variable("cloudDebug", cloudDebug, STRING);
  Spark.function("blynknow",blynkNow);

  // Publish a start up event notification
  Spark.function("publistTestE", publishTestE); // for testing events

  toggleD7LED();

#ifdef DEBUG
  Serial.println("End of setup()");
#endif

// turn off the D7 LED at the end of setup()
digitalWrite(D7, LOW);

}

/************************************ end of setup() *******************************************/

/**************************************** loop() ***********************************************/
void loop()
{

// ZZZ blynk
 Blynk.run();
 Blynk_Notify_EverySoOften();

  boolean knownCode = false;
  static unsigned long lastTimeSync = millis();  // to resync time with the cloud daily
  static boolean blinkReady = true;  // to know when non-blocking blink is ready to be triggered

  // Test for received code from a wireless sensor
  unsigned long newSensorCode = getNewSensorCode();

  if (newSensorCode != 0) // new wireless sensor code received
  {
	int i; 	// index into known sensor arrays

	// Test to see if the code is for a known sensor
	knownCode = false;
	for (i = 0; i < MAX_WIRELESS_SENSORS; i++)
	{
    	if ( newSensorCode == sensor_info[i].activateCode )
    	{
        	knownCode = true;
        	break;
    	}
	}

	// If code is from a known sensor, filter it
	if (knownCode == true)	// registered sensor was tripped
	{
    	unsigned long now;
    	now = millis();
    	if( (now - sensor_info[i].lastTripTime) > FILTER_TIME ) // filter out multiple codes
    	{
        	// Code for the sensor trip message
        	sensorCode = "Received sensor code: ";
        	sensorCode += newSensorCode;
        	sensorCode += " for ";
        	sensorCode += sensor_info[i].sensorName;

        	#ifdef DEBUG
            	Serial.println(sensorCode);  // USB print for debugging
        	#endif

        	#ifdef DEBUG_EVENT
            	debug = "Event: ";
            	debug += sensor_info[i].sensorName;
            	publishDebugRecord(debug);
        	#endif

        	sensorCode.toCharArray(cloudMsg, sensorCode.length() + 1 );  // publish to cloud
        	cloudMsg[sensorCode.length() + 2] = '\0';  // add in the string null terinator


#if SEND_EVENTS_TO_WEBPAGE
          // send notification of new sensor trip for web page
          // This can send events too fast, so it is ifdef'd until
          // we get a send queue for publishing
        	publishEvent(String(i));
#endif
        	// determine type of sensor and process accordingly
        	if (sensor_info[i].sensorType == ePIR)
        	{
            	processPIRSensor(i);
        	}
        	else                	// not a PIR, then a door sensor
        	{
            if (sensor_info[i].sensorType == eExitDoor)
            {
            	processDoorSensor(i);
        	  }
            else
            {
              processSensor(i);
            }
          }

          if (sensor_info[i].alarmOnTrip) {

            sparkPublish("SISAlarm", "Alarm sensor trip", 60 );

          }

           	// code to blink the D7 LED when a sensor trip is detected
        	if(blinkReady)
        	{
            	blinkReady = nbBlink(NUM_BLINKS, BLINK_TIME);
        	}

        	// update the trip time to filter for next trip
            sensor_info[i].lastTripTime = now;
      	}
	}
	else // not a code from a known sensor -- report without filtering; no entry in circular buffer
	{
    	sensorCode = "Received sensor code: ";
    	sensorCode += newSensorCode;
    	sensorCode += " for unknown sensor";
    	Serial.println(sensorCode);  // USB print for debugging
    	sensorCode.toCharArray(cloudMsg, sensorCode.length() );  // publish to cloud

    	#ifdef DEBUG_EVENT
        	unsigned long now;
        	now = millis();
        	if((now - lastUnregisteredTripTime) > FILTER_TIME_UNREGISTERED) // filter out multiple codes
        	{
            	lastUnregisteredTripTime = now;
            	debug = "Event: Unknown code ";
            	debug += String(sensorCode);
            	publishDebugRecord(debug);
        	}
    	#endif
	}

};

  if(!blinkReady)  // keep the non blocking blink going
	{
    	blinkReady = nbBlink(NUM_BLINKS, BLINK_TIME);
	}

  #ifdef TESTRUN
  	simulateSensor();
  #endif

#ifdef photon044
  // resync Time to the cloud once per day
  if (millis() - lastTimeSync > ONE_DAY_IN_MILLIS)
  {
 	  Spark.syncTime();
	  lastTimeSync = millis();
  }
#endif

  // Testing for "person not home"
  if(lastSensorIsDoor && (personHome == HOME) && ((Time.now() - lastDoorTime) > AWAY))
  {
	personHome = NOT_HOME;

	// log "person not home" message
	logMessage(emsgNoOneIsHome);

  }

  // Testing for "no movement"
  if(!lastSensorIsDoor && !comatose && (personHome == HOME) && ((Time.now() - lastPIRTime) > COMATOSE))
  {
  	logMessage(emsgNoMovement);
  	comatose = true;
  }

  #ifdef CLOUD_LOG
    publishCircularBuffer();  // pushes new events to the cloud, if needed
  #endif

}
/************************************ end of loop() ********************************************/

/************************************ toggleD7LED() ********************************************/
// toggleD7LED(): D7 LED is used as a test indicator. This function allows you to toggle it.
// Only actually toggle if DEBUG_LED is defined
//
void toggleD7LED(void)
{
  #ifdef DEBUG_LED
  digitalWrite(D7, LOW);
  delay(D7LED_DELAY);
  digitalWrite(D7, HIGH);
  delay(D7LED_DELAY);
  #endif
}

/************************************ end toggleD7LED loop() ***********************************/

/************************************ logMessage() *********************************************/
// logMessage(): function to create a log entry that is an advisory message.
//
// Arguments:
//  messageIndex:  index into the messages[] array of message strings
//
//XXX I hate to declare this an int when it is actually  enum_messageIndex_type
//    but I can't find a way to get that to compile.
void logMessage(int messageIndex)
{
	// create timestamp substring
	String timeStamp = "";
	timeStamp += Time.now();

	// create sequence number substring
	String sequence = "";
	sequence += upcount;
	if (upcount < 9999)  // limit to 4 digits
	{
    	upcount ++;
	}
	else
	{
    	upcount = 0;
	}

	// create the log entry
	String logEntry = "A,";  // advisory type message
	logEntry += sequence;
	logEntry += ",";
	logEntry += messages[messageIndex];
	logEntry += ",";
	logEntry += timeStamp;
	logEntry += ",";

	// pad out to 20 characters
	while (logEntry.length() < 22)
	{
    	logEntry += "x";
	}

	cBufInsert("" + logEntry);

	#ifdef DEBUG_ADVISORY
    	debugLogMessage = "Advise: ";
    	debugLogMessage += String(messages[messageIndex]);
    	publishDebugRecord(debugLogMessage);
	#endif

	return;
}

/********************************** end of logMessage() ****************************************/

/************************************* logSensor() *********************************************/
// logSensor(): function to create a log entry for a sensor trip
//
// Arguments:
//  sensorIndex:  index into the sensorName[] and activeCode[] arrays of sensor data
//
void logSensor(int sensorIndex)
{
	// create timestamp substring
	String timeStamp = "";
	timeStamp += Time.now();

	// create sequence number substring
	String sequence = "";
	sequence += upcount;
	if (upcount < 9999)  // limit to 4 digits
	{
    	upcount ++;
	}
	else
	{
    	upcount = 0;
	}

	// create the sensor index substring
	String sensor = "";
	sensor += sensorIndex;

	// create the log entry
	String logEntry = "S,"; // Sensor trip log entry
	logEntry += sequence;
	logEntry += ",";
	logEntry += sensor;
	logEntry += ",";
	logEntry += timeStamp;
	logEntry += ",";

	// pad out to 20 characters
	while (logEntry.length() < 22)
	{
    	logEntry += "x";
	}

	cBufInsert("" + logEntry);

	return;
}

/*********************************** end of logSensor() ****************************************/

/********************************** processPIRSensor() *****************************************/
// processPIRSensor():  function to process a generic sensor trip.  This function creates a
//  log entry for each registered sensor trip and records the log entry in the circular buffer.
//
//  Arguments:
//  	sensorIndex: the index into the sensorName[] and activateCode[] arrays for the sensor
//       	to be logged.

void processPIRSensor(int sensorIndex)
{
	//Test if this is the last PIR tripped
	if( (sensorIndex != lastPIR) && (supress == false))  // then process the PIR as a new PIR trip
	{
    	// log the sensor trip
    	logSensor(sensorIndex);

    	//Test for multiple persons
    	unsigned long elapsedTime = Time.now() - lastPIRTime;
    	if(elapsedTime < MULTI_TIME) 	// two different PIR within multiple person time period
    	{
        	supress = true;    	// Set the suppress flag

        	// log the suppress message
        	logMessage(emsgMultiplePersons);
    	}

    	//Test for Person is home
    	elapsedTime = Time.now() - lastDoorTime;
    	if( (elapsedTime < AWAY) && personHome != HOME) 	// less than 10 minutes since door trip
    	{
        	personHome = HOME;    	// Set the personHome state

        	// log the person is home message
        	logMessage(emsgPersonIsHome);
    	}

    	lastPIR = sensorIndex;  // update what is the last PIR to trip
    	comatose = false;   // any PIR resets comatose flag
	}

	// log the PIR trip if the person was comatose (no movement)
	if(comatose && (sensorIndex == lastPIR)) // log the sensor trip if the person was comatose
	{
    	logSensor(sensorIndex);
    	comatose = false;   // any PIR resets comatose flag
	}

    // any PIR trip indicates that person is moving
    lastPIRTime = Time.now();

    // update globals for PIR trip detection
    lastSensorIsDoor = false;

	personHome = HOME;  // if a PIR trips, someone is home, regardless (but don't log a message)
	return;
}
/***********************************end of processPIRSensor() ****************************************/

/********************************** processDoorSensor() *****************************************/
// processDoorSensor():  function to process a generic door trip.  This function creates a
//  log entry for each registered sensor trip and records the log entry in the circular buffer.
//
//  Arguments:
//  	sensorIndex: the index into the sensorName[] and activateCode[] arrays for the sensor
//       	to be logged.

void processDoorSensor(int sensorIndex)
{

	logSensor(sensorIndex);

	//Update globals for a PIR trip detection
	lastDoorTime = Time.now();
	lastSensorIsDoor = true;
	supress = false;
	lastPIR = -1;   // clear out the last PIR when a door is opened.

	return;

}
/***********************************end of processDoorSensor() ****************************************/

/********************************** processSensor() *****************************************/
// processSensor():  function to process a generic sensor trip.  This function creates a
//  log entry for each registered sensor trip and records the log entry in the circular buffer.
//
//  Arguments:
//  	sensorIndex: the index into the sensorName[] and activateCode[] arrays for the sensor
//       	to be logged.

void processSensor(int sensorIndex)
{

    logSensor(sensorIndex);

	return;

}
/***********************************end of processSensor() ****************************************/


/************************************ publishConfig() ********************************************/
// publishConfig():  function to make the local configuration available to the cloud
//

void publishConfig()
{
  String localConfig = "cBufLen: ";
  localConfig += String(BUF_LEN);
  localConfig += ", MaxSensors:";
  localConfig += String(MAX_WIRELESS_SENSORS);
  localConfig += ", version: ";
  localConfig += VERSION;
  localConfig += ", utcOffset: ";
  localConfig += utcOffset;
  localConfig += ", DSTyn: ";
  localConfig += observeDST;
  localConfig += ", resetAt: ";
  localConfig += String(resetTime);
  localConfig += "Z ";

  localConfig.toCharArray(config, localConfig.length() );

  return;
}
/******************************* end of publishConfig() ****************************************/

/************************************* registrar() ********************************************/
// registrar():  manage wireless sensor registration on the Spark Core.
//  Arguments:
//  	String action: a string representation of the registration action.  Options are:
//      	"read": read out the registration information about a sensor.  Format of "read" is
//          	"read,location", where location is a String representation of the location in
//          	the sensor registration arrays to read back.  The location is
//          	0 .. MAX_WIRELESS_SENSORS.
//      	"delete":  delete sensor registration info.  Format of "delete" is "delete,location",
//          	where location is a String representation of the location in the sensor registration
//           	arrays to read back.  The location is 0 .. MAX_WIRELESS_SENSORS.
//      	"register":  register a new sensor.  Format of "register" is
//          	"register,location,sensor_trip_code,description", where where location is a
//          	String representation of the location in the sensor registration arrays to read back.
//          	The location is 0 .. MAX_WIRELESS_SENSORS;  "sensor_trip_code" is a string (ASCII)
//          	representation of the code that the sensor sends when it is tripped; and "description"
//          	is a textual sensor trip message.
//      	"store": store the current configuration to non-volatile memory.  The format of "store" is
//          	just the single string "store" with no commas or other parameters.  "store" causes the
//          	following information to be stored into non-volatile memory:
//              	- an identifer:  "SIS-2015".  if the ID is incorrect, the config won't be restored
//                  	and defaults will be used instead.
//              	- utcOffset
//              	- DST
//              	- all sensor names
//              	- all sensor trip codes
//          	The config is restored automatically upon resetting the Core.
//

int registrar(String action)
{
	#ifdef DEBUG_COMMANDS
    	debugLogMessage = "Cmd: ";
    	debugLogMessage += String(action);
    	publishDebugRecord(debugLogMessage);
	#endif

   // requested actions
	const int READ = 0; 	// action is "read"
	const int DELETE = 1;   // action is "delete"
	const int REG = 2;  	// action is "register"
	const int OFFSET = 3;   // action is to set the local utc offset
	const int DST = 4;  	// action is to set the local observe DST (yes or no)
	const int STORE = 5;	// action is to store the sensor configuration to non-volatile memory
    const int LOAD = 6;     // action is to restore the config from non-volatile memory
	const int UKN = -1; 	// action is unknown

	int requestedAction;
	int numSubstrings;
	String registrationInformation; 	// String to hold the information about the sensor
//	unsigned long sensorCode;       	// numerical value of the sensor trip code
	int location;                   	// numerical value of ordinal number of the sensor info

	// parse the string argument into its substrings
    // parse results are in g_dest[]
	numSubstrings = parser(action);

	if(numSubstrings < 1) //  invalid command string
	{
    	return -1;
	}

	// determine the command in g_dest[0]
	if(g_dest[0] == "read")
	{
    	requestedAction = READ;
	}
	else
	{
    	if(g_dest[0] == "delete")
    	{
        	requestedAction = DELETE;
    	}
    	else
    	{
        	if(g_dest[0] == "register")
        	{
            	requestedAction = REG;
        	}
        	else
        	{
            	if(g_dest[0] == "offset")
            	{
                	requestedAction = OFFSET;
            	}
            	else
            	{
                	if(g_dest[0] == "DST")
                	{
                    	requestedAction = DST;
                	}
                	else
                	{
                    	if(g_dest[0] == "store")
                    	{
                        	requestedAction = STORE;
                    	}
                        else
                        {
                             if(g_dest[0] == "load")
                            {
                                requestedAction = LOAD;
                            }
                        	else
                        	{
                            	requestedAction = UKN;
                        	}
                        }
                	}

            	}
        	}
    	}
	}

	// obtain the location from g_dest[1]
	location = g_dest[1].toInt();

	// perform the requested action


	switch(requestedAction)
	{
    	case READ:
        	if(location >= MAX_WIRELESS_SENSORS)
        	{
            	location = MAX_WIRELESS_SENSORS - 1; // clamp the location within array bounds
        	}

        	// read action
        	registrationInformation = "loc: ";
        	registrationInformation += String(location);
        	registrationInformation += ", sensor code: ";
        	registrationInformation += sensor_info[location].activateCode;
        	registrationInformation += " is for ";
        	registrationInformation += sensor_info[location].sensorName;
        	registrationInformation += ", of type ";
        	registrationInformation += sensorType_strings[sensor_info[location].sensorType];
        	registrationInformation += ". Alarm: ";
        	registrationInformation += sensor_info[location].alarmOnTrip;
        	Serial.println(registrationInformation);

        	// write to the cloud
        	registrationInformation.toCharArray(registrationInfo, registrationInformation.length() + 1);
        	registrationInfo[registrationInformation.length() + 2] = '\0';

        	break;

    	case DELETE:
        	if(location >= MAX_WIRELESS_SENSORS)
        	{
            	break; // don't delete an invalid location
        	}

        	// delete action
            sensor_info[location].sensorName = "UNREGISTERED SENSOR";
            sensor_info[location].activateCode = 0L;
            sensor_info[location].sensorType = esensorTypeUnknown;
            sensor_info[location].alarmOnTrip = false;
        	break;

    	case REG:
        	if(location >= MAX_WIRELESS_SENSORS)
        	{
            	numSubstrings = -1; // return an error code
            	break; // don't register to an invalid location
        	}

        	// ensure that at least 4 substrings were received
        	if(numSubstrings < 4)
        	{
            	numSubstrings = -1; // return an error code
            	break;
        	}

        	// perform the new sensor registration function
            sensor_info[location].sensorName = g_dest[3];
            sensor_info[location].activateCode = g_dest[2].toInt();

            /* XXX from old code
            const int MAX_PIR = 11;      	// PIR sensors are registered in loc 0 through MAX_PIR.  Locations MAX_PIR + 1 to
                                        	//  MAX_WIRELESS_SENSORS are non-PIR sensors
            const int MAX_DOOR = 15;       // Sensors > MAX_PIR and <= MAX_DOOR are assumed to be exit doors.
            */
            if (location <= 11) {
                sensor_info[location].sensorType = ePIR;
            } else if (location <= 19) {
                sensor_info[location].sensorType = eExitDoor;
            } else {
                sensor_info[location].sensorType = eSeparation;
            }

            /* XXX from old code
            const int ALARM_SENSOR = 19;  // When this sensor is tripped, publish an SISAlarm
            */
            if (location == 19) {
                sensor_info[location].alarmOnTrip = true;
            }

            // XXX sensor_info[location].sensorType = ;  NEED TO ADD PARAMETER FOR THIS
            // XXX sensor_info[location].alarmOnTrip = ;    NEED TO ADD PARAMETER FOR THIS
            break;

    	case OFFSET:
        	utcOffset = "" + g_dest[1];
        	publishConfig();
        	break;

    	case DST:
        	observeDST = "" + g_dest[1];
        	publishConfig();
        	break;

    	case STORE:
        	writeConfig();
        	break;

        case LOAD:
            restoreConfig();
            break;

    	default:
            numSubstrings = -1; // return an error code for unknown command
        	break;
	}

	return numSubstrings;
}


/********************************** end of registrar() ****************************************/

/************************************* readBuffer() ********************************************/
// EXPOSED TO THE PARTICLE CLOUD
// SIS CLIENT CALLS THIS ROUTINE
// readBuffer(): read the contents of the circular buffer into the global variable "cloudBuf"
//  Arguments:
//  	String location:  numerical location of the buffer data to read.  The location is relative
//      	to the latest entry, which is "0".  The next to latest is "1", etc. back to BUF_LEN -1.
//       	BUF_LEN can be determined from the cloud variable "bufferSize".  If location exceeds
//       	BUF_LEN - 1, the value that is read out is the oldest value in the buffer, and
//       	-1 is returned by the function.  Otherwise, the value is determined by location
//       	and 0 is returned by this function.
//  Return:  0 if a valid location was specified, otherwise, -1.

int readBuffer(String location)
{
    int offset;
    int result;

    offset = location.toInt();
    result = readSISFromBuffer(offset, cloudBuf);

    return result;
}

/*********************************end of readBuffer() *****************************************/

/****************************************** publishCircularBuffer () ************************/
// publishCircularBuffer()
//
// This routine publishes recent events in the circular buffer cBuf[] to the Spark Cloud.
// It uses a local static variable to be sure that events are not published more frequently than once
// every 2 seconds.
// This routine is called once each time through the main loop. It could be called anytime.
//
void publishCircularBuffer() {

    static unsigned long lastPublishTime = 0;

    if (getNumToPublish() >= 0 ) {

        unsigned long currentTime = millis();
        if (currentTime - lastPublishTime > 4000)
        {

            char localBuf[90];

            readSISFromBuffer(getNumToPublish(), localBuf);      // read out the latest logged entry into localBuf

            if(sparkPublish("LogEntry", localBuf, 60))     // ... and publish it to the cloud for xteranl logging
            {
                decrementNumToPublish();

            };
            lastPublishTime = currentTime;

        }

    }

}

/****************************************** End of publishCircularBuffer () ************************/

/************************************ publishEvent() *********************************************/
// publishEvent():  Function to pubish a Core event to the cloud in JSON format
//  Arguments:
//  	data: the message to publish to the cloud
//

int publishTestE(String data)
{
  eventNumber++;

  // Make it JSON ex: {"eventNum":"1","eventData":"data"}
  String msg = "{";
  msg += makeNameValuePair("eventNum",String(eventNumber));
  msg += ",";
  msg += makeNameValuePair("eventData", data);
  msg += "}";
  sparkPublish("SISEvent", msg , 60);
  return 0;

}


#if defined(DEBUG_EVENT) || defined(DEBUG_ADVISORY) || defined(DEBUG_COMMANDS)
int publishDebugRecord(String logData)
{
	eventNumber++;

	// Make it JSON ex: {"eventNum":"1","eventData":"data"}
  	String msg = "{";
  	msg += makeNameValuePair("num",String(eventNumber));
  	msg += ",";
  	msg += makeNameValuePair("info",logData);
  	msg += "}";

  	sparkPublish("SISLogData", msg, 60);
}
#endif /* publishDebugRecord */

// Keeping a separate publishEvent because we might want to send more than one
// data field.
int publishEvent(String sensorIndex)
{
  eventNumber++;

  // Make it JSON ex: {"eventNum":"1","eventData":"data"}
  String msg = "{";
  msg += makeNameValuePair("eventNum",String(eventNumber));
  msg += ",";
  msg += makeNameValuePair("sensorLocation",sensorIndex);
  msg += "}";

  sparkPublish("SISEvent", msg, 60);

}

int sparkPublish (String eventName, String msg, int ttl)
{
  bool success = true;

	if (millis() > 5000 )  // don't publish until spark has a chance to settle down
	{
    #ifdef photon044
        success = Spark.publish(eventName, msg, ttl, PRIVATE);
    #endif

    #ifndef photon044
      //  A return code from spark.publish is only supported on photo 0.4.4 and later
      Spark.publish(eventName, msg, ttl, PRIVATE);
    #endif
	}


#ifdef DEBUG
    Serial.println("sparkPublish called");

    if (success == false)
    {
      String message = "Spark.publish failed";
      Serial.print(message);

      message = " trying to publish " + eventName + ": " + msg;

      Serial.println(message);
      Spark.process();
    }

#endif

  return success;


}


/********************************* end of publishEvent() *******************************/

/****************************** fatal error code reporting *****************************/
// Call this to flash D7 continuously. This routine never exits.
//
// Error codes
//    3 flashes:  Failed to sync time to internet. Action: power cycle
//
void reportFatalError(int errorNum)
{

#ifdef DEBUG
    String message = "unknown error code";
    Serial.print("Fatal Error: ");
    switch(errorNum)
    {
    case 3:
      message = " could not sync time to internet";
      break;
    default:
      break;
    }
    Serial.println(message);
    Spark.process();
#endif

  while(true)  // never ending loop
  {
    for (int i=0; i < errorNum; i++)
    {
      digitalWrite(D7, HIGH);
      delay(100);
      Spark.process();
      digitalWrite(D7, LOW);
      delay(100);
      Spark.process();
    }
    digitalWrite(D7, LOW);

    // Now LED off for 1500 milliseconds
    for (int i=0; i < 3; i++)
    {
      delay(500);
      Spark.process();
    }
  }

  // we will never get here.
  return;

}

/***************************** end of fatal error code reporting ****************************/
