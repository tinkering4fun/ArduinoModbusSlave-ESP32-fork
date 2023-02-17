// SlaveApplicationClass.h
#ifndef SlaveApplicationClass_h
#define SlaveApplicationClass_h

#define COIL_LED_PIN	2		// Onboard blue LED on GPIO2


#include "SlaveRtuKernelClass.h"




class SlaveApplicationClass : public SlaveRtuKernelClass {

	public:

	// Config
	// Need to be populated with factory default values
	struct ApplicationConfig : public KernelEeprom {
		
		uint16_t holdingValues[10]; // = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
	};
	
	SlaveApplicationClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId);

	bool updateAvailable(void);
	void showRegisters(void);
	
	bool coilToggle(void);
	
	private:
	
	ApplicationConfig _config;
	void _saveEeprom(void);
	
	// Instance methods connected to kernel callback
	uint8_t cbAccessHoldingRegisters(bool write, uint16_t address, uint16_t length) override;
	uint8_t cbAccessCoils(bool write, uint16_t address, uint16_t length) override;
	

	// A set of holding registers to play with
	// Register #0 is used for Coils Power-On state !
	static const uint8_t 	_numHoldingRegs = 10;
	uint16_t 		_holdingRegs[_numHoldingRegs];
	bool			_updateReceived = false;
	
	// A set of Coils, #0 connected to LED
	static const size_t	_numCoils = 4;
	uint8_t			_coilPins[_numCoils] = { COIL_LED_PIN, 0, 0, 0};
	bool 			_coilStates[_numCoils];
	bool			_coilToggle = false;
};


#endif
// eof
