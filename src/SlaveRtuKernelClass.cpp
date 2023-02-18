// SlaveRtuKernelClass.cc

// A base class for custom Modbus RTU slaves.
// The idea is to have a more abstract API to build a Modbus slave.
// This class provides some common features which are imho useful
// useful to build workable Modbus Slaves.
//
// Holding registers 
// 0x100: Slave Id
// 0x101: Baudrate
// 0x102: Communication Watchdog Timeout [ms]
// 0x103: Reboot request
//
// See examples on how to use it.
//
// Notice:
// This class based approach is probably too complex and
// ressource hungry for small MCUs like ATmega328P (e.g. Arduino Uno).
// It was developed with ESP32 in mind, which has plenty of memory
// and also a 2nd real Hardware Serial port where the RS485 interface
// can connected to.
//
// Werner Panocha, February 2023
//


// There are plenty of debug messages in the code.
// Messages will be active if 'DEBUG_SERIAL' is defined. 
// e.g.  #define DEBUG_SERIAL Serial
// to use the standard Serial port
// Comment out the following line to hide messages
#define DEBUG_SERIAL Serial

#include "SlaveRtuKernelClass.h"

#include <EEPROM.h>




// ---------------------------------------------------------------------
// To be overriden by Application class ...
// ---------------------------------------------------------------------
// This are the entities controllable with Modbus protocol
// A application class shall implement this methods according to
// the desired functionality 

// --- Holding Registers (R/W)
uint8_t SlaveRtuKernelClass::cbAccessHoldingRegisters(bool write, uint16_t address, uint16_t length){
	return STATUS_ILLEGAL_FUNCTION;
};

// --- Coils (R/W)
uint8_t SlaveRtuKernelClass::cbAccessCoils(bool write, uint16_t address, uint16_t length){
	return STATUS_ILLEGAL_FUNCTION;
};

// --- Discrete Inputs (R/O)
uint8_t SlaveRtuKernelClass::cbAccessDiscreteInputs(bool write, uint16_t address, uint16_t length){
	return STATUS_ILLEGAL_FUNCTION;
};

// --- Input Registers (R/O)
uint8_t SlaveRtuKernelClass::cbAccessInputRegisters(bool write, uint16_t address, uint16_t length){
	return STATUS_ILLEGAL_FUNCTION;
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
// The inheriting Application should override this, if applicable
void SlaveRtuKernelClass::cbCommunicationLost(void){
	DEBUG_PRINT (F("Kernel: Communication Lost Callback!\n"));
}

void SlaveRtuKernelClass::cbCommunicationReestablished(void){
	DEBUG_PRINT (F("Kernel: Communication Reestablished Callback!\n"));
}

// ---------------------------------------------------------------------
// Enable callback to SlaveApplicationClass methods
// ---------------------------------------------------------------------
// See virtual methods in SlaveRtuKernelClass and implement 
// required ones in SlaveApplicationClass
void SlaveRtuKernelClass::enableCallback(int cbVectorIdx){
	switch(cbVectorIdx){
		
		case CB_READ_HOLDING_REGISTERS:
		case CB_WRITE_HOLDING_REGISTERS:
		break;			// Vector already / always set
	
		case CB_READ_COILS:
		rtuKernel->cbVector[CB_READ_COILS] = SlaveRtuKernelClass::_cbReadCoils;
		break;
		
		case CB_WRITE_COILS:
		rtuKernel->cbVector[CB_WRITE_COILS] = SlaveRtuKernelClass::_cbWriteCoils;
		break;
		
		case CB_READ_DISCRETE_INPUTS:
		rtuKernel->cbVector[CB_READ_DISCRETE_INPUTS] = SlaveRtuKernelClass::_cbReadDiscreteInputs;
		break;
		
		case CB_READ_INPUT_REGISTERS:
		rtuKernel->cbVector[CB_READ_INPUT_REGISTERS] = SlaveRtuKernelClass::_cbReadInputRegs;
		break;
   
		case CB_READ_EXCEPTION_STATUS:
		return;			// Not supported here
  
  
		default:
		DEBUG_PRINT (F("Kernel: Bad callback vector!\n"));
		return;
	}
	
	if(cbVectorIdx < CB_MAX)
		_cbVectorUsed[cbVectorIdx] = true;
}



// ---------------------------------------------------------------------
// Constructor 
// ---------------------------------------------------------------------
// Notice: Protected, to be used only from a derived class
SlaveRtuKernelClass::SlaveRtuKernelClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId, uint8_t *config, size_t length) {

	DEBUG_PRINT (F("SlaveRtuKernelClass() "));
	DEBUG_PRINTLN (length);	
	
	// Read in EEPROM configuration
	if(config != NULL){
		DEBUG_PRINT (F("Kernel: Read App config\n"));
		// Application's config
		#ifdef ESP32
		EEPROM.begin(length);
		#endif
		_eepromRead(config, length);
		
		dumpBytes("Kernel: EEPROM for App Dump #1", config, length); /// debug
		
		// Duplicate kernel part
		memcpy((uint8_t *)&_config, config, sizeof(KernelEeprom));
	}
	else {
		DEBUG_PRINT (F("Kernel: Read only Kernel config\n"));
		#ifdef ESP32
		EEPROM.begin(sizeof(KernelEeprom));
		#endif
		_eepromRead((uint8_t *)&_config, sizeof(KernelEeprom));
	}
	
	dumpBytes("Kernel: Actual EEPROM for Kernel", &_config, sizeof(KernelEeprom)); /// debug
		
	if(_config.magic != eepromMagic){
		DEBUG_PRINT (F("Kernel: Bad EEPROM magic, need initialization!\n"));
		// EEPROM init required
		_config.magic = 0;
	}
	else {
		DEBUG_PRINT (F("Kernel: EEPROM magic OK"));
		DEBUG_PRINTLN (_config.magic, HEX);
	}
	
	 
	// Copy EEPROM config also into the configuration register buffer for Modbus
	_configRegs[holdingRegSlaveId] = _config.slaveID;
	_configRegs[holdingRegBaudRate] = _config.baudRate;
	_configRegs[holdingRegCommTimeout] = _config.commTimeout;
	
	// This register is not persistent, just to send reboot request
	_configRegs[holdingRegRebootRequest] = 0;
	
	// Communication watchdog feature
	_communicationLost = false;
	if(_configRegs[holdingRegCommTimeout]){
		setTimeout(_communicationLostTimer, _configRegs[holdingRegCommTimeout]);
	}
 
	
	// Initialize RTU kernel instance 
	// --------------------------------------------------------
	rtuKernel = new Modbus(*serialStream, _config.slaveID, transmissionControlPin);
	
	for(int i = 0; i< CB_MAX; i++)
		_cbVectorUsed[i] = false;
		
	// Set callback context as a reference to this instance
	rtuKernel->setCallbackContext((void *) this);	// <<===
	
	// Set the callbacks to static helper methods of SlaveRtuKernelClass
	// This may look not elegant, but not all C-Compilers / platforms  
	// seem to have the capability for linking class member functions 
	// as callback vectors.
	rtuKernel->cbVector[CB_READ_HOLDING_REGISTERS] = SlaveRtuKernelClass::_cbReadHoldingRegs;
	rtuKernel->cbVector[CB_WRITE_HOLDING_REGISTERS] = SlaveRtuKernelClass::_cbWriteHoldingRegs;
	
	// Start serial port and RTU kernel
	static_cast<HardwareSerial *>(serialStream)->begin(_config.baudRate);
	rtuKernel->begin(_config.baudRate);
	
	dumpBytes("Kernel: EEPROM for App Dump #2", config, length);	/// debug
	
	DEBUG_PRINT (F("Kernel: Initialized\n"));
	DEBUG_PRINT (F("Kernel: Slave ID "));
	DEBUG_PRINTLN (_config.slaveID);
	DEBUG_PRINT (F("Kernel: Baudrate "));
	DEBUG_PRINTLN (_config.baudRate);
	DEBUG_PRINT (F("Kernel: Comm. timeout "));
	DEBUG_PRINTLN (_config.commTimeout);
	DEBUG_PRINT (F("Kernel: Config register offset 0x"));
	DEBUG_PRINTLN (configAddressOffset, HEX);
}


// ---------------------------------------------------------------------
// Periodic Modbus RTU kernel call
// ---------------------------------------------------------------------
void SlaveRtuKernelClass::poll(void){
	rtuKernel->poll();
	
	if(_rebootRequest){
		// Reboot was requested by former write to Holding Register
		DEBUG_PRINT (F("Kernel: performing requested reboot\n"));
		
		#ifdef ESP32 || ESP8266
		ESP.restart();
		#endif
		#ifdef AVR
		void(* reset)(void) = 0; 	// Function @ address 0
		reset();  					 
		#endif
	}
	
	// Handle communication failure alarm, if enabled 
	if(_configRegs[holdingRegCommTimeout] 
	   && _communicationLost == false
	   && checkTimeout(_communicationLostTimer)) 
	{
	   // Issue alarm only once per failure
	   _communicationLost = true; 
	   DEBUG_PRINT (F("Kernel: comunication lost triggered\n"));
	   cbCommunicationLost(); 
	}
}


// ---------------------------------------------------------------------
// EEPROM configuration
// ---------------------------------------------------------------------

bool SlaveRtuKernelClass::eepromDefaultsRequired(void){
	return _config.magic == 0 ? true : false;
}

// Write defaults to EEPROM, when required
void SlaveRtuKernelClass::eepromWriteDefaults(uint8_t *buffer, size_t length){
	DEBUG_PRINT (F("Kernel: Writing EEPROM defaults\n"));
	
	// Check size
	if(length < sizeof(KernelEeprom)){
		DEBUG_PRINT (F("Kernel: EEPROM buffer size error\n"));
		return;
	}
	
	// Kernel buffer defaults
	_config.magic = eepromMagic;
	_config.slaveID = 1;
	_config.baudRate = 9600;
	_config.commTimeout = 0;	// Feature disabled
	
	// Defaults from App, patch in Kernel settings
	((KernelEeprom *)buffer)->magic 		= _config.magic;
	((KernelEeprom *)buffer)->slaveID 		= _config.slaveID;
	((KernelEeprom *)buffer)->baudRate 		= _config.baudRate;
	((KernelEeprom *)buffer)->commTimeout	= _config.commTimeout;
	
	_eepromWrite(buffer, length);
}


void SlaveRtuKernelClass::_eepromRead(uint8_t *buffer, size_t length){
	DEBUG_PRINT (F("Kernel: read EEPROM "));
	DEBUG_PRINTLN (length);
	
	for(int i = 0; i < length; i++){
		
		buffer[i] = EEPROM.read(i);
		// Serial.printf(" %02X ", buffer[i]);
	}
	DEBUG_PRINTLN ();
}
void SlaveRtuKernelClass::_eepromWrite(uint8_t *buffer, size_t length){
	DEBUG_PRINT (F("Kernel: write EEPROM <<<===================== "));
	DEBUG_PRINTLN (length);
	
	for(int i = 0; i <  length; i++){
		EEPROM.write(i, buffer[i]);
		// Serial.printf(" %02X ", buffer[i]);
	}
	DEBUG_PRINTLN ();
	#ifdef ESP32
	EEPROM.commit();
	#endif
}


// ---------------------------------------------------------------------
// Kernel configuration registers
// ---------------------------------------------------------------------
// This are simply Modbus Holding Registers in a separate address range.
// See --> configAddressOffset
uint8_t SlaveRtuKernelClass::_readConfigRegs(uint8_t fc, uint16_t address, uint16_t length){
	
	DEBUG_PRINT (F("Kernel: Read Config Register(s)\n"));
	
	// Adjust for address offset
	address -= configAddressOffset;
		 
	if ((address + length) > numConfigRegs)
		return STATUS_ILLEGAL_DATA_ADDRESS;
    
	for (int i = 0; i < length; i++){
		rtuKernel->writeRegisterToBuffer(i, _configRegs[address + i]);
		
		// Communication Failure Alarm feature
		// Checks for periodic read of this register
		if((address + i) == holdingRegCommTimeout){
			// Retrigger watchdog
			setTimeout(_communicationLostTimer, _configRegs[holdingRegCommTimeout]);
			if(_communicationLost){
				DEBUG_PRINT (F("Kernel: Reset pending communication alarm\n"));
				cbCommunicationReestablished(); 
			}
			_communicationLost = false;
		}
	}
	return STATUS_OK;
}

uint8_t SlaveRtuKernelClass::_writeConfigRegs(uint8_t fc, uint16_t address, uint16_t length){
	DEBUG_PRINT (F("Kernel: Write Config Register(s)\n"));
	
	// Adjust for address offset, so address is now zero-based!
	address -= configAddressOffset;
	
	if ((address + length) > numConfigRegs)
		return STATUS_ILLEGAL_DATA_ADDRESS;
    
	for (int i = 0; i < length; i++){
		uint16_t val = rtuKernel->readRegisterFromBuffer(i);
		_configRegs[address + i] = rtuKernel->readRegisterFromBuffer(i);
		
		switch(address + i){
			case holdingRegSlaveId:			// Slave ID
			DEBUG_PRINT (F("Slave ID\n"));
			_config.slaveID = val;
			break;
			
			case holdingRegBaudRate:		// Baudrate
			DEBUG_PRINT (F("Baudrate\n"));
			_config.baudRate = val;
			break;
		
			case holdingRegCommTimeout:		// Comm. timeout
			DEBUG_PRINT (F("Comm. Timeout\n"));
			_config.commTimeout = val;
			break;
			
			case holdingRegRebootRequest:	// Reboot
			DEBUG_PRINT (F("Reboot request\n"));
			if(val == 0xffff){
				// Performed with next poll();
				_rebootRequest = true;
			}
			val = 0;
			break;
			
		}
		_configRegs[address + i] = val;
	}
	
	// Write EEPROM
	_eepromWrite((uint8_t *)&_config, sizeof(_config));
	DEBUG_PRINT (F("Kernel: Config set, effective on next boot\n"));
	
	return STATUS_OK;
}


// ---------------------------------------------------------------------
// Kernel instance methods invoked on RTU message callback
// ---------------------------------------------------------------------

// -- Holding Registers
uint8_t SlaveRtuKernelClass::_readHoldingRegs(uint8_t fc, uint16_t address, uint16_t length){
	DEBUG_PRINT (F("Kernel: Read Holding Register(s)\n"));
	if(address >= configAddressOffset){
		return _readConfigRegs(fc, address, length);
	}
	else if(_cbVectorUsed[CB_READ_HOLDING_REGISTERS]){
		return cbAccessHoldingRegisters(false, address, length); // read
	}
	else
		return STATUS_ILLEGAL_DATA_ADDRESS;
}

uint8_t SlaveRtuKernelClass::_writeHoldingRegs(uint8_t fc, uint16_t address, uint16_t length){
	DEBUG_PRINT (F("Kernel: Write Holding Register(s)\n"));
	if(address >= configAddressOffset){
		return _writeConfigRegs(fc, address, length);
	}
	else if(_cbVectorUsed[CB_WRITE_HOLDING_REGISTERS]){
		return cbAccessHoldingRegisters(true, address, length);	// write
	}
	else
		return STATUS_ILLEGAL_DATA_ADDRESS;
}

// -- Coils
uint8_t SlaveRtuKernelClass::_readCoils(uint8_t fc, uint16_t address, uint16_t length){
	DEBUG_PRINT (F("Kernel: Read Coils\n"));
	if(_cbVectorUsed[CB_READ_COILS]){
		return cbAccessCoils(false, address, length); 	// read
	}
	else
		return STATUS_ILLEGAL_FUNCTION;
}

uint8_t SlaveRtuKernelClass::_writeCoils(uint8_t fc, uint16_t address, uint16_t length){
	DEBUG_PRINT (F("Kernel: Write Coils\n"));
	if(_cbVectorUsed[CB_WRITE_COILS]){
		return cbAccessCoils(true, address, length);	// write
	}
	else
		return STATUS_ILLEGAL_FUNCTION;
}


// -- Discrete Inputs (R/O)
uint8_t SlaveRtuKernelClass::_readDiscreteInputs(uint8_t fc, uint16_t address, uint16_t length){
	DEBUG_PRINT (F("Kernel: Read Discrete Inputs\n"));
	if(_cbVectorUsed[CB_READ_DISCRETE_INPUTS]){
		return cbAccessDiscreteInputs(false, address, length); 	// read
	}
	else
		return STATUS_ILLEGAL_FUNCTION;
}


// -- Input Registers (R/O)
uint8_t SlaveRtuKernelClass::_readInputRegs(uint8_t fc, uint16_t address, uint16_t length){
	DEBUG_PRINT (F("Kernel: Read Input Registers\n"));
	if(_cbVectorUsed[CB_READ_INPUT_REGISTERS]){
		return cbAccessInputRegisters(false, address, length); 	// read
	}
	else
		return STATUS_ILLEGAL_FUNCTION;
}

// ---------------------------------------------------------------------
// Static callback helper methods invoked by ModbusSlave class
// ---------------------------------------------------------------------
// The Modbus kernel provides a pointer to the associated instance of SlaveClass.
// So we can forward the request to the desired instance method

// -- Holding Registers
uint8_t SlaveRtuKernelClass::_cbReadHoldingRegs(uint8_t fc, uint16_t address, uint16_t length, void *context){
	return ((SlaveRtuKernelClass *)context)->_readHoldingRegs(fc, address, length);
}
uint8_t SlaveRtuKernelClass::_cbWriteHoldingRegs(uint8_t fc, uint16_t address, uint16_t length, void *context){
	return ((SlaveRtuKernelClass *)context)->_writeHoldingRegs(fc, address, length);
}

// -- Coils
uint8_t SlaveRtuKernelClass::_cbReadCoils(uint8_t fc, uint16_t address, uint16_t length, void *context){
	return ((SlaveRtuKernelClass *)context)->_readCoils(fc, address, length);
}
uint8_t SlaveRtuKernelClass::_cbWriteCoils(uint8_t fc, uint16_t address, uint16_t length, void *context){
	return ((SlaveRtuKernelClass *)context)->_writeCoils(fc, address, length);
}

 
// -- Discrete Inputs (R/O)
uint8_t SlaveRtuKernelClass::_cbReadDiscreteInputs(uint8_t fc, uint16_t address, uint16_t length, void *context){
	return ((SlaveRtuKernelClass *)context)->_readDiscreteInputs(fc, address, length);
}
 
// -- Input Register (R/O)
uint8_t SlaveRtuKernelClass::_cbReadInputRegs(uint8_t fc, uint16_t address, uint16_t length, void *context){
	return ((SlaveRtuKernelClass *)context)->_readInputRegs(fc, address, length);
}
	

// ---------------------------------------------------------------------
// Debug aid for EEPROM
// ---------------------------------------------------------------------
void SlaveRtuKernelClass::dumpBytes(char *text, void *ptr, size_t bytes){
	DEBUG_PRINTLN (text);
	for(int i = 0; i < bytes; i++) {
		DEBUG_PRINT (F("  "));
		DEBUG_PRINT (((uint8_t *)ptr)[i], HEX);
	}
	DEBUG_PRINTLN ();
}

// eof
