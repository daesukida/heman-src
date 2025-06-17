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

// IV (mesmo do transmissor)
uint8_t iv[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};

// Função para aplicar padding PKCS#7
void pkcs7_pad(uint8_t *data, size_t *len, size_t block_size) {
  size_t padding = block_size - (*len % block_size);
  for (size_t i = 0; i < padding; i++) {
    data[*len + i] = padding;
  }
  *len += padding;
}

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
  Serial.println("LoRa Receiver (Teste com AES)");

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSyncWord(0x72);
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  LoRa.receive();

  // Teste local de criptografia/descriptografia
  Serial.println("Teste local de AES:");
  AESLib aesLib;
  uint8_t testData[] = "Hello";
  size_t len = sizeof(testData) - 1;
  uint8_t* paddedData = new uint8_t[len + 16];
  memcpy(paddedData, testData, len);
  size_t paddedLen = len;
  pkcs7_pad(paddedData, &paddedLen, 16);
  uint8_t* cipher = new uint8_t[paddedLen];
  uint8_t ivCopy[16];
  memcpy(ivCopy, iv, 16);
  aesLib.encrypt(paddedData, paddedLen, cipher, key, sizeof(key), ivCopy);
  uint8_t* decrypted = new uint8_t[paddedLen];
  memcpy(ivCopy, iv, 16); // Restaura IV
  aesLib.decrypt(cipher, paddedLen, decrypted, key, sizeof(key), ivCopy);
  size_t unpaddedLen = paddedLen;
  pkcs7_unpad(decrypted, &unpaddedLen, 16);
  Serial.print("Decrypted test: ");
  for (size_t i = 0; i < unpaddedLen; i++) {
    Serial.print((char)decrypted[i]);
  }
  Serial.println();
  delete[] paddedData;
  delete[] cipher;
  delete[] decrypted;

  Serial.println("LoRa Initializing OK!");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received packet size: ");
    Serial.println(packetSize);

    // Validação inicial
    if (packetSize < 32) {
      Serial.println("Packet too small!");
      while (LoRa.available()) LoRa.read(); // Limpa buffer
      return;
    }

    // Lê o IV (16 bytes)
    uint8_t received_iv[16];
    for (int i = 0; i < 16; i++) {
      received_iv[i] = LoRa.read();
    }
    Serial.print("Received IV: ");
    for (int i = 0; i < 16; i++) {
      Serial.print(received_iv[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Lê o texto cifrado
    int cipherLen = packetSize - 16;
    uint8_t* cipherText = new uint8_t[cipherLen];
    for (int i = 0; i < cipherLen; i++) {
      cipherText[i] = LoRa.read();
    }
    Serial.print("Received CipherText: ");
    for (int i = 0; i < cipherLen; i++) {
      Serial.print(cipherText[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Descriptografa com AES-CBC
    AESLib aesLib;
    uint8_t* plainText = new uint8_t[cipherLen];
    uint8_t ivCopy[16];
    memcpy(ivCopy, received_iv, 16);
    aesLib.decrypt(cipherText, cipherLen, plainText, key, sizeof(key), ivCopy);

    // Depuração do texto plano antes de remover padding
    Serial.print("PlainText before unpad: ");
    for (int i = 0; i < cipherLen; i++) {
      Serial.print(plainText[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Remove padding PKCS#7
    size_t plainLen = cipherLen;
    pkcs7_unpad(plainText, &plainLen, 16);

    if (plainLen == 0) {
      Serial.println("Invalid padding");
    } else {
      String message = String((char*)plainText, plainLen);
      Serial.print("Received decrypted message: ");
      Serial.println(message);
    }

    delete[] cipherText;
    delete[] plainText;
  }
}