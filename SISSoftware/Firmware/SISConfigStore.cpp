/***************************************************************************************************/
// SISConfigStore.cpp
//  Routines used by SIS to store the configuration in non-volatile memory and to retrieve
//  that information upon restart.
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/

#include <SISConfigStore.h>
#include <SISGlobals.h>

// the ID value for a valid config in non volatile memory
const String id = "SIS-2015";  // if this changes, then previous EEPROM stores cannot be read

//  For I2C eeprom configuration
const int VIRTUAL_DEVICE_SIZE = 4096;   // divide device up into virtual eeproms 4K bytes each
const int MAX_VIRTUAL_DEVICES = 8;  	// 32 K bytes device = 8 pages of 4K each
const int VIRTUAL_DEVICE_NUM = 0;   	// use the first virtual device - change if it wears out

// XXX should this be 30?
const int CONFIG_BUFR = 32; 	// buffer to use to store and retrieve data from non-volatile memory

// variable to hold the virtual eeprom device used to store the config
int eepromOffset;

// ***************************  Internal Routines Declares ahead
// Writes one page of up to 30 bytes to the EEPROM
void i2cEepromWritePage( int deviceAddress, unsigned int eeAddressPage, char* data, byte length );
// Reads one page of up to 30 bytes from the EEPROM
void i2cEepromReadPage( int deviceAddress, unsigned int eeAddressPage, char* buffer, int length );
// *************************** End of internal declare aheads

/******************************************* configStoreInit() *********************************/
//  configStoreInit()
//  Called once when program starts to initialize this module.
//  Eventually this module should be an object with an initializer
//
void configStoreInit(){

	// select virtual device on the eeprom
	if(VIRTUAL_DEVICE_NUM < MAX_VIRTUAL_DEVICES)
	{
	  eepromOffset = VIRTUAL_DEVICE_NUM * VIRTUAL_DEVICE_SIZE;
	}
	else
	{
	  eepromOffset = MAX_VIRTUAL_DEVICES - 1;
	}

}
//************************************ end  configStoreInit() **************************************

/******************************************** writeConfig() ******************************************/
// writeConfig():  writes the sensor configuration out to non-volatile memory using the Wire library
//  and custom I2C eeprom code.  The sensor configuration consists of data buffers, each CONFIG_BUFR long.
//  The buffers are written to non-volatile memory in the following order:
//  	ID: a constant string identifier: "SIS-2015"
//  	TZ: the Core's local timezone offset, as a string, e.g. "-8.0"
//  	DST: whether the Core's locale observes daylight savings time, e.g. "yes" or "no"
//  	NAME: the name of each registered sensor is stored in its own buffer.  The number of names = MAX_WIRELESS_SENSORS
//  	CODE: the code that a tripped sensor sends.  Each sensor's code is stored in its own buffer.
//      Extension Marker: "ExtensionA" indicates that the following values are also in EEPROM
//      SensorType: For each sensor, the enumeration enum_sensorType
//      AlarmOnTrip: For each sensor, Boolean

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
