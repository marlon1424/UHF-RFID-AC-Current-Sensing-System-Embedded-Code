#include <Arduino.h>
#include <SparkFun_UHF_RFID_Reader.h> //RFID reader Library
#include <WiFiS3.h>
#include <Firebase_ESP_Client.h> //Firebase Library
#include <addons/RTDBHelper.h> //Provide the RTDB payload printing info and other helper functions

//WiFi credentials for home
//#define WIFI_SSID "SHELL-48CCF8"
//#define WIFI_PASS "aabKycW7RWpq"

//WiFi credentials for Jennison
#define WIFI_SSID "EDA-IOT"
#define WIFI_PASS "3aB1J27M"

#define DATABASE_URL "uhf-rfid-d9cd6-default-rtdb.europe-west1.firebasedatabase.app" //Firebase RTDB URL

RFID nano; //Create RFID class object
FirebaseData fbdo; //Firebase data object
FirebaseAuth auth; //Firebase auth object for authentication data
FirebaseConfig config; //Firebase config object for config data

unsigned long dataMillis = 0;
int count = 0;


boolean setupNano(long baudRate); //function prototype



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); //Sets the data rate
  
  WiFi.begin(WIFI_SSID, WIFI_PASS); //Connects Arduino board to WiFi network
  Serial.print("Connecting to WiFi");

//Waits until Arduino is connected to WiFi
  while(WiFi.status() != WL_CONNECTED){
    Serial.println(".");
    delay(300);
  }

  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  config.database_url = DATABASE_URL; //Set the URL in Firebase's configuration
  config.signer.test_mode = true;

  Firebase.reconnectNetwork(true);

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

  //Initialize the library with the Firebase auth and config
  Firebase.begin(&config, &auth);

  while(!Serial); //Wait for serial port to become available

  if(setupNano(115200) == false){ //Configure nano to run at 115200bps
    Serial.println("Module failed to respond.");
    while(1); //Freeze code
  }

  nano.setRegion(REGION_EUROPE); //European UHF frequency range (868MHz) 

  nano.setReadPower(2600); //Set read TX power to 26.00dBm. Max read TX power is 27.00dBm
  
}

void loop() {

  byte tagEPC[12]; //Twelve byte array to store tag's EPC
  byte tagEPCLength; //Stores length of tag EPC

  byte sensorCode[2]; //Two byte array to store sensor code
  byte sensorCodeLength = sizeof(sensorCode); //Determines length of sensor code
  byte responseType;
  uint8_t bank = 0x00; //Reserved memory bank address of RFID tag
  uint8_t address = 0x0B; //Bit address for sensor code in the reserved memory bank
  int sensorCodeOutput = 0; //Sensor code output integer value
  int tagRSSI = 0; //tag RSSI output integer value
  String tagEPCOutput = ""; //String to output tag EPC

  responseType = nano.readData(bank, address, sensorCode, sensorCodeLength); //Gets sensor code from reserved memory bank
  tagRSSI = nano.getTagRSSI(); //Gets tag RSSI

  if(responseType == RESPONSE_SUCCESS){ //Checks if RFID tag has been successfully interrogated
    tagEPCLength = sizeof(tagEPC); //Sets the length of the EPC of the tag that was detected
    nano.readTagEPC(tagEPC, tagEPCLength, 500); //Read the EPC of the tag detected

  //Adds each byte of the tag EPC array to the tag EPC output string
  for(byte x = 0; x < tagEPCLength; x++){
      tagEPCOutput += tagEPC[x];
      tagEPCOutput += " ";
    
  }
    //Prints tag EPC, sensor code length, sensor code and tag RSSI to serial monitor
    Serial.print("Tag EPC: ");
    Serial.println(tagEPCOutput);

    Serial.print("Size[");
    Serial.print(sensorCodeLength);
    Serial.println("]");
    
    sensorCodeOutput = sensorCode[1]; //Sets sensor code output to second byte/element of sensor code array
    Serial.print("Sensor code: ");
    Serial.println(sensorCodeOutput);
    Serial.print("Tag RSSI: ");
    Serial.println(tagRSSI);
    
  } else{
    Serial.println("Tag not found"); //Prints to serial monitor if tag has not been successfully interrogated
  }
  
  //Uploads sensor code, tag RSSI and tag EPC to Firebase Realtime Database every 500ms
  if(millis() - dataMillis > 500){
    dataMillis = millis();
    Firebase.RTDB.setInt(&fbdo, "/RFID/Sensor Code", sensorCodeOutput);
    Firebase.RTDB.setInt(&fbdo, "/RFID/Tag RSSI", tagRSSI);
    Firebase.RTDB.setString(&fbdo, "/RFID/Tag EPC", tagEPCOutput);
  }
}


//Handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.enableDebugging(Serial); //Print the debug statements to the Serial port
  
  nano.begin(Serial1); //Tell the library to communicate over Hardware Serial Port # 1 (pins 0 (RX) and 1 (TX))

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered

  //Hardware serial
  Serial1.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while(!Serial1);

  //About 200ms from power on the module will send its firmware version at 115200
  while (Serial1.available()) Serial1.read();

  nano.getVersion();

  if (nano.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    //This happens if the baud rate is correct but the module is doing a continuous read
    nano.stopReading();

    Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    //The module did not respond so assume it's just been powered on and communicating at 115200bps
    Serial1.begin(115200); //Start serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    Serial1.begin(baudRate); //Start the serial port, this time at user's chosen baud rate

    delay(250);
  }

  //Test the connection
  nano.getVersion();

  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //M6E Nano is up and running
}
