#include <SPI.h>
#include <LoRa.h>

#define ss   5  // NSS
#define rst  14 // RST
#define dio0 2  // DIO0

int i=0;

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println("LoRa Sender");

  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(433E6)) { // Ajuste a frequência conforme necessário
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {
  i+=1;
  Serial.print("Sending packet: ");
  Serial.println("Hello, LoRa!");

  LoRa.beginPacket();
  //LoRa.print("Hello, LoRa!");
  LoRa.printf("Hello, Lora", i);
  Serial.println(i);
  LoRa.endPacket();

  delay(1000); // Envia a cada 1 segundo
}