#include <Arduino.h>
#include <ArduinoJson.h>

#include <WebSocketsClient.h> 
#include <StreamString.h>

#include <IRremote.h>
#include <FirebaseESP32.h>

// Variáveis Devices
#define TV_QUARTO "5eee494f943faf256a728b5c"
#define MONITOR_QUARTO "5eee49b1943faf256a728b72"
#define AR_QUARTO "5eee7632943faf256a7292d4"
#define INFRAVERMELHO_QUARTO "5eee56ae943faf256a728dfc"

// Variáveis WebSocket
#define FIREBASE_HOST "https://msb-home.firebaseio.com/"
#define FIREBASE_AUTH "kqPQgeVq9gejmNaLD0wNkVaf83PAhQyUOLS2pXjE"

#define PATH_ACTION "/automation/action"

#define KEY_POWER "/KEY_POWER"

const String PATH_DEVICE_TV = "/automation/devices/tv";
const String PATH_DEVICE_MONITOR = "/automation/devices/monitor";
const String PATH_DEVICE_AR = "/automation/devices/ar";
const String PATH_DEVICE_INFRAVERMELHO = "/automation/devices/infravermelho";

const String PATH_CODE_TV = "/automation/codes/tv";
const String PATH_CODE_AR = "/automation/codes/ar";

const String STATUS_ON = "on";
const String STATUS_OFF = "off";

FirebaseData firebaseData;
FirebaseData firebaseDataStream;
FirebaseJson firebaseJson;

// Variáveis WebSocket
#define MY_API_KEY "6339e553-51d9-4bca-abff-250bfefa81d4" 
#define HEARTBEAT_INTERVAL 300000

WebSocketsClient webSocket;
uint64_t heartbeatTimestamp = 0;
bool isConnected = false;

// Variáveis Board
#define SEND_PIN 5
#define REC_PIN 19

#define LED_BUILTIN 2
#define LED_PIN1 4
#define LED_PIN2 21
#define LED_PIN3 16 
#define LED_PIN4 23

#define LUZ_PIN 18

// Variáveis WiFi
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Variáveis IRremote
IRsend SENDER_IR(SEND_PIN);
IRrecv RECEIVER_IR(REC_PIN);

bool isReceiving = false;
int _size = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Setup");
  delay(3000);
  setupBegin();
  Serial.println("");
  Serial.println("WiFi");
  delay(3000);
  wifiBegin();
  Serial.println("");
  Serial.println("Firebase"); 
  delay(3000);
  firebaseBegin();
  Serial.println("");
  Serial.println("WebSocket"); 
  delay(3000);
  websocketBegin();
  Serial.println("");
  Serial.println("IRremote"); 
  delay(3000);
  irremoteBegin();
  Serial.println("");
  delay(1000);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Ready");
  Serial.println("");
}

void setupBegin() {
  Serial.println("Iniciando Setup");
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(LED_PIN3, OUTPUT);
  pinMode(LED_PIN4, OUTPUT);
  pinMode(LUZ_PIN, OUTPUT);
  Serial.println("Setup configurado com sucesso!");
}

void wifiBegin() {
  Serial.println("Iniciando Wifi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Conectando Wifi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...");
    delay(1000);
  }
  Serial.print("localIP: ");
  Serial.println(WiFi.localIP());
  Serial.print("macAddress: ");
  Serial.println(WiFi.macAddress());
  digitalWrite(LED_PIN1, LOW);
  Serial.println("Wifi conectado com sucesso!");
}

void firebaseBegin() {
  
  Serial.println("Iniciando Firebase");
  digitalWrite(LED_PIN2, HIGH);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); 
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(firebaseDataStream, 5000); 
  Firebase.setwriteSizeLimit(firebaseDataStream, "tiny");
  Serial.println("Conectando Firebase");
  Firebase.setStreamCallback(firebaseDataStream, streamCallback, streamTimeoutCallback);
  if (!Firebase.beginStream(firebaseDataStream, PATH_ACTION) || !Firebase.readStream(firebaseDataStream)) {
    firebaseErrorReason(firebaseData);
  }
  digitalWrite(LED_PIN2, LOW);
  Serial.println("Firebase conectado com sucesso!");
}

void streamCallback(StreamData streamData) {
   String operation = streamData.eventType();
   Serial.println(operation);
   if(operation.equals("put")) {
      String type = streamData.dataType();
      Serial.println(type);   
      if (streamData.dataType() == "json") {
         Serial.println(streamData.jsonString());
         FirebaseJson &json = streamData.jsonObject();
         FirebaseJsonData jsonData;
         json.get(jsonData, "status");
         Serial.println(jsonData.stringValue);
         if(STATUS_ON.equals(jsonData.stringValue)){
            json.get(jsonData, "deviceId");
            String deviceId = jsonData.stringValue;
            json.get(jsonData, "intensity");
            int intensity = jsonData.intValue;
            json.get(jsonData, "key");
            String key = jsonData.stringValue;
            if(deviceId.equals(TV_QUARTO)) {
               streamTV(key, intensity);
            }
            if(deviceId.equals(MONITOR_QUARTO)) {
               streamMonitor();
            }
            if(deviceId.equals(AR_QUARTO)) {
               streamAr();
            }
            if(deviceId.equals(INFRAVERMELHO_QUARTO)) {
               streamInfravermelho(intensity);
            }
            updateFirebaseActionOff();
         }
      }
   }
}

void streamTimeoutCallback(bool timeout) {
  if(timeout) {
    Serial.println("[FB] Stream timeout, aguardando comandos...");
  }
}

void firebaseErrorReason(FirebaseData &data) { 
  Serial.println("firebaseData.errorReason: " + data.errorReason());
}

void streamTV(String key, int intensity) {
  sendIRCodesTV(getFirebaseCodesArray(PATH_CODE_TV + key));
  updateFirebaseStatus(PATH_DEVICE_TV, intensity);
}

void streamMonitor() {
  Serial.println("webSocketEventMonitor");
}

void streamAr() {
  Serial.println("webSocketEventAr");
}

void streamInfravermelho(int intensity) {
  if(intensity == 1) {
    isReceiving = true;
    digitalWrite(LED_PIN4, HIGH);
  } else {
    isReceiving = false;
    digitalWrite(LED_PIN4, LOW);       
  }
  updateFirebaseStatus(PATH_DEVICE_INFRAVERMELHO, intensity);
}

String getFirebaseString(String path) {
  if (Firebase.get(firebaseData, path)) {
    return firebaseData.stringData();
  }
  firebaseErrorReason(firebaseData);
  return "";
}

int getFirebaseInt(String path) {
  if (Firebase.get(firebaseData, path)) {
    return firebaseData.intData();
  } 
  firebaseErrorReason(firebaseData);
  return 0;
}

int* getFirebaseCodesArray(String path) {
  int* tecla = new int[0]();
  if (Firebase.get(firebaseDataStream, path)) {
    FirebaseJsonArray &arr = firebaseDataStream.jsonArray();
    for (int i = 0; i < arr.size(); i++) {
      FirebaseJsonData &jsonData = firebaseDataStream.jsonData();
      arr.get(jsonData, i);
      _size = i; 
      tecla[i] = jsonData.intValue;
    }
  } else {
    firebaseErrorReason(firebaseDataStream);
  }
  return tecla;
}

void updateFirebaseStatus(String path, int intensity) {
  firebaseJson.clear();
  firebaseJson.set("status", intensity == 0 ? STATUS_OFF : STATUS_ON);
  if (Firebase.updateNode(firebaseDataStream, path, firebaseJson)) {
    return;
  } 
  firebaseErrorReason(firebaseDataStream);
}

void updateFirebaseActionOff() {
  firebaseJson.clear();
  firebaseJson.set("deviceId", " ");
  firebaseJson.set("key", " ");         
  firebaseJson.set("intensity", -1);
  firebaseJson.set("status", STATUS_OFF);
  if (Firebase.updateNode(firebaseDataStream, PATH_ACTION, firebaseJson)) {
    return;
  } 
  firebaseErrorReason(firebaseDataStream);
}

void updateFirebaseJsonActionOn(String device, String key, int intensity) {
  firebaseJson.clear();
  firebaseJson.set("deviceId", device);
  firebaseJson.set("key", key);         
  firebaseJson.set("intensity", intensity);
  firebaseJson.set("status", STATUS_ON);
  if (Firebase.updateNode(firebaseData, PATH_ACTION, firebaseJson)) {
    return;
  } 
  firebaseErrorReason(firebaseData);
}

void websocketBegin() {
  Serial.println("Iniciando WebSocket");
  digitalWrite(LED_PIN3, HIGH);
  webSocket.begin("iot.sinric.com", 80, "/");
  Serial.println("Configurando o webSocket");
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MY_API_KEY);
  webSocket.setReconnectInterval(5000);
  digitalWrite(LED_PIN3, LOW);
  Serial.println("WebSocket configurado com sucesso!");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED: {
      isConnected = false;    
      Serial.printf("[WS] Webservice disconnected from sinric.com!\n");
    }
    break;
    case WStype_CONNECTED: {
      isConnected = true;
      Serial.printf("[WS] Service connected to sinric.com at url: %s\n", payload);
      Serial.printf("[WS] Waiting for commands from sinric.com ...\n");        
    }
    break;
    case WStype_TEXT: {
      Serial.printf("[WS] get text: %s\n", payload);   
      DynamicJsonDocument json(1024);
      deserializeJson(json, (char*) payload);
      String deviceId = json ["deviceId"];          
      if(deviceId.equals(TV_QUARTO)) {
        webSocketEventTV(isOnAction(json ["value"]["on"]));
      }
      if(deviceId.equals(MONITOR_QUARTO)) {
        webSocketEventMonitor();
      }
      if(deviceId.equals(AR_QUARTO)) {
        webSocketEventAr();
      }
      if(deviceId.equals(INFRAVERMELHO_QUARTO)) {
        webSocketEventInfravermelho(isOnAction(json ["value"]["on"]));
      }
    }
    break;
    case WStype_BIN: {
      Serial.printf("[WS] get binary length: %u\n", length);
    }
    break;
    default: break;
  }
}

bool isOnAction(String value) {
  if(value == "true") {
    return true;
  } 
  return false;
}

void webSocketEventTV(bool isOn) {
  String statusDevice = getFirebaseString(PATH_DEVICE_TV + "/status");
  int intensity = -1;
  if(!isOn && STATUS_ON.equals(statusDevice)) {
     intensity = 0;
  } else if(isOn && STATUS_OFF.equals(statusDevice)) {
     intensity = 1;
  }
  if(intensity != -1) {
     updateFirebaseJsonActionOn(TV_QUARTO, KEY_POWER, intensity);
  }
}

void webSocketEventMonitor() {
  Serial.println("webSocketEventMonitor");
}

void webSocketEventAr() {
  Serial.println("webSocketEventAr");
}

void webSocketEventInfravermelho(bool isOn) {
  String statusDevice = getFirebaseString(PATH_DEVICE_INFRAVERMELHO + "/status");
  int intensity = -1;
  if(!isOn && STATUS_ON.equals(statusDevice)) {
     intensity = 0;
  } else if(isOn && STATUS_OFF.equals(statusDevice)) {
     intensity = 1;
  }
  if(intensity != -1) {
     updateFirebaseJsonActionOn(INFRAVERMELHO_QUARTO, KEY_POWER, intensity);
  }
}

void irremoteBegin() {
  Serial.println("Iniciando IRremote");
  RECEIVER_IR.enableIRIn();
  Serial.println("IRremote configurado com sucesso!");
}

void sendIRCodesTV(int* codes) {
  Serial.println("sendIRCodesTV");
  Serial.println(_size);
  for (int i = 0; i <= _size; i++) {
    Serial.println(codes[i]);
    SENDER_IR.sendNEC(codes[i], 32);
  }
  Serial.println("sendIRCodesTV 01");
}

long ircode (decode_results *results) {
  Serial.print(results->value, HEX);
  return results->address;
}

String encoding (decode_results *results) {
  switch (results->decode_type) {
    default:
    case UNKNOWN:      return "UNKNOWN";       break ;
    case NEC:          return "NEC";           break ;
    case SONY:         return "SONY";          break ;
    case RC5:          return "RC5";           break ;
    case RC6:          return "RC6";           break ;
    case DISH:         return "DISH";          break ;
    case SHARP:        return "SHARP";         break ;
    case JVC:          return "JVC";           break ;
    case SANYO:        return "SANYO";         break ;
    case MITSUBISHI:   return "MITSUBISHI";    break ;
    case SAMSUNG:      return "SAMSUNG";       break ;
    case LG:           return "LG";            break ;
    case WHYNTER:      return "WHYNTER";       break ;
    case AIWA_RC_T501: return "AIWA_RC_T501";  break ;
    case PANASONIC:    return "PANASONIC";     break ;
    case DENON:        return "Denon";         break ;
  }
}

void dumpInfo (decode_results *results) {
  if (results->overflow) {
    Serial.println("IR code too long. Edit IRremoteInt.h and increase RAWBUF");
    return;
  }
  Serial.print("Encoding  : ");
  Serial.print(encoding(results));
  Serial.println("");
  Serial.print("Code      : ");
  ircode(results);
  Serial.print(" (");
  Serial.print(results->bits, DEC);
  Serial.println(" bits)");
}

void dumpCode (decode_results *results) {
  Serial.print("unsigned int  ");          // variable type
  Serial.print("rawData[");                // array name
  Serial.print(results->rawlen - 1, DEC);  // array size
  Serial.print("] = {");                   // Start declaration
  for (int i = 1;  i < results->rawlen;  i++) {
    Serial.print(results->rawbuf[i] * USECPERTICK, DEC);
    if ( i < results->rawlen-1 ) Serial.print(","); // ',' not needed on last one
    if (!(i & 1))  Serial.print(" ");
  }
  Serial.print("};"); 
  Serial.print("  // ");
  Serial.print(encoding(results));
  Serial.print(" ");
  ircode(results);
  Serial.println("");
  if (results->decode_type != UNKNOWN) {
    if (results->decode_type == PANASONIC) {
      Serial.print("unsigned int  addr = 0x");
      Serial.print(results->address, HEX);
      Serial.println(";");
    }
    Serial.print("unsigned int  data = 0x");
    Serial.print(results->value, HEX);
    Serial.println(";");
  }
}

void loop() {
  if(isReceiving) {
    decode_results  results;
    if (RECEIVER_IR.decode(&results)) {
      String marca = encoding(&results);
      Serial.println(marca);
      dumpInfo(&results);
      dumpCode(&results);
      Serial.print("Decimal: ");
      Serial.println(results.value, DEC);
      RECEIVER_IR.resume();
    }
  }
  
  webSocket.loop();
  if(isConnected) {
    uint64_t now = millis();
    if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
      heartbeatTimestamp = now;
      webSocket.sendTXT("H");          
    }
  }
}
