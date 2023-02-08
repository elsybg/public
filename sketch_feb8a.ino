#include <WiFi.h>
#include <WiFiClientSecure.h>  //#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <HTTPClient.h>
#include "esp_system.h"
#include <NTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const char* host = "esp32";
const char* ssid = "ivailo_wifi";
const char* password = "1234";

const String clientName = "cl1";
const String client_key = "1234";


#define RXD2 16
#define TXD2 17
#define TX_EN 18
#define ledPin 2
#define MAX_MILLIS_TO_WAIT 1000  // ms wait for answer
#define time_HTTPRequest 60000
#define send_error_reset_treshold 15
#define time_for_time_update 250
#define time_for_Entry_In_Array 5000
#define arrayDataLen 500
#define restart_millis_threshold 3600000
#define TEMP_UPDATE_INTERVAL 10000  //ms for update temperature

unsigned long starttime;
unsigned long min_last_request;    //minuta v koqto sa izprateni dannite
unsigned long have_valid_MB_data;  //1-ima validni danni ot power meter
unsigned long lastMillisTimeUpdate;
unsigned long lastTemperatureUpdate;
unsigned long lastEntryInArray;
byte RFin_bytes[255];
bool sendMBPacket;
volatile int count;
volatile int retry_count;
volatile long send_error_count;
volatile long time_update_mills;
String http_Post_Response;

WiFiClientSecure client;

const int oneWireBus = 4;     // GPIO where the DS18B20 is connected to
OneWire oneWire(oneWireBus);  // on pin 4 (a 4.7K resistor is necessary)

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// variable to hold device addresses
DeviceAddress Thermometer;

float temperature1;
float temperature2;
float temperature3;
float temperature4;
float temperature5;

int deviceCount = 0;

uint8_t sensor1[8] = { 0x28, 0x76, 0x7D, 0x3F, 0x0D, 0x00, 0x00, 0x77 };
uint8_t sensor2[8] = { 0x28, 0x9E, 0x0C, 0x48, 0xF6, 0x73, 0x3C, 0xC3 };
uint8_t sensor3[8] = { 0x28, 0x3E, 0x60, 0x48, 0xF6, 0x97, 0x3C, 0xE1 };
uint8_t sensor4[8] = { 0x28, 0xAD, 0xCB, 0x48, 0xF6, 0x9A, 0x3C, 0xC3 };
uint8_t sensor5[8] = { 0x28, 0x23, 0x18, 0x81, 0xE3, 0x23, 0x3C, 0x4F };

//Power meter values
float volt;
float ampere;
float power;
float kwh;
float f;
float pf;

int arPower[arrayDataLen];

volatile int received_CRC;
volatile int calculated_CRC;



WebServer server(80);




WiFiUDP ntpUDP;  //for ntp client
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() )
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, 60000);

String localTime;
struct RTC {
  uint16_t sec;
  uint16_t min;
  uint16_t hour;
  uint16_t dow;
  uint16_t day;
  uint16_t month;
  uint16_t year;
  uint16_t doy;  // not BCD!
};

RTC rtc;

/*
   Index page
*/
char* indexPage =
  "Index Page";

/*
   Login page
*/

const char* loginIndex =
  "Hello World!"
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>ESP32 Login Page</b></font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<br>"
  "<br>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form)"
  "{"
  "if(form.userid.value=='admin' && form.pwd.value=='admin')"
  "{"
  "window.open('/serverIndex')"
  "}"
  "else"
  "{"
  " alert('Error Password or Username')/*displays error message*/"
  "}"
  "}"
  "</script>";

/*
   Serer Restart Page
*/
const char* serverRestart =
  "Server is restarting...";

/*
   Server Index Page
*/

const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<p>"
  "Power Meter Control"
  "</p>"
  "<p>"
  "Select compiled binary file (*.bin) for firmware update"
  "</p>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' size='50' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";

String SendHTML() {
  count += 1;
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  // ptr += "<meta http-equiv=""refresh"" content=""10"" >";
  ptr += "<title>Power Meter Control</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += "table, th, td {";
  ptr += "border: 1px solid black;";
  ptr += "border-collapse: collapse;";
  ptr += "}";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Power meter live data (" + String(count) + ")</h1>\n";
  ptr += "<p>";
  ptr += "<table style=\"border:1px solid black;margin-left:auto;margin-right:auto;\"> ";
  ptr += html_table_row("Data", "Value", "Units");
  ptr += html_table_row("Voltage", String(volt, 1), "V");
  ptr += html_table_row("Current", String(ampere, 3), "A");
  ptr += html_table_row("Power", String(power, 1), "W");
  ptr += html_table_row("Energy", String(kwh, 1), "kWh");
  ptr += html_table_row("Frequency", String(f, 1), "Hz");
  ptr += html_table_row("Power factor", String(pf, 2), " ");
  ptr += html_table_row("Temperature 1 (ROM " + str_printAddress(sensor1) + ")", String(temperature1, 1), "&#8451");
  ptr += html_table_row("Temperature 2 (ROM " + str_printAddress(sensor2) + ")", String(temperature2, 1), "&#8451");
  ptr += html_table_row("Temperature 3 (ROM " + str_printAddress(sensor3) + ")", String(temperature3, 1), "&#8451");
  ptr += html_table_row("Temperature 4 (ROM " + str_printAddress(sensor4) + ")", String(temperature4, 1), "&#8451");
  ptr += html_table_row("Temperature 5 (ROM " + str_printAddress(sensor5) + ")", String(temperature5, 1), "&#8451");
  ptr += html_table_row("Millis", String(millis()), " ");
  ptr += html_table_row("Send Error Count", String(send_error_count), " ");
  ptr += html_table_row("Local Time", localTime, " ");
  ptr += html_table_row("getEpochTime", String(timeClient.getEpochTime()), " ");
  ptr += html_table_row("time_update_mills", String(time_update_mills), " ");
  ptr += html_table_row("http_Post_Response", http_Post_Response, " ");
  ptr += "</table>";
  ptr += "</p>\n";
  ptr += "<p>";
  ptr += "<td><input type='submit' onclick='check(this.form)' value='OTAWeb Update'></td>";
  ptr += "</p>\n";
  /*
    ptr += "<p>";
    ptr += "<img src=\"/chart.svg\" />";
    ptr += "</p>\n";
  */
  ptr += "</body>\n";
  ptr += "</html>\n";

  ptr += "<script>";
  ptr += "function check(form)";
  ptr += "{";
  ptr += "window.open('/serverIndex')";
  ptr += "}";
  ptr += "</script>";

  return ptr;
}

String SendXML() {
  String ptr = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ptr += "<data>\n";
  ptr += "<voltage>" + String(volt, 1) + "</voltage>\n";
  ptr += "<current>" + String(ampere, 3) + "</current>\n";
  ptr += "<power>" + String(power, 1) + "</power>\n";
  ptr += "<energy>" + String(kwh, 1) + "</energy>\n";
  ptr += "<frequency>" + String(f, 1) + "</frequency>\n";
  ptr += "<powerfactor>" + String(pf, 2) + "</powerfactor>\n";
  ptr += "<millis>" + String(millis()) + "</millis>\n";
  ptr += "<send_error_count>" + String(send_error_count) + "</send_error_count>\n";
  ptr += "</data>\n";

  return ptr;
}


/*
   Return html table row
*/
String html_table_row(String col1, String col2, String col3) {
  String tableRow = "";
  tableRow += "<tr>\n";
  tableRow += "  <td>" + col1 + "</td>\n";
  tableRow += "  <td>" + col2 + "</td>\n";
  tableRow += "  <td>" + col3 + "</td>\n";
  tableRow += "</tr>\n";
  return tableRow;
}

/*
   watchodog function
*/

const int wdtTimeout = 60000;  //time in ms to trigger the watchdog
hw_timer_t* timer = NULL;

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}


/*
   setup function
*/
void setup(void) {
  delay(1000);
  delay(1000);
  // initialize digital pin ledPin as an output.
  pinMode(ledPin, OUTPUT);
  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  //Serial1.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial2 Txd is on pin: " + String(TX));
  Serial.println("Serial2 Rxd is on pin: " + String(RX));
  pinMode(TX_EN, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(TX_EN, LOW);

  //watchdog timer
  timer = timerBegin(0, 80, true);                   //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);   //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false);  //set time in us
  timerAlarmEnable(timer);                           //enable interrupt



  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.print("Trying to connect to: ");
  Serial.print(ssid);
  Serial.print("/");
  Serial.println(password);

  // Wait for connection
  while ((WiFi.status() != WL_CONNECTED) && retry_count < 60) {
    delay(500);
    retry_count++;
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.print(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) {  //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    //  while (1) {
    //    delay(1000);
    //  }
  }
  Serial.println("mDNS responder started");

  // Start up the library
  sensors.begin();

  //Scan bus for sensors
  searchSensors();

  //init and get the time
  timeClient.begin();

  /*return index page*/
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", SendHTML());
  });

  /*return data.xml*/
  server.on("/data.xml", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", SendXML());
  });

  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  //return Restart page
  server.on("/serverRestart", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverRestart);
    //ESP.restart();
    esp_restart();
  });

  /*handling uploading firmware file */
  server.on(
    "/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OKI");
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {  //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });
  server.begin();
}
/*
   Get or update time from NTP
*/
void TimeStuff() {
  long startTime = millis();
  timeClient.update();
  //localTime = timeClient.getFormattedTime();    //return string 15:30:20
  uint32_t t = timeClient.getEpochTime();  //return  seconds since 1970
  ntp_to_rtc(t, rtc);                      //convert unix time to year,month....
  String yearStr = String(rtc.year);
  String monthStr = rtc.month < 10 ? "0" + String(rtc.month) : String(rtc.month);
  String dayStr = rtc.day < 10 ? "0" + String(rtc.day) : String(rtc.day);
  String hourStr = rtc.hour < 10 ? "0" + String(rtc.hour) : String(rtc.hour);
  String minStr = rtc.min < 10 ? "0" + String(rtc.min) : String(rtc.min);
  String secStr = rtc.sec < 10 ? "0" + String(rtc.sec) : String(rtc.sec);
  localTime = yearStr + "-" + monthStr + "-" + dayStr + " " + hourStr + ":" + minStr + ":" + secStr;

  time_update_mills = millis() - startTime;
}

//
void ntp_to_rtc(uint32_t t, RTC& rtc) {
  //Convert seconds to year, day of year, day of week, hours, minutes, seconds
  ldiv_t d = ldiv(t, 60);  // Seconds
  rtc.sec = d.rem;
  d = ldiv(d.quot, 60);  // Minutes
  rtc.min = d.rem;
  d = ldiv(d.quot, 24);  // Hours
  rtc.hour = d.rem;
  rtc.dow = ((d.quot + 4) % 7) + 1;  // Day of week
  d = ldiv(d.quot, 365);             // Day of year
  int doy = d.rem;                   //
  unsigned yr = d.quot + 1970;       // Year

  // Adjust day of year for leap years
  unsigned ly;                                // Leap year
  for (ly = 1972; ly < yr; ly += 4) {         // Adjust year and day of year for leap years
    if (!(ly % 100) && (ly % 400)) continue;  // Skip years that are divisible by 100 and not by 400
    --doy;                                    //
  }                                           //
  if (doy < 0) doy += 365, ++yr;              // Handle day of year underflow
  //yr -= 1; //????!!!!????
  rtc.year = yr;

  // Find month and day of month from day of year
  static uint8_t const dm[2][12] = {
    // Days in each month
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },        // Not a leap year
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }         // Leap year
  };                                                           //
  int day = doy;                                               // Init day of month
  rtc.month = 0;                                               // Init month
  ly = (yr == ly) ? 1 : 0;                                     // Make leap year index 0 = not a leap year, 1 = is a leap year
  while (day > dm[ly][rtc.month]) day -= dm[ly][rtc.month++];  // Calculate month and day of month
  //
  rtc.doy = doy + 1;  // - Make date ones based
  rtc.day = day + 1;
  rtc.month = rtc.month + 1;
}

void sendPostRequestToElsy_https() {
  const char* host = "elsy-bg.com";
  const int port = 443;
  //dannite koito 6te se izpra6tat
  String post_data = "client=" + clientName + "&"
                     + "client_key=" + client_key + "&"
                     + "voltage=" + String(volt, 1) + "&"
                     + "current=" + String(ampere, 3) + "&"
                     + "power=" + String(power, 1) + "&"
                     + "energy=" + String(kwh, 1) + "&"
                     + "frequency=" + String(f, 1) + "&"
                     + "powerfactor=" + String(pf, 2) + "&"
                     + "millis=" + String(millis()) + "&"
                     + "send_error_count=" + String(send_error_count) + "&"
                     + "time=" + String(localTime) + "&"
                     + "t1=" + String(temperature1, 2) + "&"
                     + "t2=" + String(temperature2, 2) + "&"
                     + "t3=" + String(temperature3, 2) + "&"
                     + "t4=" + String(temperature4, 2) + "&"
                     + "t5=" + String(temperature5, 2);
  int post_data_len = post_data.length();

  int conn;
  //client.setCACert(ca_cert);            //Only communicate with the server if the CA certificates match
  client.setInsecure();  // NOT RECOMMENDED
  conn = client.connect(host, port);

  if (conn == 1) {
    HTTPClient http;
    String url = "http://elsy-bg.com/clients/powermeterpost.php";
    http.setConnectTimeout(45);
    http.setTimeout(10000);
    http.begin(client, host, port, url, true);
    http.setConnectTimeout(45);
    http.setTimeout(10000);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header

    int httpResponseCode = http.POST(post_data);  //Send the actual POST request
    if (httpResponseCode > 0) {
      String response = http.getString();  //Get the response to the request
      Serial.print("Server response code: ");
      Serial.println(httpResponseCode);  //Print return code
      Serial.println("Server response: ");
      Serial.println(response);       //Print request answer
      http_Post_Response = response;  //for web server visualisation
      send_error_count = 0;
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
      send_error_count++;
    }
    http.end();  //Free resources


  } else {
    client.stop();
    Serial.println("Connection Failed");
  }
  Serial.println();
  client.stop();
}

String str_printAddress(DeviceAddress deviceAddress) {
  String str = "";
  str += byte_to_string(deviceAddress[0]);
  str += " ";
  str += byte_to_string(deviceAddress[1]);
  str += " ";
  str += byte_to_string(deviceAddress[2]);
  str += " ";
  str += byte_to_string(deviceAddress[3]);
  str += " ";
  str += byte_to_string(deviceAddress[4]);
  str += " ";
  str += byte_to_string(deviceAddress[5]);
  str += " ";
  str += byte_to_string(deviceAddress[6]);
  str += " ";
  str += byte_to_string(deviceAddress[7]);


  return str;
}
//convert byte to hex representation string
String byte_to_string(byte b) {
  byte nib1 = (b >> 4) & 0x0F;
  byte nib2 = (b >> 0) & 0x0F;
  char c[2];
  c[0] = 0;
  c[1] = 0;
  c[0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
  c[1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
  String s = "  ";
  s[0] = c[0];
  s[1] = c[1];
  return s;
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    Serial.print("0x");
    if (deviceAddress[i] < 0x10) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    if (i < 7) Serial.print(", ");
  }
  Serial.println("");
}

bool searchSensors() {
  // Start up the library
  sensors.begin();

  // locate devices on the bus
  Serial.print("Locating OneWire devices...");
  Serial.print("Found ");
  deviceCount = sensors.getDeviceCount();
  Serial.print(deviceCount, DEC);
  Serial.println(" devices.");

  Serial.println("Printing addresses...");
  for (int i = 0; i < deviceCount; i++) {
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(" : ");
    sensors.getAddress(Thermometer, i);
    printAddress(Thermometer);
    if (i == 0) {
      for (int j = 0; j < 8; j++) {
        sensor1[j] = Thermometer[j];
      }
    }
    if (i==1){
      for(int j = 0; j < 8; j++){
         sensor2[j] = Thermometer[j];
      }
    }
    if (i==2){
      for(int j = 0; j < 8; j++){
        sensor3[j] = Thermometer[j];
      }
    }
    if (i==3){
      for(int j = 0; j < 8; j++){
        sensor4[j] = Thermometer[j];
      }
    }
    if (i==4){
      for(int j = 0; j < 8; j++){
        sensor5[j] = Thermometer[j];
      }
    }
    
  }
}
bool readSensors() {
  sensors.requestTemperatures();
  temperature1 = sensors.getTempC(sensor1);  // Gets the values of the temperature
  temperature2 = sensors.getTempC(sensor2);  // Gets the values of the temperature
  temperature3 = sensors.getTempC(sensor3);  // Gets the values of the temperature
  temperature4 = sensors.getTempC(sensor4);  // Gets the values of the temperature
  temperature5 = sensors.getTempC(sensor5);  // Gets the values of the temperature
  return 1;
}

void drawGraph() {
  String out = "";
  char temp[100];
  int graphWidth, graphHeight;
  graphWidth = 500;
  graphHeight = 500;
  String lineColor = "blue";
  String gridColor = "grey";
  sprintf(temp, "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"%d\" height=\"%d\">\n", graphWidth, graphHeight);
  out += temp;
  sprintf(temp, "<rect width=\"%d\" height=\"%d\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n", graphWidth, graphHeight);
  out += temp;
  sprintf(temp, "<title>Active Power</title>");
  out += temp;
  //draw grid
  sprintf(temp, "<g stroke=\"%s\">\n", gridColor);
  out += temp;
  for (int x = 12; x < graphWidth; x += 12) {
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" stroke-dasharray=\"4\" />\n", x, 0, x, graphHeight);
    out += temp;
  }
  out += "</g>\n";

  sprintf(temp, "<g stroke=\"%s\">\n", lineColor);
  out += temp;
  //find maximum value
  int maxValue = 0;
  for (int x = 0; x < arrayDataLen - 1; x += 1) {
    if (arPower[x] > maxValue) maxValue = arPower[x];
  }
  //calculate Y coeff
  float yCoeff = 1;
  if (maxValue > graphHeight) {
    yCoeff = float(graphHeight) / float(maxValue) - 0.05;
  }
  int y = arPower[0] * yCoeff;
  for (int x = 0; x < arrayDataLen - 1; x += 1) {
    int y2 = float(arPower[x + 1]) * yCoeff;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, graphHeight - y, x + 1, graphHeight - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send(200, "image/svg+xml", out);
}

void writePowerMeterValuesInArray() {
  int i = 499;  //(arrayDataLen-1);
  int v;
  while (i > 0) {
    v = arPower[i];
    arPower[(i + 1)] = v;
    i--;
  }

  arPower[20] = arPower[19];
  arPower[19] = arPower[18];
  arPower[18] = arPower[17];
  arPower[17] = arPower[16];
  arPower[16] = arPower[15];
  arPower[15] = arPower[14];
  arPower[14] = arPower[13];
  arPower[13] = arPower[12];
  arPower[12] = arPower[11];
  arPower[11] = arPower[10];
  arPower[10] = arPower[9];
  arPower[9] = arPower[8];
  arPower[8] = arPower[7];
  arPower[7] = arPower[6];
  arPower[6] = arPower[5];
  arPower[5] = arPower[4];
  arPower[4] = arPower[3];
  arPower[3] = arPower[2];
  arPower[2] = arPower[1];
  arPower[1] = arPower[0];


  arPower[0] = power;
}

String print_byte_array(byte byte_array[], byte len) {
  String s = "";
  for (int i = 0; i < len; i++) {
    s += "0x";
    s += String(byte_array[i], HEX);
    s += " ";
  }
  return s;
}

// Compute the MODBUS RTU CRC
uint16_t ModRTU_CRC(byte raw_msg_data_byte[], byte len) {

  //Calc the raw_msg_data_byte CRC code
  uint16_t crc = 0xFFFF;
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)raw_msg_data_byte[pos];  // XOR byte into least sig. byte of crc
    for (int i = 8; i != 0; i--) {            // Loop over each bit
      if ((crc & 0x0001) != 0) {              // If the LSB is set
        crc >>= 1;                            // Shift right and XOR 0xA001
        crc ^= 0xA001;
      } else        // Else LSB is not set
        crc >>= 1;  // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;
}


void loop(void) {
  server.handleClient();

  //if ( min_last_request != rtc.min && have_valid_MB_data == 1)  //send on every minute
  if (min_last_request != rtc.min && millis() > 30000)  //send on every minute
  {
    min_last_request = rtc.min;

    //send data to elsy site
    sendPostRequestToElsy_https();
    if (send_error_count > 0) {
      //send again if send error
      sendPostRequestToElsy_https();
    }
  }


  if (!sendMBPacket) {
    byte buf[8] = { 0x01, 0x04, 0x00, 0x00, 0x00, 0x0a, 0x70, 0x0d}; //request to modbus device ID1    
    byte buf1[8] = {0x02, 0x04, 0x00, 0x00, 0x00, 0x0a, 0x70, 0x3e};  //request to modbus device ID2
    //patch
    //if communication error read byte to empty serial buffer
    byte incomingByte;
    byte loopCounter = 0;
    while (Serial.available() && loopCounter < 250) {
      // read the incoming byte:
      incomingByte = Serial.read();
      loopCounter++;
    }

    digitalWrite(TX_EN, HIGH);
    digitalWrite(ledPin, HIGH);
    Serial2.write(buf, 8);
    delay(9);  //time for transmitting data over RS485
    digitalWrite(TX_EN, LOW);
    digitalWrite(ledPin, LOW);
    sendMBPacket = 1;
    starttime = millis();
  }



  if ((millis() - starttime) > MAX_MILLIS_TO_WAIT) {
    if (Serial2.available() < 25) {
      // the data didn't come in - handle that problem here
      Serial.println("ERROR - Didn't get 25 bytes of data!" + Serial2.available());
      while (Serial2.available()) {
        RFin_bytes[0] = Serial2.read();
      }
      volt = 0;
      ampere = 0;
      power = 0;
      kwh = 0;
      f = 0;
      pf = 0;

    } else {
      for (int n = 0; n < 25; n++)
        RFin_bytes[n] = Serial2.read();  // Then: Get them.

      //check CRC of received data
      uint16_t calc_crc = ModRTU_CRC(RFin_bytes, 23);
      uint16_t received_crc = RFin_bytes[24] * 0x100 + RFin_bytes[23];

      //if CRC is OK process data
      if (calc_crc == received_crc) {
        volt = RFin_bytes[3] * 0x100 + RFin_bytes[4];
        volt /= 10;
        ampere = RFin_bytes[7] * 0x100000000 + RFin_bytes[8] * 0x1000 + RFin_bytes[5] * 0x100 + RFin_bytes[6];
        ampere /= 1000;
        power = RFin_bytes[11] * 0x100000000 + RFin_bytes[12] * 0x1000 + RFin_bytes[9] * 0x100 + RFin_bytes[10];
        power /= 10;
        kwh = RFin_bytes[15] * 0x100000000 + RFin_bytes[16] * 0x10000 + RFin_bytes[13] * 0x100 + RFin_bytes[14];
        //kwh /= 10;
        f = RFin_bytes[17] * 0x100 + RFin_bytes[18];
        f /= 10;
        pf = RFin_bytes[19] * 0x100 + RFin_bytes[20];
        pf /= 100;

        have_valid_MB_data = 1;
      }

      Serial.print(volt);
      Serial.print("V ");
      Serial.print(ampere);
      Serial.print("A ");
      Serial.print(power);
      Serial.print("W ");
      Serial.print(kwh);
      Serial.print("Wh ");
      Serial.print(f);
      Serial.print("Hz ");
      Serial.print(pf);
      Serial.print("[] ");
      Serial.println();

      //patch
      //if communication error read byte to empty serial buffer
      byte incomingByte;
      byte loopCounter = 0;
      while (Serial.available() && loopCounter < 250) {
        // read the incoming byte:
        incomingByte = Serial.read();
        loopCounter++;
      }
    }
    sendMBPacket = 0;
  }
  //restart Board if send error couter is too much
  if (send_error_count > send_error_reset_treshold) {
    ESP.restart();
  }

  if ((millis() - lastMillisTimeUpdate) > time_for_time_update) {
    lastMillisTimeUpdate = millis();
    TimeStuff();
  }

  timerWrite(timer, 0);  //reset timer (feed watchdog)

  if ((millis() - lastTemperatureUpdate) > TEMP_UPDATE_INTERVAL) {
    lastTemperatureUpdate = millis();

    //searchSensors();
    Serial.print("Read temperature sensors: ");
    if (readSensors() == 1) {
      Serial.print(temperature1);
      Serial.print(" ");
      Serial.print(temperature2);
      Serial.print(" ");
      Serial.print(temperature3);
      Serial.print(" ");
      Serial.print(temperature4);
      Serial.print(" ");
      Serial.print(temperature5);
      Serial.print(" ");
    }
    Serial.println();
  }

  if (millis() > restart_millis_threshold) {
    ESP.restart();
  }

  delay(1);
}
