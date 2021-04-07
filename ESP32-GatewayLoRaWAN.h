// =========================================================================================================
// =========== LoRaWAN Gateway de Canal único para ESP32/ESP8266 ===========
// Copyright (c) 2016, 2017 Maarten Westenberg versão para ESP32/ESP8266
// Versão 5.0.1
// Data: 15-11-2017
// Autor: Maarten Westenberg, E-mail: mw12554@hotmail.com
// Contibuições de Dorijan Morelj e Andreas Spies pelo suporte a OLED.
//
// ========== Tradução: AdailSilva, E-mail: adail101@hotmail.com ===========
//
// Baseado no trabalho feito por Thomas Telkamp para o gateway Raspberry PI de canal único e muitos outros.
//
// Todos os direitos reservados. Este programa e os materiais acompanhantes são disponibilizados
// sob os termos da licença MIT que acompanha esta distribuição e está disponível em:
// https://opensource.org/licenses/mit-license.php
//
// NENHUMA GARANTIA DE QUALQUER TIPO É FORNECIDA.
//
// Os protocolos e especificações usados para este gateway de canal único:
//
// 1. Especificação LoRa Versão V1.0 e V1.1 para comunicação Gateway-Node;
//
// 2. Protocolo de comunicação Semtech Básico entre o gateway LoRa e a versão 3.0.0 do servidor
//  https://github.com/Lora-net/packet_forwarder/blob/master/PROTOCOL.TXT.
//
// Notas:
//
// Este arquivo contém várias configurações de tempo de compilação que
// podem ser ativadas com: (= 1), ou desativadas com: (= 0).
// A desvantagem é sobre o tempo de compilação que é menor em comparação ao ganho
// de memória por não ter código demais compilado e carregado em seu ESP32/ESP8266.
// =========================================================================================================

// Para placas WIFI_LoRa_32 baseadas em ESP32 da Heltec (e outras também)
//REMOVER para placas com ESP8266.
#define ESP32BUILD 1

#ifdef ESP32BUILD
#define VERSION "V.5.0.2.H+ 171128a nOLED, 15/10;"
#else
#define VERSION "V.5.0.2.H 171118a nOLED, 15/10;"
#endif

// Este valor de DEBUG determina se algumas partes do código são compiladas.
// Também este é o valor inicial do parâmetro debug.
// O valor pode ser alterado usando o servidor web admin.
// Para uso operacional, configure o valor inicial de DEBUG 0.
#define DEBUG 1

// Depurar mensagem será colocada em Serial se esta estiver definida.
// Se definido como 0, não as impressões Serial USB são feitas.
// Definido como 1, ele executará todas as mensagens no nível do usuário (com o conjunto de depuração correto).
// Se definido como 2, ele também imprimirá mensagens de interrupção (não recomendado).
#define DUSB 1

// Defina a faixa de freqüência LoRa que é usada. Os valores suportados pelo TTN são 915MHz, 868MHz e 433MHz.
// Assim, os valores suportados são: 433 868 915.
#define _LFREQ 915

// O fator de espalhamento é o parâmetro mais importante a ser definido para um gateway de único canal.
// Especifica a velocidade/datarate em que o gateway e o nó se comunicam.
// Como o nome diz, em princípio o gateway de canal único escuta um canal/frequência e apenas para um fator de espalhamento.
// Este parâmetro contém o valor padrão de SF, a versão atual pode ser definida com o servidor web e ele será armazenado em SPIFF.
// NOTA: A frequência é configurada no arquivo loraModem.h e é padrão 868.100000 MHz. (Agora 916.8 MHz).
#define _SPREADING SF7

// Detecção de atividade de canal.
// Esta função irá procurar cabeçalhos LoRa válidos e em conformidade determinar o Fator de Espalhamento.
// Se definido como 1, usaremos essa função, o que significa: O gateway de 1 canal se tornará ainda mais versátil.
// Se definido como 0, usaremos o modo de escuta contínua.
// Usando esta função significa que temos que usar mais pinos dio no dispositivo RFM95/sx1276 e também conecte enable dio1 para detectar este estado.
#define _CAD 1

// Definições para o servidor web admin.
// A_SERVER determina se a página da web administrativa está ou não incluída no esboço.
// Normalmente, deixe-o entrar!
#define A_SERVER 1 // Defina WebServer local somente se esta definição estiver definida.
#define A_REFRESH 1 // O servidor atualizará ou não?
#define A_SERVERPORT 80 // porta do servidor web local.
#define A_MAXBUFSIZE 192 // Deve ser maior que 128, mas pequeno o suficiente para funcionar.

// Definições para atualizações ao vivo. No momento, suportamos OTA com IDE
// Certifique-se de que você tenha instalado o Python versão 2.7 e tenha o Bonjour em sua rede.
// O Bonjour está incluído no iTunes (que é gratuito) e o OTA é recomendado para instalar
// o firmware em seu roteador sem ter que estar realmente perto do gateway e conecta com USB.
#ifdef ESP32BUILD
// Não disponível (ainda) para ESP32.
#define A_OTA 0
#else
#define A_OTA 0
#endif

// Suportamos duas configurações de pinagem out-of-the-box: HALLARD e COMPRESULT.
// Se você usar um desses dois, apenas configure o parâmetro para o valor correto.
// Se suas definições de pinos forem diferentes, atualize o arquivo loraModem.h para refletir essas configurações.
// 1: HALLARD;
// 2: COMRESULT pin out;
// 3: Para a placa WIFI_LoRa_32 com base em ESP32 da Heltec;
// 4: Outro, defina seu próprio no loraModem.h.
#define _PIN_OUT 3
//#define _PIN_OUT 4 --> Implementado configuração para Placa com base ESP32 e NiceRF (SX1276) da AFEletrônica).

// Reunir estatísticas sobre o status do sensor e do Wifi.
// 0 = Sem estatísticas;
// 1 = Acompanhe as estatísticas das mensagens, número determinado por MAX_STAT;
// 2 = Veja 1 + Acompanhe as mensagens recebidas por cada SF.
#define STATISTICS 2

// Número máximo de registros estatísticos reunidos. 20 é um bom máximo (memória intensiva).
#define MAX_STAT 20

// Gateways de canal único se eles se comportarem estritamente devem usar apenas um canal de frequência e um fator de espalhamento.
// No entanto, o backend TTN responde ao intervalo de tempo RX2 dos fatores de espalhamento SF9-SF12.
// Além disso, o servidor responderá com SF12 no intervalo de tempo RX2.
// Se o gateway 1ch estiver funcionando e para nós que SOMENTE transmitem e recebem no conjunto
// e frequência acordada e fator de propagação. Certifique-se de definir o STRICT como 1.
// Neste caso, a frequência e o fator de propagação para mensagens de downlink são adaptados por este gateway.
// NOTA: Se o seu nó tiver apenas uma frequência ativada e um SF, você deverá definir isso como 1 para receber mensagens de downlink.
// NOTA: Em todos os outros casos, o valor 0 funciona para a maioria dos gateways com CAD ativado.
#define _STRICT_1CH 1

// Permite a configuração através da configuração do WifiManager AP. Deve ser 0 ou 1.
#ifdef ESP32BUILD
// Não disponível (ainda) para ESP32.
#define WIFIMANAGER 0
#else
#define WIFIMANAGER 0
#endif

// Definir o nome do ponto de acesso do gateway no modo de ponto de acesso (está recebendo o SSID WiFi e senha usando o WiFiManager).
#ifdef ESP32BUILD
#define AP_NAME "ESP32-LoRaWANGateway-AdailSilva"
#else
#define AP_NAME "ESP8266-LoRaWANGateway-AdailSilva"
#endif
#define AP_PASSWD "Hacker101"

// Define se o gateway também relatará o valor do sensor/status no MQTT.
// Um gateway também pode ser um nó para o sistema.
// Defina seu endereço LoRa e a chave abaixo neste arquivo.
// Consulte as especificações. para 4.3.2.
#define GATEWAYNODE 0
#define _CHECK_MIC 0

// Esta seção define se usamos o gateway como um repetidor.
// Para ele, usamos outra saída como o canal (padrão == 0) em que estamos recebendo as mensagens.
#define REPEATER 0

// Vamos usar o Mutex ou não?
// + SPI é entrada para SPI, SPO é saída para SPI.
#define MUTEX 1
#define MUTEX_SPI 0
#define MUTEX_SPO 0
// Protege o módulo de interrupção.
#define MUTEX_INT 0

// Defina se queremos gerenciar o gateway através do UDP (próximo ao gerenciamento através da interface web).
// Isso nos permitirá enviar mensagens pela conexão UDP para gerenciar o gateway e seus parâmetros.
// Às vezes, o gateway não é acessível por controle remoto, neste caso, permitiríamos que ele usasse a conexão UDP do SERVER para receber mensagens também.
// NOTA: Esteja ciente de que essas mensagens NÃO são compatíveis com as especificações LoRa e não LoRa Gateway.
// No entanto, isso não deve interferir na operação normal do gateway, mas sim oferecer funções para definir/redefinir certos parâmetros do remoto.
#define GATEWAYMGT 0

// Nome do arquivo de configuração no sistema de arquivos SPIFFs.
// Neste arquivo nós armazenamos a configuração e outras informações relevantes que devem sobreviver a uma reinicialização do gateway.
#define CONFIGFILE "/gwayConfig.txt"

// Definir as configurações do servidor (IMPORTANTE).
#define _LOCUDPPORT 1700 // Porta UDP do gateway! Muitas vezes 1700 ou 1701 é usado para comms upstream.

// Cronometragens.
#define _MSG_INTERVAL 15
#define _PULL_INTERVAL 55 // PULL_DATA mensagens para o servidor para receber downstream em milissegundos.
#define _STAT_INTERVAL 120 // Envie uma mensagem 'stat' para o servidor.
#define _NTP_INTERVAL 3600 // Quantas vezes queremos tempo sincronização NTP.
#define _WWW_INTERVAL 20 // Número de segundos antes de atualizar a página WWW.

// Definições MQTT, essas configurações devem ser padrão para o TTN e não precisam ser alteradas.
#define _TTNPORT 1700 // Porta padrão para TTN.
#define _TTNSERVER "thethings.meshed.com.au"

// Se você tiver um segundo servidor de backend definido como Semtech ou loriot.io.
// Você pode definir _THINGPORT e _THINGSERVER com seu próprio valor.
// Se não, certifique-se de que você não os definiu, o que economizará tempo de CPU
// Port é a porta UDP neste programa.
// Padrão para teste: off (desligado).
//#define _THINGPORT 1700 // dash.westenberg.org:8057
//#define _THINGSERVER "seuServidor.com" // URL do servidor do manipulador LoRa-udp.js.

// Definições de identidade do gateway.
#define _DESCRIPTION "ESP LoRa Gateway - AdailSilva" // Descrição do gateway.
#define _EMAIL "adail101@hotmail.com" // E-mail do proprietário do gateway.
#define _PLATFORM "ESP32" // Plataforma usada com o Rádio para formar o Gateway (MCU).
#define _LAT -5.075361 // Latitude do Gateway.
#define _LON -42.8182692 // Longitude do Gateway.
#define _ALT 98 // Altitude do Gateway.

// Configurações do servidor ntp (Network Time Protocol).
#define NTP_TIMESERVER "br.pool.ntp.org" // País e região específicos.
#define NTP_TIMEZONES -3 // Até que ponto o fuso horário do UTC (exclua horário de verão/horário de verão).
#define SECS_PER_HOUR 3600 // Quantidade de segundos por hora.
#define NTP_INTR 0 // Faça o processamento NTP com interrupções ou em loop();

#if GATEWAYNODE == 1
#define _DEVADDR { 0x26, 0x00, 0x00 0x00 }
#define _APPSKEY { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define _NWKSKEY { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define _SENSOR_INTERVAL 300
#endif

// Defina o tipo de rádio correto que você está usando.
//#define CFG_sx1272_radio
#define CFG_sx1276_radio

// Velocidade da porta serial.
#define _BAUDRATE 115200 // Funciona para mensagens de depuração no monitor serial.

// Se o display OLED estiver conectado ao i2c.
#define OLED 1 // Defina 1 on-line se você tiver um display OLED conectado.
#define OLED_SCL 15 // GPIO5 / D1
#define OLED_SDA 4 // GPIO4 / D2
#define OLED_ADDR 0x3C // Padrão 0x3C para 0.96", para 1.3" ficará 0x78.

// Setando PINOS da comunicação SPI.
// Placa ESP32 Heltec.
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18

// Placa AFEletrônica (NiceRF - SX1276).
//#define SCK xx
//#define MISO xx
//#define MOSI xx
//#define SS xx

// Definições de Wifi:
// WPA é uma matriz com registros de SSID e senha. Definir o tamanho WPA para o número de entradas no array.
// Ao usar o WiFiManager, sobrescreveremos a primeira entrada com o accesspoint em que nos conectamos pela última vez com o WifiManager.
// NOTA: A estrutura precisa de pelo menos uma entrada (vazia).
// Então WPASIZE deve ser >= 1.
struct wpas
{
    char login[32]; // Tamanho Máximo do Buffer (e memória alocada).
    char passw[64];
};

// Por favor, preencha pelo menos UM SSID e senha da sua própria rede WiFI abaixo. Isso é necessário para que o gateway funcione.
// Nota: NÃO use a primeira e a última linha da estrutura, estas devem ser cadeias vazias e a primeira linha na estrutura é reservada para o WifiManager.
wpas wpa[] = {
    {"", ""}, // Reservado para o WiFi Manager.
    {"IoT C++ Java LoRaWAN", "Hacker101"},
    {"(  ( ( AdailSilva ) )  )", "Hacker101"},
    {"", ""}};

// Para asseverar e testar, as seguintes definições são usadas.
#if !defined(CFG_noassert)
#define ASSERT(cond) \
    if (!(cond))     \
    gway_failed(__FILE__, __LINE__)
#else
#define ASSERT(cond)
#endif
