#include <Credentials.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266SSDP.h>
#include <Keypad.h>

const char* DEVICE_NAME = "GARAGE KEYPAD";

const char* keypad_code_topic = "garage/keypad/code";
//These two are from the garage controller, used here to play the success tune on buzzer
const char* frontdoor_status_topic = "garage/door/status/front";
const char* backdoor_status_topic = "garage/door/status/back";
const char* restart_topic = "garage/door/status/restart";
const char* unknown_code_topic = "garage/keypad/code/error";

char input[8];  // keypad input
int inputIndex = 0;
unsigned long lastKeyPressTime = 0;
const unsigned long NO_ACTIVITY_INPUT_TIMEOUT = 20000;  // Timeout 20 sec

//Keyapad details
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

byte rowPins[ROWS] = { 0, 4, 5, 16 };    //D3 D2 D1 D0
byte colPins[COLS] = { 13, 12, 14, 2 };  //D7 D6 D5 D4

//Tones - sound frequency and melodies
int successFailTone[] = { 261, 277, 294, 311, 330, 349, 370, 392, 415, 440 };
int normalKeypressTone = 1760;
int cancelTone = 3520;
int errorTone = 370;

const int SPEAKER_PIN = 15;

//Initialize Objects
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
WiFiClient garageKeypad;
PubSubClient client(garageKeypad);

void setup() {
  Serial.begin(115200);
  setupWiFi();
  setupMQTT();
  setupOTA();
}

void loop() {
  //If MQTT client can't connect to broker, then reconnect
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  //the mqtt function that processes MQTT messages
  ArduinoOTA.handle();
  readKeypadPresses();
}

void callback(char* topic, byte* payload, unsigned int length) {

  payload[length] = '\0';
  char* strTopic = topic;

  if (strcmp(strTopic, frontdoor_status_topic) == 0 || strcmp(strTopic, backdoor_status_topic) == 0) {
    char* doorState = (char*)payload;
    if (strcmp(doorState, "open") == 0 || strcmp(doorState, "closed") == 0) {
      successTone();
    } else {
      failTone();
    }
  } else if (strcmp(strTopic, unknown_code_topic) == 0) {
    failTone();
  } else if (strcmp(strTopic, restart_topic) == 0) {
    restartESP();
  }
}

void readKeypadPresses() {

  char key = keypad.getKey();

  if (key == NO_KEY) {
    if (millis() - lastKeyPressTime >= NO_ACTIVITY_INPUT_TIMEOUT) {
      clearInput();
    }
    return;
  }

  lastKeyPressTime = millis();

  if (key != 'C' && key != '#') {

    buttonPressSoundEffect();
    if (inputIndex < sizeof(input) - 1) {
      input[inputIndex] = key;
      inputIndex++;
      input[inputIndex] = '\0';
    }
  }

  if (key == 'C') {
    clearInput();
  }

  if (key == '#') {
    buttonPressSoundEffect();
    //Only send if payload not empty
    if (input[0] != '\0') {
      client.publish(keypad_code_topic, input);
      inputIndex = 0;
      input[0] = '\0';
    }
  }
}

void clearInput() {
  cancelSoundEffect();
  if (input[0] != '\0') {
    inputIndex = 0;
    input[0] = '\0';
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

void setupWiFi() {

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    // Timeout if connection takes to long (15 seconds)
    if (millis() - startTime > 15000) {
      restartESP();
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC: ");
  Serial.println(WiFi.macAddress());
}

void setupMQTT() {
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);  // callback is the function that gets called for a topic subscription

  Serial.println("Attempting MQTT connection...");
  if (client.connect(DEVICE_NAME, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("Connected to MQTT broker");
    subscribeMQTTTopics();
  } else {
    Serial.print("Failed to connect to MQTT broker. RC=");
    Serial.println(client.state());
    Serial.println("Restarting ESP...");
    restartESP();
  }
}

void subscribeMQTTTopics() {
  client.subscribe("garage/door/status/#");
  client.subscribe("garage/keypad/code/error");
}

void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.begin();
}

void reconnect() {

  // Reconnect to WiFi and MQTT
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  // Check if MQTT connection is not established
  if (!client.connected()) {
    setupMQTT();
  }
}

void restartESP() {
  ESP.restart();
}
