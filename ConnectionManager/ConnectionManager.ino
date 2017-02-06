#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "./DNSServer.h"
#include <ESP8266WebServer.h>

//define the dns port and create an ip address
const byte DNS_PORT = 53;
IPAddress apIP(10, 10, 10, 1);

//define variables for ssid and password
String ssid = "";
String password = "";

//define variables to save if web and dns server are enabled or not
boolean webServerEnabled = false;
boolean dnsServerEnabled = false;

//define a variable to save the html with the scanned networks
String configPageHtml;

//define a variable to know if we´re connected to the wifi
boolean connected = false;

//start an web server on port 80 and an dns server on the specified port
ESP8266WebServer webServer(80);
DNSServer dnsServer;


//these functions need to be called by the user
boolean begin() {
  //this will be the constructor
  
  //begin a serial connection to debug the sketch
  Serial.begin(115200);

  //begin an eeprom connection
  EEPROM.begin(512);
  delay(10);

  //try to connect
  if (connect()) {
    return true;
  } else {
    //wait until @connected is true
    while (!connected) {
      //do nothing
    }
    return true;
  }
}

void looper() {
  if (dnsServerEnabled) dnsServer.processNextRequest();
  if (webServerEnabled) webServer.handleClient();
}


//these functions are only for internal use
boolean connect() {
  Serial.println("connecting... ");
  
  //read ssid and password
  readSsidAndPassword();

  //try to connect
  if (wifiConnected()) {
    //stop the servers
    disableDnsServer();
    disableWebServer();

    //write the ip address
    Serial.println(WiFi.localIP());

    //set @connected to true
    connected = true;
    
    return true;
  } else {
    //create own ap with captive portal
    createOwnAP();

    return false;
  }
}

boolean wifiConnected() {
  Serial.print("trying to connect... ");
  //try to connect to the network @ssid by using password @password
  //the timeout for the connection is 15 seconds (30 delays of 500ms). after that time, return false

  //turn on the sta mode (no ap)
  WiFi.mode(WIFI_STA);

  //try to connect
  WiFi.begin(ssid.c_str(), password.c_str());

  //create a counter and wait until it connects or the timeout is reached
  int counter = 0;

  while (WiFi.status() != WL_CONNECTED) {
    //send a short wait message

    //increase counter and check, if its higher than 30, timeout
    counter++;
    if (counter > 30) {
      //timeout -> return false
      return false;
      Serial.println("failure -> done");
    }

    //delay 500ms
    delay(500);
  }

  //connected successfull
  Serial.println("successfull -> done");
  return true;
}

void createOwnAP() {
  Serial.print("Creating own AccessPoint... ");
  //generate config page html
  configPageHtml = generateConfigPage();
  
  //go to ap mode
  WiFi.mode(WIFI_AP);

  //configure the ap
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255 ,0));
  WiFi.softAP("HomeControl-Client Setup", "");

  //enable dns and wev servers
  enableDnsServer();
  enableWebServer();

  //show the config page for each site
  webServer.onNotFound(sendConfigPortal);
  //enable a system to save the sent ssid and password
  webServer.on("/save", saveSentSsidAndPassword);
  
  Serial.println("AP done");
}

String generateConfigPage() {
  Serial.print("Generating config page... ");
  //scan the wifi networks and create the config page

  //go to sta mode and disconnect all devices
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  //create var for the html
  String html = "<!DOCTYPE html><html><head><title>SmartHome-Client Setup</title><meta charset='utf-8'></head><body style='font-size: 45px'><div id='container' style='width: 90%; margin-left: 5%'><h1>SmartHome-Client Setup</h1><p style='text-align: justify'>In this setup screen you´re able to connect the sensor or acteur with your WiFi network.<br>You can also see the ID number of this client so it´s easier for you to select this single client on the SmartHome-ControlServer.</p><hr><h2>WiFi-Networks</h2><form action='/save' method='get'><p>Please select your WiFi network here:<br>";

  //scan networks
  int n = WiFi.scanNetworks();

  //walk trough all networks and create an entry in the html
  for (int i = 0; i < n; i++) {
    html += "<input type='radio' id='network-" + String(i) + "' name='ssid' value='" + WiFi.SSID(i) + "'><label for='network-" + String(i) + "'> " + WiFi.SSID(i) + "</label><br>";
  }

  //append html
  html += "</p><p>Please type in your WiFi password here:<br><input type='text' name='password' placeholder='Password' style='width: 90%; font-size: 45px; margin-top:10px'></p><p><button type='submit' style='width: 50%; font-size: 45px; margin-left: 20%'>Save</button></p></form><hr><h2>ID-Number</h2><p>The ID-Number of this client is <b>";

  //give ANYTHING address as id
  html += "id";

  //append html
  html += "</b>. This number will be showen on the ControlServer to identify this client. You can set an individual name there.</p></div></body></html>";

  //return the html
  Serial.println("done");
  return html;
}



void readSsidAndPassword() {
  Serial.print("Reading SSID and password... ");
  //read ssid and password from the EEPROM into the variables
  //INFO: max length of ssid is 32 byte, password is 64 bytes

  //read ssid
  ssid = "";
  for (int i = 0; i < 32; i++) {
    ssid += char(EEPROM.read(i));
  }

  //read password
  password = "";
  for (int i = 32; i < 96; i++) {
    password += char(EEPROM.read(i));
  }
  Serial.println("done");
}

void writeSsidAndPassword(String newSsid, String newPassword) {
  Serial.print("Writing SSID and password... ");
  //write ssid and password from the paramaters to the EEPROM

  //clear the eeprom
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  //write ssid
  for (int i = 0; i < newSsid.length(); i++) {
    EEPROM.write(i, newSsid[i]);
  }
  EEPROM.commit();

  //write password
  for (int i = 0; i < newPassword.length(); i++) {
    EEPROM.write(i + 32, newPassword[i]);
  }
  EEPROM.commit();
  Serial.println("done");
}

void saveSentSsidAndPassword() {
  Serial.print("save sent SSID and password... ");

  //read ssid and password from the url as arguments
  String newSsid = "";
  String newPassword = "";
  if (webServer.hasArg("ssid")) {
    newSsid = webServer.arg("ssid");
  }
  if (webServer.hasArg("password")) {
    newPassword = webServer.arg("password");
  }

  Serial.println("done");

  //save ssid and password
  writeSsidAndPassword(newSsid, newPassword);
  
  Serial.print("New SSID: ");
  Serial.println(newSsid);
  Serial.print("New Password: ");
  Serial.println(newPassword);

  //disable web and dns server
  disableWebServer();
  disableDnsServer();

  //restart connecting
  connect();
}

void sendConfigPortal() {
  //listener which creates and sends the configuration page
  Serial.print("Sending config portal... ");
  webServer.send(200, "text/html", configPageHtml);
  Serial.println("done");
}



void enableWebServer() {
  Serial.print("Enable web server... ");
  //enable the web server
  if (!webServerEnabled) webServer.begin();

  //set the var
  webServerEnabled = true;
  Serial.println("done");
}
void disableWebServer() {
  Serial.print("Disbale web server... ");
  //disable the web server
  if (webServerEnabled) webServer.stop();

  //set the var
  webServerEnabled = false;
  Serial.println("done");
}
void enableDnsServer() {
  Serial.print("Enable dns server... ");
  //enable the dns server
  //configure dns server to redirect all (wildcard *) request to the own web server (apIP)
  if (!dnsServerEnabled) dnsServer.start(DNS_PORT, "*", apIP);

  //set the var
  dnsServerEnabled = true;
  Serial.println("done");
}
void disableDnsServer() {
  Serial.print("Disable dns server... ");
  //disable the web server
  if (dnsServerEnabled) dnsServer.stop();

  //set the var
  webServerEnabled = false;
  Serial.println("done");
}
