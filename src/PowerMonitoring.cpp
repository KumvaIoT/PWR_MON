/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "/Users/abdulhannanmustajab/Desktop/IoT/Power-Monitoring/PowerMonitoring/src/PowerMonitoring.ino"
/*
 * Project PowerMonitoring
 * Description: Remote Monitoring System Kumva 
 * Author: Abdul Hannan Mustajab
 * Date: 1 March 2021
 */

/* 
  Firmware for Kumva Remote Power Monitoring System. Device samples every 10 secs and if there is a change of more than 0.5A, then data will be sent to the cloud. 
  Else it will send data every 10 minutes. It can be adjusted in "wakeBoundary" and "reportingBoundary" variables.

    * Each Boron has 6 sensors connected over Analog. 
      * A5 is Connected to CT1
      * A4 is connected to CT2
      * A3 is connected to CT3
      * A2 is connected to CT4
      * A1 is connected to CT5
      * A0 is connected to CT6
   
    * Particle Functions  
      * Update the constants values.
      * Set Operation Mode
      * Enable or Disable Sensors
      * Enable or Disable 3rd Party Sim
      * Set KeepAlive Value
      * Set reporting duration. (0 hour 20 minutes 0 seconds) -> Enter value in seconds. [20 Minutes => 20*60]
    
    * Particle Variables   
      * Display constants values
      * Display Battery level
      * KeepAlive Values
      * Display Operation Mode

  There are two main operation modes, SINGLE PHASE and MULTI PHASE.
    * SINGLE PHASE
      * Very straightforward. Current across each wire is measured and sent to the cloud. EG: AC, Lights, Iron etc.
      * Current is calculated using emonLib.  

  Constant Values 
    * For 100A | 0.05A and 22ohms resistors -> (100/0.05) / 22 = 90.91
    * For 200A | 0.3A and 20 ohms -> (200/0.3)/20 = 33.3
    * For 400A | 0.33A and 10 ohms -> (400/0.33)/10 = 121
    * For 600A | 
*/

// v1.00  - First release: Changed to emonLibrary, state machine working fine on argon, 10 seconds publish frequency. 
// v1.01  - Fixed error state bug, added waituntill particle connect, added functions for changing constant value and showing them in console.
// v1.02  - Added check in takeMeasurements to send data if sensor is connected. 
// v1.03 -  Fixed pin chart.
// v1.04 - Added a particle function to enable sensors from the console.
// v1.05 - Add particle functions to disable sensors from the console.
// v1.06 - Moved the code to change based reporting. So it will only report if change is detected. Otherwise send every minute.
// v1.07 - Testing Three phase code with Prince's Library.
// v1.08 - Created Particle function to set operation Mode, made changes to takeMeasurements function to check for different operation mode for Three phase load with 3 and 4 Wire.
// v1.09 - Fixed sendEvent bug.
// v1.10 - Fixed minor bugs reported by Chip.
// v1.11 - Added function and variable to manage reporting duration.
// v2.00 - Updated code for boron and stable release.
// v3.00 - Fixed operating mode. 
// v4.00 - Changed from 10BIT to 12BIT operations.
// v5.00 - Test loading sensor constants from the function.
// v6.00 - Changed FRAM Settings. 
// v8.00 - Added get data usage function. 


void setup();
void loop();
void loadSystemDefaults();
void loadConstantDefaults();
void checkSystemValues();
void checkConstantValues();
void watchdogISR();
void petWatchdog();
void keepAliveMessage();
void sendEvent();
void UbidotsHandler(const char *event, const char *data);
void blinkLED(int LED);
void publishStateTransition(void);
void getBatteryContext();
int setThirdPartySim(String command);
int setKeepAlive(String command);
int setConstantOne(String command);
int setConstantTwo(String command);
int setConstantThree(String command);
int setConstantFour(String command);
int setConstantFive(String command);
int setConstantSix(String command);
int setReportingDuration(String command);
int measureNow(String command);
int setVerboseMode(String command);
void updateConstantValues();
int enableSensor(String Sensor);
int disableSensor(String Sensor);
int setOperatingMode(String Sensor);
bool takeMeasurements();
void getBatteryCharge();
void loadEmonlib();
int resetSystem(String Command);
#line 66 "/Users/abdulhannanmustajab/Desktop/IoT/Power-Monitoring/PowerMonitoring/src/PowerMonitoring.ino"
PRODUCT_ID(11734);
PRODUCT_VERSION(9); 

const char releaseNumber[8] = "9.00";                                                      // Displays the release on the menu

// Included Libraries
#include "math.h"
#include "PublishQueueAsyncRK.h"                                                            // Async Particle Publish
#include "MB85RC256V-FRAM-RK.h"                                                             // Rickkas Particle based FRAM Library
#include "MCP79410RK.h"                                                                     // Real Time Clock
#include "EmonLib.h"                                                                        // Include Emon Library
#include "AC_MonitorLib.h"                                                                  // Include Load_Monitoring Library


// Prototypes and System Mode calls
SYSTEM_MODE(AUTOMATIC);                                                                     // This will enable user code to start executing automatically.
SYSTEM_THREAD(ENABLED);                                                                     // Means my code will not be held up by Particle processes.
STARTUP(System.enableFeature(FEATURE_RESET_INFO));
MB85RC64 fram(Wire, 0);                                                                     // Rickkas' FRAM library
MCP79410 rtc;                                                                               // Rickkas MCP79410 libarary
retained uint8_t publishQueueRetainedBuffer[2048];                                          // Create a buffer in FRAM for cached publishes
PublishQueueAsync publishQueue(publishQueueRetainedBuffer, sizeof(publishQueueRetainedBuffer));
Timer keepAliveTimer(1000, keepAliveMessage);
Load_Monitor KUMVA_IO;                                                                       //Create an instance

// Define the memory map - note can be EEPROM or FRAM - moving to FRAM for speed and to avoid memory wear
namespace FRAM {                                                                            // Moved to namespace instead of #define to limit scope
  enum Addresses {
    versionAddr           = 0x00,                                                           // Where we store the memory map version number - 8 Bits
    sysStatusAddr         = 0x01,                                                           // This is the status of the device
    sensorDataAddr        = 0x200,                                                           // Where we store the latest sensor data readings
    sensorConstantsAddr   = 0xA0                                                            // Where we store CT sensor constant values.
   };
};

const int FRAMversionNumber = 22;                                                            // Increment this number each time the memory map is changed

struct systemStatus_structure {                     
  uint8_t structuresVersion;                                                                // Version of the data structures (system and data)
  bool thirdPartySim;                                                                       // If this is set to "true" then the keep alive code will be executed
  int keepAlive;                                                                            // Keep alive value for use with 3rd part SIMs
  uint8_t connectedStatus;
  uint8_t verboseMode;
  uint8_t lowBatteryMode;
  int stateOfCharge;                                                                                // Battery charge level
  uint8_t batteryState;                                                                             // Stores the current battery state
  int resetCount;                                                                                   // reset count of device (0-256)
  unsigned long lastHookResponse;                                                                   // Last time we got a valid Webhook response
  bool sensorOneConnected;                                                                   // Check if sensor One is connected.                                   
  bool sensorTwoConnected;                                                                  // Check if sensor Two is connected.                                   
  bool sensorThreeConnected;;                                                                // Check if sensor Three is connected.                                   
  bool sensorFourConnected;                                                                 // Check if sensor Three is connected.                                   
  bool sensorFiveConnected;                                                                 // Check if sensor Three is connected.                                   
  bool sensorSixConnected;                                                                  // Check if sensor Three is connected.     
  int reportingBoundary;                                                       // 0 hour 20 minutes 0 seconds

  int operatingMode=1;                                                                              // Check the operation mode,  
  /*
    * 1 if single phase mode
    * 2 if three phase mode with 3 wires.
    * 3 if three phase mode with 4 wires.
  */                   
  int Vrms;          // Default voltage value     
  
} sysStatus;

struct sensor_constants{
   float sensorOneConstant;
   float sensorTwoConstant;
   float sensorThreeConstant;
   float sensorFourConstant;
   float sensorFiveConstant;
   float sensorSixConstant;
} sensorConstants;

struct sensor_data_struct {                                                               // Here we define the structure for collecting and storing data from the sensors
  float sensorOneCurrent;
  float sensorTwoCurrent;
  float sensorThreeCurrent;
  float sensorFourCurrent;
  float sensorFiveCurrent;
  float sensorSixCurrent;
  
  float sensorOnePrevious;
  float sensorTwoPrevious;
  float sensorThreePrevious;
  float sensorFourPrevious;
  float sensorFivePrevious;
  float sensorSixPrevious;

  // Three phase sensors with 3 wires. Maximum of two such devices can be connected.

  float I_ThreePhaseLoad_One[3]={0};                                                               // Three phase load with 3 Wires.
  float P_ThreePhaseLoad_One[3]={0};                                                               // Power for three phase load with 3 wires.

  float I_ThreePhaseLoad_Two[3]={0};                                                               // Three phase load with 3 Wires.
  float P_ThreePhaseLoad_Two[3]={0};                                                               // Power for three phase load with 3 wires.

  // Three Phase sensor with 4 wires. Only one such device can be connected.
  
  float Four_ThreePhaseLoad_I[4]={0};                                                               // Three phase load with 4 Wires.
  float Four_ThreePhaseLoad_P[4]={0};                                                               // Power for three phase load with 4 wires.
  
  unsigned long timeStamp;
  int stateOfCharge;
  bool validData;
} sensorData;


// State Machine Variables
enum State { INITIALIZATION_STATE, ERROR_STATE, IDLE_STATE, MEASURING_STATE, REPORTING_STATE, REPORTING_DETERMINATION, RESP_WAIT_STATE};
char stateNames[8][26] = {"Initialize", "Error", "Idle", "Measuring","Reporting","Reporting Determination", "Response Wait"};
State state = INITIALIZATION_STATE;
State oldState = INITIALIZATION_STATE;


// Pin Constants
const int blueLED =   D7;                                                               // This LED is on the Electron itself
const int wakeUpPin = D8;  
const int donePin = D5;

volatile bool watchdogFlag=false;                                                           // Flag to let us know we need to pet the dog

// Timing Variables
const unsigned long webhookWait = 45000;                                                    // How long will we wair for a WebHook response
const unsigned long resetWait   = 300000;                                                   // How long will we wait in ERROR_STATE until reset

unsigned long webhookTimeStamp  = 0;                                                        // Webhooks...
unsigned long resetTimeStamp    = 0;                                                        // Resets - this keeps you from falling into a reset loop
bool dataInFlight = false;

// Variables Related To Particle Mobile Application Reporting
// Simplifies reading values in the Particle Mobile Application
char temperatureString[16];
char humidityString[16];
char batteryContextStr[16];                                                                 // One word that describes whether the device is getting power, charging, discharging or too cold to charge

// Constant Values to be printed in console.
char sensorOneConstantStr[32];                                                                      // String for Sensor One Constant Value
char sensorTwoConstantStr[32];
char sensorThreeConstantStr[32];
char sensorFourConstantStr[32];
char sensorFiveConstantStr[32];
char sensorSixConstantStr[32];
char batteryString[16];

bool sysStatusWriteNeeded = false;                                                       // Keep track of when we need to write     
bool sensorDataWriteNeeded = false; 
bool constantsStatusWriteNeeded = false;
float voltage;                                                                             // Battery voltage Argon
double Vrms=220;                                                                          // Standard Utility Voltage , Can be modified for better accuracy

// Time Period Related Variables
const int wakeBoundary = 0*3600 + 0*60 + 10;                                                // 0 hour 20 minutes 0 seconds

/************************CT LIBRARY RELATED Instances, Pinouts etc.***********************************/

//  Pins assignment/Functions definition-> DON'T CHANGE/MODIFY THESE                               
uint8_t CT1_PIN=A5;
uint8_t CT2_PIN=A4;
uint8_t CT3_PIN=A3;
uint8_t CT4_PIN=A2;
uint8_t CT5_PIN=A1;
uint8_t CT6_PIN=A0;

// Initialize the emon library.

EnergyMonitor emon1,emon2,emon3,emon4,emon5,emon6, emon[6];               // Create an instance

  /*  Examples of 3 phase loads 
    * a load is a three phase when it use 3 or 4 CTs
    * is connected to CTx1 (Ax1), CTx2 (Ax2), CTx3 (Ax3), CTx4 (Ax4)
    * Specifit the CTs used calibration constants
    * Create a struct as shown below: 
  */

  // Three Phase Load with 3 Wires - Load One
  Load_Monitor::CT_Property_Struct ThreePhaseLoadOne[3] = {
    {CT1_PIN,sensorConstants.sensorOneConstant}, // R phase
    {CT2_PIN,sensorConstants.sensorTwoConstant}, // T phase
    {CT3_PIN,sensorConstants.sensorThreeConstant} // S phase 
  };

  // Three Phase Load with 3 Wires - Load Two
  Load_Monitor::CT_Property_Struct ThreePhaseLoadTwo[3]=
  {  
        {CT4_PIN,sensorConstants.sensorFourConstant}, // R phase
        {CT5_PIN,sensorConstants.sensorFiveConstant}, // T phase
        {CT6_PIN,sensorConstants.sensorSixConstant} // S phase 
  };

  // Three Phase Load with 4 Wires - Load One
  Load_Monitor::CT_Property_Struct ThreePhaseLoadFourWires[4]={                                 // 4- wires Three phase Load 
      {CT1_PIN,sensorConstants.sensorOneConstant},                                              // R phase
      {CT2_PIN,sensorConstants.sensorTwoConstant},                                              // S phase
      {CT3_PIN,sensorConstants.sensorThreeConstant},                                            // T phase 
      {CT4_PIN,sensorConstants.sensorFourConstant}                                              // N phase 
   };
   
// setup() runs once, when the device is first turned on.
void setup() {
  pinMode(wakeUpPin,INPUT);                                                                 // This pin is active HIGH, 
  pinMode(donePin,OUTPUT);                                                                  // Allows us to pet the watchdog

  petWatchdog();                                                                            // Pet the watchdog - This will reset the watchdog time period AND 
  attachInterrupt(wakeUpPin, watchdogISR, RISING);                                          // The watchdog timer will signal us and we have to respond


  char StartupMessage[64] = "Startup Successful";                                           // Messages from Initialization
  state = INITIALIZATION_STATE;

  Particle.subscribe(System.deviceID() + "/hook-response/powermonitoring_hook/", UbidotsHandler, MY_DEVICES);

  // Particle Variables for debugging and mobile application
  Particle.variable("Release",releaseNumber);
  Particle.variable("Battery", batteryString);                                              // Battery level in V as the Argon does not have a fuel cell
  Particle.variable("BatteryContext",batteryContextStr);
  Particle.variable("Keep Alive Sec",sysStatus.keepAlive);
  Particle.variable("3rd Party Sim", sysStatus.thirdPartySim);
  
  Particle.variable("Constant One", sensorOneConstantStr);
  Particle.variable("Constant Two", sensorTwoConstantStr);
  Particle.variable("Constant Three", sensorThreeConstantStr);
  Particle.variable("Constant Four", sensorFourConstantStr);
  Particle.variable("Constant Five", sensorFiveConstantStr);
  Particle.variable("Constant Six", sensorSixConstantStr);

  Particle.variable("Reporting Duration",sysStatus.reportingBoundary);
  Particle.variable("Operation Mode",sysStatus.operatingMode);


  Particle.function("Measure-Now",measureNow);
  Particle.function("Verbose-Mode",setVerboseMode);
  Particle.function("Keep Alive",setKeepAlive);
  Particle.function("3rd Party Sim", setThirdPartySim);
  Particle.function("Set Constant One",setConstantOne);
  Particle.function("Set Constant Two",setConstantTwo);
  Particle.function("Set Constant Three",setConstantThree);
  Particle.function("Set Constant Four",setConstantFour);
  Particle.function("Set Constant Five",setConstantFive);
  Particle.function("Set Constant Six",setConstantSix);
  Particle.function("Enable Sensor",enableSensor);                                          // Use this function to enable or disable a sensor from the console.
  Particle.function("Disable Sensor",disableSensor);                                          // Use this function to disable a sensor from the console.
  Particle.function("Operating Mode",setOperatingMode);                                          // Use this function to disable a sensor from the console.
  Particle.function("Reporting Duration(MINUTES)",setReportingDuration);                                   // Set reporting duration from the console, (IN MINUTES.)
  Particle.function("Reboot Device",resetSystem);

  rtc.setup();                                                        // Start the real time clock
  rtc.clearAlarm();                                                   // Ensures alarm is still not set from last cycle

  // Load FRAM and reset variables to their correct values
  fram.begin();                                                                             // Initialize the FRAM module

  byte tempVersion;
  fram.get(FRAM::versionAddr, tempVersion);
  if (tempVersion != FRAMversionNumber) {                                                   // Check to see if the memory map in the sketch matches the data on the chip
    fram.erase();                                                                           // Reset the FRAM to correct the issue
    fram.put(FRAM::versionAddr, FRAMversionNumber);                                         // Put the right value in
    fram.get(FRAM::versionAddr, tempVersion);                                               // See if this worked
    if (tempVersion != FRAMversionNumber) state = ERROR_STATE;                              // Device will not work without FRAM
    else {
      publishQueue.publish("Loading Defaults","Setup Loop",PRIVATE);
      loadSystemDefaults();                                                                 // Out of the box, we need the device to be awake and connected
      loadConstantDefaults();
    }
  }
  else {
    publishQueue.publish("Loading From FRAM","Setup Loop",PRIVATE);
    fram.get(FRAM::sensorConstantsAddr,sensorConstants);
    fram.get(FRAM::sysStatusAddr,sysStatus);                                                // Loads the System Status array from FRAM
  }

  checkConstantValues();
  checkSystemValues();                                                                      // Make sure System values are all in valid range

  loadEmonlib();
  

  if (sysStatus.thirdPartySim) {
    waitFor(Particle.connected,30 * 1000); 
    Particle.keepAlive(sysStatus.keepAlive);                                              // Set the keep alive value
    keepAliveTimer.changePeriod(sysStatus.keepAlive*1000);                                  // Will start the repeating timer
  }

  updateConstantValues();
  takeMeasurements();                                                                       // For the benefit of monitoring the device


  if(sysStatus.verboseMode) publishQueue.publish("Startup",StartupMessage,PRIVATE);                       // Let Particle know how the startup process went

  waitUntil(Particle.connected);

  if (state == INITIALIZATION_STATE) state = IDLE_STATE;                                    // We made it throughgo let's go to idle
  
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  switch(state) {
  case IDLE_STATE:                                                                          // Idle state - brackets only needed if a variable is defined in a state    
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();

    if (!(Time.now() % wakeBoundary)) state = REPORTING_DETERMINATION;                                                     
    
    break;

  case REPORTING_DETERMINATION:
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();
    // Case One:
    if (takeMeasurements()) state = REPORTING_STATE;
    // Case Two:
    else if (!(Time.now() % sysStatus.reportingBoundary)) state = MEASURING_STATE;
    // Go back to IDLE
    else state = IDLE_STATE;
    
    break;

  case MEASURING_STATE:                                                                     // Take measurements prior to sending
    
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();
    takeMeasurements();
    state = REPORTING_STATE;
   
   break;

  case REPORTING_STATE: 
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();               // Reporting - hourly or on command
    if (Particle.connected()) {
      if (Time.hour() == 12) Particle.syncTime();                                           // Set the clock each day at noon
      // takeMeasurements();
      sendEvent();                                                                          // Send data to Ubidots
      state = RESP_WAIT_STATE;                                                              // Wait for Response
    }
    else {
      Particle.connect();
      state = IDLE_STATE;
      resetTimeStamp = millis();
    }
    break;

  case RESP_WAIT_STATE:
    if (sysStatus.verboseMode && state != oldState) publishStateTransition();

    if (!dataInFlight && !(Time.now() % wakeBoundary))                                       // Response received back to IDLE state - make sure we don't allow repetivie reporting events
    {
     state = IDLE_STATE;
    }
    else if (millis() - webhookTimeStamp > webhookWait) {                                   // If it takes too long - will need to reset
      resetTimeStamp = millis();
      publishQueue.publish("spark/device/session/end", "", PRIVATE);                        // If the device times out on the Webhook response, it will ensure a new session is started on next connect
      state = ERROR_STATE;                                                                  // Response timed out
      resetTimeStamp = millis();
    }
    break;

  
  case ERROR_STATE:                                                                         // To be enhanced - where we deal with errors
    if (state != oldState) publishStateTransition();
    if (millis() > resetTimeStamp + resetWait)
    {
      if (Particle.connected()) publishQueue.publish("State","Error State - Reset", PRIVATE); // Brodcast Reset Action
      delay(2000);
      System.reset();
    }
    break;
  }

  rtc.loop();                                                                               // keeps the clock up to date

  if (watchdogFlag) petWatchdog();                                                          // Watchdog flag is raised - time to pet the watchdog

  if (sysStatusWriteNeeded) {
    fram.put(FRAM::sysStatusAddr,sysStatus);
    sysStatusWriteNeeded = false;
  }

  if (sensorDataWriteNeeded) {
    fram.put(FRAM::sensorDataAddr,sensorData);
    sensorDataWriteNeeded = false;
  }

  if (constantsStatusWriteNeeded) {
    fram.put(FRAM::sensorConstantsAddr,sensorConstants);
    constantsStatusWriteNeeded = false;
  }

}


void loadSystemDefaults() {                                                                 // Default settings for the device - connected, not-low power and always on
  if (Particle.connected()) publishQueue.publish("Mode","Loading System Defaults", PRIVATE);
  sysStatus.thirdPartySim = 1;
  sysStatus.keepAlive = 120;
  sysStatus.structuresVersion = 1;
  sysStatus.verboseMode = false;
  sysStatus.lowBatteryMode = false;
  sysStatus.reportingBoundary = 10*60;
  sysStatus.operatingMode = 1;
  sysStatus.sensorOneConnected = 1;
  sysStatus.sensorFiveConnected=1;
  fram.put(FRAM::sysStatusAddr,sysStatus);                                                  // Write it now since this is a big deal and I don't want values over written
}

void loadConstantDefaults(){                                                 // Default settings for sensor constants.
  if (Particle.connected()) publishQueue.publish("Mode","Loading Constant Defaults 90.9", PRIVATE);
  sensorConstants.sensorOneConstant = 90.9;
  sensorConstants.sensorTwoConstant = 90.9;
  sensorConstants.sensorThreeConstant = 90.9;
  sensorConstants.sensorFourConstant = 90.9;
  sensorConstants.sensorFiveConstant = 667;
  sensorConstants.sensorSixConstant = 90.9;
  fram.put(FRAM::sensorConstantsAddr,sensorConstants);
}

void checkSystemValues() {                                                                  // Checks to ensure that all system values are in reasonable range 
  if (sysStatus.connectedStatus < 0 || sysStatus.connectedStatus > 1) {
    if (Particle.connected()) sysStatus.connectedStatus = true;
    else sysStatus.connectedStatus = false;
  }
  if (sysStatus.keepAlive < 0 || sysStatus.keepAlive > 1200) sysStatus.keepAlive = 600;
  if (sysStatus.verboseMode < 0 || sysStatus.verboseMode > 1) sysStatus.verboseMode = true;
  if (sysStatus.lowBatteryMode < 0 || sysStatus.lowBatteryMode > 1) sysStatus.lowBatteryMode = 0;
  if (sysStatus.resetCount < 0 || sysStatus.resetCount > 255) sysStatus.resetCount = 0;
  if (sysStatus.operatingMode<0 || sysStatus.operatingMode>5) sysStatus.operatingMode = 1;
  sysStatusWriteNeeded = true;
}

void checkConstantValues() {                                                                  // Checks to ensure that all system values are in reasonable range 
  if ( sensorConstants.sensorOneConstant < 0.0  || sensorConstants.sensorOneConstant > 3000.0) sensorConstants.sensorOneConstant = 90.91;
  if ( sensorConstants.sensorTwoConstant < 0.0  || sensorConstants.sensorTwoConstant > 3000.0) sensorConstants.sensorTwoConstant = 90.91;
  if ( sensorConstants.sensorThreeConstant < 0.0  || sensorConstants.sensorThreeConstant > 3000.0) sensorConstants.sensorThreeConstant = 90.91;
  if ( sensorConstants.sensorFourConstant < 0.0  || sensorConstants.sensorFourConstant > 3000.0) sensorConstants.sensorFourConstant = 90.91;
  if ( sensorConstants.sensorFiveConstant < 0.0  || sensorConstants.sensorFiveConstant > 3000.0) sensorConstants.sensorFiveConstant = 90.91;
  if ( sensorConstants.sensorSixConstant < 0.0  || sensorConstants.sensorSixConstant > 3000.0) sensorConstants.sensorSixConstant = 90.91;
  fram.put(FRAM::sensorConstantsAddr,sensorConstants);
}


void watchdogISR()
{
  watchdogFlag = true;
}

void petWatchdog()
{
  digitalWrite(donePin, HIGH);                                                              // Pet the watchdog
  digitalWrite(donePin, LOW);
  watchdogFlag = false;
  if (Particle.connected && sysStatus.verboseMode) publishQueue.publish("Watchdog","Petted",PRIVATE);
}

void keepAliveMessage() {
  Particle.publish("*", PRIVATE,NO_ACK);
}

void sendEvent()
{
  char data[512];                 
  if (sysStatus.operatingMode == 1){
    snprintf(data, sizeof(data), "{\"sensorOne\":%4.1f, \"sensorTwo\":%4.1f,  \"sensorThree\":%4.1f,  \"sensorFour\":%4.1f,  \"sensorFive\":%4.1f,\"sensorSix\":%4.1f,\"Mode\":1}", sensorData.sensorOneCurrent,sensorData.sensorTwoCurrent,sensorData.sensorThreeCurrent,sensorData.sensorFourCurrent,sensorData.sensorFiveCurrent,sensorData.sensorSixCurrent);
  } else if (sysStatus.operatingMode == 2){
    snprintf(data, sizeof(data), "{\"SensorOneR\":%4.1f, \"SensorOneS\":%4.1f,  \"SensorOneT\":%4.1f,  \"sensorTwoR\":%4.1f,  \"sensorTwoS\":%4.1f,\"sensorTwoT\":%4.1f,\"Mode\":2}", sensorData.I_ThreePhaseLoad_One[0],sensorData.I_ThreePhaseLoad_One[1],sensorData.I_ThreePhaseLoad_One[2],sensorData.I_ThreePhaseLoad_Two[0],sensorData.I_ThreePhaseLoad_Two[1],sensorData.I_ThreePhaseLoad_Two[2]);
  }else if (sysStatus.operatingMode == 3){
    snprintf(data, sizeof(data), "{\"SensorOneR\":%4.1f, \"SensorOneS\":%4.1f,  \"SensorOneT\":%4.1f,  \"sensorFour\":%4.1f,  \"sensorFive\":%4.1f,\"sensorSix\":%4.1f,\"Mode\":3}", sensorData.I_ThreePhaseLoad_One[0],sensorData.I_ThreePhaseLoad_One[1],sensorData.I_ThreePhaseLoad_One[2],sensorData.sensorFourCurrent,sensorData.sensorFiveCurrent,sensorData.sensorSixCurrent);
  }else if (sysStatus.operatingMode == 4){
    snprintf(data, sizeof(data), "{\"SensorOneR\":%4.1f, \"SensorOneS\":%4.1f,  \"SensorOneT\":%4.1f,  \"SensorOneN\":%4.1f,  \"sensorFive\":%4.1f,\"sensorSix\":%4.1f,\"Mode\":4}", sensorData.Four_ThreePhaseLoad_I[0] ,sensorData.Four_ThreePhaseLoad_I[1],sensorData.Four_ThreePhaseLoad_I[2],sensorData.Four_ThreePhaseLoad_I[3],sensorData.sensorFiveCurrent,sensorData.sensorSixCurrent);
  }
  publishQueue.publish("powermonitoring_hook", data, PRIVATE);
  // Update the previous sensor values.
  sensorData.sensorOnePrevious = sensorData.sensorOneCurrent;
  sensorData.sensorTwoPrevious = sensorData.sensorTwoCurrent;
  sensorData.sensorThreePrevious = sensorData.sensorThreeCurrent;
  sensorData.sensorFourPrevious = sensorData.sensorFourCurrent;
  sensorData.sensorFivePrevious = sensorData.sensorFiveCurrent;
  sensorData.sensorSixPrevious = sensorData.sensorSixCurrent;
  dataInFlight = true;                                                                      // set the data inflight flag
  webhookTimeStamp = millis();
}

void UbidotsHandler(const char *event, const char *data) {            // Looks at the response from Ubidots - Will reset Photon if no successful response
  char responseString[64];
    // Response is only a single number thanks to Template
  if (!strlen(data)) {                                                // No data in response - Error
    snprintf(responseString, sizeof(responseString),"No Data");
  }
  else if (atoi(data) == 200 || atoi(data) == 201) {
    snprintf(responseString, sizeof(responseString),"Response Received");
    sysStatus.lastHookResponse = Time.now();                          // Record the last successful Webhook Response
    sysStatusWriteNeeded = true;
    dataInFlight = false;                                             // Data has been received
  }
  else {
    snprintf(responseString, sizeof(responseString), "Unknown response recevied %i",atoi(data));
  }
  publishQueue.publish("Ubidots Hook", responseString, PRIVATE);
}

// Function to Blink the LED for alerting. 
void blinkLED(int LED)                                                                      // Non-blocking LED flashing routine
{
  const int flashingFrequency = 1000;
  static unsigned long lastStateChange = 0;

  if (millis() - lastStateChange > flashingFrequency) {
    digitalWrite(LED,!digitalRead(LED));
    lastStateChange = millis();
  }
}

// These are the particle functions that allow you to configure and run the device
// They are intended to allow for customization and control during installations
// and to allow for management.


void publishStateTransition(void)
{
  char stateTransitionString[40];
  snprintf(stateTransitionString, sizeof(stateTransitionString), "From %s to %s", stateNames[oldState],stateNames[state]);
  oldState = state;
  if(Particle.connected()) publishQueue.publish("State Transition",stateTransitionString, PRIVATE);
}

void getBatteryContext() 
{
  const char* batteryContext[7] ={"Unknown","Not Charging","Charging","Charged","Discharging","Fault","Diconnected"};
  // Battery conect information - https://docs.particle.io/reference/device-os/firmware/boron/#batterystate-
  // sysStatus.batteryState = System.batteryState();
  snprintf(batteryContextStr, sizeof(batteryContextStr),"%s", batteryContext[sysStatus.batteryState]);
  sysStatusWriteNeeded = true;
}

/*
  * These functions change the values of the constant. 
*/

// This section contains all the Particle Functions which can be operated in the console. 

int setThirdPartySim(String command) // Function to force sending data in current hour
{
  if (command == "1")
  {
    sysStatus.thirdPartySim = true;
    Particle.keepAlive(sysStatus.keepAlive);                                                // Set the keep alive value
    keepAliveTimer.changePeriod(sysStatus.keepAlive*1000);                                  // Will start the repeating timer
    if (Particle.connected()) publishQueue.publish("Mode","Set to 3rd Party Sim", PRIVATE);
    sysStatusWriteNeeded = true;
    return 1;
  }
  else if (command == "0")
  {
    sysStatus.thirdPartySim = false;
    if (Particle.connected()) publishQueue.publish("Mode","Set to Particle Sim", PRIVATE);
    sysStatusWriteNeeded = true;
    return 1;
  }
  else return 0;
}

int setKeepAlive(String command)
{
  char * pEND;
  char data[256];
  int tempTime = strtol(command,&pEND,10);                                                  // Looks for the first integer and interprets it
  if ((tempTime < 0) || (tempTime > 1200)) return 0;                                        // Make sure it falls in a valid range or send a "fail" result
  sysStatus.keepAlive = tempTime;
  Particle.keepAlive(sysStatus.keepAlive);                                                // Set the keep alive value
  keepAliveTimer.changePeriod(sysStatus.keepAlive*1000);                                  // Will start the repeating timer
  snprintf(data, sizeof(data), "Keep Alive set to %i sec",sysStatus.keepAlive);
  publishQueue.publish("Keep Alive",data, PRIVATE);
  sysStatusWriteNeeded = true;                                                           // Need to store to FRAM back in the main loop
  return 1;
}


int setConstantOne(String command){
  sensorConstants.sensorOneConstant = command.toFloat();
  publishQueue.publish("Constant One Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantTwo(String command){
  sensorConstants.sensorTwoConstant = command.toFloat();
  publishQueue.publish("Constant Two Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantThree(String command){
  sensorConstants.sensorThreeConstant = command.toFloat();
  publishQueue.publish("Constant Three Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantFour(String command){
  sensorConstants.sensorFourConstant = command.toFloat();
  publishQueue.publish("Constant Four Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantFive(String command){
  sensorConstants.sensorFiveConstant = command.toFloat();
  publishQueue.publish("Constant Five Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

int setConstantSix(String command){
  sensorConstants.sensorSixConstant = command.toFloat();
  publishQueue.publish("Constant Six Value set to ",String(command),PRIVATE);
  updateConstantValues();
  return 1;
}

// This function is used to update the reportingDuration. 
int setReportingDuration(String command){
  sysStatus.reportingBoundary = command.toFloat();
  publishQueue.publish("Reporting Time Set to %s Minutes.",String(command),PRIVATE);
  sysStatusWriteNeeded = true;
  return 1;
}

int measureNow(String command) // Function to force sending data in current hour
{
  if (command == "1") {
    state = MEASURING_STATE;
    return 1;
  }
  else return 0;
}

int setVerboseMode(String command) // Function to force sending data in current hour
{
  if (command == "1")
  {
    sysStatus.verboseMode = true;
    publishQueue.publish("Mode","Set Verbose Mode",PRIVATE);
    sysStatusWriteNeeded = true;
    return 1;
  }
  else if (command == "0")
  {
    sysStatus.verboseMode = false;
    publishQueue.publish("Mode","Cleared Verbose Mode",PRIVATE);
    sysStatusWriteNeeded = true;
    return 1;
  }
  else return 0;
}


void updateConstantValues()
{   
    snprintf(sensorOneConstantStr,sizeof(sensorOneConstantStr),"CT One: %3.1f", sensorConstants.sensorOneConstant);
    snprintf(sensorTwoConstantStr,sizeof(sensorTwoConstantStr),"CT Two %3.1f", sensorConstants.sensorTwoConstant);
    snprintf(sensorThreeConstantStr,sizeof(sensorThreeConstantStr),"CT Three: %3.1f", sensorConstants.sensorThreeConstant);
    snprintf(sensorFourConstantStr,sizeof(sensorFourConstantStr),"Sensor Four Constant : %3.1f", sensorConstants.sensorFourConstant);
    snprintf(sensorFiveConstantStr,sizeof(sensorFiveConstantStr),"Sensor Five Constant : %3.1f", sensorConstants.sensorFiveConstant);
    snprintf(sensorSixConstantStr,sizeof(sensorSixConstantStr),"Sensor Six Constant : %3.1f", sensorConstants.sensorSixConstant);
    loadEmonlib();
    constantsStatusWriteNeeded = true;                                                         // This function is called when there is a change so, we need to update the FRAM
} 

/* 
  EnableSensor (String Sensor)
  This function takes in the sensor number as integer and enables or disable the sensor accordingly.
*/
int enableSensor(String Sensor){
  char * pEND;
  char data[256];
  int tempSensor = strtol(Sensor,&pEND,10);                                                  // Looks for the first integer and interprets it
  if ((tempSensor < 1) || (tempSensor >7) ) return 0;
 
  if (tempSensor == 1){
    sysStatus.sensorOneConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor One");
    sysStatusWriteNeeded = true;  
    return 1;     
  } else if (tempSensor == 2){
    sysStatus.sensorTwoConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Two");
    sysStatusWriteNeeded = true;   
    return 1;    
  }
  else if (tempSensor == 3){
    sysStatus.sensorThreeConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Three");
    sysStatusWriteNeeded = true;       
    return 1;
  }
  else if (tempSensor == 4){
    sysStatus.sensorFourConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Four");
    sysStatusWriteNeeded = true;     
    return 1;  
  }
  else if (tempSensor == 5){
    sysStatus.sensorFiveConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Five");
    sysStatusWriteNeeded = true;    
    return 1;   
  }
  else if (tempSensor == 6){
    sysStatus.sensorSixConnected = true;
    snprintf(data, sizeof(data), "Enabled Sensor Six");
    sysStatusWriteNeeded = true; 
    return 1;      
  }
}

/* 
  EnableSensor (String Sensor)
  This function takes in the sensor number as integer and enables or disable the sensor accordingly.
*/
int disableSensor(String Sensor){
  char * pEND;
  char data[256];
  int tempSensor = strtol(Sensor,&pEND,10);                                                  // Looks for the first integer and interprets it
  if ((tempSensor < 1) || (tempSensor >7) ) return 0;
 
  if (tempSensor == 1){
    sysStatus.sensorOneConnected = false;
    snprintf(data, sizeof(data), "Disabled Sensor One");
    publishQueue.publish("Sensor Status",data,PRIVATE);
    sysStatusWriteNeeded = true;  
    return 1;     
  } else if (tempSensor == 2){
    sysStatus.sensorTwoConnected = false;
    snprintf(data, sizeof(data), "Disabled Sensor Two");
    publishQueue.publish("Sensor Status",data,PRIVATE);
    sysStatusWriteNeeded = true;   
    return 1;    
  }
  else if (tempSensor == 3){
    sysStatus.sensorThreeConnected = false;
    snprintf(data, sizeof(data), "Disabled Sensor Three");
    publishQueue.publish("Sensor Status",data,PRIVATE);
    sysStatusWriteNeeded = true;       
    return 1;
  }
  else if (tempSensor == 4){
    sysStatus.sensorFourConnected = false;
    snprintf(data, sizeof(data), "Disabled Sensor Four");
    publishQueue.publish("Sensor Status",data,PRIVATE);
    sysStatusWriteNeeded = true;     
    return 1;  
  }
  else if (tempSensor == 5){
    sysStatus.sensorFiveConnected = false;
    snprintf(data, sizeof(data), "Disabled Sensor Five");
    publishQueue.publish("Sensor Status",data,PRIVATE);
    sysStatusWriteNeeded = true;    
    return 1;   
  }
  else if (tempSensor == 6){
    sysStatus.sensorSixConnected = false;
    snprintf(data, sizeof(data), "Disabled Sensor Six");
    publishQueue.publish("Sensor Status",data,PRIVATE);
    sysStatusWriteNeeded = true; 
    return 1;      
  }
}

int setOperatingMode(String Sensor){
  char * pEND;
  char data[256];
  int tempSensor = strtol(Sensor,&pEND,10);                                                  // Looks for the first integer and interprets it
  if ((tempSensor < 0) || (tempSensor >4) ) return 0;
  else{
    sysStatus.operatingMode = tempSensor;
    snprintf(data, sizeof(data), "Operation Mode %i",tempSensor);
    publishQueue.publish("Mode",data,PRIVATE);
    sysStatusWriteNeeded = true;  
    return 1; 
  }
}

//---------------------------------------------------------------------------------------------Three Phase

// Wires=3 for 3-Wires: R,S,T
// Wires=4 for 4 wires: R,S,T and N

void Three_Phase_Monitor(uint8_t Wires,Load_Monitor::CT_Property_Struct Load_Name[],float *Current_rms_per_Phase,float *Power_rms_per_Phase){
   uint8_t p=0;
   p=Wires;
   float i_rms_per_Phase[p]={0};
   
  for (uint8_t i=0;i<p;i++){

   i_rms_per_Phase[i]=KUMVA_IO.calcIrms(Load_Name[i]);
  
  Current_rms_per_Phase[i]=i_rms_per_Phase[i];
  Power_rms_per_Phase[i]=((i_rms_per_Phase[i]*Vrms)/1000); //in kW
    
  }
  
}


/*
void Three_Phase_Monitor(uint8_t Wires,float *Current_rms_per_Phase,float *Power_rms_per_Phase){
   uint8_t p=0;
   p=Wires;
   float i_rms_per_Phase[p]={0};
   
  for (uint8_t i=0;i<p;i++){
  i_rms_per_Phase[i]=emon[i].calcIrms(1480);
  
  Current_rms_per_Phase[i]=i_rms_per_Phase[i];
  Power_rms_per_Phase[i]=((i_rms_per_Phase[i]*Vrms)/1000); //in kW
    
  }
  
}
*/



// These are the functions that are part of the takeMeasurements call

bool takeMeasurements() 
{
    sensorData.validData = false;
    getBatteryContext();     
    
    // If operatingMode is '1'. All single phase
    if ((sysStatus.operatingMode) == 1){
      if (sysStatus.sensorOneConnected) sensorData.sensorOneCurrent =   emon1.calcIrms(1480);
      if (sysStatus.sensorTwoConnected) sensorData.sensorTwoCurrent =   emon2.calcIrms(1480);
      if (sysStatus.sensorThreeConnected) sensorData.sensorThreeCurrent=  emon3.calcIrms(1480);
      if (sysStatus.sensorFourConnected) sensorData.sensorFourCurrent =  emon4.calcIrms(1480);
      if (sysStatus.sensorFiveConnected) sensorData.sensorFiveCurrent =  emon5.calcIrms(1480);               
      if (sysStatus.sensorSixConnected) sensorData.sensorSixCurrent =   emon6.calcIrms(1480);
    } 
    // OperatingMode 2 means all 6 sensors enabled with Three phase mode. CT1 to CT3 Load A and CT4 to CT6 load B.
    else if ((sysStatus.operatingMode) == 2){
      // Load One
      Three_Phase_Monitor(3,ThreePhaseLoadOne,sensorData.I_ThreePhaseLoad_One,sensorData.P_ThreePhaseLoad_One);
      // Load Two 
      Three_Phase_Monitor(3,ThreePhaseLoadTwo,sensorData.I_ThreePhaseLoad_Two,sensorData.P_ThreePhaseLoad_Two);
    }
    // In operation mode 3 of the three wire load, CT1 to CT 3 are load A and CT4 to CT6 are available for single phase operation.
    else if (sysStatus.operatingMode == 3){
      // Load for three phase with 3 Wires.
      Three_Phase_Monitor(3,ThreePhaseLoadOne,sensorData.I_ThreePhaseLoad_One,sensorData.P_ThreePhaseLoad_One);
      
      // CT4 to CT6 are available for single phase operation.
      if (sysStatus.sensorFourConnected) sensorData.sensorFourCurrent =  emon4.calcIrms(1480); 
      else sensorData.sensorFourCurrent=0;
      if (sysStatus.sensorFiveConnected) sensorData.sensorFiveCurrent =  emon5.calcIrms(1480);    
      else sensorData.sensorFiveCurrent=0;           
      if (sysStatus.sensorSixConnected) sensorData.sensorSixCurrent =   emon6.calcIrms(1480);  
      else sensorData.sensorSixCurrent=0;
    }
    // In operation mode 4, The load is of 4 wires. CT1 to CT 4 are load A and CT5 to CT6 are available for single phase operation.
    else if (sysStatus.operatingMode == 4){
      // Load for three phase with 3 Wires.
      Three_Phase_Monitor(4,ThreePhaseLoadFourWires,sensorData.Four_ThreePhaseLoad_I,sensorData.Four_ThreePhaseLoad_P);
      
      // CT5 & CT6 are available for single phase operation.
      if (sysStatus.sensorFiveConnected) sensorData.sensorFiveCurrent =  emon5.calcIrms(1480);       
      else sensorData.sensorFiveCurrent=0;                  
      if (sysStatus.sensorSixConnected) sensorData.sensorSixCurrent =   emon6.calcIrms(1480);  
      else sensorData.sensorSixCurrent=0;
    }

    sensorDataWriteNeeded = true;

    if (((abs(sensorData.sensorOneCurrent)-(sensorData.sensorOnePrevious)) >= 1.5) || ((abs(sensorData.sensorTwoCurrent)-(sensorData.sensorTwoPrevious)) >= 1.5) || ((abs(sensorData.sensorThreeCurrent)-(sensorData.sensorThreePrevious)) >= 1.5) || ((abs(sensorData.sensorFourCurrent)-(sensorData.sensorFourPrevious)) >= 1.5) || ((abs(sensorData.sensorFiveCurrent)-(sensorData.sensorFivePrevious)) >= 1.5) || ((abs(sensorData.sensorSixCurrent)-(sensorData.sensorSixPrevious)) >= 1.5)) {
      // Indicate that this is a valid data array and store it
      sensorData.validData = true;
      sensorData.timeStamp = Time.now();
      sensorDataWriteNeeded = true;
      return 1;
      } else return 0;
  }


  void getBatteryCharge()
{
  // voltage = analogRead(BATT) * 0.0011224;
  // snprintf(batteryString, sizeof(batteryString), "%3.1f V", voltage);
}

void loadEmonlib(){
 
  emon1.current(CT1_PIN,sensorConstants.sensorOneConstant);
  emon2.current(CT2_PIN,sensorConstants.sensorTwoConstant);
  emon3.current(CT3_PIN,sensorConstants.sensorThreeConstant);
  emon4.current(CT4_PIN,sensorConstants.sensorFourConstant);
  emon5.current(CT5_PIN,sensorConstants.sensorFiveConstant);
  emon6.current(CT6_PIN,sensorConstants.sensorSixConstant);

  emon[0].current(CT1_PIN,sensorConstants.sensorOneConstant);
  emon[1].current(CT2_PIN,sensorConstants.sensorTwoConstant);
  emon[2].current(CT3_PIN,sensorConstants.sensorThreeConstant);
  emon[3].current(CT4_PIN,sensorConstants.sensorFourConstant);
  emon[4].current(CT5_PIN,sensorConstants.sensorFiveConstant);
  emon[5].current(CT6_PIN,sensorConstants.sensorSixConstant);
  constantsStatusWriteNeeded = true;

}

int resetSystem(String Command){
  char * pEND;
  int command = strtol(Command,&pEND,10);                                                  // Looks for the first integer and interprets it
  if (command == 1) {
    
    publishQueue.publish("Reset","Device Reset Success",PRIVATE);
    delay(5000);
    System.reset();
    return 1;
    }
  else return 0;  
}