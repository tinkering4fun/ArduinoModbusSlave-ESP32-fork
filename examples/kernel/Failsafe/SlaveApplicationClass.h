// SlaveApplicationClass.h
#ifndef SlaveApplicationClass_h
#define SlaveApplicationClass_h

#define COIL_LED_PIN	2		// Onboard blue LED on GPIO2


#include "SlaveRtuKernelClass.h"




class SlaveApplicationClass : public SlaveRtuKernelClass {

	public:

	SlaveApplicationClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId);

	
	private:
	// How many Coils to support
	static const size_t	_numCoils = 4;

	
	// Config params for the application
	struct ApplicationEeprom : public KernelEeprom {
		
		// No deficated application config, but we want to read the
		// Kernel config settings
	};
	
	ApplicationEeprom _config;


	
	// Instance methods connected to kernel callback
	uint8_t cbAccessCoils(bool write, uint16_t address, uint16_t length) override;
	
	void cbDriveFailsafeCoils(bool phase, uint16_t	mask, uint16_t	safeState) override;
	
	// A set of Coils, #0 connected to LED
	// Others may be set here, 0 == not connected
	uint8_t			_coilPins[_numCoils] = { COIL_LED_PIN, 0, 0, 0};
	bool 			_coilStates[_numCoils];
	
	// Buffer the mask of failsafe coils
	uint16_t		_failsafeCoils = 0;
};


#endif
// eof
