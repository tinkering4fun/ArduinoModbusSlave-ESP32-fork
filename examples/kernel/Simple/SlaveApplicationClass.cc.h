// SlaveApplicationClass.cc(.h)
//
// Example for class based Slave implementation
//
// This example class maintains 
//    4 Coils (#0 connected to a LED)
//   10 Holding registers for read/write (#0 for Coils power-on state)
//
// It also inherits the feature auf the SlaveRtuKernelClass 
//
// Holding registers 
// 0x100: Slave Id
// 0x101: Baudrate
// 0x102: Communication Watchdog Timeout [ms]
// 0x103: Reboot request
//
// Werner Panocha, February 2023


#include <SlaveRtuKernelClass.h>
#include "SlaveApplicationClass.h"

// Constructor
SlaveApplicationClass::SlaveApplicationClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId) 
: SlaveRtuKernelClass(serialStream, baud, transmissionControlPin, slaveId, (uint8_t *)(&_config), sizeof(ApplicationConfig)) 
{
	
	DEBUG_PRINTLN ("SlaveApplicationClass()");

	dumpBytes("App: EEPROM #1", &_config, sizeof(ApplicationConfig));	/// debug
	
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
			_config.holdingValues[i] = i;
		
		eepromWriteDefaults((uint8_t *)&_config, sizeof(ApplicationConfig));
	}

	dumpBytes("App: EEPROM #2", &_config, sizeof(ApplicationConfig)); /// debug
	
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
	DEBUG_PRINT (F("Use Modbus FC's 3, 6, 16 to play with the 10 Holding Registers (#0 defines Coil Power-On state)\n"));
	
	showRegisters();
}

// ---------------------------------------------------------------------
// Implement enabled callback methods
// ---------------------------------------------------------------------

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
	
	for (int i = 0; i < length; i++){
		int coilNum = address + i;
		if(write) {
			bool oldState = _coilStates[coilNum];
			
			// Set new state
			bool newState = rtuKernel->readCoilFromBuffer(i);
			_coilStates[coilNum] = newState;
			
			// Signal if any coil toggle
			if(newState != oldState )
				_coilToggle = true;
				
			// Coil #0 is connected to LED
			if(coilNum == 0)
				digitalWrite(_coilPins[coilNum], _coilStates[coilNum]);
		}
		else
			rtuKernel->writeCoilToBuffer(i, _coilStates[coilNum]);
	}	
	
	return STATUS_OK;
}

// ---------------------------------------------------------------------
// Check for Coil toggle
// ---------------------------------------------------------------------
bool SlaveApplicationClass::coilToggle(void){
	
	if(_coilToggle){
		// Only one signal per update
		_coilToggle = false;
		return(true);
	} 
	else {
		return(false);
	}
}

// ---------------------------------------------------------------------
// Check for register update
// ---------------------------------------------------------------------
bool SlaveApplicationClass::updateAvailable(void){
	
	if(_updateReceived){
		// Only one signal per update
		_updateReceived = false;
		return(true);
	} 
	else {
		return(false);
	}
}

// ---------------------------------------------------------------------
// Show current register content
// ---------------------------------------------------------------------
void SlaveApplicationClass::showRegisters(void){
	char buf[128];
	int len = 0;
	for(int i = 0; i < _numHoldingRegs; i++){
		len +=	snprintf(buf + len, sizeof(buf)-(len + 1), "   %d:%04X", i, _holdingRegs[i]);
	}
	Serial.print(F("regs"));
	Serial.println(buf);
}

void SlaveApplicationClass::_saveEeprom(void){
	DEBUG_PRINT (F("App: save EEPROM\n"));
	_eepromWrite((uint8_t *)&_config, sizeof(_config));
}

// eof
