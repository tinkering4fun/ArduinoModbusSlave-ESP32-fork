// HTU21SlaveClass.cc(.h)
//
//
// Holding Registers:
// # 0:	Measurement interval in ms (persistent in EEPROM)
//
// Input Registers:
// # 0: Status
// # 1: Temperature
// # 2: Humidity
//
//
// It also inherits the feature auf the SlaveRtuKernelClass which
// means we have EEPROM configuration for Slave-ID and Baudrate.
//

/* TO DO:
 * Maintain status register ...
 * Feature flags, e.g. Temp in Fahrenheit
*/


#include "Wire.h"


// We use this library as this one support asynchronous data aquisition
// while most others make use of delay()
//
// See  https://github.com/RobTillaart/SHT2x
#include "SHT2x.h"



#include <SlaveRtuKernelClass.h>
#include "HTU21SlaveClass.h"


// Load timer macro definitions
#include "MyTimeout.h"

// Try to define a correct LED pin
#ifndef LED_BUILTIN
#ifdef ESP32
#define LED_BUILTIN 2
#endif
#endif


// Constructor
HTU21SlaveClass::HTU21SlaveClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId) 
: SlaveRtuKernelClass(serialStream, baud, transmissionControlPin, slaveId, (uint8_t *)(&_eeprom), sizeof(HTU21SlaveEeprom)) 
{
	
	DEBUG_PRINTLN ("HTU21SlaveClass()");

	dumpBytes("App: EEPROM #1", &_eeprom, sizeof(HTU21SlaveEeprom));	/// debug
	
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, 0);
	
	
	
	// Verify EEPROM config
	if(eepromDefaultsRequired()){
		DEBUG_PRINT (F("App: Setting EEPROM defaults <====================\n"));
		
		// WTF is this ?
		// Without this ouput I could not see messages about EEPROM init ..
		for(int i =0; i < 5; i++){
			DEBUG_PRINT (F("~~  "));
			delay(500); 
		}
		DEBUG_PRINTLN ();

		
		// Populate EEPROM config structure with default values
		for(int i = 0; i < numHoldingRegs; i++){
			switch(i){
				
				// Sensor cycle time
				case holdingRegCycletime:
				_eeprom.holdingValues[i] = 5000;	// 5000 ms
				break;
				
			}
		}
		
		eepromWriteDefaults((uint8_t *)&_eeprom, sizeof(HTU21SlaveEeprom));
	}

	dumpBytes("App: EEPROM #2", &_eeprom, sizeof(HTU21SlaveEeprom)); /// debug
	
	// Populate Holding Registers from EEPROM
	for(int i = 0; i < numHoldingRegs; i++)
		_holdingRegs[i] = _eeprom.holdingValues[i];
	
	// Prepare Input Registers
	for(int i = 0; i < numInputRegs; i++)
		_inputRegs[i] = 0;
	
	// Reset cycle timer so it appears to be exausted
	_resetConversion();

	// Create sensor object
	_sensor = new SHT2x();
	_sensor->begin();
	_sensorState = _sensor->getStatus();
  
	
	// Enable the callbacks for the desired Modbus RTU messages to act on
	enableCallback(CB_READ_INPUT_REGISTERS);
	enableCallback(CB_READ_HOLDING_REGISTERS);
	enableCallback(CB_WRITE_HOLDING_REGISTERS);
	
	DEBUG_PRINT (F("HTU21SlaveClass(): initialized\n"));
	DEBUG_PRINT (F("Use Modbus FC 4 to read from Input Registers\n"));
	DEBUG_PRINT (F("Use Modbus FC's 3, 6, 16 to read/write Holding Registers\n"));
}

// ---------------------------------------------------------------------
// Reset flags and timer for periodic conversion logic
// ---------------------------------------------------------------------
void HTU21SlaveClass::_resetConversion(void)
{
	_sensorCycleTime = _holdingRegs[holdingRegCycletime];
	resetTimeout(_sensorTimer);
	_conversionInProgress = false;
}

// ---------------------------------------------------------------------
// Overload the poll() method to run also any tasks for the sensor
// ---------------------------------------------------------------------
void HTU21SlaveClass::poll(void)
{
	
	static enum { reqTemperature, reqHumidity } requestType;
	
	// Handle periodic sensor polling
	// In a given intervall, we request a pair of conversions
	if (checkTimeout(_sensorTimer)) {
	    // Timer exhausted, 'reload' for next cycle
	    nextTimeout(_sensorTimer, _sensorCycleTime);
	   
	    // Request new conversion cycle, start with Temperature
	    _sensor->requestTemperature();
	    requestType = reqTemperature;
	    _conversionInProgress = true;
	}
	else if(_conversionInProgress){
		// Conversion cycle  in progress, it has 2 steps
		switch(requestType){
			
			// Step 1
			case reqTemperature:
			if(_sensor->reqTempReady()){
				// Temperature data available		
				_sensor->readTemperature();
				// Make available in Input Register
				_inputRegs[inputRegTemperature] = int(_sensor->getTemperature() * 10);
				
				_sensor->requestHumidity();		// Request humidity conversion
				requestType = reqHumidity;
				
				DEBUG_PRINT (_sensor->getTemperature(), 1);
			}
			break;
			
			// Step 2
			case reqHumidity:
			if(_sensor->reqHumReady()){
				// Humidity data available
				_sensor->readHumidity();		 
				// Make available in Input Register
				_inputRegs[inputRegHumidity] = int(_sensor->getHumidity() * 10);
				
				_conversionInProgress = false;	// Conversion cycle complete
				
				DEBUG_PRINT ("    ");
				DEBUG_PRINTLN (_sensor->getHumidity(), 1);
			}
			break;
		}
	}
	
	// Run also the Modbus kernel stuff
	SlaveRtuKernelClass::poll();
}

// ---------------------------------------------------------------------
// Communication Watchdog feature
// ---------------------------------------------------------------------
// If the Configuration Register is not zero, it's value is assumed
// as timeout value in ms.
// In this case we expect periodic reads of this register, otherwise
// a communication failure Alarm is issued.
//
// Slaves for control of critical appliances (e.g. heating) may use
// this to take safety measures when the communication to the
// Modbus Master MCU is broken.
// 
// This is a silly proof of concept with a Alarm LED
void HTU21SlaveClass::cbCommunicationLost(void){
	DEBUG_PRINT (F("Application: Communication lost!\n"));
	digitalWrite(LED_BUILTIN, 1);	// Alarm
}
void HTU21SlaveClass::cbCommunicationReestablished(void){
	DEBUG_PRINT (F("Application: Communication reestablished!\n"));
	digitalWrite(LED_BUILTIN, 0);	// OK
}

// ---------------------------------------------------------------------
// Implement enabled callback methods
// ---------------------------------------------------------------------

// -- Input Registers (R/O)
uint8_t HTU21SlaveClass::cbAccessInputRegisters(bool write, uint16_t address, uint16_t length)
{
	DEBUG_PRINT (F("cbAccessInputRegisters()\n"));
	
	if ((address + length) > numInputRegs)
		return STATUS_ILLEGAL_DATA_ADDRESS;
    
	for (int i = 0; i < length; i++){
		rtuKernel->writeRegisterToBuffer(i, _inputRegs[address + i]);
	}
	return STATUS_OK;
}

// -- Holding Registers (R/W)
uint8_t HTU21SlaveClass::cbAccessHoldingRegisters(bool write, uint16_t address, uint16_t length)
{
	DEBUG_PRINT (F("cbAccessHoldingRegisters(): "));
	if(write)
		DEBUG_PRINT (F("write\n"));
	else
		DEBUG_PRINT (F("read\n"));
	
	if ((address + length) > numHoldingRegs)
		return STATUS_ILLEGAL_DATA_ADDRESS;
    
	for (int i = 0; i < length; i++){
		if(write)
			_holdingRegs[address + i] = rtuKernel->readRegisterFromBuffer(i);
		else
			rtuKernel->writeRegisterToBuffer(i, _holdingRegs[address + i]);
	}
	
	if(write) {
		// Copy to config & EEPROM 
		for(int i = 0; i < numHoldingRegs; i++){
			
			_eeprom.holdingValues[i] = _holdingRegs[i];
			
			// Take extra actions if applicable / required
			switch(i){
				
				// Sensor cycle time
				case holdingRegCycletime:
				_resetConversion();
				break;
				
			}
		}
		_saveEeprom();
	}
	return STATUS_OK;
};



void HTU21SlaveClass::_saveEeprom(void){
	DEBUG_PRINT (F("App: save EEPROM\n"));
	_eepromWrite((uint8_t *)&_eeprom, sizeof(_eeprom));
}

// eof
