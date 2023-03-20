#include <ESP8266WiFi.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <SD.h>
#include <microDS18B20.h>


#include <GyverNTP.h>
//gmt timezone
GyverNTP ntp(2);

ESP8266HTTPUpdateServer httpUpdate;

ESP8266WebServer server(80); // create a web server on port 80
MicroDS18B20<2> sensor1;

void readLog(){
  String argument ="fileName";
  if (server.hasArg(argument)) {
    Serial.println("Trying to read file "+server.arg(argument));
    File dataFile = SD.open(server.arg(argument), FILE_READ);
    if (dataFile) {
      server.sendHeader("Content-Type", "text/plain");
      server.streamFile(dataFile, "text/plain");
      dataFile.close();
    } else {
      server.send(404, "text/plain", "File not found!");
    }
  }
}

void showSDFiles(){
  String files="[";
  File root = SD.open("/"); // open the root directory of the SD card
  if (root) { // check if the root directory was successfully opened
    Serial.println("Files in root directory:");
    while (true) { // loop through all files in the root directory
      File file = root.openNextFile();
      if (!file) break; // stop looping when all files have been read
      //Serial.println(file.name()); // print the name of the file
      files=files+"\""+file.name()+"\",";
      file.close(); // close the file
    }
    files=files+"]";
    root.close(); // close the root directory
  } else { // print an error message if the root directory could not be opened
    Serial.println("Failed to open root directory");
    server.send(500, "application/json", "Failed to open root directory");
  }
   String message = "{\"Files\": " + files + "}"; // create a message with the value
  server.send(200, "application/json", message); // send the message to the client
}

void readSensor() {
  int value = analogRead(A0); // read the analog value from pin A0
  int temp;
   if (sensor1.readTemp()) temp=(sensor1.getTemp());
    else temp=-255;

  String message = "{\"Analog\": " + String(value) + ",\"temp\": " + String(temp) +"}"; // create a message with the value
  server.send(200, "application/json", message); // send the message to the client
}

void handleRoot() {
  server.send(200, "text/html", "<html><body><h1>Hello World!</h1></body></html>"); // display a simple HTML page
}

void handleResource() {
  String resourcePath = server.uri(); // get the requested resource path
  String contentType = "text/plain"; // set the content type to plain text by default
  Serial.println(resourcePath);


  if (resourcePath.endsWith(".html")) { // check if the resource is an HTML file
    contentType = "text/html"; // set the content type to HTML
  } else if (resourcePath.endsWith(".css")) { // check if the resource is a CSS file
    contentType = "text/css"; // set the content type to CSS
  } else if (resourcePath.endsWith(".js")) { // check if the resource is a JavaScript file
    contentType = "application/javascript"; // set the content type to JavaScript
  }
  
  if (SPIFFS.exists(resourcePath)) { // check if the resource exists in the internal memory
    File file = SPIFFS.open(resourcePath, "r"); // open the resource file in read mode
    server.streamFile(file, contentType); // stream the file to the client with the correct content type
    file.close(); // close the file
  } else { // if the resource does not exist
    server.send(404, "text/plain", "Resource not found"); // send a 404 Not Found status
  }
}



bool logEnabled = false;

const char* ssid = "YourSSID"; // replace with your Wi-Fi network name
const char* password = "YourPassword"; // replace with your Wi-Fi password

void SDinit(){
  if (!SD.begin(4)) { // initialize the SD card on pin 4
    Serial.println("SD card failed to initialize");
    //return;
  }
  else{
    Serial.println("SD card initialized");
    logEnabled = true;
  }
}


unsigned long previousMillis = 0; 
const long interval = 1000;

void setup() {
  pinMode(A0, INPUT); // set pin A0 as an input
  Serial.begin(9600);

  SDinit() ;
  sensor1.setResolution(9);
  sensor1.requestTemp();
  
  WiFi.mode(WIFI_STA); // set ESP8266 in Station mode
  if (WiFi.SSID()) { // check if Wi-Fi credentials are stored
    Serial.println("Stored Wi-Fi credentials found.");
    WiFi.begin(); // connect to Wi-Fi
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 5) { // make few attempts to connect
      delay(1000);
      Serial.println("Attempting to connect to stored Wi-Fi...");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to Wi-Fi!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Failed to connect to stored Wi-Fi.");
      WiFi.disconnect(); // disconnect from Wi-Fi
    }
  } else {
    Serial.println("No stored Wi-Fi credentials found.");
    WiFi.mode(WIFI_AP); // set ESP8266 in Access Point mode
    WiFi.softAP(ssid, password); // start AP
    Serial.print("AP started with SSID: ");
    Serial.println(ssid);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }
  server.on("/", handleRoot); // handle requests to the root URL
  server.on("/data",readSensor);  
  server.on("/SD",showSDFiles);
  server.on("/readLog",readLog);
  server.onNotFound(handleResource); // handle requests to all other URLs

  httpUpdate.setup(&server);
  server.begin(); // start the server
  Serial.println("Web server started");
  
  
  ntp.begin();
  ntp.asyncMode(false);
  ntp.ignorePing(true);
  ntp.tick();
    
  SPIFFS.begin();
}

void writeLog(String logData){
  if(!logEnabled){
    Serial.println("SD card logging is not enabled");
    return;
    }
  String logName = ntp.dateString();
  File dataFile;
  if (SD.exists(logName+".txt")){
    dataFile = SD.open(logName+".txt", FILE_WRITE);
    if(dataFile){
      dataFile.println(logData);
      dataFile.close();    
    }
    else{
      Serial.println("Couldnt open file");
      return;
      }
  }
  else{
    Serial.println("Trying create file " + logName+".txt");
    dataFile = SD.open(logName+".txt", FILE_WRITE);
    if(dataFile){
      Serial.println("File created");
      dataFile.close();    
      }
    
    
    }
}


void loop() {
  server.handleClient(); // handle client requests
  //Serial.println("Temp is : "+String(sensor1.getTempInt()));
   unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    int temp;
   if (sensor1.readTemp()) temp=(sensor1.getTemp());
    else temp=-255;

  Serial.println(ntp.timeString());
  if(ntp. synced()){
    //writeLog(String(analogRead(A0)));
    writeLog("{\"time\":\""+ntp.timeString()+"\",\"value\":"+(String(analogRead(A0)))+",\"temp\":"+String(temp)+"},");
  }
  sensor1.requestTemp();
  }
}
