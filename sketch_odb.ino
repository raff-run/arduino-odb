//Include the SoftwareSerial library
#include "SoftwareSerial.h"

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
//Operações
#define OP_MODO_AT_SLAVE 20
#define OP_MODO_AT_MASTER 21

//Create a new software  serial
SoftwareSerial bluetooth(2, 3); // TX, RX (Bluetooth) 



const int tamanhoComando = 32;

unsigned long tempoInicial; // Guarda o momento do início de uma tarefa
unsigned long tempoAtual; // Guarda o momento atual

int byteLido;      // a variable to read incoming serial data into
char buffer[tamanhoComando];
int ultimoChar;
bool modoAT = false;
bool isKeyOn = false;
int estado = DEBUGGING;

void setup() {
  //Initialize the software serial
  bluetooth.begin(9600);
  Serial.begin(9600);
  
  pinMode(pinoKey, OUTPUT);
  pinMode(pinoEN, OUTPUT);
  digitalWrite(pinoEN, HIGH);
  
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
    if(Serial.available()){
      bluetooth.write(Serial.read());
    }
    
    
    if(bluetooth.available()){
      char resposta[6];
      lerBytesBluetooth(5, resposta);
      Serial.println(resposta);
      if(strncmp(resposta, "sai\r\n\0", 4) == 0){
        // desconectar
        alternarModoAT(true);
        enviarComandoAT("DISC");
        estado = DEBUGGING;
      }
      limparBufferSerialBluetooth();
    }
    
  }
  else if (estado == OP_MODO_AT_SLAVE){
    
  }
  else if (estado == OP_MODO_AT_MASTER){
    
  } else if (estado == S_ESPERANDO_CONEXAO){
    estado = DEBUGGING;
  }
}

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
  delaySuperior(500);
  int numChars = bluetooth.available();
  int i;
  int leitura;
  if(debug){
    Serial.print(F("Descartando até o fim da linha: ["));
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
        char gambiarra2 = lerByteBluetooth(); // e as vezes vem um \r\n do NADA também
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
            enviarComandoAT("PAIR=", bufferBluetooth[i], ",9");// tentar parear com timeout de 9s
            delaySuperior(9000);
            lerBytesBluetooth(4, resposta);
            if(strncmp(resposta, "OK\r\n\0", 4) == 0){
              Serial.println(F("Pareado ao OBD2!"));
              enviarComandoAT("DISC");
              delaySuperior(2000);
              descartarLinhaBluetooth(true);
              descartarLinhaBluetooth(true);
              enviarComandoAT("LINK=", bufferBluetooth[i]);
              lerBytesBluetooth(4, resposta);
              if(strncmp(resposta, "OK\r\n\0", 4) == 0){
                Serial.println(F("Ligado ao OBD2!"));
                Serial.println(F("Configuracao completa"));
                enviarComandoAT("CMODE=", "1"); // Arduino só se conectará ao OBD2 agora
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
    
    estado = S_ESPERANDO_CONEXAO;
  }
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
    } else if(strcmp_P(buffer, (PGM_P) F("teste at 2")) == 0){
      Serial.println(F("Testando AT!"));
      bluetooth.write("AT\r\n");
      char resposta[5];
      delaySuperior(500);
      resposta[0] = lerByteBluetooth();
      resposta[1] = lerByteBluetooth();
      resposta[2] = lerByteBluetooth();
      resposta[3] = lerByteBluetooth();
      resposta[4] = '\0';
      Serial.print(F("Resposta:"));
      Serial.print(resposta);
    }
    else if(strcmp_P(buffer, (PGM_P) F("ajuda")) == 0){
      Serial.println(F("Comandos disponiveis:"));
      Serial.println(F("(des)ligar key - (Des)liga o pino KEY (usado para entrar no modo AT quando ligado)"));
      Serial.println(F("(sair) terminal at - Sai/Entra no modo terminal AT para enviar comandos AT ao modulo"));
      Serial.println(F("bluetooth baud alto/baixo - Muda o baud da comunicação bluetooth para 38400/9600"));
      Serial.println(F("bluetooth modo master/slave - Alterna entre o modo master(capta info obd) e o modo slave(transmite info obd)"));
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