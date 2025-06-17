#include <SPI.h>
#include <LoRa.h>
#include <AESLib.h>

// Definição dos pinos para o módulo LoRa
#define SS   5   // NSS
#define RST  14  // RST
#define DIO0 2   // DIO0

// Chave AES (mesma do transmissor)
uint8_t key[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

// Função para remover padding PKCS#7
void pkcs7_unpad(uint8_t *data, size_t *len, size_t block_size) {
  if (*len == 0) return;
  size_t padding = data[*len - 1];
  if (padding > block_size || padding == 0) {
    *len = 0;
    return;
  }
  for (size_t i = 1; i < padding; i++) {
    if (data[*len - i] != padding) {
      *len = 0;
      return;
    }
  }
  *len -= padding;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Receiver (Controle Remoto)");

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSyncWord(0x72);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  // LoRa.setCRC(true); // Removido, pois não é suportado
  LoRa.receive();

  Serial.println("LoRa Initializing OK!");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    uint8_t received_iv[16];
    for (int i = 0; i < 16; i++) {
      received_iv[i] = LoRa.read();
    }

    int cipherLen = packetSize - 16;
    uint8_t* cipherText = new uint8_t[cipherLen];
    for (int i = 0; i < cipherLen; i++) {
      cipherText[i] = LoRa.read();
    }

    AESLib aesLib;
    uint8_t* plainText = new uint8_t[cipherLen];
    aesLib.decrypt(cipherText, cipherLen, plainText, key, sizeof(key), received_iv);

    size_t plainLen = cipherLen;
    pkcs7_unpad(plainText, &plainLen, 16);

    if (plainLen == 0) {
      Serial.println("Invalid padding");
      delete[] cipherText;
      delete[] plainText;
      return;
    }

    String message = String((char*)plainText, plainLen);
    Serial.print("Received decrypted command: ");
    Serial.println(message);

    int colonIndex = message.indexOf(':');
    if (colonIndex != -1) {
      String command = message.substring(colonIndex + 1);
      Serial.print("Command received: ");
      Serial.println(command);
      // Adicione lógica para executar o comando aqui
    }

    delete[] cipherText;
    delete[] plainText;
  }
}