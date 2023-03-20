#include <Credentials.h>
#include <SimpleTimer.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266SSDP.h>
#include <Keypad.h>

const char* DEVICE_NAME = "GARAGE KEYPAD";

#define keypad_code_topic "garage/keypad/code"
//These two are from the garage controller, used here to play the success tune on buzzer
#define frontdoor_status_topic "garage/door/status/front"
#define backdoor_status_topic "garage/door/status/back"
#define restart_topic "garage/door/status/restart"
#define unknown_code_topic "garage/keypad/code/error"

//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

//Initialize Objects
WiFiClient garageKeypad;
SimpleTimer timer;
int timerId;
PubSubClient client(garageKeypad);

String input = "";   // keypad input
String strTopic;
String strPayload;

//Keyapad details
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {0, 4, 5, 16}; //D3 D2 D1 D0
byte colPins[COLS] = {13, 12, 14, 2}; //D7 D6 D5 D4

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

//Tones - sound frequency and melodies
int successFailTone[] = {261, 277, 294, 311, 330, 349, 370, 392, 415, 440};
int normalKeypressTone = 1760;
int cancelTone = 3520;
int errorTone = 370;

const int SPEAKER_PIN = 15;

void setup() {
  Serial.begin(115200);
  setup_wifi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback); //callback is the function that gets called for a topic sub

  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.begin();

  timerId = timer.setInterval(20000, clearInput); //Clear input every 10 sec if not empty
  timer.disable(timerId);
}

void loop() {
  //If MQTT client can't connect to broker, then reconnect
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); //the mqtt function that processes MQTT messages
  ArduinoOTA.handle();
  readKeypadPresses();
  timer.run();
}

void clearInput() {
  if (input != "") {
    input = "";
    cancelSoundEffect();
    timer.disable(timerId);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  //Handle mqtt messages received my this NodeMCU
  payload[length] = '\0';
  strTopic = String((char*)topic);
  if (strTopic == frontdoor_status_topic || strTopic == backdoor_status_topic) {
    String doorState = String((char*)payload);
    if (doorState == "open" || doorState == "closed") {
      successTone();
    } else {
      failTone();
    }
  } else if (strTopic == unknown_code_topic){
    failTone();
  }else if (strTopic == restart_topic) {
    restartESP();
  }
}

void readKeypadPresses() {

  char key = keypad.getKey();

  if (key) {
    timer.enable(timerId);

    if (key != 'C' && key != '#') {
      timer.restartTimer(timerId);
      buttonPressSoundEffect();
      input += key;
    }

    if (key == 'C') {
      cancelSoundEffect();
      input = "";
      timer.disable(timerId);
    }

    if (key == '#') {
      buttonPressSoundEffect();
      client.publish(keypad_code_topic, String(input).c_str());
      input = "";
      timer.disable(timerId);
    }
  }
}

void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC: ");
  Serial.println(WiFi.macAddress());
}

void reconnect() {
  //Reconnect to Wifi and to MQTT. If Wifi is already connected, then autoconnect doesn't do anything.

  int retries = 0;
  while (!client.connected()) {
    if (retries < 15) {
      Serial.print("Attempting MQTT connection...");
      //Attempt to connect
      if (client.connect(DEVICE_NAME, MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println("connected");
        client.subscribe("garage/door/status/#");
        client.subscribe("garage/keypad/code/error");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if (retries > 14) {
      ESP.restart();
    }
  }
}

//SOUND EFFECTS
void buttonPressSoundEffect() {
  tone(SPEAKER_PIN, normalKeypressTone);
  delay(100);
  noTone(SPEAKER_PIN);
}

void successTone() {
  for (int note : successFailTone) {
    tone(SPEAKER_PIN, note);
    delay(50);
  }
  noTone(SPEAKER_PIN);
}

void failTone() {
  for (int i = 11; i > 0; i--) {
    tone(SPEAKER_PIN, successFailTone[i]);
    delay(50);
  }
  noTone(SPEAKER_PIN);
}

void cancelSoundEffect() {
  tone(SPEAKER_PIN, cancelTone);
  delay(100);
  noTone(SPEAKER_PIN);
  tone(SPEAKER_PIN, cancelTone);
  delay(100);
  noTone(SPEAKER_PIN);
}

void restartESP() {
  ESP.restart();
}
