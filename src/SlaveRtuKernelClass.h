// SlaveRtuKernelClass.h

#ifndef SlaveRtuKernelClass_h
#define SlaveRtuKernelClass_h
#include "Arduino.h" 

#include <ModbusSlave.h>

// -------------------------------------------------------------------
// Debug output macros
// -------------------------------------------------------------------

#ifdef DEBUG_SERIAL

 
  #pragma message("Debug messages enabled")
  
  // Macro for printing
  #define DEBUG_PRINT(...) DEBUG_SERIAL.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) DEBUG_SERIAL.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif
 
 
// -------------------------------------------------------------------
// Timeout macros 
// -------------------------------------------------------------------
// Intended for short internal timing of some tasks like
// LED blinking, relay contact debounce, etc.
// A 'timer' is simply a 16bit memory variable, and timing intervals
// are restricted to apprx. 30 seconds as we have a
// 15 bit counter for milliseconds, + rollover


// Memory buffer variable used for timing operations
// This needs to be signed value!
#if defined(__AVR__)
// AVR specific code here
#define MY_TIMER_MASK 0xFFFF
typedef int mytimer_t;
#elif defined(ESP8266)
// ESP8266 specific code here
#define MY_TIMER_MASK 0xFFFF
typedef int16_t mytimer_t;
#elif defined(ESP32)
// ESP32 specific code here
#define MY_TIMER_MASK 0xFFFF
typedef int16_t mytimer_t;
#endif


// Load timer with timeout value (absolute value from now on)
#define setTimeout(x, t)   x =  mytimer_t(mytimer_t(millis() & MY_TIMER_MASK) + t)

// (Re-)Load timer with timeout value, relative to last timeout
// Use it in continous periodic tasks
#define nextTimeout(x,t)    x = x + t

// Returns true, if timeout is true
// Notice: works only for the next 30 seconds after timeout, beware
// of rollover!
#define checkTimeout(x)   (mytimer_t(mytimer_t(millis() & MY_TIMER_MASK)  - x) >= 0 ? true : false)

// Reset timer to current time.
// Afterwards checkTimeout() will return true.
#define resetTimeout(x)   x =  mytimer_t(millis() & MY_TIMER_MASK)

// Get timer value compared to current time
// Negative values means timeout not reached, 
// positive values telling the latency time.
#define getTimeoutLatency(x) mytimer_t((mytimer_t(millis() & MY_TIMER_MASK) - x) & MY_TIMER_MASK)

// Return the max. achievable timeout value
#define getMaxTimeout() ((size_t)0x7FFF)
// Test whether timeout value is allowed
#define isValidTimeout(x)  (x > getMaxTimeout() ? false : true) 


// -------------------------------------------------------------------
// Slave kernel functions
// -------------------------------------------------------------------
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
	static const unsigned long eepromMagic = 0x12345678;
		
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
	uint16_t _configRegs[numConfigRegs];
	
	
	// Handle Holding Registers reserved for Kernel Configuration
	uint8_t _readConfigRegs(uint8_t fc, uint16_t address, uint16_t length);
	uint8_t _writeConfigRegs(uint8_t fc, uint16_t address, uint16_t length);	

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
