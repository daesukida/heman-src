#include <SPI.h>
#include <LoRa.h>
#include <AESLib.h>

// Definição dos pinos para o módulo LoRa
#define SS   5
#define RST  14
#define DIO0 2

// Definição dos pinos para controle dos motores (BTS7960) e relé
#define ponteHR_En   33
#define ponteHL_En   25
#define h1RPWM       26
#define h1LPWM       27
#define ponteH2R_En  12
#define ponteH2L_En  13
#define h2LPWM       17
#define h2RPWM       16
#define releEntrada1 15

// --- Variáveis Globais e do FreeRTOS ---

// Chave e IV para AES
uint8_t key[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
uint8_t iv[]  = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};

// Handles para as filas
QueueHandle_t loraPacketQueue;    // Fila para pacotes brutos LoRa
QueueHandle_t motorCommandQueue;  // Fila para comandos do motor já processados

// A DEFINIÇÃO DO TIMEOUT FOI REMOVIDA
// #define SAFETY_TIMEOUT_MS 500

// Enum para representar os comandos (deve ser o mesmo do transmissor)
enum Comando {
  CMD_STOP,
  CMD_FORWARD,
  CMD_BACKWARD,
  CMD_LEFT,
  CMD_RIGHT,
  CMD_RELE
};

// Estrutura para passar pacotes LoRa na fila
struct LoraPacket {
  uint8_t buffer[128]; // Tamanho seguro para os pacotes esperados
  size_t size;
};

// --- Funções Auxiliares de Criptografia e Motores ---

// Função para remover padding PKCS#7 (sem modificações)
void pkcs7_unpad(uint8_t *data, size_t *len, size_t block_size) {
  if (*len == 0) return;
  size_t padding = data[*len - 1];
  if (padding > block_size || padding == 0) { *len = 0; return; }
  for (size_t i = 1; i < padding; i++) {
    if (data[*len - i] != padding) { *len = 0; return; }
  }
  *len -= padding;
}

// Funções de controle dos motores
void stopAllMovements() {
  digitalWrite(h1RPWM, LOW); digitalWrite(h1LPWM, LOW);
  digitalWrite(h2RPWM, LOW); digitalWrite(h2LPWM, LOW);
}

void moveForward() {
  digitalWrite(h1RPWM, LOW);  digitalWrite(h1LPWM, HIGH);
  digitalWrite(h2RPWM, LOW);  digitalWrite(h2LPWM, HIGH);
}

void moveBackward() {
  digitalWrite(h1RPWM, HIGH); digitalWrite(h1LPWM, LOW);
  digitalWrite(h2RPWM, HIGH); digitalWrite(h2LPWM, LOW);
}

void turnLeft() {
  digitalWrite(h1RPWM, HIGH); digitalWrite(h1LPWM, LOW);
  digitalWrite(h2RPWM, LOW);  digitalWrite(h2LPWM, HIGH);
}

void turnRight() {
  digitalWrite(h1RPWM, LOW);  digitalWrite(h1LPWM, HIGH);
  digitalWrite(h2RPWM, HIGH); digitalWrite(h2LPWM, LOW);
}

void toggleRele() {
  digitalWrite(releEntrada1, !digitalRead(releEntrada1));
}


// --- Tarefas do FreeRTOS ---

// Tarefa 1: Apenas recebe pacotes LoRa e os coloca na fila (sem modificações)
void taskRecepcaoLoRa(void *pvParameters) {
  Serial.println("Task de Recepção LoRa iniciada.");
  for (;;) {
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0 && packetSize < 128) {
      LoraPacket packet;
      packet.size = packetSize;
      LoRa.readBytes(packet.buffer, packetSize);
      
      // Envia o pacote para a fila de processamento sem bloquear
      xQueueSend(loraPacketQueue, &packet, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Pequena pausa para não sobrecarregar a CPU
  }
}

// Tarefa 2: Processa os pacotes da fila (descriptografa) (sem modificações)
void taskProcessamentoComando(void *pvParameters) {
  Serial.println("Task de Processamento de Comandos iniciada.");
  LoraPacket receivedPacket;
  AESLib aesLib;
  
  uint8_t plainText[128];
  
  for (;;) {
    if (xQueueReceive(loraPacketQueue, &receivedPacket, portMAX_DELAY) == pdPASS) {
      if (receivedPacket.size < 16) continue;

      uint8_t* received_iv = receivedPacket.buffer;
      uint8_t* cipherText = receivedPacket.buffer + 16;
      size_t cipherLen = receivedPacket.size - 16;

      aesLib.decrypt(cipherText, cipherLen, plainText, key, sizeof(key), received_iv);
      
      size_t plainLen = cipherLen;
      pkcs7_unpad(plainText, &plainLen, 16);

      Comando final_cmd = CMD_STOP;
      if (plainLen > 0) {
        String message = String((char*)plainText, plainLen);
        Serial.print("Comando recebido: ");
        Serial.println(message);
        
        if (message == "FORWARD")  final_cmd = CMD_FORWARD;
        else if (message == "BACKWARD") final_cmd = CMD_BACKWARD;
        else if (message == "LEFT")     final_cmd = CMD_LEFT;
        else if (message == "RIGHT")    final_cmd = CMD_RIGHT;
        else if (message == "RELE")     final_cmd = CMD_RELE;
        else                            final_cmd = CMD_STOP;
      } else {
        Serial.println("Erro de padding ou descriptografia.");
      }
      xQueueSend(motorCommandQueue, &final_cmd, 0);
    }
  }
}

// ######################################################################
// ## TAREFA 3 MODIFICADA PARA REMOVER O TIMEOUT DE SEGURANÇA           ##
// ######################################################################
void taskControleMotores(void *pvParameters) {
  Serial.println("Task de Controle dos Motores iniciada (sem timeout).");
  Comando cmd;

  for (;;) {
    // Espera INDEFINIDAMENTE por um comando na fila.
    // A tarefa ficará bloqueada (sem consumir CPU) até um comando chegar.
    if (xQueueReceive(motorCommandQueue, &cmd, portMAX_DELAY) == pdPASS) {
      // Assim que um comando é recebido, ele é executado.
      // O robô manterá este estado até o próximo comando chegar.
      switch(cmd) {
        case CMD_FORWARD:   moveForward();  break;
        case CMD_BACKWARD:  moveBackward(); break;
        case CMD_LEFT:      turnLeft();     break;
        case CMD_RIGHT:     turnRight();    break;
        case CMD_RELE:      toggleRele();   break;
        case CMD_STOP:
        default:            stopAllMovements(); break;
      }
    }
    // O bloco "else" para o timeout foi completamente removido.
  }
}

// --- Setup e Loop ---

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Receptor Robô com FreeRTOS");
  
  pinMode(h1RPWM, OUTPUT); pinMode(h1LPWM, OUTPUT);
  pinMode(h2RPWM, OUTPUT); pinMode(h2LPWM, OUTPUT);
  pinMode(ponteHR_En, OUTPUT); pinMode(ponteHL_En, OUTPUT);
  pinMode(ponteH2R_En, OUTPUT); pinMode(ponteH2L_En, OUTPUT);
  pinMode(releEntrada1, OUTPUT);

  digitalWrite(ponteHR_En, HIGH); digitalWrite(ponteHL_En, HIGH);
  digitalWrite(ponteH2R_En, HIGH); digitalWrite(ponteH2L_En, HIGH);
  digitalWrite(releEntrada1, HIGH); 
  stopAllMovements();

  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Falha ao iniciar LoRa!");
    while (1);
  }
  LoRa.setSyncWord(0x72);
  LoRa.receive();
  Serial.println("LoRa inicializado. Aguardando pacotes...");

  loraPacketQueue = xQueueCreate(5, sizeof(LoraPacket));
  motorCommandQueue = xQueueCreate(5, sizeof(Comando));

  xTaskCreatePinnedToCore(taskRecepcaoLoRa, "RecepcaoLoRa", 2048, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(taskProcessamentoComando, "Processamento", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskControleMotores, "ControleMotores", 2048, NULL, 2, NULL, 0);
  
  Serial.println("Sistema FreeRTOS iniciado.");
}

void loop() {
  vTaskDelete(NULL);
}