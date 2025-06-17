#include <SPI.h>
#include <LoRa.h>

// Definição dos pinos para o módulo LoRa
#define SS   5   // NSS
#define RST  14  // RST
#define DIO0 2   // DIO0

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Receiver (Teste)");

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSyncWord(0x72);
  LoRa.setSpreadingFactor(11);
  LoRa.setSignalBandwidth(125E3);
  LoRa.receive();

  Serial.println("LoRa Initializing OK!");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String message = "";
    while (LoRa.available()) {
      message += (char)LoRa.read();
    }
    Serial.print("Received message: ");
    Serial.println(message);
  }
}