# ModbusSlave - Kernel Extension

#### SlaveRTUKernelClass (a facade for the ModbusSlave library)

The basic idea was to provide some core features you may eventually like to have in every custom Modbus slave ..

* Holding Registers backed up in EEPROM
* Set Modbus Interface parameters via Holding Registers
	* Slave ID
	* Baudrate
* Communication Failure Watchdog (optional)
* Restart command



#### Access methods for the Modbus Entity types

* Holding Registers (read/write)
* Coils (read/write)
* Discrete Inputs (read only)
* Input Registers (read only)

A derived application logic class shall overwrite the virtual access method(s) for each required entity.

#### Communication Failure Watchdog 
Allows to implement safety measures in case of communication loss due due broken wires or some 
failure of the Modbus Master MCU (software).

If the feature is enabled by Holding Register #0x000, this register needs to be periodically polled
in the programmed interval.  
Otherwise the Watchdog will trigger an the virtual method  ```cbCommunicationLost()``` will be called once.  
If a poll is received again ```cbCommunicationReestablished()``` will be called once.

The application slave class can make use of the Watchdog by overwriting the above methods.


#### Restart Command  

If the value 0xFFFF was written to Holding Register #0x000, a software restart executed with the next ```poll()```  





-------------------------------------------------------------
*To be continued / completed ...*
