// SlaveRtuKernelClass.h

#ifndef SlaveRtuKernelClass_h
#define SlaveRtuKernelClass_h
#include "Arduino.h" 

#include <ModbusSlave.h>

#include "MyTimeout.h"


 
// Uncomment the following line to disable debug messages
//#define DEBUG_SERIAL Serial
#ifdef DEBUG_SERIAL

 
  #pragma message("Debug messages enabled")
  
  // Macro for printing
  #define DEBUG_PRINT(...) DEBUG_SERIAL.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) DEBUG_SERIAL.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif
 

class SlaveRtuKernelClass {

private:	
	// Modbus Holding Registers for configuration
	enum {
		holdingRegSlaveId = 0,
		holdingRegBaudRate,
		holdingRegCommTimeout,
		holdingRegRebootRequest,	// special function, not persistent!
		numConfigRegs,
		
		configAddressOffset = 0x100
	};
	
public:

	void poll(void);
	
protected:
	// You may change this value to enforce re-initialization of EEPROM
	static const unsigned long eepromMagic = 0x112233ab;
		
	bool eepromDefaultsRequired(void);
	void eepromWriteDefaults(uint8_t *buffer, size_t length);
	
	// Configuration managed in Kernel
	struct KernelEeprom {
		uint16_t 		slaveID;
		uint16_t		baudRate;
		uint16_t		commTimeout;
		unsigned long 	magic;
	};
	
	
	
	// Constructor is protected, use only from  a derived class
	SlaveRtuKernelClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId, uint8_t *config, size_t length);
	
	// Modbus RTU kernel
	Modbus *rtuKernel;
	void enableCallback(int cbVectorIdx);
	
	// To be implemented by application class (optionally)	
	// This is the interface to the Modbus entities
	virtual uint8_t cbAccessHoldingRegisters(bool write, uint16_t address, uint16_t length);
	virtual uint8_t cbAccessCoils(bool write, uint16_t address, uint16_t length);
	virtual uint8_t cbAccessDiscreteInputs(bool write, uint16_t address, uint16_t length);
	virtual uint8_t cbAccessInputRegisters(bool write, uint16_t address, uint16_t length);

	virtual void	cbCommunicationLost(void);
	virtual void	cbCommunicationReestablished(void);

	// EEPROM stuff
	void _eepromRead(uint8_t *buffer, size_t length);
	void _eepromWrite(uint8_t *buffer, size_t length);
	
	void dumpBytes(char *text, void *ptr, size_t bytes);

private:	

	bool _rebootRequest = false;
	// Configuration registers buffered in EEPROM
///	static const uint16_t configAddressOffset = 0x0100;
///	static const size_t numConfigRegs = 2;
	uint16_t _configRegs[numConfigRegs];
	
	
	// Handle Holding Registers reserved for Kernel Configuration
	uint8_t _readConfigRegs(uint8_t fc, uint16_t address, uint16_t length);
	uint8_t _writeConfigRegs(uint8_t fc, uint16_t address, uint16_t length);

	
/*   
- CB_READ_COILS = 0,
- CB_READ_DISCRETE_INPUTS,
- CB_READ_HOLDING_REGISTERS,
- CB_READ_INPUT_REGISTERS,
- CB_WRITE_COILS,
- CB_WRITE_HOLDING_REGISTERS,
  CB_READ_EXCEPTION_STATUS,
  CB_MAX
*/	

	// Instance methods connected to kernel callback
	uint8_t _readHoldingRegs(uint8_t fc, uint16_t address, uint16_t length);
	uint8_t _writeHoldingRegs(uint8_t fc, uint16_t address, uint16_t length);
	uint8_t _readCoils(uint8_t fc, uint16_t address, uint16_t length);
	uint8_t _writeCoils(uint8_t fc, uint16_t address, uint16_t length);
	uint8_t _readDiscreteInputs(uint8_t fc, uint16_t address, uint16_t length);
	uint8_t _readInputRegs(uint8_t fc, uint16_t address, uint16_t length);


	// Hooks for kernel callback
	static uint8_t _cbReadHoldingRegs(uint8_t fc, uint16_t address, uint16_t length, void *context);
	static uint8_t _cbWriteHoldingRegs(uint8_t fc, uint16_t address, uint16_t length, void *context);
	static uint8_t _cbReadCoils(uint8_t fc, uint16_t address, uint16_t length, void *context);
	static uint8_t _cbWriteCoils(uint8_t fc, uint16_t address, uint16_t length, void *context);
	static uint8_t _cbReadDiscreteInputs(uint8_t fc, uint16_t address, uint16_t length, void *context);
	static uint8_t _cbReadInputRegs(uint8_t fc, uint16_t address, uint16_t length, void *context);
	
	bool _cbVectorUsed[CB_MAX];
	
	KernelEeprom _config;
	
	bool _communicationLost;
	mytimer_t _communicationLostTimer;
	
};
#endif
