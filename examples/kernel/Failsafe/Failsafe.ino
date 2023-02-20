// Failsafe.ino
/*
    Modbus Slave example demonstrating the 'Failsafe Coils' feature.
    
 
    Werner Panocha, February 2023

	

    https://github.com/yaacov/ArduinoModbusSlave
*/

// Comment out the following line to disable debug messages
#define DEBUG_SERIAL Serial
// Hint: <SlaveRtuKernelClass.h> defines the macros for Debug output


// Base class for a Modbus Slave, built on top of ModbusSlave
#include <SlaveRtuKernelClass.h>

// Check wether the feature is compiled in
#ifndef FAILSAFE_COILS_SUPPORT
//#error "Compile option FAILSAFE_COILS_SUPPORT is not set!"
#endif


// Application logic of a Modbus Slave
#include "SlaveApplicationClass.h"			// Header


// Dirty trick to use a C++ class in Arduino IDE w/o making a library ...
// Name the .cc Class file as .h Header file.
// The file will appear as a new Tabstrip in the Arduino IDE
#include "SlaveApplicationClass.cc.h"		 


#define SLAVE_ID 1

// RS485 Modbus Interface 
#define RS485_BAUDRATE 	9600 						// Baudrate for Modbus communication.
#define RS485_SERIAL 	Serial2   					// Serial port for Modbus communication.
#define RS485_CTRL_PIN	MODBUS_CONTROL_PIN_NONE 	// GPIO number for Control pin (optionally)

// The Modbus slave hided in a class
SlaveApplicationClass *slaveInstance;

// ---------------------------------------------------------------------
void setup()
// ---------------------------------------------------------------------
{
	// Use this for debug messages etc.
	Serial.begin(115200);
	//Serial.print(F("\nClass based  Modbus RTU server example\n"));
	
	DEBUG_PRINT (F("\n--- Modbus RTU server example - Supporing 'Failsafe Coils' ---\n"));
		
	slaveInstance = new SlaveApplicationClass(&(RS485_SERIAL), RS485_BAUDRATE, RS485_CTRL_PIN, SLAVE_ID);
	
}

// ---------------------------------------------------------------------
void loop()
// ---------------------------------------------------------------------
{
	// Call this periodically ...
	// It will run the ModbusSlave Library and also invoke any
	// enabled callback methods of the slave instance.
	// The slave instance will do whatever it takes, when a
	// Modbus entity was accessed.
    slaveInstance->poll();
    
    // So it may be little to nothing left to do in this loop.


}

// eof
