#include <SPI.h>
#include <LoRa.h>

// Definição dos pinos para o módulo LoRa RA02
#define SS   5   // NSS (Chip Select)
#define RST  14  // Reset
#define DIO0 2   // Interrupção digital
int i = 0;
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Sender (Teste)");

  // Configura os pinos do módulo LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSyncWord(0x72);
  LoRa.setSpreadingFactor(11);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(20);

  Serial.println("LoRa Initializing OK!");
}

void loop() {
  i+=1;
  String message = "Hello";
  Serial.print("Sending message: ");
  Serial.println(message);
  Serial.println(i);
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.print(i);
  LoRa.endPacket();
  delay(100); // Envia a cada 2 segundos

}