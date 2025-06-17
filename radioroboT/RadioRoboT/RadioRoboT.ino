#include <SPI.h>
#include <LoRa.h>
#include <AESLib.h>

// Definição dos pinos para o módulo LoRa RA02
#define SS   5   // NSS (Chip Select)
#define RST  14  // Reset
#define DIO0 2   // Interrupção digital

// Chave AES de 16 bytes para AES-128 (deve ser igual no receptor)
uint8_t key[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

// IV (Initialization Vector) de 16 bytes para AES-CBC
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

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Sender (Teste com AES)");

  // Configura os pinos do módulo LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSyncWord(0x72);
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setTxPower(20);

  // Teste local de criptografia
  Serial.println("Teste local de AES:");
  AESLib aesLib;
  uint8_t testData[] = "Hello";
  size_t len = sizeof(testData) - 1;
  uint8_t* paddedData = new uint8_t[len + 16];
  memcpy(paddedData, testData, len);
  size_t paddedLen = len;
  pkcs7_pad(paddedData, &paddedLen, 16);
  uint8_t* cipher = new uint8_t[paddedLen];
  aesLib.encrypt(paddedData, paddedLen, cipher, key, sizeof(key), iv);
  Serial.print("Local CipherText: ");
  for (size_t i = 0; i < paddedLen; i++) {
    Serial.print(cipher[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  delete[] paddedData;
  delete[] cipher;

  Serial.println("LoRa Initializing OK!");
}

void loop() {
  String message = "Frente";
  Serial.print("Encrypting message: ");
  Serial.println(message);

  // Converte mensagem para bytes
  size_t len = message.length();
  uint8_t* plainText = new uint8_t[len + 16];
  memcpy(plainText, message.c_str(), len);
  size_t plainLen = len;
  pkcs7_pad(plainText, &plainLen, 16);

  // Depuração do texto plano com padding
  Serial.print("Padded PlainText: ");
  for (size_t i = 0; i < plainLen; i++) {
    Serial.print(plainText[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Criptografa com AES-CBC
  AESLib aesLib;
  uint8_t* cipherText = new uint8_t[plainLen];
  uint8_t ivCopy[16]; // Cria uma cópia para evitar sobrescrita
  memcpy(ivCopy, iv, 16);
  aesLib.encrypt(plainText, plainLen, cipherText, key, sizeof(key), ivCopy);

  // Prepara pacote: IV + texto cifrado
  uint8_t* packet = new uint8_t[16 + plainLen];
  memcpy(packet, iv, 16); // Usa IV original
  memcpy(packet + 16, cipherText, plainLen);

  // Depuração
  Serial.print("Packet size: ");
  Serial.println(16 + plainLen);
  Serial.print("IV: ");
  for (int i = 0; i < 16; i++) {
    Serial.print(packet[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  Serial.print("CipherText: ");
  for (int i = 0; i < plainLen; i++) {
    Serial.print(cipherText[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Envia o pacote
  LoRa.beginPacket();
  LoRa.write(packet, 16 + plainLen);
  LoRa.endPacket(true);

  // Libera memória
  delete[] plainText;
  delete[] cipherText;
  delete[] packet;

  delay(2000);
}