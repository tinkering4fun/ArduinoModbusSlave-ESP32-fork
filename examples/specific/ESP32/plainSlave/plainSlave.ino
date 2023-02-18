/*
    ESP32 as Modbus RTU (RS485) slave example
    
    
    Control and Read  Espressif ESP32  I/Os using Modbus RTU.
    
    This sketch is an enhanced copy of the 'simple.ino' example (V2.1.1),
    originally created by Yaacov Zamir and updated by Yorick Smilda.
    
    See https://github.com/yaacov/ArduinoModbusSlave

    This sketch shows how you can use the callback vector for reading and
    controlling ESP32 I/Os.
    RS485 serial communication is connected to UART2 (aka Serial2), 
    thus the standard Serial is left free for diagnostic messages etc.

	From Modbus perspective this Slave provides
	
	2 Coils				(digital output pins) *)
	2 Discrete Inputs	(digital input pins)
	2 Input Registers	( 2 x 12-bit A/D converter, aka analog input pins)
	4 Holding Registers	( 2 x 8-bit D/A converter, aka analog output pins
	                     plus access to digital I/O in bank mode)
	
	5 Holding Registers for configuration parameters backed up in EEPROM.
	
	All normal object's raw adresses starting at 0.
	Except for the 5 Holding registers backed up in EEPROM starting at 0x100.
 
	*) A compile option 'BLUE_LED' uses the onboard LED as Coil 0,
	   so one can play already w/o any extra hardware on a Dev Board.

	
	Test environment
	- Arduino 1.8.19 / esp32 2.0.6 on ESP32 Dev Module
	- ArduinoModbus library version V2.1.1
	- RS485 adapter with automatic flow control
	- Modbus master: Some Raspberry Pi program based on libmodbus
	- External hardware: Jumper wires for loopback
	                     DVM to check A/D and D/A values (optional)
    
    
    Provided by Werner Panocha - February 2023
*/


// If wrong board type, stop compilation right here
#ifndef ESP32
#error "This code is intended for ESP32 boards only!"
#endif


#include <EEPROM.h>
#include <ModbusSlave.h>


// Notice: WiFi will be disabled on setup
#include <WiFi.h>
#include <esp_wifi.h>
#include "driver/adc.h"

// Uncomment this if LED shall not used as Modbus coil #0
#define BLUE_LED 2		// Onboard blue LED on GPIO2

// Pin definitions for ESP32 GPIO
#define ADC_0	36		// ADC1,CH0 == GPIO36
#define ADC_1	39		// ADC1,CH3 == GPIO39
#define DAC_0	25
#define DAC_1	26
#define DI_0	34
#define DI_1	35
#ifdef BLUE_LED
#define DO_0	BLUE_LED 
#else
#define DO_0 	32
#endif
#define DO_1	33



#define SLAVE_ID 1           // Default Modbus slave ID, configurable in EEPROM register
#define SERIAL_BAUDRATE 9600 // Default Modbus baudrate, configurable in EEPROM register

// ESP32 has a 2nd serial I/F, let's make use of it
// So it's easy to report debug messages on the default serial port.
#define SERIAL_PORT Serial2   // Serial port to use for RS485 communication

// Change to the pin the RE/DE pin of the RS485 controller is connected to.
// Keep 'MODBUS_CONTROL_PIN_NONE' if hardware supports automatic flow control
#define RS485_CTRL_PIN   MODBUS_CONTROL_PIN_NONE 

// The position in the array determines the address. 
uint8_t input_pins[] = {DI_0, DI_1};   	// Add the pins you want to read as a Discrete input.
uint8_t output_pins[] = {DO_0, DO_1};   // Add the pins you want to control via a Coil.
uint8_t adc_pins[] = {ADC_0, ADC_1}; 	// Add the ADC pins you want to read as a Input register.
uint8_t dac_pins[] = {DAC_0, DAC_1}; 	// Add the DAC pins you want to write as a Holding register.


// You shouldn't have to change anything below this to get this example to work

const uint8_t input_pins_size = sizeof(input_pins) / sizeof(input_pins[0]);    // Get the size of the input_pins array
const uint8_t output_pins_size = sizeof(output_pins) / sizeof(output_pins[0]); // Get the size of the output_pins array
const uint8_t adc_pins_size = sizeof(adc_pins) / sizeof(adc_pins[0]); // Get the size of the adc_pins array
const uint8_t dac_pins_size = sizeof(dac_pins) / sizeof(dac_pins[0]); // Get the size of the dac_pins array


// Shadow for output pins state (digitalRead() does not work for outputs on ESP32 / ESP8266)
uint8_t output_pins_state[output_pins_size];

// Buffer for DAC pin holding registers (not in EEPROM)
uint16_t dac_pins_values[dac_pins_size];


// Modbus object declaration
Modbus *slave;



// ---------------------------------------------------------------------
// Configuration feature: Holding registers backed up in EEPROM
//
// 0x0100: Modbus ID
// 0x0101: Baudrate
// 0x0102: Power on state of output pins (max 16)
// 0x0103: Power on state of DAC 0
// 0x0104: Power on state of DAC 1

// Number of configuration registers
#define NUM_CFG_REG		( 3 + dac_pins_size	)

#define CFG_REG_OFFSET	0x100	// Offset to 1st configuration register
#define MAGIC 			0xABCD	// To verify EEPROM initialization

typedef struct {
	uint16_t	id;								// Slave ID
	uint16_t	baud;							// Baudrate
	uint16_t	pon_dout_mask;					// Power on pin state
	uint16_t	pon_dacs[dac_pins_size];		// Power on dac state
	
	uint16_t	magic;			// Notice: Not a Modbus register
} config_t;

// RAM buffer for configuration stored in EEPROM 
config_t config;		



// Read EEPROM into buffer
void readConfig()
{
	Serial.printf("Read EEPROM\n");
	uint8_t *ptr = (uint8_t *)(&config);
	for(int i = 0; i < sizeof(config_t); i++){
		ptr[i] = EEPROM.read(i);
	}
}

// Write buffer to EEPROM
void writeConfig()
{
	Serial.printf("Write EEPROM\n");
	uint8_t *ptr = (uint8_t *)(&config);
	for(int i = 0; i <  sizeof(config_t); i++){
		EEPROM.write(i, ptr[i]);
	}
	EEPROM.commit();
}			
// ---------------------------------------------------------------------


// ---------------------------------------------------------------------
// Digital I/O in bank mode feature
//
// Sometimes it may desirable to access the defined I/O pins 
// as 16-bit word (instead of Coil / Discrete Input)
//
// Notice:
// This is just an alternative way to deal with the pins defined
// in output_pins[] and input_pins[]
//
// We have 2 Holding registers to accomplish this.
// 0x0002: Digital output pins, read/write
// 0x0003: Digital input pins, read only
#define NUM_DIO_REG 2


// Be aware of this limitation ...
static_assert(input_pins_size <= 16, 
	"This code cannot support more than 16 input pins in bank mode");
static_assert(output_pins_size <= 16, 
	"This code cannot support more than 16 output pins in bank mode");


// Write digital output pins from 16-bit value
void writeCoilsFromRegister(uint16_t value){

	uint16_t mask = 1;
	for(int i = 0; i < output_pins_size; i++){
		bool coil = value & mask;
		digitalWrite(output_pins[i], coil);
		output_pins_state[i] = coil;	// Remember last written state
		mask = mask << 1;
	}
}

// Read state of digital output pins into 16-bit value
uint16_t readDigitalOutputsToRegister(void){

	uint16_t value = 0;
	uint16_t mask = 1;
	for(int i = 0; i < output_pins_size; i++){
		if(output_pins_state[i])
			value |= mask;
		mask = mask << 1;
	}
	return(value);	
}

// Read digital input pins into 16-bit value
uint16_t readDiscreteInputsToRegister(void){
	
	uint16_t value = 0;
	uint16_t mask = 1;
	for(int i = 0; i < input_pins_size; i++){
		if(digitalRead(input_pins[i]))
			value |= mask;
		mask = mask << 1;
	}
	return(value);
}
// ---------------------------------------------------------------------


// ---------------------------------------------------------------------
void setup()
// ---------------------------------------------------------------------
{
	// Use this for debug messages etc.
	Serial.begin(115200);
	
	// Initialize EEPROM access
	EEPROM.begin(sizeof(config_t));
	
	Serial.printf("\nESP32 Modbus server\n");
	
	// Disable WiFi, as a pure Modbus server has no need for it
	adc_power_off();
	WiFi.disconnect(true);  // Disconnect from the network
	WiFi.mode(WIFI_OFF);    // Switch WiFi off
	
	
	// Load EEPROM configuration
	readConfig();
	if(config.magic != MAGIC){
		Serial.printf("WARNING: MAGIC not found, writing EEPROM initial values\n");
		
		// Set defaults
		config.id 		= SLAVE_ID;
		config.baud 	= SERIAL_BAUDRATE;
		config.magic 	= MAGIC;
		
		config.pon_dout_mask = 0;
		for(int i = 0; i < dac_pins_size; i++)
			config.pon_dacs[i] = 0;
			
		// Write values back
		writeConfig();
	}
	
	#ifdef BLUE_LED
	Serial.printf("Compile option: Blue LED is simulating coil #1\n");
	#endif
	Serial.printf("Listening on UART-2, ID %d, %d baud\n", config.id, config.baud);
	


	// Set the defined input pins to input mode.
	for (int i = 0; i < input_pins_size; i++)
	{
		pinMode(input_pins[i], INPUT);
	}

	// Set the defined analog pins to input mode.
	for (int i = 0; i < adc_pins_size; i++)
	{
		pinMode(adc_pins[i], INPUT);
	}

	// Set the defined output pins to output mode.
	for (int i = 0; i < output_pins_size; i++)
	{
		pinMode(output_pins[i], OUTPUT);
		output_pins_state[i] = (config.pon_dout_mask >> i) & 1;	// Buffer
		digitalWrite(output_pins[i], output_pins_state[i]);		// Output
	}

	// Preset the defined DAC outputs
	for (int i = 0; i < dac_pins_size; i++)
	{
		dac_pins_values[i] = config.pon_dacs[i];	// Preset buffer
		dacWrite(dac_pins[i], dac_pins_values[i]);	// Set DAC
	}

	// Slave instance
	slave = new Modbus(SERIAL_PORT, config.id, RS485_CTRL_PIN);
	
	// Register functions to call when a certain function code is received.
	slave->cbVector[CB_WRITE_COILS] = writeDigitalOut;
	slave->cbVector[CB_READ_COILS] = readDigitalOut;
	slave->cbVector[CB_READ_DISCRETE_INPUTS] = readDigitalIn;
	slave->cbVector[CB_READ_INPUT_REGISTERS] = readAnalogIn;
	slave->cbVector[CB_READ_HOLDING_REGISTERS] = readHoldingRegs;
	slave->cbVector[CB_WRITE_HOLDING_REGISTERS] = writeHoldingRegs;

	// Set the serial port and slave to the given baudrate.
	SERIAL_PORT.begin(config.baud);
	slave->begin(config.baud);
}

// ---------------------------------------------------------------------
void loop()
// ---------------------------------------------------------------------
{
    // Listen for modbus requests on the serial port.
    // When a request is received it's going to get validated.
    // And if there is a function registered to the received function code, this function will be executed.
    slave->poll();
}


// ---------------------------------------------------------------------
// Modbus handler functions
// ---------------------------------------------------------------------
// The handler functions must return an uint8_t and take the following parameters:
//		uint8_t  fc - function code
//		uint16_t address - first register/coil address
//		uint16_t length/status - length of data / coil status
//		void *context - pointer to instance's context (see issue #97)

// ---------------------------------------------------------------------
// Handle the function codes 
// Force Single Coil (FC=05) and 
// Force Multiple Coils (FC=15)
// ---------------------------------------------------------------------
// Set the corresponding digital output pins (coils).
uint8_t writeDigitalOut(uint8_t fc, uint16_t address, uint16_t length, void *context)
{
	Serial.printf("Force Coil(s) - FC %02X, addr: %04X, len: %d\n", fc, address, length);
 	if(slave->isBroadcast())
			Serial.printf(">> This is a Broadcast\n");
			
    // Check if the requested addresses exist in the array
    if (address > output_pins_size || (address + length) > output_pins_size)
    {
        Serial-printf("ILLEGAL_DATA_ADDRESS\n");
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Set the output pins to the given state.
    for (int i = 0; i < length; i++)
    {
        // Write the value in the input buffer to the digital pin.
        uint8_t coil = slave->readCoilFromBuffer(i);
        Serial.printf("coil: %d, state: %s\n", address + i, coil ? "on" : "off");
        digitalWrite(output_pins[address + i], coil);
        output_pins_state[address + i] = coil;	// Remember last written state
    }
 
	Serial-printf("OK\n");
    return STATUS_OK;
}

// ---------------------------------------------------------------------
// Handle the function code Read Coils (FC=01) 
// ---------------------------------------------------------------------
// This is only a read back of the output pin status
// As ESP32 cannot read back status of output pins with digitalRead()
// we maintain an internal buffer for the last written state.
uint8_t readDigitalOut(uint8_t fc, uint16_t address, uint16_t length, void *context)
{
	Serial.printf("Read Coils - FC %02X, addr: %04X, len: %d\n", fc, address, length);
 		
    // Check if the requested addresses exist in the array
    if (address > output_pins_size || (address + length) > output_pins_size)
    {
        Serial-printf("ILLEGAL_DATA_ADDRESS\n");
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Get the output pins actual state.
    for (int i = 0; i < length; i++)
    {
        // Retrieve state from extra pin state buffer
        slave->writeCoilToBuffer(i, output_pins_state[address + i]);
    }
 
	Serial-printf("OK\n");
    return STATUS_OK;
}

// ---------------------------------------------------------------------
// Handle the function code Read Input Status (FC=02) 
// ---------------------------------------------------------------------
// and write back the values from the digital input pins (discrete input).
uint8_t readDigitalIn(uint8_t fc, uint16_t address, uint16_t length, void *context)
{
	Serial.printf("Read Input Status - FC %02X, addr: %04X, len: %d\n", fc, address, length);
    // Check if the requested addresses exist in the array
    if (address > input_pins_size || (address + length) > input_pins_size)
    {
				Serial-printf("ILLEGAL_DATA_ADDRESS\n");
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Read the digital inputs.
    for (int i = 0; i < length; i++)
    {
        // Write the state of the digital pin to the response buffer.
        slave->writeCoilToBuffer(i, digitalRead(input_pins[address + i]));
    }

		Serial-printf("OK\n");
    return STATUS_OK;
}

// ---------------------------------------------------------------------
// Handle the function code Read Input Registers (FC=04) 
// ---------------------------------------------------------------------
// Write back the values from analog input pins (input registers).
uint8_t readAnalogIn(uint8_t fc, uint16_t address, uint16_t length, void *context)
{
	Serial.printf("Read Input Registers - FC %02X, addr: %04X, len: %d\n", fc, address, length);
		
    // Check if the requested addresses exist in the array
    if (address > adc_pins_size || (address + length) > adc_pins_size)
    {
		Serial-printf("ILLEGAL_DATA_ADDRESS\n");
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Read the analog inputs
    for (int i = 0; i < length; i++)
    {
        // Write the state of the analog pin to the response buffer.
        slave->writeRegisterToBuffer(i, analogRead(adc_pins[address + i]));
    }

	Serial-printf("OK\n");
    return STATUS_OK;
}

// ---------------------------------------------------------------------
// Handle the function code Read Holding Registers (FC=03) 
// ---------------------------------------------------------------------
// Write back either buffered DAC / digital output values or
// EEPROM configuration values.
uint8_t readHoldingRegs(uint8_t fc, uint16_t address, uint16_t length, void *context)
{
	Serial.printf("Read Holding Registers - FC %02X, addr: %04X, len: %d\n", fc, address, length);
	
	// Dispatch address ranges
	if (address >= 0  && (address + length) <= dac_pins_size + NUM_DIO_REG)
	{
		// Holding register range for DAC output pins and digital I/O
		Serial.printf("Read DAC or I/O pins\n");
		for (int i = 0; i < length; i++)
	    {
			Serial.printf("dispatch addr: 0x%04X\n", address + i);
			switch(address + i){
				case 0:		// DAC 0 .. 1
				case 1:	
				// Write the buffered DAC value to the response buffer.
				slave->writeRegisterToBuffer(i, dac_pins_values[address + i]);
				break;
				
				case 2:		// Digial output status
				slave->writeRegisterToBuffer(i, readDigitalOutputsToRegister());
				break;
				
				case 3:		// Digital input
				slave->writeRegisterToBuffer(i, readDiscreteInputsToRegister());
				break;
				
				default:
				Serial-printf("ILLEGAL_DATA_ADDRESS\n");
				return STATUS_ILLEGAL_DATA_ADDRESS;
			}
		}
	}
	else if(address >= CFG_REG_OFFSET  && (address + length) <= (CFG_REG_OFFSET + NUM_CFG_REG)){
		// Holding register range for EEPROM configuration
		Serial.printf("Read EEPROM config\n");
	    // Read the requested configuration registers from EEPROM buffer.
	    for (int i = 0; i < length; i++)
	    {
			uint16_t regval;
		
			switch(address - CFG_REG_OFFSET + i){
				case 0:				// Modbus Slave ID
				regval = config.id;
				break;
				
				case 1:				// Baudrate
				regval = config.baud;
				break;
				
				case 2:				// Power on output pin state
				regval = config.pon_dout_mask;
				break;
				
				case 3:				// Power on DAC0 output value
				regval = config.pon_dacs[0];
				break;
				
				case 4:				// Power on DAC1 output value
				regval = config.pon_dacs[1];
				break;
				
				default:
				Serial-printf("ILLEGAL_DATA_ADDRESS\n");
				return STATUS_ILLEGAL_DATA_ADDRESS;
			}
			// Write the value to the response buffer.
			slave->writeRegisterToBuffer(i, regval);
		}
	}
	else
	{
		Serial-printf("ILLEGAL_DATA_ADDRESS\n");
		return STATUS_ILLEGAL_DATA_ADDRESS;
	}

	Serial-printf("OK\n");
	return STATUS_OK;
}

// ---------------------------------------------------------------------
// Handle the function codes Write Holding Register(s) (FC=06, FC=16) 
// ---------------------------------------------------------------------
// Write data either to DAC output pins or EEPROM configuration.
uint8_t writeHoldingRegs(uint8_t fc, uint16_t address, uint16_t length, void *context)
{
	uint16_t regval;
	
	Serial.printf("Write Holding Registers - FC %02X, addr: %04X, len: %d\n", fc, address, length);
	if(slave->isBroadcast())
			Serial.printf(">> This is a Broadcast\n");
			
	// Dispatch address ranges
	if (address >= 0  && (address + length) <= dac_pins_size + NUM_DIO_REG)
	{
		// Holding register range for DAC output pins and digital output in bankmode
		Serial.printf("Write DAC or I/O pins\n");
		for (int i = 0; i < length; i++)
	    {
			// Get value from buffer
			regval = slave->readRegisterFromBuffer(i);
			
			// Dispatch register address
			switch(address + i){
				
				case 0:			// DAC ouput
				case 1:
				// Value check
				if(regval > 0xff){
					Serial.printf("ILLEGAL_DATA_VALUE\n");
					return STATUS_ILLEGAL_DATA_VALUE;
				}
				// Set DAC output
				dac_pins_values[address+i] = regval;
				dacWrite(dac_pins[address+i], regval);
				break;
				
				case 2:			// Coils in bank mode
				writeCoilsFromRegister(regval);
				break;
				
				case 3:			// Inputs in bank mode, not writeable!!
				Serial.printf("Cannot write to Discrete Inputs register\n");
				
				default:
				Serial-printf("ILLEGAL_DATA_ADDRESS\n");
				return STATUS_ILLEGAL_DATA_ADDRESS;
			}
			
		}
	}
	else if(address >= CFG_REG_OFFSET  && (address + length) <= (CFG_REG_OFFSET + NUM_CFG_REG))
	{
		// Holding register range for EEPROM configuration
		Serial.printf("Write EEPROM config\n");
		
	    // Write data to EEPROM buffer.
	    for (int i = 0; i < length; i++)
	    {
			regval = slave->readRegisterFromBuffer(i);
		
			switch(address - CFG_REG_OFFSET + i){
				case 0:			// Modbus Slave ID
				if(regval < 1 || regval > 247){
					Serial.printf("ILLEGAL_DATA_VALUE\n");
					return STATUS_ILLEGAL_DATA_VALUE;
				}
				config.id = regval & 0xff;
				break;
				
				case 1:			// Baudrate
				switch(regval){
					case 1200:
					case 2400:
					case 4800:
					case 9600:
					case 19200:
					case 38400:
					case 57600: // <-- Max value fitting in uint16_t
					config.baud = regval;
					break;
					
					default:
					Serial.printf("ILLEGAL_DATA_VALUE\n");
					return STATUS_ILLEGAL_DATA_VALUE;
				}
				break;
				
				case 2:		// Power on output pin state
				config.pon_dout_mask = regval;
				break;
				
				case 3:		// Power on DAC0 output value
				config.pon_dacs[0] = regval;
				break;
				
				case 4:		// Power on DAC1 output value
				config.pon_dacs[1] = regval;
				break;
				
				default:
				// Should never occur
				Serial-printf("ILLEGAL_DATA_ADDRESS\n");
				return STATUS_ILLEGAL_DATA_ADDRESS;
			}
		}
	
		// Save to EEPROM, no further effect before next reset
	    writeConfig();
	    Serial-printf("Config data saved, effective only after power cycle / reset\n");
	}
	else
	{
		Serial-printf("ILLEGAL_DATA_ADDRESS\n");
		return STATUS_ILLEGAL_DATA_ADDRESS;
	}

	Serial.printf("OK\n");
    return STATUS_OK;
}

// eof
