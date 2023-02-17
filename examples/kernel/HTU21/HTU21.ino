// HTU21.ino
/*
    Modbus Slave for HTU21 sensor
    
    This sketch makes a full functional Modbus
    Thermometer / Hygrometer based on a HTU21 sensor
    
	Some operation parameters are configurable with Holding Registers
	backed up in EEPROM.
	
	The sensor data is available as Input Registers.
	
	
    Created 15-02-2023
    By Werner Panocha

	
	Based on the pretty ArduinoModbusSlave library 
    https://github.com/yaacov/ArduinoModbusSlave
*/


// Comment out the following line to disable debug messages
#define DEBUG_SERIAL Serial
// Hint: <SlaveRtuKernelClass.h> defines the macros for Debug output


// Base class for a Modbus Slave, built on top of ModbusSlave
#include <SlaveRtuKernelClass.h>


// Dirty trick to use a C++ class in Arduino IDE w/o making a library ...
// Name the .cc Class file like .h Header file
#include "HTU21SlaveClass.h"		// Header
#include "HTU21SlaveClass.cc.h"		// Source code


#define SLAVE_ID 1

// RS485 Modbus Interface 
#define RS485_BAUDRATE 	9600 						// Baudrate for Modbus communication.
#define RS485_SERIAL 	Serial2   					// Serial port for Modbus communication.
#define RS485_CTRL_PIN	MODBUS_CONTROL_PIN_NONE 	// GPIO number for Control pin (optionally)



// The Modbus slave, hided in a class
HTU21SlaveClass *slave;

// ---------------------------------------------------------------------
void setup()
// ---------------------------------------------------------------------
{

	
	// Use this for debug messages etc.
	#ifdef DEBUG_SERIAL
	DEBUG_SERIAL.begin(115200);
	#endif
	
	DEBUG_PRINT (F("\n--- HTU21D sensor on Modbus ---\n"));
		
	slave = new HTU21SlaveClass(&(RS485_SERIAL), RS485_BAUDRATE, RS485_CTRL_PIN, SLAVE_ID);
	
}

// ---------------------------------------------------------------------
void loop()
// ---------------------------------------------------------------------
{
	// Call this periodically ...
	// It will run the ModbusSlave library and also invoke any
	// enabled callback methods of the slave instance.
	// The slave instance will do whatever it takes, when a
	// Modbus entity was accessed.
    slave->poll();
    
	// So it may be little to nothing left to do here ...
}

// eof
