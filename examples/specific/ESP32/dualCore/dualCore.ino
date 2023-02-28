// dualCore.ino
/*
    Modbus slave, demonstrating multithreading on ESP32.

	 
	This slave has just a single Input register to read back
	a flag maintained in a separate sensor task.
	
	Just to demonstrate that the Modbus Slave is responsive while a
	a parallel task performs blocking sensor code ...
	A big benefit of the ESP32 to solve challenges with some sensors etc.
	
	And best of all, it is quite simple ...
	
	Many credits to Rui Santos for writing this great tuturial:
	==> "How to use ESP32 Dual Core with Arduino IDE"
	see https://randomnerdtutorials.com/esp32-dual-core-arduino-ide/
 
	May also help as a template for new projects.
	
	
    Werner Panocha, February 2023

 

    https://github.com/yaacov/ArduinoModbusSlave
*/

#include <ModbusSlave.h>

SemaphoreHandle_t registerSemaphore;
bool semaphoreTakeFailure = false;

TaskHandle_t ModbusTask;
TaskHandle_t SensorTask;


#define SLAVE_ID 1

// RS485 Modbus Interface 
#define RS485_BAUDRATE 	9600 						// Baudrate for Modbus communication.
#define RS485_SERIAL 	Serial2   					// Serial port for Modbus communication.
#define RS485_CTRL_PIN	MODBUS_CONTROL_PIN_NONE 	// GPIO number for Control pin (optionally)

// Pointer to Modbus slave object
Modbus *slave;

// A set of input registers with sensor data
uint16_t input_regs[] = {0};

// Number of available registers
const uint8_t num_input_regs = sizeof(input_regs) / sizeof(input_regs[0]);

// ---------------------------------------------------------------------
// Handle the function code Read Input Registers (FC=04) 
// ---------------------------------------------------------------------
uint8_t readInputRegs(uint8_t fc, uint16_t address, uint16_t length, void *context)
{
	Serial.printf("Core %d: Modbus read Input Registers (callback)\n", xPortGetCoreID());
	if ((address + length) > num_input_regs)
		return STATUS_ILLEGAL_DATA_ADDRESS;
    
    // Be safe  
    if(semaphoreTakeFailure) {
		// Should never occur, because it's a programmer's error
		Serial.printf("Core %d: >>> Encountered semaphoreTakeFailure flag\n", xPortGetCoreID());
		return STATUS_SLAVE_DEVICE_FAILURE;
	}
    // Need to aquire semaphore to ensure a consistent readout without
    // interference by updates from sensor task!
    // We expect to wait only for a short time (5 ms)
    if(xSemaphoreTake(registerSemaphore, 5 * portTICK_PERIOD_MS)){
		// Holding semaphore
		// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			
		// Move data from register buffer to Modbus send buffer
		for (int i = 0; i < length; i++){
			slave->writeRegisterToBuffer(i, input_regs[address + i]);
		}
	
		// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
		// Release semaphore
		xSemaphoreGive(registerSemaphore);
	}
	else {
		// We could not acquire the semaphore, which is an unexpected error
		Serial.printf("Core %d: >>> Failed to aquire semaphore\n", xPortGetCoreID());
		return STATUS_SLAVE_DEVICE_FAILURE;
	}
	
	
	return STATUS_OK;
}



// ---------------------------------------------------------------------
void setup()
// ---------------------------------------------------------------------
{
	// Use this for debug messages etc.
	Serial.begin(115200);
	Serial.print("\nModbus server - dual core example\n");
	
	// Initialize slave instance
	slave = new Modbus(RS485_SERIAL, SLAVE_ID, RS485_CTRL_PIN);
	
	// Minimalistic slave functionality
	slave->cbVector[CB_READ_INPUT_REGISTERS] 	= readInputRegs;
	
	
	// Set the serial port and slave to the given baudrate.
	RS485_SERIAL.begin(RS485_BAUDRATE);
	slave->begin(RS485_BAUDRATE);

	// Create annd release semaphore for register access
	registerSemaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(registerSemaphore);
	
    // Create a separate task for the Sensor's stuff
	xTaskCreatePinnedToCore(
		SensorFunction, // Function to implement the task
		"Sensor", 		// Name of the task
		10000,  		// Stack size in words 
		input_regs, 	// Task input parameter ==> pointer to registers
		0,  			// Priority of the task 
		&SensorTask,	// Task handle.
		0); 			// Core where the task should run


	// Arduino code runs per default in Core 1
	Serial.printf("Core %d: Modbus task, I will serve Modbus requests\n", xPortGetCoreID());
}

// ---------------------------------------------------------------------
void loop()
// ---------------------------------------------------------------------
{
	// Listen for modbus requests on the serial port.
	// Will trigger registered callback if appropriate message was received
	slave->poll();
}


// The following function is started in other core, so
// it does not interfere with the Modbus Server task
// ---------------------------------------------------------------------
void SensorFunction( void * parameter) 
// ---------------------------------------------------------------------
{
	Serial.printf("Core %d: Sensor task, I will block most of the time ...\n", xPortGetCoreID());
	
	// Assume the parameter is a pointer to our input register array
	uint16_t *inputs = (uint16_t *)parameter;
	
	// Endless loop ...
	while(true){
   
		// Sensor data aquisition code should go here e.g. I2C comm.
		// -------------------------------------------------------------
		
		// Lets have a toggling flag as the sensor's value
		static bool flag = false;
		
		Serial.printf("Core %d: Reading from sensor ...\n", xPortGetCoreID(), flag);
		// This silly simulated sensor data aquisition task is 
		// now 'busy' for a long time ...
		// But the Modbus Slave is still responsive
		delay(10000);	
		
		
		flag = !flag;	// Our new sensor data
		Serial.printf("Core %d: Sensor flag now: %d\n", xPortGetCoreID(), flag);
		
			
		// Make new sensor data available to Modbus Slave task
		//		
		// Need to aquire semaphore to ensure a consistent readout  
	    // of registers from Modbus task!
	    // We expect to wait only for a short time (5 ms)
	    if(xSemaphoreTake(registerSemaphore, 5 * portTICK_PERIOD_MS)){
			
			// Holding semaphore
			// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			
			// This piece of code should run as short as possible !
			// Just move consistent sensor results between memory buffers.
				
			inputs[0] = flag;
			
			// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
			// Release semaphore when done
			xSemaphoreGive(registerSemaphore);
		}
		else {
			// We could not acquire the semaphore, which is an unexpected error
			// Set global flag, so Modbus tasks gets aware of this
			semaphoreTakeFailure = true;
			Serial.printf("Core %d: >>> Failed to aquire semaphore\n", xPortGetCoreID());
		}
	}
  
}
// eof
