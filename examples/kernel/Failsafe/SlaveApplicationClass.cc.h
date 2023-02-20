// SlaveApplicationClass.cc(.h)
//
// Example for class based Slave implementation making use
// of the 'Failsafe Coils' feature.
//
// This example  maintains 
//    4 Coils (#0 connected to a LED)
//    1 Holding registers for read/write for Coils power-on state
//
// It also inherits the features from SlaveRtuKernelClass 
//
// Holding registers 
// 0x100: Slave Id
// 0x101: Baudrate
// 0x102: Communication Watchdog Timeout [ms]
// 0x103: Reboot request
//
// And for the 'Failsafe Coils' option we have ...
// 0x104: Coils enable mask
// 0x105: Coils safe state
// 0x106: Coils On Time [ms]
// 0x107: Coils Off Time [ms]
// 
//
// Werner Panocha, February 2023


#include <SlaveRtuKernelClass.h>
#include "SlaveApplicationClass.h"

// Constructor
SlaveApplicationClass::SlaveApplicationClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId) 
: SlaveRtuKernelClass(serialStream, baud, transmissionControlPin, slaveId, (uint8_t *)(&_config), sizeof(ApplicationEeprom)) 
{
	
	DEBUG_PRINTLN ("SlaveApplicationClass()");

	dumpBytes("App: EEPROM #1", &_config, sizeof(ApplicationEeprom));	/// debug
	
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

		
		// Populate config with default values
		for(int i = 0; i < _numHoldingRegs; i++)
			_config.holdingValues[i] = 0;
		
		eepromWriteDefaults((uint8_t *)&_config, sizeof(ApplicationEeprom));
	}

	dumpBytes("App: EEPROM #2", &_config, sizeof(ApplicationEeprom)); /// debug

	
	// Populate Holding Registers from EEPROM
	for(int i = 0; i < _numHoldingRegs; i++)
		_holdingRegs[i] = _config.holdingValues[i];
	
	// Populate coil state buffer from Holding Register #0
	for(int i = 0; i < _numCoils; i++)
		_coilStates[i] = (_holdingRegs[0]  >> i) & 1;

	// Hardware init (Coils)
	for(int i =0; i < _numCoils; i++){
		if(_coilPins[i] > 0){
			pinMode(_coilPins[i], OUTPUT);
			digitalWrite(_coilPins[i], _coilStates[i]);
		}
	}
	
	// Enable the callbacks for the desired RTU messages to act on
	enableCallback(CB_READ_COILS);
	enableCallback(CB_WRITE_COILS);
	enableCallback(CB_READ_HOLDING_REGISTERS);
	enableCallback(CB_WRITE_HOLDING_REGISTERS);
	
	
	
	DEBUG_PRINT (F("SlaveApplicationClass(): initialized\n"));
	DEBUG_PRINT (F("Use Modbus FC's 1, 5, 15 to play with the 4 Coils (#0 is LED)\n"));
	DEBUG_PRINT (F("'Failsafe Coils' feature is available. Holding registers @ 0x104 .. 7\n"));
}

// ---------------------------------------------------------------------
// Implement enabled callback methods
// ---------------------------------------------------------------------


// -- Failsafe Coils feature
// ---------------------------------------------------------------------
// This method is responsible for pulsing the output pin of active coils
// if they are configured as 'Failsafe Coils'.
//
// Note: The coils are activated by cbAccessCoils()
void SlaveApplicationClass::cbDriveFailsafeCoils(bool phase, uint16_t mask, uint16_t safeState)
{
	// DEBUG_PRINT (F("cbDriveFailsafeCoils()\n"));
	
	// Cache the mask, it is referred in cbAccessCoils()
	_failsafeCoils = mask;
	
	
	// Iterate through pin mask ...
	for(int i = 0; i < _numCoils; i++){
		
		if( (mask & 1) 
		    && _coilPins[i] 	// Pin defined ?
		    &&_coilStates[i])	// Coil activated ?
		{
			// If a masked coil is set active, do pulsing ...

			// Notice:  
			// Write to the hardware pin only, leaving the buffered
			// coil state untouched!
			digitalWrite(_coilPins[i], phase);
		}
		mask = mask >> 1;
	}
}

// -- Coils
uint8_t SlaveApplicationClass::cbAccessCoils(bool write, uint16_t address, uint16_t length)
{
	DEBUG_PRINT (F("cbAccessCoils(): "));
	if(write)
		DEBUG_PRINT (F("write\n"));
	else
		DEBUG_PRINT (F("read\n"));
	
	if ((address + length) > _numCoils)
		return STATUS_ILLEGAL_DATA_ADDRESS;
	
	// Iterate through addressed coils
	for (int i = 0; i < length; i++){
		int coilNum = address + i;
		if(write) {
			// --- Write Coil
			// Set new state in buffer
			bool newState = rtuKernel->readCoilFromBuffer(i);
			_coilStates[coilNum] = newState;
		
			// Check whether this is a failsafe coil
			if((_failsafeCoils >> coilNum) & 1){
				// Leave the failsafe one's untouched as they are
				// driven in cbDriveFailsafeCoils()
			} else {
				// Normal coils are driven here
				if(_coilPins[coilNum])
					digitalWrite(_coilPins[coilNum], _coilStates[coilNum]);
			}	
		}
		else
			// --- Read Coil
			rtuKernel->writeCoilToBuffer(i, _coilStates[coilNum]);
	}	
	return STATUS_OK;
}


// -- Holding Registers
uint8_t SlaveApplicationClass::cbAccessHoldingRegisters(bool write, uint16_t address, uint16_t length)
{
	DEBUG_PRINT (F("cbAccessHoldingRegisters(): "));
	if(write)
		DEBUG_PRINT (F("write\n"));
	else
		DEBUG_PRINT (F("read\n"));
	
	if ((address + length) > _numHoldingRegs)
		return STATUS_ILLEGAL_DATA_ADDRESS;
    
	for (int i = 0; i < length; i++){
		if(write)
			_holdingRegs[address + i] = rtuKernel->readRegisterFromBuffer(i);
		else
			rtuKernel->writeRegisterToBuffer(i, _holdingRegs[address + i]);
	}
	
	if(write) {
		_updateReceived = true;
		
		// Copy to config & EEPROM
		for(int i = 0; i < _numHoldingRegs; i++)
			_config.holdingValues[i] = _holdingRegs[i];
		
		_saveEeprom();
	}
		
	return STATUS_OK;
};




void SlaveApplicationClass::_saveEeprom(void){
	DEBUG_PRINT (F("App: save EEPROM\n"));
	_eepromWrite((uint8_t *)&_config, sizeof(_config));
}

// eof
