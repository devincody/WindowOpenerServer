/*
 * WindowOpenerServer.ino
 * Author: Devin Cody
 * Date: 10/24/2020
 * 
 * ESP32-based webserver which controls a stepper motor to open and close a window.
 * Based on the WebServer library which makes interacting with web requests very nice.
 * Uses interrupts on limit switches to detect when the window is fully open or closed.
 * Uses a timer interrupt to control step signal needed for stepper motor.
 */

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// Code to hide my password, SSID, port numbers
// You can either include a header file named "WindowOpenerServer.h" with the 
// three #define statements below, or set USE_PASSWORD_HEADER to 0.
#define USE_PASSWORD_HEADER 0
#ifdef USE_PASSWORD_HEADER
  #include "WindowOpenerServer.h"
#else
  #define SSID ("*****")
  #define PORT (5000)
  #define PASSWORD ("*****")
#endif
  
const char* ssid     = SSID;
const char* password = PASSWORD;
int port_number = PORT;

WebServer server(port_number);

// defines pins numbers
const int led = 2;
const int stepPin = 33; 
const int dirPin = 32; 
const int m1Pin = 12;
const int m2Pin = 14;
const int m3Pin = 27;
const int nSleepPin = 25;
const int nResetPin = 26;
const int nEnablePin = 13;

const int limit_switch_near = 34;
const int limit_switch_far = 35;
 
const int step_time = 1000; //microseconds

//volatile int near_pressed = 0;
//volatile int far_pressed = 0;

// Flag which specifies whether the uC should be opening (1), closing(-1), or doing nothing (0)
volatile int open_close_flag = 0;
// Flag which states if we are done an open/closing state
volatile int open_close_done_flag = 0;
// Keep track of the number of steps taken in a given direction
volatile uint32_t steps = 0;
volatile uint32_t last_steps = 0;

// Timer for waveform generation
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer(){
  // Increment the step counter
  portENTER_CRITICAL_ISR(&timerMux);
  steps++;
  portEXIT_CRITICAL_ISR(&timerMux);
  // It is safe to use digitalRead/Write here if you want to toggle an output
  digitalWrite(stepPin, steps&1);
}


// ISR 1
void IRAM_ATTR near_switch_pressed(void){
  if (digitalRead (limit_switch_near) == HIGH && open_close_flag == 1){
    // if you were trying to open the window, and the near switch was toggled, 
    // disable controller and stop trying to open the window.
    digitalWrite(nSleepPin, LOW);
    digitalWrite(nResetPin, LOW);
    digitalWrite(nEnablePin, HIGH);
    open_close_flag == 0;
    open_close_done_flag = 1;
    last_steps = steps;
  }  
}

// ISR 2
void IRAM_ATTR far_switch_pressed(void){
  // if you were trying to close the window, and the far switch was toggled, 
  // disable controller and stop trying to close the window.
  if (digitalRead (limit_switch_far) == HIGH && open_close_flag == -1){
    digitalWrite(nSleepPin, LOW);
    digitalWrite(nResetPin, LOW);
    digitalWrite(nEnablePin, HIGH);
    open_close_flag == 0;
    open_close_done_flag = 1;
    last_steps = steps;
  }
}

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from esp32!");
  digitalWrite(led, 0);
}

void open_window() {
  digitalWrite(led, 1);
  char resp[100];
  sprintf(resp, "Sup, last_steps = %d, steps = %d\n", last_steps, steps);
  server.send(200, "text/plain", resp);
  Serial.println("got open");
  digitalWrite(led, 0);

//  near_pressed = digitalRead (limit_switch_near);
  if (digitalRead (limit_switch_near) == 0){
    open_close_flag = 1;
    steps = 0;
    digitalWrite(dirPin, HIGH); // set open window direction
    digitalWrite(nSleepPin, HIGH); // Turn on stepper controller
    digitalWrite(nResetPin, HIGH);
    digitalWrite(nEnablePin, LOW);
  } else {
    open_close_flag = 0;
  }

}

void close_window() {
  digitalWrite(led, 1);
  char resp[100];
  sprintf(resp, "Sup, last_steps = %d, steps = %d\n", last_steps, steps);
  server.send(200, "text/plain", resp);
  Serial.println("got close");
  digitalWrite(led, 0);
  
//  far_pressed = digitalRead (limit_switch_far);
  if (digitalRead (limit_switch_far) == 0){
    open_close_flag = -1;
    steps = 0;
    digitalWrite(dirPin, LOW); // set close window direction
    digitalWrite(nSleepPin, HIGH); // Turn on stepper controller
    digitalWrite(nResetPin, HIGH);
    digitalWrite(nEnablePin, LOW);
  } else {
    open_close_flag = 0;
  }
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void) {
  digitalWrite(m1Pin,HIGH); 
  digitalWrite(m2Pin,HIGH); 
  digitalWrite(m3Pin,HIGH);
  digitalWrite(nSleepPin,LOW);
  digitalWrite(nResetPin, LOW);
  digitalWrite(nEnablePin, HIGH);

  pinMode(led,OUTPUT); 
  pinMode(stepPin,OUTPUT); 
  pinMode(dirPin,OUTPUT);
  pinMode(m1Pin,OUTPUT);
  pinMode(m2Pin,OUTPUT);
  pinMode(m3Pin,OUTPUT);
  pinMode(nSleepPin,OUTPUT);
  pinMode(nResetPin, OUTPUT);
  pinMode(nEnablePin, OUTPUT);
  
  pinMode(limit_switch_near,INPUT);
  pinMode(limit_switch_far,INPUT);
  
  digitalWrite(nSleepPin, LOW);
  
  attachInterrupt(digitalPinToInterrupt(limit_switch_near), near_switch_pressed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(limit_switch_far), far_switch_pressed, CHANGE);

  // setup step waveform generation
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, step_time, true); // Setup to toggle a variable amount, 
  timerAlarmEnable(timer);
 
  
  pinMode(led, OUTPUT);
  digitalWrite(led, 1);
  Serial.begin(115200);

  // Needs to connect twice to work reliably, Here we allow up to 10 attempts
  // https://github.com/espressif/arduino-esp32/issues/1212
  for (int i = 0; i < 10; i++){
    Serial.print("WIFI status = ");
    Serial.println(WiFi.getMode());
    WiFi.disconnect(true);
    delay(1000);
    WiFi.mode(WIFI_STA);
    delay(1000);
    Serial.print("WIFI status = ");
    Serial.println(WiFi.getMode());
  
    
    WiFi.begin(ssid, password);
    Serial.println("");
  
    // Wait for connection
    int connect = 0;
    for (int j = 0; j < 10; j++){
      if(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      } else {
        Serial.print("connected");
        connect = 1;
        break;
      }
    }

    if( connect == 1){
      break;
    }
  }
  digitalWrite(led, 0);

  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/open_window", open_window);
  server.on("/close_window", close_window);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  digitalWrite(m1Pin,HIGH); //MSB
  digitalWrite(m2Pin,HIGH); 
  digitalWrite(m3Pin,HIGH); //LSB
  
  server.handleClient();

  if (open_close_done_flag == 1){
    Serial.print(steps);
    Serial.println(" steps");
    open_close_done_flag = 0;
  }
}
