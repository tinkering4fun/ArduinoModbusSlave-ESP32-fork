// HTU21SlaveClass.h
#ifndef HTU21SlaveClass_h
#define HTU21SlaveClass_h


#include "SHT2x.h"


#include "SlaveRtuKernelClass.h"

#include "MyTimeout.h"


class HTU21SlaveClass : public SlaveRtuKernelClass {

	private:
	
	// Modbus Input Registers
	enum {					
		inputRegStatus = 0,
		inputRegTemperature,
		inputRegHumidity,
		
		numInputRegs
	};

	// Modbus Holding Registers
	enum {
		holdingRegCycletime = 0,
		
		numHoldingRegs
	};



	// EEPROM configuration values (e.g. Holding Register)
	struct HTU21SlaveEeprom : public KernelEeprom {
		uint16_t holdingValues[numHoldingRegs]; 
	};
	
	public:
	HTU21SlaveClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId);

	void poll(void);
	
	protected:
	
	
	
	private:
	
	SHT2x *		_sensor;  		// HTU21D Sensor
	uint8_t 	_sensorState;
	
	mytimer_t	_sensorTimer;
	int			_sensorCycleTime;
	bool 		_conversionInProgress;
	
	HTU21SlaveEeprom _eeprom;
	void _saveEeprom(void);
	void _resetConversion(void);
	
	
	// Instance methods connected to kernel callback
	uint8_t cbAccessHoldingRegisters(bool write, uint16_t address, uint16_t length) override;
	uint8_t cbAccessInputRegisters(bool write, uint16_t address, uint16_t length) override;
	
	void	cbCommunicationLost(void) override;
	void	cbCommunicationReestablished(void) override;

	// A set of Input Registers to return Sensor data
	uint16_t	_inputRegs[numInputRegs];

	// A set of holding registers  
	uint16_t 	_holdingRegs[numHoldingRegs];

};


#endif
// eof
