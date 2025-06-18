#include <Arduino.h>
#include <LoRa.h>
#include <AESLib.h>

// Definição dos pinos para o módulo LoRa RA02
#define SS   5   // NSS (Chip Select)
#define RST  14  // Reset
#define DIO0 2   // Interrupção digital

// Pinos dos botões
#define BTN_FORWARD  27
#define BTN_BACKWARD 26
#define BTN_LEFT     25
#define BTN_RIGHT    33
#define BTN_RELE     13

// --- Variáveis Globais e do FreeRTOS ---

// Chave AES de 16 bytes para AES-128
uint8_t key[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

// IV (Vetor de Inicialização) de 16 bytes para AES-CBC
uint8_t iv[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};

// Handle para a fila de comandos
QueueHandle_t comandoQueue;

// Enum para representar os comandos de forma eficiente
enum Comando {
  CMD_STOP,
  CMD_FORWARD,
  CMD_BACKWARD,
  CMD_LEFT,
  CMD_RIGHT,
  CMD_RELE
};

// --- Funções Auxiliares ---

// Função para aplicar padding PKCS#7
void pkcs7_pad(uint8_t *data, size_t *len, size_t block_size) {
  size_t padding = block_size - (*len % block_size);
  for (size_t i = 0; i < padding; i++) {
    data[*len + i] = padding;
  }
  *len += padding;
}

// Função para enviar o comando (agora otimizada)
void sendCommand(String command) {
  Serial.print("Enviando comando: ");
  Serial.println(command);

  // Converte mensagem para bytes
  size_t len = command.length();
  
  // Usa arrays na stack em vez de alocação dinâmica (new/delete) - mais seguro
  uint8_t plainText[len + 16]; 
  uint8_t cipherText[len + 16];
  
  memcpy(plainText, command.c_str(), len);
  size_t plainLen = len;
  pkcs7_pad(plainText, &plainLen, 16);

  // Criptografa com AES-CBC
  AESLib aesLib;
  uint8_t ivCopy[16];
  memcpy(ivCopy, iv, 16); // IV é modificado pela biblioteca, então usamos uma cópia
  aesLib.encrypt(plainText, plainLen, cipherText, key, sizeof(key), ivCopy);

  // Prepara pacote: IV + texto cifrado
  uint8_t packet[16 + plainLen];
  memcpy(packet, iv, 16);
  memcpy(packet + 16, cipherText, plainLen);

  // Envia o pacote via LoRa
  LoRa.beginPacket();
  LoRa.write(packet, 16 + plainLen);
  LoRa.endPacket(true);

  Serial.println("Comando enviado!");
}

// --- Tarefas do FreeRTOS ---

// Tarefa para ler o estado dos botões
void taskLeituraBotoes(void *pvParameters) {
  Serial.println("Task de Leitura de Botoes iniciada no Core 0");
  Comando cmd_a_enviar;
  
  for (;;) { // Loop infinito da tarefa
    cmd_a_enviar = CMD_STOP; // Comando padrão é "parado"

    if (digitalRead(BTN_FORWARD) == LOW) {
      cmd_a_enviar = CMD_FORWARD;
    } else if (digitalRead(BTN_BACKWARD) == LOW) {
      cmd_a_enviar = CMD_BACKWARD;
    } else if (digitalRead(BTN_LEFT) == LOW) {
      cmd_a_enviar = CMD_LEFT;
    } else if (digitalRead(BTN_RIGHT) == LOW) {
      cmd_a_enviar = CMD_RIGHT;
    } else if (digitalRead(BTN_RELE) == LOW) {
      cmd_a_enviar = CMD_RELE;
    }
    
    // Envia o comando para a fila. Outra tarefa irá processá-lo.
    // O '0' no final significa que não vamos esperar se a fila estiver cheia.
    xQueueSend(comandoQueue, &cmd_a_enviar, 0);

    // Pequeno delay para evitar sobrecarga da CPU e debouncing simples
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Tarefa para criptografar e enviar dados via LoRa
void taskEnvioLoRa(void *pvParameters) {
  Serial.println("Task de Envio LoRa iniciada no Core 1");
  Comando cmd_recebido;
  Comando ultimo_cmd_enviado = CMD_STOP;

  for (;;) { // Loop infinito da tarefa
    // Espera por um item na fila por tempo indefinido (portMAX_DELAY)
    if (xQueueReceive(comandoQueue, &cmd_recebido, portMAX_DELAY) == pdPASS) {
      
      // Só envia se o comando for diferente do último enviado
      // Isso evita enviar "p" repetidamente e só envia quando o estado muda.
      if (cmd_recebido != ultimo_cmd_enviado) {
        switch (cmd_recebido) {
          case CMD_FORWARD:
            sendCommand("FORWARD");
            break;
          case CMD_BACKWARD:
            sendCommand("BACKWARD");
            break;
          case CMD_LEFT:
            sendCommand("LEFT");
            break;
          case CMD_RIGHT:
            sendCommand("RIGHT");
            break;
          case CMD_RELE:
            sendCommand("RELE");
            break;
          case CMD_STOP:
            sendCommand("p"); // 'p' para parado/pause
            break;
        }
        ultimo_cmd_enviado = cmd_recebido;
      }
    }
  }
}


// --- Setup e Loop ---

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Controle Robô com FreeRTOS e LoRa");

  // Configura pinos dos botões
  pinMode(BTN_FORWARD, INPUT_PULLUP);
  pinMode(BTN_BACKWARD, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_RELE, INPUT_PULLUP);

  // Configura os pinos e inicializa o módulo LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Falha ao iniciar LoRa!");
    while (1);
  }
  LoRa.setSyncWord(0x72);
  LoRa.setTxPower(20);
  Serial.println("LoRa inicializado com sucesso!");

  // Cria a fila para comunicar os comandos entre as tarefas
  // A fila pode conter até 5 comandos do tipo 'Comando'
  comandoQueue = xQueueCreate(5, sizeof(Comando));

  if (comandoQueue == NULL) {
    Serial.println("Erro ao criar a fila!");
    while(1);
  }

  // Cria as tarefas e as fixa em núcleos específicos do ESP32
  xTaskCreatePinnedToCore(
      taskLeituraBotoes,    // Função da tarefa
      "LeituraBotoes",      // Nome da tarefa
      2048,                 // Tamanho da pilha (stack)
      NULL,                 // Parâmetros da tarefa
      1,                    // Prioridade
      NULL,                 // Handle da tarefa
      0);                   // Núcleo 0

  xTaskCreatePinnedToCore(
      taskEnvioLoRa,        // Função da tarefa
      "EnvioLoRa",          // Nome da tarefa
      4096,                 // Tamanho da pilha (maior por causa da criptografia e LoRa)
      NULL,                 // Parâmetros da tarefa
      2,                    // Prioridade (maior que a leitura)
      NULL,                 // Handle da tarefa
      1);                   // Núcleo 1
      
  Serial.println("Tarefas do FreeRTOS criadas. O sistema está rodando.");
}

void loop() {
  // O loop principal fica vazio. O escalonador do FreeRTOS está no controle.
  vTaskDelete(NULL); // Opcional: deleta a tarefa do loop() para liberar recursos.
}