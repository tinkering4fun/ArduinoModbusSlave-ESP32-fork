# ModbusSlave - Kernel Extension

#### SlaveRTUKernelClass (a facade for the ModbusSlave library)

The basic idea was to bundle some core features I like to see in all of my custom Modbus Slaves ...

* Holding Registers backed up in EEPROM
* Configuration of Modbus Interface parameters via Holding Registers
	* Slave ID
	* Baudrate
* Communication Failure Watchdog (optional)
* Restart command

**Holding Registers** managed by this class:

| Addr.     | Function                       |
|-----------|:-------------------------------|  
| 0x0100    | __Slave ID__                   |  
| 0x0101    | __Baud Rate__                  |  
| 0x0102    | __Comm. Watchdog Timeout__ [ms]|  
| 0x0103    | __Restart request__            |  

## A word on Modbus Entity types and Addresses (RTU Mode)

_Just a brief description because I felt some confusion about Modbus addressing._   
_On the other hand there is plenty of good documentation of this pretty old Modbus RTU protocol._

A Modbus Slave is addressed by an 8-bit __Slave ID__ which must be __unique on the RS485 bus__.

The Modbus Protocol knows  __4 different entity types__:

* Holding Registers (read/write)
* Coils (read/write)
* Discrete Inputs (read only)
* Input Registers (read only)

A __Modbus slave may serve any combination and number of entities__ from the above collection.  
_Thus slaves are nowadays also named **Modbus Server** and the master is named **Modbus Client**._  

On Modbus Protocol level an entity address is a 16-bit word.  
Be aware that those __addresses are not to be considered as unique__ on a slave.  
Because __each entity type spans its own 16-bit address range__ starting at 0x0000.  
Thus a slave may present a Coil with address 0x0000 as well as a Holding Register with address 0x0000 etc.

This works because a Modbus Request Message consists of an 8-bit function code (FC).  
This __FC selects the entity type and desired operation__ on the addressed entity item(s).

Now comes the funny / confusing part ...  
Modbus was originally invented in 1979 as extension bus for PLCs.  
In the PLC world's documentation we see a __5-digit decimal addressing convention__, where the __1st digit__ implies the __entity type__.  
_Rem: I guess this concept was choosen to hide Modbus Protocol details from the PLC programmer, allowing to simply read e.g. either register 10001 or 40001,  (without considering the different FCs required to achieve the intended operations).  
This address notation bears also some more semantic information about the addresed target. E.g. that it is not possible to write into 10001 but 40001._

| Addr           | Entity Type (operation)           |  Intended Use                               |
|:---------------|:----------------------------------|:--------------------------------------------|
|00001 - 09999   | 0] Coils (read/write)             | Relays, single Bit Output, Transistor, etc. |
|10001 - 19999   | 1] Discrete Inputs (read only)    | Contacts, Optocoupler, etc.                 |
|30001 - 39999   | 3] Input Registers (read only)    | Sensor values, A/D converter etc.           |
|40001 - 49999   | 4] Holding Registers (read/write) | Persistent configuration parameters etc.    |

Note:  
The __5-digit addresses are 1-based__, meaning each X0001 - X9999 range translates in the 16-bit address range 0x0000 - 0x270E.

Assuming a slave with a Holding Register 0x0005, and you are testing with some __PLC oriented Modbus Master Software__ dealing with 5-digit addresses.  
In this case it may require to specify e.g. address 40006 (5 + 40001) to make the software using the Modbus FC's applicable for Holding Register access and set entity address to 0x0005.  



## Access methods for the Modbus Entity types
A derived application class shall overwrite the applicable virtual entity access method(s).

Refer to example ```kernel/Simple.ino``` for an illustration of the concept.  
The application logic is contained in ```SlaveApplicationClass.cc.h``` 
which appears in a separate Tab of the Arduino IDE.  

_Notice the naming trick for the class file, to get things compiled in Arduino IDE._   





## Communication Failure Watchdog 
Intended to implement safety measures in case of a communication loss due to broken wires or some 
failure of the Modbus Master MCU (software).  

Assume a Modbus Slave used to control a heating or pump etc.. If the connection to the Master MCU is lost, 
one may like to switch off the appliance in order to avoid any damage when running out of control.  
Of course, this won't protect against a crash of the Modbus Slave's firmware. So any critical 
application would require additional measures in hardware. You have been warned ...

The feature is enabled by writing a timeout value other than zero to Holding Register #0x0102.  
Now the register needs to be periodically read within the programmed interval.  
__Fix Me:__ Need to implement a reasonable minimum value check of the watchdog interval (e.g.200 ms).  

Wihout periodic reads, the Watchdog will trigger and the virtual method  **```cbCommunicationLost()```**  will be called once.  
If a read request is received again **```cbCommunicationReestablished()```** will be called once.

The application slave class needs to overwrite these virtual methods to get informed about the events..


## Restart Command  

If the value 0xFFFF was written to Holding Register #0x0103, a software restart executed with the next ```poll()```  



-------------------------------------------------------------

_To be continued / completed ..._     
_Werner Panocha, Februray 2023_  

