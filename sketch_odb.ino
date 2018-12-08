//Include the SoftwareSerial library
#include "SoftwareSerial.h"
#include <SdFat.h>

#define pinoKey 9
#define pinoEN 8
#define quantEnderecosBT 5
#define tamanhoBufferBT 16 // 14-15 de endereço com vírgulas + NULL

// Estados de funcionamento
//Inicial
#define DEBUGGING -1
#define DESCONFIGURADO 0
//Slave
#define S_ESPERANDO_CONEXAO 1
#define S_PAREADO 2
//Master
#define M_ESPERANDO_MOVIMENTO 5
#define M_LENDO_DADOS_OBD 6
#define M_TOLERANCIA_PARADA 7
//Operações
#define OP_MODO_AT_SLAVE 20
#define OP_MODO_AT_MASTER 21

//Create a new software  serial
SoftwareSerial bluetooth(2, 3); // TX, RX (Bluetooth) 



const int tamanhoComando = 32;

unsigned long tempoInicial; // Guarda o momento do início de uma tarefa
unsigned long tempoAtual; // Guarda o momento atual
unsigned long tempoInicialTolerancia; // Guarda o momento do início de uma tarefa
unsigned long tempoAtualTolerancia; // Guarda o momento atual
const unsigned long tolerancia = 30000L * 1L; // Tolerância de (30s)15min antes de encerrar uma viagem
bool errosLidos = false; // Guarda se os códigos de erro já foram lidos durante aquela viagem 
long respostaOBD;
bool emMovimento;

int byteLido;      // a variable to read incoming serial data into
char buffer[tamanhoComando];
int ultimoChar;
bool modoAT = false;
bool isKeyOn = false;
int estado = DEBUGGING;
SdFat sdCard;
SdFile meuArquivo;

void setup() {
  //Initialize the software serial
  bluetooth.begin(9600);
  Serial.begin(9600);
  
  pinMode(pinoKey, OUTPUT);
  pinMode(pinoEN, OUTPUT);
  digitalWrite(pinoEN, HIGH);
  
  if(!sdCard.begin(4,SPI_HALF_SPEED))sdCard.initErrorHalt();
  
  Serial.println(F("Use o final de linha Newline."));
  Serial.println(F("Digite ajuda para ver os comandos disponiveis."));
}

void loop() {
  if(estado == DEBUGGING){
    if(modoAT){
      if(Serial.available()){
        byteLido = Serial.read();
        if(byteLido != '\n'){
          buffer[ultimoChar] = (char) byteLido;
          ultimoChar++;
          if(ultimoChar == tamanhoComando){
            ultimoChar--;
          }
        } else {
          buffer[ultimoChar] = '\0';
          limparBufferSerial();
          processarComando();
        }
      }
      if (bluetooth.available())
      {
          byteLido = bluetooth.read();
          Serial.write(byteLido);
      }
    }
    else{
      if(Serial.available()){
        byteLido = Serial.read();
        if(byteLido != '\n'){
          buffer[ultimoChar] = (char) byteLido;
          ultimoChar++;
          if(ultimoChar == tamanhoComando){
            ultimoChar--;
          }
        } else {
          buffer[ultimoChar] = '\0';
          limparBufferSerial();
          processarComando();
        }
      }
      // see if there's incoming serial data:
      if (bluetooth.available() > 0) {
        // read the oldest byte in the serial buffer:
        byteLido = bluetooth.read();
        
        Serial.print(byteLido);
        Serial.println(F(" "));
        //bluetooth.println(byteLido);
      }
    }
  }
  else if (estado == M_ESPERANDO_MOVIMENTO) {
    //Condição para troca para o modo slave aqui
    
    Serial.println(F("Lendo velocidade!"));
    lerRespostaOBD(13, 1); // ler velocidade
    Serial.print(F("Velocidade:"));
    Serial.println(respostaOBD);
    if(respostaOBD > 5L){
      Serial.println(F("Carro em movimento!"));
      estado = M_LENDO_DADOS_OBD;
      emMovimento = true;
      if (!meuArquivo.open("leitura_OBDII.txt", O_RDWR | O_CREAT | O_AT_END))
      {
        sdCard.errorHalt("Erro na abertura do arquivo leitura_OBDII.txt!");
      }
      delaySuperior(1000);
      meuArquivo.print("VIAGEMINI - ");
      meuArquivo.print(millis());
      meuArquivo.println(":");
      meuArquivo.write("{");
      lerRespostaOBD(81, 1); // PID 81, 1 Byte de resposta
      gravarRespostaOBD("FuelType");
      lerRespostaOBD(33, 2); // PID 33, 2 Bytes de resposta
      // Sem cálculo
      gravarRespostaOBD("DistanceWithMILOn");
      enviarComandoOBD(3);
      delaySuperior(500);
      char resp[4];
      lerBytesBluetooth(3, resp);
      if(strncmp("43 \0", resp, 4) == 0){
        Serial.println(F("DTCs detectados!"));
        meuArquivo.write("\"DTCs\":[");
        int primeiroByte = 0;
        int segundoByte = 0;
        int i = 0;
        lerBytesBluetooth(3, buffer);
        primeiroByte = strtol(buffer, NULL, HEX);
          
        lerBytesBluetooth(3, buffer);
        segundoByte = strtol(buffer, NULL, HEX);
        do {
          for(i = 0; i < 3; i++){
            int primeiroDTC;
            int segundoDTC;
            int outrosDTC;
            
            primeiroDTC = primeiroByte & B00000011;
            if(primeiroDTC == 0){
              meuArquivo.write("\"P");
            } else if (primeiroDTC == 1){
              meuArquivo.write("\"C");
            } else if (primeiroDTC == 2){
              meuArquivo.write("\"B");
            } else if (primeiroDTC == 3){
              meuArquivo.write("\"U");
            }
            segundoDTC = primeiroByte & B00001100;
            meuArquivo.write(segundoDTC);
            
            outrosDTC = primeiroByte & B11110000;
            meuArquivo.print(outrosDTC, HEX);
            
            outrosDTC = segundoByte & B00001111;
            meuArquivo.print(outrosDTC, HEX);
            
            outrosDTC = segundoByte & B11110000;
            meuArquivo.print(outrosDTC, HEX);
            meuArquivo.write("\"");
            if(i != 2){ // Só tentar ler mais códigos + 2 vezes (0 e 1)
              lerBytesBluetooth(3, buffer);
              primeiroByte = strtol(buffer, NULL, HEX);
              
              lerBytesBluetooth(3, buffer);
            segundoByte = strtol(buffer, NULL, HEX);
            }
            
            if((primeiroByte + segundoByte) > 0){ // se a próxima sequência tiver mais códigos
              meuArquivo.write("\",");
            } else {
              break;
            }
          }
          descartarLinhaBluetooth(true, 0);
          lerBytesBluetooth(3, resp);
        } while (strncmp("43 \0", resp, 4) == 0); // Enquanto a próxima linha tiver mais códigos
        
        limparBufferSerialBluetooth();
        
        meuArquivo.write("]");
        meuArquivo.write("\"leiturasDuranteDTC\":{");
        lerRespostaOBDFreeze(17, 1); // PID 4, 1 Byte de resposta
        respostaOBD = (respostaOBD * 100) / 255;
        gravarRespostaOBD("AccelerationLoad");
        
        lerRespostaOBDFreeze(4, 1); // PID 4, 1 Byte de resposta
        respostaOBD = (respostaOBD * 100) / 255;
        gravarRespostaOBD("EngineLoad");
        
        lerRespostaOBDFreeze(5, 1); // PID 5, 1 Byte de resposta
        respostaOBD = respostaOBD - 40;
        gravarRespostaOBD("EngineTemp");
        
        lerRespostaOBDFreeze(12, 2); // PID 12, 2 Bytes de resposta
        respostaOBD = respostaOBD / 4;
        gravarRespostaOBD("EngineRPM");
        
        lerRespostaOBDFreeze(13, 1); // PID 13, 1 Byte de resposta
        // Sem cálculo
        gravarRespostaOBD("VehicleSpeed");
        
        lerRespostaOBDFreeze(47, 1); // PID 4, 1 Byte de resposta
        respostaOBD = (respostaOBD * 100) / 255;
        gravarRespostaOBD("FuelLevel");
        
        lerRespostaOBDFreeze(49, 2); // PID 49, 2 Bytes de resposta
        // Sem cálculo
        gravarRespostaOBD("DistanceWithoutMILOn");
        
        lerRespostaOBDFreeze(51, 1); // PID 51, 1 Bytes de resposta
        // Sem cálculo
        gravarRespostaOBD("BarometricPressure", false);
        meuArquivo.write("}");
      }
      meuArquivo.write("\"viagem\": [");
    }
  } else if (estado == M_LENDO_DADOS_OBD){
    long velocidade;
    Serial.println(F("Gravando leituras..."));
    meuArquivo.write("{");
    lerRespostaOBD(17, 1); // PID 4, 1 Byte de resposta
    respostaOBD = (respostaOBD * 100) / 255;
    gravarRespostaOBD("AccelerationLoad");
    
    lerRespostaOBD(4, 1); // PID 4, 1 Byte de resposta
    respostaOBD = (respostaOBD * 100) / 255;
    gravarRespostaOBD("EngineLoad");
    
    lerRespostaOBD(5, 1); // PID 5, 1 Byte de resposta
    respostaOBD = respostaOBD - 40;
    gravarRespostaOBD("EngineTemp");
    
    lerRespostaOBD(12, 2); // PID 12, 2 Bytes de resposta
    respostaOBD = respostaOBD / 4;
    gravarRespostaOBD("EngineRPM");
    
    lerRespostaOBD(13, 1); // PID 13, 1 Byte de resposta
    velocidade = respostaOBD;
    // Sem cálculo
    gravarRespostaOBD("VehicleSpeed");
    
    lerRespostaOBD(33, 2); // PID 33, 2 Bytes de resposta
    // Sem cálculo
    gravarRespostaOBD("DistanceWithMILOn");
    
    lerRespostaOBD(47, 1); // PID 4, 1 Byte de resposta
    respostaOBD = (respostaOBD * 100) / 255;
    gravarRespostaOBD("FuelLevel");
    
    lerRespostaOBD(49, 2); // PID 49, 2 Bytes de resposta
    // Sem cálculo
    gravarRespostaOBD("DistanceWithoutMILOn");
    
    lerRespostaOBD(51, 1); // PID 51, 1 Bytes de resposta
    // Sem cálculo
    gravarRespostaOBD("BarometricPressure", false);
    meuArquivo.write("}");
    if(velocidade == 0){
      Serial.println(F("Carro parado! Esperando periodo de tolerancia."));
      estado = M_TOLERANCIA_PARADA;
      tempoInicialTolerancia = millis();
      tempoAtualTolerancia = tempoInicialTolerancia;
    } else {
      meuArquivo.write(",");
      delaySuperior(2000);
    }
  }
  else if (estado == S_ESPERANDO_CONEXAO){
    if(bluetooth.available()){
      Serial.print("Mensagem bt:");
      if(bluetooth.read() == 83){
        while (meuArquivo.available()) {
          bluetooth.write(meuArquivo.read());
        }
        // close the file:
        meuArquivo.close();
      }
        meuArquivo.open("leitura_OBDII.txt");
        lerBytesBluetooth(2);
      
    }
  }
  else if (estado == M_TOLERANCIA_PARADA){
    //Condição para troca para o modo slave aqui
    if(tempoAtualTolerancia - tempoInicialTolerancia > tolerancia){
      meuArquivo.println("]}");
      estado = M_ESPERANDO_MOVIMENTO;
      errosLidos = false;
      Serial.println(F("Fim de viagem!"));
      meuArquivo.close();
    } else {
      delaySuperior(2000);
      tempoAtualTolerancia = millis();
    }
    lerRespostaOBD(13, 1); // PID 13, 1 Byte de resposta
    if(respostaOBD > 5){
      Serial.println(F("Carro voltou a se mover!"));
      meuArquivo.write(",");
      estado = M_LENDO_DADOS_OBD;
    }
  }
}

// Atenção: território dos métodos utilitários
// Não espere ver ordem alguma a partir desse ponto

void limparBufferSerial(){
  delaySuperior(500);
  int chars = Serial.available();
  int i;
  for(i = 0; i < chars; i++){
    Serial.read();
  }
}

void limparBufferSerialBluetooth(){
  delaySuperior(500);
  int numChars = bluetooth.available();
  int i;
  int leitura;
  Serial.print(numChars);
  Serial.print(F(" Bytes descartados: ["));
  for(i = 0; i < numChars; i++){
    leitura = lerByteBluetooth();
    if(leitura == 10){
      Serial.print(F("<LF>"));
    }
    else if(leitura == 13){
      Serial.print(F("<CR>"));
    } else {
      Serial.print((char) leitura);
    }
  }
  Serial.println(F("]"));
}

void descartarLinhaBluetooth(bool debug){
  descartarLinhaBluetooth(debug, 500);
}

void descartarLinhaBluetooth(bool debug, unsigned long delay){
  delaySuperior(delay);
  int numChars = bluetooth.available();
  int i;
  int leitura;
  if(debug){
    Serial.print(F("Descartando ate o fim da linha: ["));
  }
  for(i = 0; i < numChars; i++){
    leitura = lerByteBluetooth();
    if(leitura == 10){
      if(debug){
        Serial.print(F("<LF>"));
      }
      break;
    }
    else if(leitura == 13){
      if(debug){
        Serial.print(F("<CR>"));
      }
    } else {
      if(debug){
        Serial.print((char) leitura);
      }
    }
  }
  if(debug){
    Serial.println(F("]"));
  }
}

void autoConfiguracao(bool modoMaster){
  if(!alternarModoAT(true)){
    return;
  }
  if(modoMaster){
    enviarComandoAT("DISC");
    descartarLinhaBluetooth(true);
    descartarLinhaBluetooth(true);
    
    enviarComandoAT("UART=","9600,0,0"); // alterando baud de comunicação sem fio
    //enviarComandoAT("UART=", "115200, 0, 0");
    descartarLinhaBluetooth(false); // Descartando OK
    
    enviarComandoAT("NAME=", "Leitor Peca Preco"); // setando nome
    descartarLinhaBluetooth(false); // Descartando OK
    
    enviarComandoAT("PSWD=", "1234"); // setando senha
    descartarLinhaBluetooth(false); // Descartando OK
    
    enviarComandoAT("RMAAD"); // removendo lista de disp pareados
    descartarLinhaBluetooth(false); // Descartando OK
    
    enviarComandoAT("ROLE=", "1"); // Setando role para Master
    descartarLinhaBluetooth(false); // Descartando OK
    
    enviarComandoAT("CMODE=", "0"); // permitir conectar-se a qualquer um
    descartarLinhaBluetooth(false); // Descartando OK
    
    enviarComandoAT("INQM=", "0,5,9"); // setando parâmetros de busca para buscar 5 disps em 9 segundos
    descartarLinhaBluetooth(false); // Descartando OK
    
    enviarComandoAT("INIT"); // Inicia o perfil de porta serial
    descartarLinhaBluetooth(false); // Descartando OK
    
    delaySuperior(1500);
    limparBufferSerialBluetooth(); // descartar se sobrou algo
    bool pareado = false;
    char bufferBluetooth[quantEnderecosBT][tamanhoBufferBT]; // Guarda os endereços dos dispositivos encontrados
    Serial.println(F("Comecando busca por dispositivos Bluetooth."));
    while(!pareado){
      enviarComandoAT("INIT"); // Inicia o perfil de porta serial
      descartarLinhaBluetooth(false); // Descartando OK
      enviarComandoAT("RMAAD"); // removendo lista de disp pareados
      descartarLinhaBluetooth(false); // Descartando OK
      enviarComandoAT("INQ"); // Começa a busca por dispositivos
      
      delaySuperior(12000); // Espera 12s
      if(bluetooth.peek() == 'O'){ // Nenhum dispositivo encontrado
        Serial.println(F("Nenhum dispositivo Bluetooth detectado."));
        descartarLinhaBluetooth(false);
      }
      int numeroEnderecosLidos = 0;
      for(int i = 0; i < quantEnderecosBT; i++){
        if(bluetooth.peek() == '+'){
          lerInquiryResponse(bufferBluetooth[i]);
          descartarLinhaBluetooth(true);
          numeroEnderecosLidos++;
        } else {
          break;
        }
      }
      
      delaySuperior(2000);
      limparBufferSerialBluetooth();
      
      for(int i = 0; i < numeroEnderecosLidos; i++){
        descartarLinhaBluetooth(true);
        enviarComandoAT("RNAME?", bufferBluetooth[i]); // Verificar os nomes dos disps descobertos
        delaySuperior(2000);
        char resposta[8];
        char inicioResposta = lerByteBluetooth();
        char gambiarra = lerByteBluetooth(); // as vezes a primeira leitura consegue um espaço não sei por que
        char gambiarra2 = lerByteBluetooth(); // e as vezes vem um \r\n DO NADA também
        Serial.print("Peeks RNAME:");
        Serial.print(inicioResposta);
        Serial.print(", ");
        Serial.print(gambiarra);
        Serial.print(", ");
        Serial.println(gambiarra2);
        if(inicioResposta == '+' || gambiarra == '+'){
          if(gambiarra == '+'){
            lerBytesBluetooth(5); // Descartando "NAME:"
          } else if (gambiarra2 == '+') {
            lerBytesBluetooth(6); // Descartando "RNAME:"
          } else {
            lerBytesBluetooth(4); // Descartando "AME:"
          }
          lerBytesBluetooth(7, resposta);
          
          Serial.print(F("Inicio do nome do dispositivo:"));
          Serial.println(resposta);
          if(strncmp(resposta, "OBDII\r\n", 7) == 0){
            Serial.println(F("OBDII encontrado!"));
            limparBufferSerialBluetooth(); // Bytes em um nome de dispositivo variam, melhor esperar e limpar todo o buffer
            enviarComandoAT("PAIR=", bufferBluetooth[i], ",30");// tentar parear com timeout de 9s
            delaySuperior(30000);
            lerBytesBluetooth(4, resposta);
            Serial.print("Resposta pair:");
            Serial.println(resposta);
            if(strncmp(resposta, "OK\r\n\0", 4) == 0){
              Serial.println(F("Pareado ao OBD2!"));
              enviarComandoAT("DISC");
              delaySuperior(2000);
              descartarLinhaBluetooth(true);
              descartarLinhaBluetooth(true);
              //Serial.println(F("Ligado ao OBD2!"));
              //Serial.println(F("Configuracao completa."));
              enviarComandoAT("CMODE=", "1"); // Arduino só se conectará ao OBD2 agora
              //if (!meuArquivo.open("leitura_OBDII.txt", O_RDWR | O_CREAT | O_AT_END)) // Preparando para escrever
              //{
              //  sdCard.errorHalt("Erro na abertura do arquivo leitura_OBDII.txt!");
              //}
              //estado = M_ESPERANDO_MOVIMENTO;
              //delaySuperior(1000);
              //limparBufferSerialBluetooth();
              //alternarModoAT(false);
              //return;
              enviarComandoAT("LINK=", bufferBluetooth[i]); //LINK(conectar) não é necessário, o pareamento também conecta
              lerBytesBluetooth(4, resposta);
              if(strncmp(resposta, "OK\r\n\0", 4) == 0){
                Serial.println(F("Ligado ao OBD2!"));
                Serial.println(F("Configuracao completa"));
                enviarComandoAT("CMODE=", "1"); // Arduino só se conectará ao OBD2 agora
                alternarBaudBluetooth(false);
                estado = M_ESPERANDO_MOVIMENTO;
                delaySuperior(1000);
                limparBufferSerialBluetooth();
                alternarModoAT(false);
                return;
              }
              else {
                  Serial.println(F("O link falhou!"));
                  enviarComandoAT("RMAAD"); // removendo lista de disp pareados
                  descartarLinhaBluetooth(false); // Descartando OK
              } 
            } else {
              Serial.println(F("O pareamento falhou!"));
            }
          } else {
            descartarLinhaBluetooth(true); // Descarta o resto do nome que não é obd
            descartarLinhaBluetooth(true); // Descarta o OK depois da resposta RNAME
          }
        }
      }
      // Se o for terminou e nenhum dispositivo OBD foi encontrado
      Serial.println(F("Nenhum adaptador OBD foi encontrado. Tentando novamente."));
      delaySuperior(1000);
      limparBufferSerialBluetooth();
    }
  } else { // Se é para trocar para o modo slave
    enviarComandoAT("RMAAD"); // removendo lista de disp pareados
    descartarLinhaBluetooth(false); // Descartando OK
    enviarComandoAT("ROLE=", "0"); // removendo lista de disp pareados
    descartarLinhaBluetooth(false); // Descartando OK
    enviarComandoAT("NAME=", "Interface Peça Preço"); // setando nome
    descartarLinhaBluetooth(false); // Descartando OK
    
    alternarModoAT(false);
    meuArquivo.open("leitura_OBDII.txt");
    estado = S_ESPERANDO_CONEXAO;
  }
}

// Envia uma solicitação OBDII. Default: serviço 1
void enviarComandoOBD(int comando){
  enviarComandoOBD(1, comando, true);
}

// Envia uma solicitação OBDII para algum serviço
void enviarComandoOBD(int servico, int comando, bool usarComando){
  bluetooth.print(servico, HEX);
  if(usarComando){
    bluetooth.print(comando, HEX);
  }
  bluetooth.write("\r\n");
  delaySuperior(250);
  descartarLinhaBluetooth(true, 500);
}

// Envia uma solicitação OBDII
void lerRespostaOBD(int comando, int quantBytes){
  enviarComandoOBD(comando);
  lerBytesBluetooth(6);
  lerBytesBluetooth(2, buffer);
  respostaOBD = strtol(buffer, NULL, HEX);
  if(quantBytes == 2) {
    lerBytesBluetooth(2, buffer);
    respostaOBD = respostaOBD * 0x100;
    respostaOBD = respostaOBD + strtol(buffer, NULL, HEX);
  }
  descartarLinhaBluetooth(true, 0);
}

// Envia uma solicitação OBDII para o serviço 2
void lerRespostaOBDFreeze(int comando, int quantBytes){
  enviarComandoOBD(2, comando, true);
  lerBytesBluetooth(9);
  lerBytesBluetooth(2, buffer);
  respostaOBD = strtol(buffer, NULL, HEX);
  if(quantBytes == 2) {
    lerBytesBluetooth(2, buffer);
    respostaOBD = respostaOBD * 0x100;
    respostaOBD = respostaOBD + strtol(buffer, NULL, HEX);
  }
  descartarLinhaBluetooth(true, 0);
}

//grava uma resposta OBDII no SD
void gravarRespostaOBD(const char *nomeDado, bool usarVirgula){
  meuArquivo.write("\"");
  meuArquivo.write(nomeDado);
  meuArquivo.write("\":");
  
  meuArquivo.print(respostaOBD);
  
  if(usarVirgula){
    meuArquivo.write(",");
  }
}
// Default é com vírgula
void gravarRespostaOBD(const char* nomeDado){
  gravarRespostaOBD(nomeDado, true);
}
// Envia um comando, opcionalmente com um parâmetro e sufixo.
// Envia AT puro caso o comando também seja vazio.
// Função: Evitar esquecer o \r\n no final
// Uso: o basicão enviarComandoAT(), para testes
// ou enviarComandoAT("NAME", "Peça preço")
// ou envarComandoAT("PAIR", chararray, ",9")
void enviarComandoAT(const char *comando, const char *parametro, const char *sufixo){
  bluetooth.write("AT");
  
  if(strcmp_P(comando, (PGM_P) F("")) != 0){
    bluetooth.write('+');
    bluetooth.write(comando);
    
    if(strcmp_P(parametro, (PGM_P) F("")) != 0){
      bluetooth.write(parametro);
      
      if(strcmp_P(sufixo, (PGM_P) F("")) != 0){
        bluetooth.write(sufixo);
      }
    }
  }
  
  bluetooth.write("\r\n");
}
// Para não precisar colocar "" em parâmetros opcionais não usados
void enviarComandoAT(const char *comando, const char *parametro){
  enviarComandoAT(comando, parametro, "");
}

void enviarComandoAT(const char *comando){
  enviarComandoAT(comando, "", "");
}

void enviarComandoAT(){
  enviarComandoAT("", "", "");
}

// Retorna um array no formato certo para os comandos RNAME, PAIR, BIND e LINK
// Ex: XXXX,XX,XXXXXX
void lerInquiryResponse(char *resposta){
  int i;
  char leitura;
  lerBytesBluetooth(5); // descartando "+INQ:"
  for(i = 0; i < 16; i++){
    leitura = lerByteBluetooth();
    if(leitura != ':' && leitura != ','){
      resposta[i] = leitura;
    } else if (leitura == ':'){
      resposta[i] = ',';
    } else { // fim do endereço encontrado (vírgula)
      break;
    }
  }
  resposta[i] = '\0';
  Serial.print(F("Dispositivo encontrado: "));
  Serial.println(resposta);
}

bool alternarModoAT(bool paraAT){
  if (paraAT){
    alternarPinoKey(true);
    Serial.println(F("Ligando key."));
    alternarBaudBluetooth(false);
    Serial.println(F("Tentando em 9600 baud..."));
    limparBufferSerialBluetooth();
    bool sucesso = false;
    int i;
    for(i = 0; i < 3; i++){
      enviarComandoAT();
      char resposta[5];
      lerBytesBluetooth(4, resposta);
      Serial.print(F("Resposta:"));
      Serial.println(resposta);
      if (strncmp(resposta, "OK\r\n\0", 5) == 0){
        sucesso = true;
        Serial.println(F("OK recebido!"));
        break;
      } else if (strncmp(resposta, "ERRO", 4) == 0) { // Se ERROR:(0)\r\n
        limparBufferSerialBluetooth(); // descartar o resto da mensagem (\r e \n são um byte só cada)
      }
    }
    if (!sucesso){ // Comunicação não funcionou com um alto baud
      Serial.println(F("O modulo nao responde em baixo baud."));
      alternarBaudBluetooth(true);
      Serial.println(F("Aumentando para 38400 baud..."));
      limparBufferSerialBluetooth();
      for(i = 0; i < 3; i++){
        enviarComandoAT();
        char resposta[5];
        lerBytesBluetooth(4, resposta);
        if (strncmp(resposta, "OK\r\n\0", 5) == 0){
          sucesso = true;
          Serial.println(F("OK recebido!"));
          break;
        } else if (strncmp_P(resposta, "ERRO", 4) == 0) { // Se ERROR:(0)\r\n
          lerBytesBluetooth(7); // descartar o resto da mensagem (\r e \n são um byte só)
        }
      }
      
    }
    if (sucesso){
      Serial.println(F("Modo AT ativado com sucesso!"));
      modoAT = true;
      return true;
    } else {
      Serial.println(F("ERRO - O modulo nao responde a comandos AT."));
      return false;
    }
  } else {
    alternarPinoKey(false); // Sem pino key, ao resetar ele sai do modo AT
    Serial.println(F("Desligando key."));
    enviarComandoAT("RESET");
    lerBytesBluetooth(4);
    alternarBaudBluetooth(false); // Voltando ao baud padrão de comunicação
    Serial.println(F("Modo AT desativado!"));
    modoAT = false;
    return true;
  }
  return false;
}

char lerByteBluetooth(){
  tempoInicial = millis();
  tempoAtual = tempoInicial;
  while(tempoAtual - tempoInicial <= 10){
    tempoAtual = millis();
  }
  int aval = bluetooth.available();
  if(aval){
    return bluetooth.read();
  } else {
    return '\0';
  }
}

void lerBytesBluetooth(int quantBytes, char bufferStr[]){
  int i;
  delaySuperior(500);
  for(i = 0; i < quantBytes; i++){
    bufferStr[i] = lerByteBluetooth();
  }
  bufferStr[quantBytes] = '\0';
}
// Para descarte do buffer bluetooth
void lerBytesBluetooth(int quantBytes){
  int i;
  delaySuperior(500);
  for(i = 0; i < quantBytes; i++){
    lerByteBluetooth();
  }
}

void alternarPinoKey(bool ligado){
  if (ligado){
    digitalWrite(pinoKey, HIGH);
  }
  else {
    digitalWrite(pinoKey, LOW);
  }
}

void alternarPinoEN(bool ligado){
  if (ligado){
    digitalWrite(pinoEN, HIGH);
  }
  else {
    digitalWrite(pinoEN, LOW);
  }
}

void processarComando(){
  Serial.print(F(">"));
  Serial.print(buffer);
  if(!modoAT){
    Serial.println();
    if(strcmp_P(buffer, (PGM_P) F("ligar key")) == 0){
      alternarPinoKey(true);
      Serial.println(F("Pino KEY ligado!"));
    } 
    else if (strcmp_P(buffer, (PGM_P) F("desligar key")) == 0) {
      alternarPinoKey(false);
      Serial.println(F("Pino KEY desligado!"));
    }
    else if(strcmp_P(buffer, (PGM_P) F("terminal at")) == 0){
      modoAT = true;
      Serial.println(F("Modo terminal AT ativado! Digite comandos AT ou \"sair terminal at\" para sair."));
    } else if(strcmp_P(buffer, (PGM_P) F("bluetooth baud alto")) == 0){
      alternarBaudBluetooth(true);
      Serial.println(F("Trocando baud para 38400!"));
    } else if(strcmp_P(buffer, (PGM_P) F("bluetooth baud baixo")) == 0){
      alternarBaudBluetooth(false);
      Serial.println(F("Trocando baud para 9600!"));
    } else if(strcmp_P(buffer, (PGM_P) F("bluetooth modo master")) == 0){
      Serial.println(F("Alternando para o modo master!"));
      autoConfiguracao(true);
    } else if(strcmp_P(buffer, (PGM_P) F("bluetooth modo slave")) == 0){
      Serial.println(F("Alternando para o modo slave!"));
      autoConfiguracao(false);
    } else if(strcmp_P(buffer, (PGM_P) F("teste at")) == 0){
      Serial.println(F("Testando AT!"));
      bluetooth.write("AT\r\n");
      char resposta[5];
      lerBytesBluetooth(4, resposta);
      Serial.print(F("Resposta:"));
      Serial.println(resposta);
    } else if(strcmp_P(buffer, (PGM_P) F("teste sd")) == 0){
      Serial.println(F("Testando SD!"));
      if(!sdCard.begin(4,SPI_HALF_SPEED))sdCard.initErrorHalt();
      if (!meuArquivo.open("teste SD.txt", O_RDWR | O_CREAT | O_AT_END))
      {
        sdCard.errorHalt("Erro na abertura do arquivo teste SD.txt!");
      }
      meuArquivo.print("Teste de gravação: ");
      meuArquivo.println("TESTANDOOOOOOOOOOOOO");
      meuArquivo.close();
    }
    else if(strcmp_P(buffer, (PGM_P) F("ajuda")) == 0){
      Serial.println(F("Comandos disponiveis:"));
      Serial.println(F("(des)ligar key - (Des)liga o pino KEY (usado para entrar no modo AT quando ligado)"));
      Serial.println(F("(sair) terminal at - Sai/Entra no modo terminal AT para enviar comandos AT ao modulo"));
      Serial.println(F("bluetooth baud alto/baixo - Muda o baud da comunicação bluetooth para 38400/9600"));
      Serial.println(F("bluetooth modo master/slave - Alterna entre o modo master(capta info obd) e o modo slave(transmite info obd)"));
      Serial.println(F("teste at - Testa se o modulo bluetooth responde a comandos AT. O pino key precisa estar ligado."));
      Serial.println(F("teste sd - Testa se o modulo de cartão SD/mini sd funciona."));
    }
  }
  else {
    if(strcmp_P(buffer, (PGM_P) F("sair terminal at")) == 0){
      Serial.println();
      modoAT = false;
      Serial.println(F("Modo terminal AT desativado! Digite ajuda para ver os comandos disponiveis."));
    } else {
      if(ultimoChar == (tamanhoComando-1)){         // resultado = [...,(sobrescrito)\r,(sobrescrito)\n,\0]
        buffer[ultimoChar-2] = '\r';
        buffer[ultimoChar-1] = '\n';
      } else if (ultimoChar == (tamanhoComando-2)){ // resultado = [...,(sobrescrito)\r,(antigo null)\n,(vazio)\0]
        buffer[ultimoChar-1] = '\r';
        buffer[ultimoChar] = '\n';
        buffer[ultimoChar+1] = '\0';
      } else {                                      //resultado = [..., (antigo null)\r, (vazio)\n, (vazio)\0]
        buffer[ultimoChar] = '\r';
        buffer[ultimoChar+1] = '\n';
        buffer[ultimoChar+2] = '\0';
      }
      Serial.print(F(" ("));
      Serial.print(bluetooth.write(buffer));
      Serial.println(F(" bytes enviados)"));
    }
  }
  ultimoChar = 0;
}

void alternarBaudBluetooth(bool baudAlto){
  if(baudAlto){
    bluetooth.end();
    bluetooth.begin(38400);
  } else {
    bluetooth.end();
    bluetooth.begin(9600);
  }
}

void delaySuperior(unsigned long tempoMs){
  tempoInicial = millis();
  tempoAtual = tempoInicial;
  while (tempoAtual - tempoInicial <= tempoMs)
  {
    tempoAtual = millis();
  }
}