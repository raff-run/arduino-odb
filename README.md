# arduino-odb
Desenvolvido para o projeto da disciplina Tópicos de programação ministrada pelo professor Thiago da Silva Souza na Unigranrio, no curso Sistemas de Informação.

Para não ter problema de instalar bibliotecas e etc, sugiro usar o arduino create(https://create.arduino.cc) mesmo, assim a ide, o código e as bibliotecas ficam na web. É só copiar o código inteiro desse sketch e jogar lá no create que funciona. A página só irá pedir que instale o plugin uma única vez.

Para testar com um simulador rodando via bluetooth em um computador Windows, use a taxa de Baud 38400. Taxas menores ou maiores não funcionarão.

Ou use o app Bluetooth Terminal (por padrão o código se conectará ao primeiro dispositivo chamado "OBDII" que ele encontrar).

Lista de comandos AT para reconfigurar o módulo: https://cdn.instructables.com/ORIG/FKY/Z0UT/HX7OYY7I/FKYZ0UTHX7OYY7I.pdf
(use enviarComandoAT("comando"[,param[, param]]))

Lista de comandos OBDII: https://en.wikipedia.org/wiki/OBD-II_PIDs
Comandos precisam terminar com \r\n.

Para ler as respostas do bluetooth, recomendo usar o utilitário lerBytesBluetooth(quantidade, buffer) dentro de um if(bluetooth.available()). O tamanho de buffer precisa ser de no mínimo quantidade+1, pois é inserido um caractere NULL(terminador) em buffer[quantidade+1]. Para descartar bytes do bluetooth, use lerBytesBluetooth(quantidade). 1 Byte é 1 caractere.

Dicas de Arduino:
* Tente criar métodos utilitários pra todos os trechos de código que se repetem. Isso ajuda a ter um código mais fácil de entender(até pra você mesmo) e um código total menor, o que é importante no Arduino.
* Não use delay(). Sério. delay() pára o Arduino inteiro, incluindo outros módulos seus. Usar delay faz o módulo bluetooth não ouvir mensagens, por exemplo. Para esperar, use um loop e verifique com a função millis() para ver se passou tempo o suficiente. Ou use o método delaySuperior(tempoMs).
* Não ponha acentuação em strings do arduino, exceto em comentários.
    * As vezes os caracteres aparecem, as vezes não, as vezes tudo buga...
* Não use a biblioteca String. Use array de chars.(aliás, quando eu disser string logo abaixo, eu me refiro a arrays)
* Não use prints demais. Printar muito frequentemente/muita coisa as vezes faz o Arduino agir estranho.
* Sempre que usar um print para o monitor serial, coloque F() em volta da string. Ex: Serial.println(F("Iniciando Arduino")). Isto é por que TODA string sem F() vira uma variável global.
    * Ou seja, cada pedaço de texto vai ocupar alguns bytes da sua grande RAM de 2KB. Usar F() diz ao compilador para guardar essa string dentro da memória Flash(32KB, onde o resto do código fica) ao invés da RAM. Não dá pra colocar tudo na memória Flash, porém, pois na minha experiência só os metodos do Serial trabalham bem com isso.
    * Para comparar uma string com algo estático(ex: o que o usuário digitou com um comando) use o método strcmp_P. Esse aceita strings da memória Flash. Você só irá precisar fazer um cast para PGM_P. 
        * Ex: strcmp_P(buffer, (PGM_P) F("sair terminal at"))
* O modo AT do módulo bluetooth só aceita comandos terminados por \r\n.
* O módulo bluetooth só fica no modo AT enquanto o pino KEY receber energia. Isso pode ser feito apertando e deixando apertado o botão do módulo... ou ligando um pino digital direto no pino 34 do módulo(o plástico do módulo vai deixar o cabo preso). O pino fica aqui: http://www.martyncurrey.com/wp-content/uploads/2014/10/HC-05_zs-040_01_1200-584x623.jpg
    * Com isso, você poderá ativar/desativar o modo AT automaticamente, via código, ao mandar HIGH ou LOW pro pino ligado ao KEY.
* Divisores de tensão 5v para 3.3v (todo tutorial de hc-05 vai indicar usar um) podem ser de QUALQUER resistência em Ohms, desde que a resistência indo para o GND seja 2x mais alta do que a resistencia indo do breadboard para a entrada 3.3v. 220 e (220+220), 1k e 2k, 330 e (330+330), tanto faz, desde que siga essa regra.

         * Como saber qual é a cor do resistor: http://www.audioacustica.com.br/exemplos/Valores_Resistores/Calculadora_Ohms_Resistor.html
* Não use as portas 0 e 1 (TX e RX) do Arduino. Elas são compartilhadas com o USB, ou seja, enquanto as portas estiverem sendo usadas, não dará para carregar código para o Arduino. Use a biblioteca SoftwareSerial, desta forma você conseguirá usar qualquer outro par(recomendo 2(TX) e 3(RX) para comunicação serial, ex: com o módulo bluetooth)

O código não irá mais ser atualizado -- a partir de agora provavelmente será privado.
