#include <SPI.h>
#include <LoRa.h>

#define ss   5  // NSS
#define rst  14 // RST
#define dio0 2  // DIO0

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println("LoRa Receiver");

  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(433E6)) { // Ajuste a frequência conforme necessário
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received packet: ");
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }
    Serial.println();
  }
}