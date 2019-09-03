#include <SimpleTimer.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266SSDP.h>
#include <Keypad.h>

const char* host = "Garage Controller Keypad";
const char* ssid = "SSID";
const char* password = "WIFI PASSWORD";
const char* mqtt_user = "MQTT Username";
const char* mqtt_pass = "MQTT Password";

#define mqtt_server "192.168.0.3"
#define keypad_code_topic "garage/keypad/code"
//These two are from the garage controller, used here to play the success tune on buzzer
#define frontdoor_status_topic "garage/door/status/front"
#define backdoor_status_topic "garage/door/status/back"

//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

//Initialize Objects
WiFiClient espCgarageKeypad;
SimpleTimer timer;
int timerId;
PubSubClient client(espCgarageKeypad);

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

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); //callback is the function that gets called for a topic sub

  ArduinoOTA.setHostname("Garage Keypad");
  ArduinoOTA.begin();

  timerId = timer.setInterval(10000, clearInput); //Clear input every 10 sec if not empty
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
    }else{
      failTone();
    }
  }
}

void readKeypadPresses() {
  char key = keypad.getKey();
  if (key != NO_KEY) {
    //    check if (key != 'A' && key != 'B' && key != 'C' && key != 'D' && key != '#' && key != '*')
    if (key != 'C' && key != '#') {
      timer.enable(timerId);
      buttonPressSoundEffect();
      input += key;
//      Serial.print(key);
    }
  }

  if (key == 'C') {
    cancelSoundEffect();
    input = "";
    timer.disable(timerId);
//    Serial.print(key);
  }

  if (key == '#') {
    buttonPressSoundEffect();
    client.publish(keypad_code_topic, String(input).c_str());
    input = "";
    timer.disable(timerId);
//    Serial.print(key);
  }
}

void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

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
  Serial.print("Attempting MQTT connection...");
  if (client.connect(host, mqtt_user, mqtt_pass)) {
    Serial.println("connected");
     client.subscribe("garage/door/status/#");
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
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
