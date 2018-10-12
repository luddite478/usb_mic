
#include <ESP8266WiFi.h>
#include <stdint.h> 
#include "RingBuffer.h"
#include <math.h>

#define SERIAL_DEBUG_ON true
unsigned int lastLog = 0;
unsigned int lastClientCheck = 0;

#define MICROPHONE_PIN A0

#define AUDIO_BUFFER_MAX 8192
#define SINGLE_PACKET_MIN 512
#define SINGLE_PACKET_MAX 1024

#define CPU_CYCLES_INTERVAL 5000 // for 16khz read

const char* ssid     = "-------";
const char* password = "--------";

const char* host = "-----";
uint16_t port = 3000;



uint8_t txBuffer[SINGLE_PACKET_MAX + 1];

SimpleRingBuffer audio_buffer;

// version without timer
unsigned long lastRead = micros();
unsigned long lastSend = millis();

WiFiClient client;
uint8_t co = 0;
void ICACHE_RAM_ATTR readMic(void) {
  if(co > 800){
    Serial.println("1 sec");
    co = 0;
  }
  uint16_t value = analogRead(MICROPHONE_PIN);
  value = map(value, 0, 4095, 0, 255);
  audio_buffer.put(value);    
  timer0_write(ESP.getCycleCount() + CPU_CYCLES_INTERVAL); // 80MHz == 1sec
  co++;
}

void setup() {
    #if SERIAL_DEBUG_ON
    Serial.begin(115200);
    #endif
    Serial.println();
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
    Serial.println(WiFi.localIP());
    if (client.connect(host, port )) {
       Serial.println("Client Connected");     
    } else {
       Serial.println("No Connection");
    }
    pinMode(MICROPHONE_PIN, INPUT);

    audio_buffer.init(AUDIO_BUFFER_MAX);

    noInterrupts();
    timer0_isr_init();
    timer0_attachInterrupt(readMic);
    timer0_write(ESP.getCycleCount() + CPU_CYCLES_INTERVAL); 
    interrupts();
}



void loop() {
    unsigned int now = millis();
    
    if ((now - lastClientCheck) > 2000) {
        lastClientCheck = now;
        client.write("hihi");
        if (!client.connected()) {
            Serial.println("Dicsonnected from server");
        }
    }

    sendEvery(2000);
}


void sendEvery(int delay_for_message) {
    if ((millis() - lastSend) >= delay_for_message) {
        sendAudio();
        client.write("hihi");
        lastSend = millis();
    }
}


void sendAudio(void) {
    if (!client.connected()) {
        return;
    }
    int count = 0;
    int storedSoundBytes = audio_buffer.getSize();


    while (count < storedSoundBytes) {
        int currSize = audio_buffer.getSize();
        if (currSize < SINGLE_PACKET_MIN) {
            break;
        }
        
        int size = currSize <= SINGLE_PACKET_MAX
          ? currSize
          : SINGLE_PACKET_MAX;
        int c = 0;
        for(int c = 0; c < size; c++) {
            txBuffer[c] = audio_buffer.get();
        }
        count += size;

        client.write(txBuffer, size);
    }
}
