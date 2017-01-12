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
//
// Temporary fix for I2C library issue with Photon
#ifdef PLATFORM_ID
    #include "i2c_hal.h"
#endif

#include <testinclude.ino>

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

const int INTERRUPT_315 = 3;   // the 315 MHz receiver is attached to interrupt 3, which is D3 on an Spark
const int INTERRUPT_433 = 4;   // the 433 MHz receiver is attached to interrupt 4, which is D4 on an Spark
const int WINDOW = 200;    	// timing window (+/- microseconds) within which to accept a bit as a valid code
const int TOLERANCE = 60;  	// % timing tolerance on HIGH or LOW values for codes
const unsigned long ONE_DAY_IN_MILLIS = 86400000L;	// 24*60*60*1000
const int MAX_WIRELESS_SENSORS = 20;
const unsigned long FILTER_TIME = 5000L;  // Once a sensor trip has been detected, it requires 5 seconds before a new detection is valid
const int BUF_LEN = 100;     	// circular buffer size.
const int MAX_SUBSTRINGS = 6;   // the largest number of comma delimited substrings in a command string
const byte NUM_BLINKS = 2;  	// number of times to blink the D7 LED when a sensor trip is received
const unsigned long BLINK_TIME = 300L; // number of milliseconds to turn D7 LED on and off for a blink

// XXX should this be 30?
const int CONFIG_BUFR = 32; 	// buffer to use to store and retrieve data from non-volatile memory
const String id = "SIS-2015";   // the ID value for a valid config in non volatile memory
const String DOUBLEQ = "\"";

//  For I2C eeprom configuration
const int VIRTUAL_DEVICE_SIZE = 4096;   // divide device up into virtual eeproms 4K bytes each
const int MAX_VIRTUAL_DEVICES = 8;  	// 32 K bytes device = 8 pages of 4K each
const int VIRTUAL_DEVICE_NUM = 0;   	// use the first virtual device - change if it wears out

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
time_t resetTime;       	// variable to hold the time of last reset

// NEW These structures are meant to compartmentalize the existing code. When tested, remove this comment.
// Holds information about each sensor configured in the system
enum enum_sensorType {
    esensorTypeUnknown,
    ePIR,
    eSeparation,
    eExitDoor
};
String sensorType_strings[] {
    "Unknown",
    "PIR",
    "Separation",
    "Exit Door"
};
struct type_sensor {
    String sensorName = "No Name Assigned";
    unsigned long activateCode = 0;
    unsigned long lastTripTime = 0;
    enum_sensorType sensorType = esensorTypeUnknown;
    bool alarmOnTrip = 0;

};
type_sensor sensor_info[MAX_WIRELESS_SENSORS];
// END of NEW



unsigned long upcount = 0L; // sequence number added to the circular buffer log entries

	// other data that needs to be stored in non-volatile memory
String utcOffset = "-8.0";	// Pacific Standard Time
String observeDST = "yes";	// no" if locale does not observe DST

	// variable to hold the virtual eeprom device used to store the config
int eepromOffset;

	// array to hold parsed substrings from a command string
String g_dest[MAX_SUBSTRINGS];

	// Strings to publish data to the cloud
String sensorCode = String("");
String g_bufferReadout = String("");
char cloudDebug[80];    // used when debugging to give the debug client a message
char cloudMsg[80];  	// buffer to hold last sensor tripped message
char cloudBuf[90];  	// buffer to hold message read out from circular buffer
char registrationInfo[80]; // buffer to hold information about registered sensors

String cBuf[BUF_LEN];   // circular buffer to store events and messages as they happen
                        // Expected format of the string stored in cBuf is:
                        // TYPE,SEQUENCENUMBER,INDEX,EPOCTIME
                        // where
                        //    TYPE is A (advisory) or S (sensor)
                        //    SEQUENCENUMBER is a monotonically increasing global (eventNumber)
                        //    INDEX is into sensorName[] for type sensor
                        //          or enum_messageIndex for type advisory
                        //    EPOCTIME is when the entry happened
                        // see cBufInsert(), cBufRead(), readFromBuffer(), logSensor(), logMessage()

int head = 0;       	// index of the head of the circular buffer
int tail = 0;       	// index of the tail of the buffer
int g_numToPublish = -1; // Number of entries in cBuf[] that remain to be published to spark cloud.
                         // This is incremented when events are added to the cBuf[] and decremented
                         // when an entry is published to the spark cloud.

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

/**************************************** setup() ***********************************************/
void setup()
{
  // Use D7 LED as a test indicator.  Light it for the time spent in setup()
  pinMode(D7, OUTPUT);
  digitalWrite(D7, HIGH);

  // select virtual device on the eeprom
  if(VIRTUAL_DEVICE_NUM < MAX_VIRTUAL_DEVICES)
  {
    eepromOffset = VIRTUAL_DEVICE_NUM * VIRTUAL_DEVICE_SIZE;
  }
  else
  {
    eepromOffset = MAX_VIRTUAL_DEVICES - 1;
  }

  toggleD7LED();

	// initialize the I2C comunication
  Wire.setSpeed(CLOCK_SPEED_100KHZ);
  Wire.stretchClock(false);
  Wire.begin();

  toggleD7LED();

  #ifdef DEBUG
	 Serial.begin(9600);
  #endif

  pinMode(INTERRUPT_315, INPUT);
  pinMode(INTERRUPT_433, INPUT);

  attachInterrupt(INTERRUPT_315, isr315, CHANGE);   // 315 MHz receiver on interrupt 3 => that is pin #D3
  attachInterrupt(INTERRUPT_433, isr433, CHANGE);   // 433 MHz receiver on interrupt 4 => that is pin #D4

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

  // Publish a start up event notification
  Spark.function("publistTestE", publishTestE); // for testing events

  toggleD7LED();

  /*
  // Initialize the lastTripTime[] array
  //XXX won't need to do this.
  for (int i = 0; i < MAX_WIRELESS_SENSORS; i++)
  {
  	lastTripTime[i] = 0L;
  }
*/

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

  boolean knownCode = false;
  static unsigned long lastTimeSync = millis();  // to resync time with the cloud daily
  static boolean blinkReady = true;  // to know when non-blocking blink is ready to be triggered

  // Test for received code from a wireless sensor
  if (codeAvailable) // new wireless sensor code received
  {
	int i; 	// index into known sensor arrays

	// Test to see if the code is for a known sensor
	knownCode = false;
	for (i = 0; i < MAX_WIRELESS_SENSORS; i++)
	{
    	if ( receivedSensorCode == sensor_info[i].activateCode ) // XXX activateCode[i])  )
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
    	// XXX if((now - lastTripTime[i]) > FILTER_TIME) // filter out multiple codes
    	if( (now - sensor_info[i].lastTripTime) > FILTER_TIME ) // filter out multiple codes
    	{
        	// Code for the sensor trip message
        	sensorCode = "Received sensor code: ";
        	sensorCode += receivedSensorCode;
        	sensorCode += " for ";
        	sensorCode += sensor_info[i].sensorName; //XXX sensorName[i];

        	#ifdef DEBUG
            	Serial.println(sensorCode);  // USB print for debugging
        	#endif

        	#ifdef DEBUG_EVENT
            	debug = "Event: ";
            	debug += sensor_info[i].sensorName; //XXX  sensorName[i]);
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
        	// XXX if(i <= MAX_PIR)    	// then the sensor is a PIR
            if (sensor_info[i].sensorType == ePIR)
        	{
            	processPIRSensor(i);
        	}
        	else                	// not a PIR, then a door sensor
        	{
            // XXX if (i <= MAX_DOOR)
            if (sensor_info[i].sensorType == eExitDoor)
            {
            	processDoorSensor(i);
        	  }
            else
            {
              processSensor(i);
            }
          }

          //XXX if (i == ALARM_SENSOR) {
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
    	sensorCode += receivedSensorCode;
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
            	debug += String(receivedSensorCode);
            	publishDebugRecord(debug);
        	}
    	#endif
	}

	codeAvailable = false;  // reset the code available flag if it was set
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

/******************************************** writeConfig() ******************************************/
// writeConfig():  writes the sensor configuration out to non-volatile memory using the Wire library
//  and custom I2C eeprom code.  The sensor configuration consists of data buffers, each CONFIG_BUFR long.
//  The buffers are written to non-volatile memory in the following order:
//  	ID: a constant string identifier: "SIS-2015"
//  	TZ: the Core's local timezone offset, as a string, e.g. "-8.0"
//  	DST: whether the Core's locale observes daylight savings time, e.g. "yes" or "no"
//  	NAME: the name of each registered sensor is stored in its own buffer.  The number of names = MAX_WIRELESS_SENSORS
//  	CODE: the code that a tripped sensor sends.  Each sensor's code is stored in its own buffer.

void writeConfig()
{
	String id = "SIS-2015"; 	// the ID value
	char buf[CONFIG_BUFR];  	// temporary buffer to write to non-volatile memory
	String temp;            	// temporary string storage

    int addr;
	int addr_Version = eepromOffset;
    int addr_utcOffset = eepromOffset +            CONFIG_BUFR;
    int addr_dst = eepromOffset +             (2 * CONFIG_BUFR);
    int addrStart_sensorName = eepromOffset + (3 * CONFIG_BUFR);
    int addrStart_activateCode = eepromOffset + ((3 +      MAX_WIRELESS_SENSORS)  * CONFIG_BUFR);
    int addrConfigExtA = eepromOffset =         ((3 + (2 * MAX_WIRELESS_SENSORS)) * CONFIG_BUFR);
    int addrStart_sensorType = eepromOffset +   ((4 + (2 * MAX_WIRELESS_SENSORS)) * CONFIG_BUFR);
    int addrStart_sensorAlarm = eepromOffset +  ((4 + (3 * MAX_WIRELESS_SENSORS)) * CONFIG_BUFR);

	// write the ID into non-volatile memory
	id.toCharArray(buf, id.length()+1);
	buf[id.length()+1] = '\0'; // terminate string with a null
	i2cEepromWritePage(0x50, addr_Version, buf, sizeof(buf)); // write to EEPROM

	// write the timezone into non-volatile memory
	utcOffset.toCharArray(buf,utcOffset.length()+1);
	buf[id.length()+1] = '\0'; // terminate string with a null
	i2cEepromWritePage(0x50, addr_utcOffset, buf, sizeof(buf)); // write to EEPROM

	// write the dst info into non-volatile memory
	observeDST.toCharArray(buf,observeDST.length()+1);
	buf[id.length()+1] = '\0'; // terminate string with a null
	i2cEepromWritePage(0x50, addr_dst, buf, sizeof(buf)); // write to EEPROM

    String ExtensionA = "ExtensionA";
    ExtensionA.toCharArray(buf,ExtensionA.length()+1);
    buf[ExtensionA.length()+1] = '\0';
    i2cEepromWritePage(0x50,addrConfigExtA, buf, sizeof(buf)); // write to EEPROM

	// write the sensor names and trip codes to non volatile memory
	for(int i = 0; i < MAX_WIRELESS_SENSORS; i++)
	{
        int addr_entryOffset = i * CONFIG_BUFR;

    	temp = sensor_info[i].sensorName;
    	// check to see if name is too long, truncate if it is
    	if(temp.length() >= (CONFIG_BUFR - 2))
    	{
        	temp.substring(0, (CONFIG_BUFR - 2));
    	}

    	// store names
    	temp.toCharArray(buf, temp.length()+1);
    	buf[temp.length()+1] = '\0'; // terminate string with a null
    	addr = addrStart_sensorName + addr_entryOffset; //names address
    	i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM

    	// store trip codes
    	temp = "";
    	temp += sensor_info[i].activateCode; // String concat does conversion from long
    	temp.toCharArray(buf, temp.length()+1);
    	buf[temp.length()+1] = '\0'; // terminate string with a null
    	addr = addrStart_activateCode + addr_entryOffset; //trip codes address
    	i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM

        // store sensor type
        temp = "";
        temp += sensor_info[i].sensorType;  // String concat does conversion from enum
        temp.toCharArray(buf, temp.length()+1);
        buf[temp.length()+1] = '\0'; // terminate string with a null
        addr = addrStart_sensorType + addr_entryOffset; //trip codes address
        i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM

        // store alarm on trip
        temp = "";
        temp += sensor_info[i].alarmOnTrip; // String concat does conversion from boolean
        temp.toCharArray(buf, temp.length()+1);
        buf[temp.length()+1] = '\0'; // terminate string with a null
        addr = addrStart_sensorAlarm + addr_entryOffset; //trip codes address
        i2cEepromWritePage(0x50, addr, buf, sizeof(buf)); // write to EEPROM
	}

	return;
}
/***************************************** end of writeConfig() ******************************************/

/******************************************* restoreConfig() *********************************************/
// restoreConfig():  restores the sensor configuration from non-volatile memory using the Wire library
//  and custom I2C eeprom code.  The sensor configuration consists of data buffers, each CONFIG_BUFR long.
//  The buffers are written to non-volatile memory in the following order:
//  	ID: a constant string identifier: "SIS-2015"
//  	TZ: the Core's local timezone offset, as a string, e.g. "-8.0"
//  	DST: whether the Core's locale observes daylight savings time, e.g. "yes" or "no"
//  	NAME: the name of each registered sensor is stored in its own buffer.  The number of names = MAX_WIRELESS_SENSORS
//  	CODE: the code that a tripped sensor sends.  Each sensor's code is stored in its own buffer.

void restoreConfig()
{
	String ID = "SIS-2015"; 	// the ID value
	char buf[CONFIG_BUFR];  	// temporary buffer to write to non-volatile memory

    int addr;
	int addr_Version = eepromOffset;
    int addr_utcOffset = eepromOffset +            CONFIG_BUFR;
    int addr_dst = eepromOffset +             (2 * CONFIG_BUFR);
    int addrStart_sensorName = eepromOffset + (3 * CONFIG_BUFR);
    int addrStart_activateCode = eepromOffset + ((3 +      MAX_WIRELESS_SENSORS)  * CONFIG_BUFR);
    int addrConfigExtA = eepromOffset =         ((3 + (2 * MAX_WIRELESS_SENSORS)) * CONFIG_BUFR);
    int addrStart_sensorType = eepromOffset +   ((4 + (2 * MAX_WIRELESS_SENSORS)) * CONFIG_BUFR);
    int addrStart_sensorAlarm = eepromOffset +  ((4 + (3 * MAX_WIRELESS_SENSORS)) * CONFIG_BUFR);

	// read the ID and return immediately if ID is not correct
	i2cEepromReadPage(0x50, addr_Version, buf, CONFIG_BUFR);

	// make sure that the buffer contains a valid string
	buf[31] = '\0';
	String data(buf);

	if(data.equals(ID)) 	// restore the rest of the config
	{

    	// restore the timezone
    	i2cEepromReadPage(0x50, addr_utcOffset, buf, CONFIG_BUFR);

    	// make sure that the buffer contains a valid string
    	buf[31] = '\0';
    	utcOffset = buf;

    	// restore the dst
    	i2cEepromReadPage(0x50, addr_dst, buf, CONFIG_BUFR);

    	// make sure that the buffer contains a valid string
    	buf[31] = '\0';
    	observeDST = buf;

        // does the stored config support extensionA ?
        i2cEepromReadPage(0x50,addrConfigExtA, buf, CONFIG_BUFR);
        buf[31] = '\0'; // make sure that the buffer contains a valid string
        String temp = String(buf);
        bool bSupportsExtA = false;
        if (temp.startsWith("ExtensionA")) {
            bSupportsExtA = true;
        }

    	// restore the sensor names and trip codes
    	for(int i = 0; i < MAX_WIRELESS_SENSORS; i++)
    	{
            int addr_entryOffset = i * CONFIG_BUFR;

        	// retrieve names
        	addr=addrStart_sensorName + addr_entryOffset; //names address
        	i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);
        	buf[31] = '\0'; // make sure that the buffer contains a valid string
            sensor_info[i].sensorName = buf;

        	// retrieve trip codes
        	addr=addrStart_activateCode + addr_entryOffset; //trip codes address
        	i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);
        	buf[31] = '\0'; // make sure that the buffer contains a valid string
        	String temp = String(buf);
            sensor_info[i].activateCode = temp.toInt();

            if (bSupportsExtA) {
                // retrieve sensor type
                addr=addrStart_sensorType + addr_entryOffset; //type enum address
                i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);
                buf[31] = '\0'; // make sure that the buffer contains a valid string
                temp = String(buf);
                sensor_info[i].sensorType = (enum_sensorType)temp.toInt();

                // retrieve alarm on trip
        	    addr=addrStart_sensorAlarm + addr_entryOffset; //alarm boolean address
        	    i2cEepromReadPage(0x50, addr, buf, CONFIG_BUFR);
        	    buf[31] = '\0'; // make sure that the buffer contains a valid string
        	    temp = String(buf);
                sensor_info[i].alarmOnTrip = (bool)temp.toInt();

            } else {

                // EEPROM store does not have extensionA values
                // MAX_PIR = 11;      	// PIR sensors are registered in loc 0 through MAX_PIR.  Locations MAX_PIR + 1 to
                                                //  MAX_WIRELESS_SENSORS are non-PIR sensors
                // MAX_DOOR = 15;       // Sensors > MAX_PIR and <= MAX_DOOR are assumed to be exit doors.
                if (i <= 11) {
                    sensor_info[i].sensorType = ePIR;
                } else if (i <= 19) {
                    sensor_info[i].sensorType = eExitDoor;
                } else {
                    sensor_info[i].sensorType = eSeparation;
                }

                // ALARM_SENSOR = 19;  // When this sensor is tripped, publish an SISAlarm
                if (i == 19) {
                    sensor_info[i].alarmOnTrip = true;
                }

            }

    	}

	} else {
        // Invalid saved config, initialze the config
        utcOffset = "0";
        observeDST = "N";
        for (int i = 0; i < MAX_WIRELESS_SENSORS; i++) {
            sensor_info[i].sensorName = "Unknown" + String(i);
            sensor_info[i].sensorType = esensorTypeUnknown;
            sensor_info[i].alarmOnTrip = false;
        }
    }

	return;

}
/****************************************** end of restoreConfig() ******************************************/

/******************************************* i2cEepromWritePage() *******************************************/
// i2cEepromWritePage():  writes a page of data to the I2C eeprom device at deviceAddress.  The
//  page size cannot exceed 32 characters owing to a limitation of the Spark Wire library.
//
// Arguments:
//  deviceAddress.  The I2C bus address of the EEPROM device; nominally 0x50
//  eeAddressPage.  The starting address in EEPROM of the page to be written
//  data.  Pointer to a character array (string) of data to be written to EEPROM
//  length. The number of characters to be written to EEPROM (i.e. page size)
//
void i2cEepromWritePage( int deviceAddress, unsigned int eeAddressPage, char* data, byte length )
{
    // XXX shouldn't this be 30?
	if(length > 32) return; // make sure you don't blow the I2C library buffer!
	Wire.beginTransmission(deviceAddress);
	Wire.write((byte)( (eeAddressPage & 0xFF00) >> 8)); // MSB
	Wire.write((byte)(eeAddressPage & 0xFF)); // LSB
	for (byte c = 0; c < length; c++)
	{
    	Wire.write(data[c]);
	}
	Wire.endTransmission();
	delay(5);   // recommended delay for I2C bus
	return;
}

/******************************************** end of i2cEepromWritePage() ************************************/

/********************************************** i2cEepromReadPage() ******************************************/
// i2cEepromReadPage():  reads a page of data from the I2C eeprom device at deviceAddress.  The
//  page size cannot exceed 32 characters owing to a limitation of the Spark Wire library.
//
// Arguments:
//  deviceAddress.  The I2C bus address of the EEPROM device; nominally 0x50
//  eeAddressPage.  The starting address in EEPROM of the page to be read
//  buffer.  Pointer to a character array (buffer) where the data from the EEPROM is to be written
//  length. The number of characters to be read from EEPROM (i.e. page size)
//
void i2cEepromReadPage( int deviceAddress, unsigned int eeAddressPage, char* buffer, int length )
{
	if(length > 32) return; // make sure you don't blow the I2C library buffer!
	Wire.beginTransmission(deviceAddress);
	Wire.write((byte)( (eeAddressPage & 0xFF00) >> 8)); // MSB
	Wire.write((byte)(eeAddressPage & 0xFF)); // LSB
	Wire.endTransmission();
	Wire.requestFrom(deviceAddress,length,true);
	int c = 0;
	for ( c = 0; c < length; c++ )
	{
    	while (!Wire.available())
    	{
        	// wait for data to be ready
    	}
    	buffer[c] = Wire.read();
	}
	delay(5);   // recommended delay for I2C bus
	return;
}
/********************************************* end of i2cEepromReadPage() *************************************/

/*************************************** parser() **********************************************/
// parser(): parse a comma delimited string into its individual substrings.
//  Arguments:
//  	source:  String object to parse
//  Return: the number of substrings found and parsed out
//
//  This functions uses the following global constants and variables:
//  	const int MAX_SUBSTRINGS -- nominal value is 6
//  	String g_dest[MAX_SUBSTRINGS] -- array of Strings to hold the parsed out substrings

int parser(String source)
{
	int lastComma = 0;
	int presentComma = 0;

	//parse the string argument until there are no more substrings or until MAX_SUBSTRINGS
	//  are parsed out
	int index = 0;
	do
	{
    	presentComma = source.indexOf(',', lastComma);
    	if(presentComma == -1)
    	{
        	presentComma = source.length();
    	}
      g_dest[index++] = "" + source.substring(lastComma, presentComma);
    	lastComma = presentComma + 1;

	} while( (lastComma < source.length() ) && (index < MAX_SUBSTRINGS) );

	return (index);
}
/************************************ end of parser() ********************************************/

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
	const int UKN = -1; 	// action is unknown

	int requestedAction;
	int numSubstrings;
	String registrationInformation; 	// String to hold the information about the sensor
//	unsigned long sensorCode;       	// numerical value of the sensor trip code
	int location;                   	// numerical value of ordinal number of the sensor info

	// parse the string argument into its substrings
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
                        	requestedAction = UKN;
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

    	default:
            numSubstrings = -1; // return an error code for unknown command
        	break;
	}

	return numSubstrings;
}


/********************************** end of registrar() ****************************************/

/************************************* readBuffer() ********************************************/
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
    result = readFromBuffer(offset, cloudBuf);

    return result;
}

/*********************************end of readBuffer() *****************************************/

/********************************** readFromBuffer() ******************************************/
// readFromBuffer(): utility fujction to read from the circular buffer into the
//  character array passed in as stringPtr[].
//  Arguments:
//      int offset: the offset into the circular buffer to read from. 0 is the latest entry.  The
//          next to latest entry is 1, etc. back to BUF_LEN -1.
//       	BUF_LEN can be determined from the cloud variable "bufferSize".  If location exceeds
//       	BUF_LEN - 1, the value that is read out is the oldest value in the buffer, and
//       	-1 is returned by the function.  Otherwise, the value is determined by location
//       	and 0 is returned by this function.
//      char stringPtr[]: pointer to the string that will be returned from this
//        function. The format of the string expected by the web site is one of:
//            (S:nnn) SENSORNAME tripped at DATETIME Z (epoc:EPOCTIME)
//            (S:nnn) SENSORNUMBER detected at DATETIME Z (epoc:EPOCTIME)
//
//  Return:  0 if a valid location was specified, otherwise, -1.

int readFromBuffer(int offset, char stringPtr[])
{
	int result;     	// the result code to return

	// check and fix, if necessary, the offset into the circular buffer
	if(offset >= BUF_LEN)
	{
    	offset = BUF_LEN - 1;   // the maximum offset possible
    	result = -1;        	// return the error code
	}
	else
	{
    	result = 0;         	// return no error code
	}


	// now retrieve the data requested from the circular buffer and place the result string
    // in g_bufferReadout
	g_bufferReadout = "" + cBufRead(offset);

	#ifdef DEBUG
    	Serial.println(g_bufferReadout);
	#endif

	// create the readout string for the cloud from the buffer data
	if(g_bufferReadout != "")  // skip empty log entries
	{
    	int index;

       // parse the comma delimited string into its substrings
      // result of parse is in global array g_dest[]

    	parser(g_bufferReadout);

    	// format the sequence number and place into g_bufferReadout
        g_bufferReadout = "(S:";
        g_bufferReadout += g_dest[1];
        g_bufferReadout += ")";

        // Determine message type
    	if(g_dest[0] == "S")  	// sensor type message
    	{

        	// format the sensor Name from the index
        	index = g_dest[2].toInt();
            g_bufferReadout += sensor_info[index].sensorName;
            g_bufferReadout += " tripped at ";
    	}
    	else    	// advisory type message
    	{
            g_bufferReadout += g_dest[2];
            g_bufferReadout += " detected at ";
    	}

    	// add in the timestamp

    	index = g_dest[3].toInt();
        g_bufferReadout += Time.timeStr(index).c_str();
        g_bufferReadout += " Z (epoch:";
        g_bufferReadout += g_dest[3];
        g_bufferReadout += "Z)";

	}

    g_bufferReadout.toCharArray(stringPtr, g_bufferReadout.length() + 1 );
	stringPtr[g_bufferReadout.length() + 2] = '\0';

	return result;

}

/********************************** end of readFromBuffer() ****************************************/

/******************************************** cBufInsert() *****************************************/
// cBufInsert():  insert a string into the circular buffer at the current tail position.
//  Arguments:
//	String data:  the string data (string object) to store in the circular buffer
//	return:  none.

void cBufInsert(String data)
{
  static boolean fullBuf = false;	// false for the first pass (empty buffer locations)

  cBuf[tail] = data;	// write the data at the end of the buffer
  g_numToPublish++;     // note that there is a new buffer entry to publish

  //  adjust the tail pointer to the next location in the buffer
  tail++;
  if(tail >= BUF_LEN)
  {
	tail = 0;
	fullBuf = true;
  }

  //  the first time through the buffer, the head pointer stays put, but after the
  //	buffer wraps around, the head of the buffer is the tail pointer position
  if(fullBuf)
  {
	head = tail;
  }

}
/***************************************** end of cBufInsert() **************************************/

/********************************************* cBufRead() *******************************************/
// cBufRead():  read back a String object from the "offset" location in the cirular buffer.  The
//	offset location of zero is the latest value in (tail of) the circular buffer.
//  Arguments:
//	int offset:  the offset into the buffer where zero is the most recent entry in the circular buffer
//       and 1 is the next most recent, etc.
//  Return:  the String at the offset location in the circular buffer.

String cBufRead(int offset)
{
  int locationInBuffer;

  locationInBuffer = tail -1 - offset;
  if(locationInBuffer < 0)
  {
    locationInBuffer += BUF_LEN;
  }

  return cBuf[locationInBuffer];

}
/****************************************** end of cBufRead() ***************************************/

/****************************************** publishCircularBuffer () ************************/
// publishCircularBuffer()
//
// This routine publishes recent events in the circular buffer cBuf[] to the Spark Cloud. It
// uses the global variable g_numToPublish to keep track of how many events are awaiting publication.
// It uses a local static variable to be sure that events are not published more frequently than once
// every 2 seconds.
// This routine is called once each time through the main loop. It could be called anytime.
//
void publishCircularBuffer() {

    static unsigned long lastPublishTime = 0;

    if (g_numToPublish >= 0 ) {

        unsigned long currentTime = millis();
        if (currentTime - lastPublishTime > 4000)
        {

            char localBuf[90];

            readFromBuffer(g_numToPublish, localBuf);      // read out the latest logged entry into localBuf

            if(sparkPublish("LogEntry", localBuf, 60))     // ... and publish it to the cloud for xteranl logging
            {
                g_numToPublish--;

            };
            lastPublishTime = currentTime;

        }

    }

}

/****************************************** End of publishCircularBuffer () ************************/

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

/************************************** nbBlink() ************************************************/
// nbBlink():  Blink the D7 LED without blocking.  Note: setup() must declare
//          	pinMode(D7, OUTPUT) in order for this function to work
//  Arguments:
//  	numBlinks:  the number of blinks for this function to implement
//  	blinkTime:  the time, in milliseconds, for the LED to be on or off
//
//  Return:  true if function is ready to be triggered again, otherwise false
//

boolean nbBlink(byte numBlinks, unsigned long blinkTime)
{
	const byte READY = 0;
	const byte LED_ON = 1;
	const byte LED_OFF = 2;
	const byte NUM_BLINKS = 3;


	static byte state = READY;
	static unsigned long lastTime;
	static unsigned long newTime;
	static byte blinks;

	newTime = millis();

	switch(state)
	{
    	case(READY):
        	digitalWrite(D7, HIGH); 	// turn the LED on
        	state = LED_ON;
        	lastTime = newTime;
        	blinks = numBlinks;
        	break;

    	case(LED_ON):
        	if( (newTime - lastTime) >= blinkTime) // time has expired
        	{
            	state = LED_OFF;
            	lastTime = newTime;
        	}
        	break;

    	case(LED_OFF):
        	digitalWrite(D7, LOW);  	// turn the LED off
        	if( (newTime - lastTime) >= blinkTime)
        	{
            	if(--blinks > 0) 	// another blink set is needed
            	{
                	digitalWrite(D7, HIGH);
                	state = LED_ON;
                	lastTime = newTime;
            	}
            	else
            	{
                	state = READY;
            	}

        	}
        	break;

    	default:
        	digitalWrite(D7, LOW);
        	state = READY;

	}

	if(state == READY)
	{
    	return true;
	}
	else
	{
    	return false;
	}
}

/*********************************** end of nbBlink() ********************************************/

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

  return success;

}



String makeNameValuePair(String name, String value)
{
	String nameValuePair = "";
	nameValuePair += DOUBLEQ + name + DOUBLEQ;
	nameValuePair += ":";
	nameValuePair += DOUBLEQ + value + DOUBLEQ;
	return nameValuePair;
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
