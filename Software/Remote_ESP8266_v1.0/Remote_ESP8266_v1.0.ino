#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
// библиотека парсер JSON https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
// Удобный менеджер WiFi https://github.com/tzapu/WiFiManager
#include <WiFiManager.h>
// Графическая библиотека Adafruit GFX https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_GFX.h>
// Библиотека работы с OLED эккранами https://github.com/adafruit/Adafruit_SSD1306
#include <Adafruit_SSD1306.h>
// Библиотека работы с кнопками 
#include "SButton.h"

/**
 * Определение пинов
 */
uint8_t PIN_LED1  = 16;
uint8_t PIN_BTN1  = 0;
uint8_t PIN_BTN2  = 12;
uint8_t PIN_INP   = 14;
uint8_t PIN_SDA   = 4;
uint8_t PIN_SCK   = 5;


#define TM_TRIG 5000
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
WiFiManager wifiManager;
 
SButton btn1(PIN_BTN1);
SButton btn2(PIN_BTN2);
SButton trig(PIN_INP);




uint32_t ms0=0,ms1=0, ms2=0;
uint32_t ms_trig;
bool isConnect = false;
bool isRecord  = false;
bool isLed     = false;
uint8_t vcc_proc = 100;

void RequestCam(String cmd, String params = "" );
String ShootMode;
String CameraStatus;
bool isNewStatus = false;
bool isReqStatus = false;

void setup(){
  pinMode(PIN_LED1,OUTPUT);
  Serial.begin(115200);  
  btn1.SetLongClick(5000);
  Wire.begin(PIN_SDA,PIN_SCK); 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setRotation(2);
  ConnectCamera();
  RequestCam("getApplicationInfo");
  delay(1000);   
 }


void loop(){
   uint32_t ms = millis();
   if( WiFi.status() != WL_CONNECTED ){
       isConnect = false;
       ConnectCamera();
   } 

   switch( btn1.Loop() ){
      case SB_CLICK :
         Serial.println("BTN1 click");
         if( isRecord == false ){
            if( ShootMode == "movie" )RequestCam("setShootMode","\"still\"");
            else if( ShootMode == "still" )RequestCam("setShootMode","\"intervalstill\""); 
            else RequestCam("setShootMode","\"movie\"");
         }
         break;
      case SB_LONG_CLICK :   
         Serial.println("BTN1 long click");
         wifiManager.setTimeout(180);       
         DisplayText("CONFIG",3,0,5);  
         if (!wifiManager.startConfigPortal("SONY_HDR_Remote")) {
            Serial.println("Failed config");
            DisplayText("FAIL",3,0,5); 
            delay(3000);
         } 
      ESP.reset();
      delay(5000);     
      break;
   }
   if( btn2.Loop() == SB_CLICK ){
      Serial.println("BTN2 click");
      if( ShootMode == "movie" ){ 
        if( isRecord == false )RequestCam("startMovieRec");
        else RequestCam("stopMovieRec");
      }
      else if( ShootMode == "intervalstill" ){
        if( isRecord == false )RequestCam("startIntervalStillRec");
        else RequestCam("stopIntervalStillRec");
      }
      else if( ShootMode == "still" ){
        digitalWrite(PIN_LED1,true);
        RequestCam("actTakePicture");
        digitalWrite(PIN_LED1,false);
      }
   }
   if( trig.Loop() == SB_CLICK ){
      if( ms_trig == 0 || ms_trig > ms || (ms - ms_trig)>TM_TRIG ){
         ms_trig = ms;
         Serial.println("TRIG on");
         if( ShootMode == "still" ){
           digitalWrite(PIN_LED1,true);
           RequestCam("actTakePicture");
           digitalWrite(PIN_LED1,false);
         }
      }
   }

   if( ms1 == 0 || ms < ms1 || (ms-ms1)>10000 ){
       ms1 = ms;
       int a0 = analogRead(A0);
       if( a0 < 400 )vcc_proc = 0;
       else if( a0 < 413 )vcc_proc = 10;   
       else if( a0 < 429 )vcc_proc = 20;   
       else if( a0 < 446 )vcc_proc = 30;   
       else if( a0 < 460 )vcc_proc = 40;   
       else if( a0 < 474 )vcc_proc = 50;   
       else if( a0 < 486 )vcc_proc = 60;   
       else if( a0 < 504 )vcc_proc = 70;   
       else if( a0 < 518 )vcc_proc = 80;   
       else if( a0 < 532 )vcc_proc = 90;   
       else vcc_proc = 100;   
       Serial.printf("ADC=%d vcc_proc=%d\n",a0,(int)vcc_proc);
   }
   
   if( ms0 == 0 || ms < ms0 || (ms-ms0)>1000 ){
       ms0 = ms;  
       RequestStatus();
       if( ShootMode == "movie" ){
          DisplayText("VIDEO",3,0,5);
          if( CameraStatus == "IDLE" ){
             digitalWrite(PIN_LED1,false);
             isRecord = false;
          }
          else {
             digitalWrite(PIN_LED1,true);
             isRecord = true;
          }
       }
       else if( ShootMode == "still" ){
          DisplayText("PHOTO",3,0,5);
       }
       else if( ShootMode == "intervalstill" ){
          DisplayText("LOOPS",3,0,5);
          if( CameraStatus == "IDLE" ){
             digitalWrite(PIN_LED1,false);
             isRecord = false;
          }
          else {
             digitalWrite(PIN_LED1,true);
             isRecord = true;
          }
       }
       else {
          DisplayText("READY",3,0,5);
       }

    }
}

/**
 *  Функция парсер состояния камеры
*/ 
void RequestStatus(){
   DynamicJsonDocument doc1(10000);  
   HTTPClient http;
   http.begin("http://192.168.122.1:10000/sony/camera");
   http.addHeader("Content-Type", "text/plain");
   http.POST("{ \"method\": \"getEvent\", \"params\": [false], \"id\": 1, \"version\": \"1.0\" }");
   String ret = http.getString();

   
   deserializeJson(doc1, ret );
//   JsonObject obj1 = doc1.as<JsonObject>();
   Serial.println(ret);
   
   JsonArray a1 = doc1["result"];
   isNewStatus = false;
   isReqStatus = false;
//   Serial.println(a1.size());
   for( int i=0; i<a1.size();i++ ){
     String type = "";
     JsonObject obj1;
     if( a1[i].isNull() )continue;
     if( a1[i].is<JsonObject>() ){
         obj1 = a1[i].as<JsonObject>();
         type = obj1["type"].as<String>();
     }
     else if( a1[i].is<JsonArray>() ){
        JsonArray a2 = a1[i].as<JsonArray>(); 
        for( int j=0; j<a2.size();j++ ){
           obj1 = a2[j].as<JsonObject>(); 
           type = obj1["type"].as<String>();
           if( type != NULL )continue;
        }
            
     }
//     JsonObject obj1 = a1[i].as<JsonObject>();
//     if( a1[i].isNull() )Serial.printf("null ");
//     if( a1[i].is<JsonObject>() )Serial.printf("object ");
//     if( a1[i].is<JsonArray>() )Serial.printf("array ");
     
//     Serial.printf("%d %s ",i, type.c_str());
// Эти цифры можно использовать если камера поддерживает выдачу времени записи и количество
// снимков в LOOPS. AS100 не поддерживает (((
     if( type == "storageInformation" ){
        int tm1  = obj1["recordableTime"].as<int>();
        int num1 = obj1["numberOfRecordableImages"].as<int>(); 
//        Serial.printf("time = %d num = %d\n", tm1, num1);
     }

     
     if( type == "cameraStatus" ){
         isReqStatus = true;
         String val = obj1["cameraStatus"].as<String>();
         if( CameraStatus != val ){
             CameraStatus = val;
             isNewStatus = true;
         }
     }
     if( type == "shootMode" ){
         isReqStatus = true;
         String val = obj1["currentShootMode"].as<String>();
         if( ShootMode != val ){
             ShootMode = val;
             isNewStatus = true;
         }      
     }
   }

   
   if( isNewStatus ){
       Serial.print("Status = ");
       Serial.print(CameraStatus);
       Serial.print(", mode= ");
       Serial.println(ShootMode);
   }
//   else if( isReqStatus )Serial.println("OK");

   http.end();
}

/**
 * Функция посылающая запрос к камере 
*/
void RequestCam(String cmd, String params ){

  String req = "{ \"method\": \""+cmd+"\", \"params\": ["+params+"], \"id\": 1, \"version\": \"1.0\" }";
  HTTPClient http;
  Serial.print(">>> ");
  Serial.println(req);
  http.begin("http://192.168.122.1:10000/sony/camera");
  http.addHeader("Content-Type", "text/plain");
  http.POST(req);
  Serial.print("<<< ");
  Serial.println(http.getString());
  http.end();
}

void DisplayText( char *t, uint8_t size, uint8_t x, uint8_t y ){
  display.clearDisplay();
  display.setTextSize(size);             
  display.setTextColor(WHITE);        
  display.setCursor(x,y);             
  display.println(t); 
  DisplayBat();
  display.display();
  
}

/**
 * Функция определения состояния батареи
*/
void DisplayBat(){
  display.drawRect(113,2,15,30,WHITE);
  display.fillRect(117,0,7,2,WHITE);
//  if( vcc_proc >0  )display.fillRect(113,28,15,2,WHITE);
  if( vcc_proc >10 )display.fillRect(113,26,15,5,WHITE);
//  if( vcc_proc >20 )display.fillRect(113,22,15,2,WHITE);
  if( vcc_proc >30 )display.fillRect(113,20,15,5,WHITE);
//  if( vcc_proc >40 )display.fillRect(113,16,15,2,WHITE);
  if( vcc_proc >50 )display.fillRect(113,14,15,5,WHITE);
//  if( vcc_proc >60 )display.fillRect(113,10,15,2,WHITE);
  if( vcc_proc >70 )display.fillRect(113,8,15,5,WHITE);
//  if( vcc_proc >80 )display.fillRect(113,4,15,2,WHITE);
  if( vcc_proc >90 )display.fillRect(113,2,15,5,WHITE);
}

/**
* Функция соединения с камерой по WiFi
*/
void ConnectCamera(){
  DisplayText("CONNECT..",2,0,5);
    // Connect to WiFi network
  WiFi.begin("", "");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     if( isLed == false )isLed = true;
     else isLed = false;
     digitalWrite(PIN_LED1,isLed);
     Serial.print(".");
  }
  isConnect = true;
  Serial.println("");
  Serial.print("Connected to ");
//  Serial.println(ssid);
  Serial.print("IP   address: ");
  Serial.println(WiFi.localIP());
  Serial.print("GW   address: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("MASK address: ");
  Serial.println(WiFi.subnetMask());

  char url[50];
  IPAddress gw = WiFi.gatewayIP();
  sprintf(url,"http://%d.%d.%d.%d/camera",gw[0],gw[1],gw[2],gw[3]);
  Serial.print("URL: ");
  Serial.println(url);
  DisplayText("READY",3,0,5);
  isLed = false;
  digitalWrite(PIN_LED1,isLed);

}


  
