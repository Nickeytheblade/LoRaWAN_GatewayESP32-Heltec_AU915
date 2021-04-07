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
//	https://github.com/Lora-net/packet_forwarder/blob/master/PROTOCOL.TXT.
//
// Notas:
//
// - Chame uma vez gethostbyname() para obter o IP para serviços, depois disso use somente IP
// endereços (muitos nomes de gethost tornam o ESP instável);
//
// - Chama somente yield() no fluxo principal (não para o NTP sync em segundo plano).
// =========================================================================================================

#include "ESP32-GatewayLoRaWAN.h" // Este arquivo contém a configuração do Gateway.
#include <Esp.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <sys/time.h>
#include <cstring>
#include <SPI.h>
#include <TimeLib.h> // https://playground.arduino.cc/code/time/

#ifdef ESP32BUILD
//#include "esp_wifi.h"
#include "WiFi.h"
#include "SPIFFS.h"
#else
#include <ESP8266WiFi.h>
#include <DNSServer.h> // Servidor DNS local.
#endif

#include "FS.h"
#include <WiFiUdp.h>
#include <pins_arduino.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>
#include <gBase64.h> // https://github.com/adamvr/arduino-base64 (mudou o nome).

#ifndef ESP32BUILD
#include <ESP8266mDNS.h>

extern "C"
{
#include "user_interface.h"
#include "lwip/err.h"
#include "lwip/dns.h"
#include "c_types.h"
}
#endif

#if MUTEX_LIB == 1
#include <mutex.h> // Veja o diretório lib.
#endif

#include "loraModem.h"
#include "loraFiles.h"

#if WIFIMANAGER > 0
#include <WiFiManager.h> // Biblioteca para configuração do WiFi no ESP através de um access point - ponto de acesso (AP).
#endif

#if A_OTA == 1
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>
#endif

#if A_SERVER == 1
#include <ESP8266WebServer.h>
#endif

#if GATEWAYNODE == 1
#include "AES-128_V10.h"
#endif

#if OLED == 1
#include "SSD1306.h"
SSD1306 display(OLED_ADDR, OLED_SDA, OLED_SCL); // i2c ADDR & SDA, SCL na Placa Wemos D1.
#endif

int debug = 2; // Nível de depuração! 0 não mostra mensagens, 1 log normal, 2 log extenso.

// Você pode desativar o servidor se não for necessário, mas provavelmente é melhor deixá-lo.
#if A_SERVER == 1
#ifdef ESP32BUILD
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
WebServer server(A_SERVERPORT);
#else
#include <Streaming.h> // http://arduiniana.org/libraries/streaming/
ESP8266WebServer server(A_SERVERPORT);
#endif
#endif
using namespace std;

byte currentMode = 0x81;

//char b64[256];
bool sx1272 = false; // Na verdade, usamos sx1276/RFM95. (Estava verdadeiro).

uint32_t cp_nb_rx_rcv;
uint32_t cp_nb_rx_ok;
uint32_t cp_nb_rx_bad;
uint32_t cp_nb_rx_nocrc;
uint32_t cp_up_pkt_fwd;

uint8_t MAC_array[6];
//char MAC_char[19];

// ---------------------------------------------------------------------------------------------------------
// Configuramos estes valores somente se necessário!
// ---------------------------------------------------------------------------------------------------------

// Setar fator de espalhamento (SF7 à SF12).
sf_t sf = _SPREADING;
sf_t sfi = _SPREADING; // Valor inicial de SF.

// Definimos a localização, descrição e outros parâmetros de configuração, Definido em ESP-sc_gway.h.
// Informações específicas da configuração:
float lat = _LAT;
float lon = _LON;
int alt = _ALT;
char platform[24] = _PLATFORM;		 // Definição de plataforma.
char email[40] = _EMAIL;			 // Usado para email de contato.
char description[64] = _DESCRIPTION; // Usado para descrição de forma livre.

// Definições dos servidores:
IPAddress localhost;   // IP Local.
IPAddress ntpServer;   // Endereço IP de NTP_TIMESERVER.
IPAddress ttnServer;   // Endereço IP do servidor de rede (TTN neste caso específico).
IPAddress thingServer; // Endereço IP do servidor de rede (Thing neste caso específico).

WiFiUDP Udp;
uint32_t stattime = 0; // Última vez que enviamos uma mensagem stat para o servidor.
uint32_t pulltime = 0; // Última vez que enviamos um pedido pull_data ao servidor.
uint32_t lastTmst = 0;
#if A_SERVER == 1
uint32_t wwwtime = 0;
#endif
#if NTP_INTR == 0
uint32_t ntptimer = 0;
#endif

SimpleTimer timer; // Variável Timer é necessária para envios atrasados.

#define TX_BUFF_SIZE 1024 // Buffer de upstream para enviar ao MQTT.
#define RX_BUFF_SIZE 1024 // Downstream recebido do MQTT.
#define STATUS_SIZE 512   // Deve (!) Ser suficiente com base no texto estático .. foi 1024.

#if GATEWAYNODE == 1
uint16_t frameCount = 0; // Escrevemos isso no arquivo da SPIFFS.
#endif

// volatile bool inSPI
// Este valor inicial do mutex é para ser livre, o que significa que seu valor é 1 (!).
int inSPI = 1;
int inSPO = 1;
int mutexSPI = 1;

// Variável booleana volátil inIntr = false;
int inIntr;

// ---------------------------------------------------------------------------------------------------------
// DECARAÇÕES DE AVANÇO
// Estas declarações de encaminhamento são feitas porque o _loraModem.ino é vinculado pelo
// compilador / linker APÓS o arquivo principal do ESP-sc-gway.ino.
// E, espessially ao chamar funções com ICACHE_RAM_ATTR, o compilador não deseja isso.
// ---------------------------------------------------------------------------------------------------------
void ICACHE_RAM_ATTR Interrupt_0();
void ICACHE_RAM_ATTR Interrupt_1();

#if MUTEX == 1
// Declarações de encaminhamento:
void ICACHE_FLASH_ATTR CreateMutux(int *mutex);
bool ICACHE_FLASH_ATTR GetMutex(int *mutex);
void ICACHE_FLASH_ATTR ReleaseMutex(int *mutex);
#endif

// ---------------------------------------------------------------------------------------------------------
// DIE não é mais usado ativamente no código-fonte.
// Ele é substituído por um comando Serial.print, então sabemos que temos um problema em algum lugar.
// Existem pelo menos 3 outras maneiras de reiniciar o ESP. Escolha um se você quiser.
// ---------------------------------------------------------------------------------------------------------
void die(const char *s)
{
	Serial.println(s);
	if (debug >= 2)
		Serial.flush();

	delay(50);
	// system_restart();  // Função SDK.
	// ESP.reset();
	abort(); // Dentro de um segundo.
}

// ---------------------------------------------------------------------------------------------------------
// gway_failed é uma função chamada por ASSERT em ESP32-GatewayLoRaWAN.h
// ---------------------------------------------------------------------------------------------------------
void gway_failed(const char *file, uint16_t line)
{
	Serial.print(F("Falhou o programa no arquivo: "));
	Serial.print(file);
	Serial.print(F(", na linha: "));
	Serial.println(line);
	if (debug >= 2)
		Serial.flush();
}

// ---------------------------------------------------------------------------------------------------------
// Imprime líder '0' dígitos por horas(0) e segundos(0) ao imprimir valores menores que 10:
// ---------------------------------------------------------------------------------------------------------
void printDigits(unsigned long digits)
{
	// Função de utilidade para exibição do relógio digital: imprime 0 à esquerda.
	if (digits < 10)
		Serial.print(F("0"));
	Serial.print(digits);
}

// ---------------------------------------------------------------------------------------------------------
// Imprime valores de utin8_t em HEX com 0 inicial quando necessário:
// ---------------------------------------------------------------------------------------------------------
void printHexDigit(uint8_t digit)
{
	// Função de utilidade para imprimir valores hexadecimais com os principais 0.
	if (digit < 0x10)
		Serial.print('0');
	Serial.print(digit, HEX);
}

// ---------------------------------------------------------------------------------------------------------
// Imprime a hora atual:
// ---------------------------------------------------------------------------------------------------------
static void printTime()
{
	switch (weekday())
	{
	case 1:
		Serial.print(F("Domingo"));
		break;
	case 2:
		Serial.print(F("Segunda-Feira"));
		break;
	case 3:
		Serial.print(F("Terça-Feira"));
		break;
	case 4:
		Serial.print(F("Quarta-Feira"));
		break;
	case 5:
		Serial.print(F("Quinta-Feira"));
		break;
	case 6:
		Serial.print(F("Sexta-Feira"));
		break;
	case 7:
		Serial.print(F("Sábado"));
		break;
	default:
		Serial.print(F("ERRO, não é possível identificar o dia da semana."));
		break;
	}
	Serial.print(F(" "));
	printDigits(hour());
	Serial.print(F(":"));
	printDigits(minute());
	Serial.print(F(":"));
	printDigits(second());
	return;
}

// ---------------------------------------------------------------------------------------------------------
// Converte um float em string para impressão:
//
// Parâmetros:
//
// f é o valor float para converter.
// p é precisão em dígitos decimais.
// val é array de caracteres para resultados.
// ---------------------------------------------------------------------------------------------------------
void ftoa(float f, char *val, int p)
{
	int j = 1;
	int ival, fval;
	char b[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	for (int i = 0; i < p; i++)
	{
		j = j * 10;
	}

	ival = (int)f; // Faz parte do inteiro.
	fval = (int)((f - ival) * j); // Faz fração, tem o mesmo sinal que a parte inteira.
	if (fval < 0)
		fval = -fval; // Então, se for negativo, faça a fração positiva novamente.
	// "sprintf" NÃO se encaixa na memória.
	strcat(val, itoa(ival, b, 10)); // Copia a parte inteira primeiro, base 10, terminada em null.
	strcat(val, "."); // Copiar ponto decimal.

	itoa(fval, b, 10); // Copiar a parte da fração base 10.
	for (int i = 0; i < (p - strlen(b)); i++)
	{
		strcat(val, "0"); // Primeiro número de 0 da fração?
	}

	// Fração pode ser qualquer coisa de 0 a 10 ^ p, então pode ter menos dígitos.
	strcat(val, b);
}

// =========================================================================================================
// Funções de TEMPO NTP.
// =========================================================================================================
// ---------------------------------------------------------------------------------------------------------
// Envia o pacote de requisição para o servidor NTP.
// ---------------------------------------------------------------------------------------------------------
int sendNtpRequest(IPAddress timeServerIP)
{
	const int NTP_PACKET_SIZE = 48; // Tamanho fixo do registro NTP.
	byte packetBuffer[NTP_PACKET_SIZE];

	memset(packetBuffer, 0, NTP_PACKET_SIZE); // Zerar o buffer.

	packetBuffer[0] = 0b11100011; // LI, versão, modo.
	packetBuffer[1] = 0; // Stratum ou tipo de relógio.
	packetBuffer[2] = 6; // Intervalo de Polling.
	packetBuffer[3] = 0xEC; // Precisão do Relógio de Pares.
	// 8 bytes de zero para atraso de raiz e dispersão de raiz.
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;

	if (!sendUdp((IPAddress)timeServerIP, (int)123, packetBuffer, NTP_PACKET_SIZE))
	{
		gwayConfig.ntpErr++;
		gwayConfig.ntpErrTime = millis();
		return (0);
	}
	return (1);
}

// ---------------------------------------------------------------------------------------------------------
// Obtém o tempo NTP de um dos servidores de tempo.
//
// Nota:
// Como esta função é chamada de SyncInterval em segundo plano verifique se não temos chamadas de bloqueio nessa função.
// ---------------------------------------------------------------------------------------------------------
time_t getNtpTime()
{
	gwayConfig.ntps++;

	if (!sendNtpRequest(ntpServer)) // Envie o pedido para novo horário.
	{
		if (debug > 0)
			Serial.println(F("sendNtpRequest:: falhou."));
		return (0);
	}

	const int NTP_PACKET_SIZE = 48; // Tamanho fixo do registro NTP.
	byte packetBuffer[NTP_PACKET_SIZE];
	memset(packetBuffer, 0, NTP_PACKET_SIZE); // Definir o conteúdo do buffer para zero.

	uint32_t beginWait = millis();
	delay(10);
	while (millis() - beginWait < 1500)
	{
		int size = Udp.parsePacket();
		if (size >= NTP_PACKET_SIZE)
		{

			if (Udp.read(packetBuffer, NTP_PACKET_SIZE) < NTP_PACKET_SIZE)
			{
				break;
			}
			else
			{
				// Extrai a parte dos segundos.
				unsigned long secs;
				secs = packetBuffer[40] << 24;
				secs |= packetBuffer[41] << 16;
				secs |= packetBuffer[42] << 8;
				secs |= packetBuffer[43];
				// UTC é uma correção de fuso horário, quando não há horário de verão.
				return (secs - 2208988800UL + NTP_TIMEZONES * SECS_PER_HOUR);
			}
			Udp.flush();
		}
		delay(100); // Aguarde 100 milissegundos, permite que o kernel atue quando necessário.
	}

	Udp.flush();

	// Se estamos aqui, não conseguimos ler a hora da internet.
	// Então, incrementa o contador.
	gwayConfig.ntpErr++;
	gwayConfig.ntpErrTime = millis();
	if (debug > 0)
		Serial.println(F("getNtpTime:: falhou a leitura!"));
	return (0); // retorna 0 se não conseguir o tempo.
}

// ---------------------------------------------------------------------------------------------------------
// Configuramos a sincronização regular do servidor NTP e a hora local.
// ---------------------------------------------------------------------------------------------------------
#if NTP_INTR == 1
void setupTime()
{
	setSyncProvider(getNtpTime);
	setSyncInterval(_NTP_INTERVAL);
}
#endif

// =========================================================================================================
// FUNÇÕES DE UDP E WLAN.
// =========================================================================================================
// ---------------------------------------------------------------------------------------------------------
// config.txt é um arquivo de texto que contém linhas (!) com itens de configuração WPA.
// Cada linha contém um par: "chave":"valor", descrevendo a configuração do gateway.
// ---------------------------------------------------------------------------------------------------------
int WlanReadWpa()
{

	readConfig(CONFIGFILE, &gwayConfig);

	if (gwayConfig.sf != (uint8_t)0)
		sf = (sf_t)gwayConfig.sf;
	ifreq = gwayConfig.ch;
	debug = gwayConfig.debug;
	_cad = gwayConfig.cad;
	_hop = gwayConfig.hop;
	gwayConfig.boots++; // A cada inicialização do sistema é incrementada a redefinição.

#if GATEWAYNODE == 1
	if (gwayConfig.fcnt != (uint8_t)0)
		frameCount = gwayConfig.fcnt + 10;
#endif

#if WIFIMANAGER > 0
	String ssid = gwayConfig.ssid;
	String pass = gwayConfig.pass;

	char ssidBuf[ssid.length() + 1];
	ssid.toCharArray(ssidBuf, ssid.length() + 1);
	char passBuf[pass.length() + 1];
	pass.toCharArray(passBuf, pass.length() + 1);
	Serial.print(F("WlanLerWpa: "));
	Serial.print(ssidBuf);
	Serial.print(F(", "));
	Serial.println(passBuf);

	strcpy(wpa[0].login, ssidBuf); // XXX Alteração de wpa [0] [0] = ssidBuf.
	strcpy(wpa[0].passw, passBuf);

	Serial.print(F("WlanLerWpa: < "));
	Serial.print(wpa[0].login); // XXX
	Serial.print(F(" >, < "));
	Serial.print(wpa[0].passw);
	Serial.println(F(" >"));
	Serial.println(F("."));
#endif
}

// ---------------------------------------------------------------------------------------------------------
// Imprime os dados WPA do último WiFiManager enviado para o arquivo na memória SPIFFS.
// ---------------------------------------------------------------------------------------------------------
#if WIFIMANAGER == 1
int WlanWriteWpa(char *ssid, char *pass)
{

	Serial.print(F("WlanWriteWpa:: SSID = "));
	Serial.print(ssid);
	Serial.print(F(", Senha = "));
	Serial.print(pass);
	Serial.println();

	// Uso da versão 3.3 do arquivo de configuração.
	String s((char *)ssid);
	gwayConfig.ssid = s;

	String p((char *)pass);
	gwayConfig.pass = p;

#if GATEWAYNODE == 1
	gwayConfig.fcnt = frameCount;
#endif
	gwayConfig.ch = ifreq;
	gwayConfig.sf = sf;
	gwayConfig.cad = _cad;
	gwayConfig.hop = _hop;

	writeConfig(CONFIGFILE, &gwayConfig);
	return 1;
}
#endif

// ---------------------------------------------------------------------------------------------------------
// Função para se juntar à rede Wi-Fi:
// É uma questão de retornar ao loop() o mais rápido possível e certificar-se de que no próximo loop a reconexão seja feita primeiro.
//
// Parâmetros:
// int maxTtry: Número de reties que fazemos:
// 0: Tente sempre. Qual é normalmente o que queremos, exceto para instalação talvez.
// 1: Tente uma vez e se o retorno não for bem-sucedido (0).
// x: Tente x vezes.
//
// Retorna:
// Na falha: Return -1
// int número de tentativas necessárias.
// ---------------------------------------------------------------------------------------------------------
int WlanConnect(int maxTry)
{

#if WIFIMANAGER == 1
	WiFiManager wifiManager;
#endif

	unsigned char agains = 0;
	unsigned char wpa_index = (WIFIMANAGER > 0 ? 0 : 1); // Pula o primeiro registro do WiFiManager.

	// Portanto, tente conectar-se à WLAN, desde que não estejam conectados.
	// Os parâmetros try nos dizem quantas vezes tentamos antes de desistir.
	int i = 0;

	// Nós tentamos 5 vezes antes de desistir da conexão.
	while ((WiFi.status() != WL_CONNECTED) && (i < maxTry))
	{

		// Tentamos todos os SSID em wap array até o sucesso.
		for (int j = wpa_index; j < (sizeof(wpa) / sizeof(wpa[0])); j++)
		{

			// Começamos com pontos de acesso conhecidos na lista.
			char *ssid = wpa[j].login;
			char *password = wpa[j].passw;

			Serial.print(i);
			Serial.print(':');
			Serial.print(j);
			Serial.print(F(" ('~')/Tentando se conectar a Rede Local: ")); // Esse que executa primeiro na inicialização.
			Serial.print(ssid);
			Serial.println(F("."));

			// Conte o número de vezes que chamamos WiFi.begin.
			gwayConfig.wifis++;
			writeGwayCfg(CONFIGFILE);

			WiFi.begin(ssid, password);

			// Aumentamos o tempo de conexão, mas tentamos o mesmo SSID, Nós tentamos por 3 vezes.
			agains = 0;
			while ((WiFi.status() != WL_CONNECTED) && (agains < 3))
			{
				delay(agains * 400);
				agains++;
				if (debug >= 2)
					Serial.println(F("."));
			}
			if (WiFi.status() == WL_CONNECTED)
				break;
			else
				WiFi.disconnect();
		}	//for
		i++; // Número de vezes que tentamos nos conectar.
	}

#if DUSB >= 1
	if (i > 0)
	{
		Serial.print(F("WLAN reconectado."));
		Serial.println();
	}
#endif

	// Ainda não está conectado?
	if (WiFi.status() != WL_CONNECTED)
	{
#if WIFIMANAGER == 1
		Serial.println(F("Iniciando o modo de Access Point (AP)"));
		Serial.print(F("Ligue seu WiFi ao ponto de acesso: "));
		Serial.print(AP_NAME);
		Serial.print(F(" e conecte-se ao (ALTERAR AQUI) IP: 192.168.0.101"));
		Serial.println();
		wifiManager.autoConnect(AP_NAME, AP_PASSWD);
		//wifiManager.startConfigPortal(AP_NAME, AP_PASSWD);

		// Neste ponto, há um ponto de acesso Wi-Fi encontrado e conectado.
		// Devemos nos conectar ao armazenamento local "SPIFFS", para armazenar o ponto de acesso.
		String s = WiFi.SSID();					// Estava comentado.
		char ssidBuf[s.length() + 1];			// Estava comentado.
		s.toCharArray(ssidBuf, s.length() + 1); // Estava comentado.

		// Agora procure a senha.
		struct station_config sta_conf;
		wifi_station_get_config(&sta_conf);

		WlanWriteWpa(ssidBuf, (char *)sta_conf.password); // Estava comentado.
		WlanWriteWpa((char *)sta_conf.ssid, (char *)sta_conf.password);
#else
		return (-1);
#endif
	}
	yield();
	return (1);
}

// ---------------------------------------------------------------------------------------------------------
// Leitura de um pacote do soquete UDP, este pode vir de qualquer servidor.
// As mensagens são recebidas quando o servidor responde às solicitações do gateway e dos rádios LoRa nos nodes,
// Por exemplo, solicitações de JOIN, ou quando o servidor tiver metadados de recebimento de dados.
// Respondemos apenas ao servidor que nos enviou uma mensagem!
//
// Nota: Então normalmente podemos esquecer aqui os códigos que fazem upstream.
//
// Parâmetros:
// Packetsize: tamanho do buffer para ler, como lido pela função de chamada loop ().
//
// Retorna:
// -1 ou falso se não for lido.
// Ou o número de caracteres lidos com sucesso.
// ---------------------------------------------------------------------------------------------------------
int readUdp(int packetSize)
{
	uint8_t protocol;
	uint16_t token;
	uint8_t ident;
	uint8_t buff[32];				 // Buffer geral a ser usado pelo UDP, definido como 64. (Observação! :/ == 32)
	uint8_t buff_down[RX_BUFF_SIZE]; // Buffer para downstream.

	if (WlanConnect(10) < 0)
	{
#if DUSB >= 1
		Serial.print(F("readUdp:: ERRO ao tentar conectar à WLAN."));
		if (debug >= 2)
			Serial.flush();
#endif
		Udp.flush();
		yield();
		return (-1);
	}
	yield();

	if (packetSize > RX_BUFF_SIZE)
	{
#if DUSB >= 1
		Serial.print(F("readUDP:: Tamanho do pacote de ERRO: "));
		Serial.println(packetSize);
#endif
		Udp.flush();
		return (-1);
	}

	// Assumimos aqui que conhecemos o originador da mensagem.
	// Porém na prática pode ser qualquer remetente!
	if (Udp.read(buff_down, packetSize) < packetSize)
	{
#if DUSB >= 1
		Serial.println(F("readUsb:: Lendo menos chars, DebugUSB==1."));
		return (-1);
#endif
	}

	// Endereço Remoto deve ser conhecido.
	IPAddress remoteIpNo = Udp.remoteIP();

	// Porta do servidor remoto TTN ou do servidor NTP (= 123).
	unsigned int remotePortNo = Udp.remotePort();

	if (remotePortNo == 123)
	{
		// Esta é uma mensagem NTP chegando
#if DUSB >= 1
		if (debug > 0)
		{
			Serial.println(F("readUdp:: Mensagem NTP recebida."));
		}
#endif
		gwayConfig.ntpErr++;
		gwayConfig.ntpErrTime = millis();
		return (0);
	}

	// Se não for NTP, deve ser uma mensagem LoRa para gateway ou nó.
	else
	{
		uint8_t *data = (uint8_t *)((uint8_t *)buff_down + 4);
		protocol = buff_down[0];
		token = buff_down[2] * 256 + buff_down[1];
		ident = buff_down[3];

		// Analisamos agora o tipo de mensagem do servidor (se houver).
		switch (ident)
		{

			// Esta mensagem é usada pelo gateway para enviar dados do sensor para o servidor.
			// Como esta função é usada somente para downstream, esta opção nunca será selecionado, mas está inclusa apenas como referência.
		case PKT_PUSH_DATA: // 0x00 UP (Para Cima - Uplink).

#if DUSB >= 1
			if (debug >= 1)
			{
				Serial.println(F("Gateway enviou dados do sensor para o servidor, UPLink: Sensor --> Gateway --> AppServer."));
				Serial.print(F("PKT_PUSH_DATA:: Tamanho: "));
				Serial.print(packetSize);
				Serial.print(F(", do IP: "));
				Serial.print(remoteIpNo);
				Serial.print(F(", porta: "));
				Serial.print(remotePortNo);
				Serial.print(F(", dados: "));
				for (int i = 0; i < packetSize; i++)
				{
					Serial.print(buff_down[i], HEX);
					Serial.print(':');
				}
				Serial.println(F("."));
				if (debug >= 2)
					Serial.flush();
			}
#endif
			break;

		// Esta mensagem é enviada pelo servidor para confirmar o recebimento de uma mensagem.
		// (sensor) enviada com o código acima.
		case PKT_PUSH_ACK: // 0x01 DOWN (Para Baixo - Downlink).

#if DUSB >= 1
			if (debug >= 2)
			{
				Serial.println(F("Confirmação do servidor sobre o recebimento de uma mensagem, DOWNLink: AppServer --> Gateway."));
				Serial.print(F("PKT_PUSH_ACK:: Tamanho: "));
				Serial.print(packetSize);
				Serial.print(F(", do IP: "));
				Serial.print(remoteIpNo);
				Serial.print(F(", porta: "));
				Serial.print(remotePortNo);
				Serial.print(F(", Token: "));
				Serial.println(token, HEX);
				Serial.println(F("."));
			}
#endif
			break;

		case PKT_PULL_DATA: // 0x02 UP (Para Cima - Uplink).
#if DUSB >= 1
			Serial.println(F("Puxar Dados, UPLink: Sensor --> Gateway --> AppServer"));
#endif
			break;

			// Esse tipo de mensagem é usado para confirmar a mensagem OTAA para o nó.
			// XXX Esse formato de mensagem também pode ser usado para outra comunicação downstream.
		case PKT_PULL_RESP: // 0x03 DOWN (Para Baixo - Downlink).
#if DUSB >= 1
			if (debug >= 0)
			{
				Serial.println(F("Confirmação da mensagem de OTAA para o nó, DOWNLink: AppServer --> Gateway --> Nó."));
				Serial.println(F("PKT_PULL_RESP:: recebido."));
			}
#endif
			lastTmst = micros(); // Armazena o último "tmst" deste pacote foi recebido.

			// Enviamos para o nó LoRa, primeiro o "tempo", em seguida, enviamos as mensagens.
			_state = S_TX;
			if (sendPacket(data, packetSize - 4) < 0)
			{
				return (-1);
			}

			// Agora responda com um PKT_TX_ACK; 0x04 UP (Para Cima - Uplink).
			buff[0] = buff_down[0];
			buff[1] = buff_down[1];
			buff[2] = buff_down[2];
			//buff[3]=PKT_PULL_ACK; // Puxe pedido / Mudança de Mogyi.
			buff[3] = PKT_TX_ACK;
			buff[4] = MAC_array[0];
			buff[5] = MAC_array[1];
			buff[6] = MAC_array[2];
			buff[7] = 0xFF;
			buff[8] = 0xFF;
			buff[9] = MAC_array[3];
			buff[10] = MAC_array[4];
			buff[11] = MAC_array[5];
			buff[12] = 0;
#if DUSB >= 1
			Serial.println(F("readUdp:: Buffer de TX (Transmissão) preenchido."));
#endif
			// Apenas envie o PKT_PULL_ACK para o soquete UDP que acabou de enviar os dados!
			Udp.beginPacket(remoteIpNo, remotePortNo);

#ifdef ESP32BUILD
			if (Udp.write(buff, 12) != 12)
			{
#else
			if (Udp.write((char *)buff, 12) != 12)
			{
#endif
#if DUSB >= 1
				if (debug >= 0)
					Serial.println(F("PKT_PULL_ACK:: ERRO na gravação do UDP."));
#endif
			}
			else
			{
#if DUSB >= 1
				if (debug >= 0)
				{
					Serial.print(F("PKT_TX_ACK:: tmst="));
					Serial.println(micros());
				}
#endif
			}

			if (!Udp.endPacket())
			{
#if DUSB >= 1
				if (debug >= 0)
					Serial.println(F("PKT_PULL_DATALL, ERRO em: Udp.endpaket."));
#endif
			}
			yield();

#if DUSB >= 1
			if (debug >= 1)
			{
				Serial.print(F("PKT_PULL_RESP:: Tamanho: "));
				Serial.print(packetSize);
				Serial.print(F(", do IP: "));
				Serial.print(remoteIpNo);
				Serial.print(F(", porta: "));
				Serial.print(remotePortNo);
				Serial.print(F(", dados: "));
				data = buff_down + 4;
				data[packetSize] = 0;
				Serial.print((char *)data);
				Serial.println(F("."));
			}
#endif
			break;

		case PKT_PULL_ACK: // 0x04 DOWN; o servidor envia um PULL_ACK para confirmar o recibo de PULL_DATA.
#if DUSB >= 1
			if (debug >= 2)
			{
				Serial.print(F("PKT_PULL_ACK:: Tamanho: "));
				Serial.print(packetSize);
				Serial.print(F(", do IP: "));
				Serial.print(remoteIpNo);
				Serial.print(F(", porta: "));
				Serial.print(remotePortNo);
				Serial.print(F(", dados: "));
				for (int i = 0; i < packetSize; i++)
				{
					Serial.print(buff_down[i], HEX);
					Serial.print(':');
				}
				Serial.println(F("."));
			}
#endif
			break;

		default:
#if GATEWAYMGT == 1
			// Para simplificar, enviamos os primeiros 4 bytes também.
			gateway_mgt(packetSize, buff_down);
#else

#endif
#if DUSB >= 1
			Serial.print(F(", ERRO, identificador não reconhecido = "));
			Serial.print(ident);
			Serial.println(F("."));
#endif
			break;
		}
#if DUSB >= 2
		if (debug >= 1)
		{
			Serial.print(F("readUdp:: Retornando = "));
			Serial.print(packetSize);
			Serial.println(F("."));
		}
#endif
		// Para mensagens downstream.
		return packetSize;
	}
} //readUdp

// ---------------------------------------------------------------------------------------------------------
// Envia uma mensagem UDP/DGRAM para o servidor MQTT.
// Se enviarmos para mais de um host (não sabemos por que), precisamos definir o sockaddr antes de enviá-lo.
//
// Parâmetros:
// IPAddress.
// port.
// msg *.
// length (de msg - tamanho da mensagem).
//
// Retorna valores:
// 0: ERRO.
// 1: Sucesso.
// ---------------------------------------------------------------------------------------------------------
int sendUdp(IPAddress server, int port, uint8_t *msg, int length)
{

	// Verifica se estamos conectados ao WiFi e à internet.
	if (WlanConnect(3) < 0)
	{
#if DUSB >= 1
		Serial.print(F("sendUdp: ERRO, conectando ao WiFi."));
		Serial.flush();
#endif
		Udp.flush();
		yield();
		return (0);
	}
	yield();

// envia a atualização.
#if DUSB >= 1
	if (debug >= 2)
		Serial.println(F("WiFi conectado."));
#endif
	if (!Udp.beginPacket(server, (int)port))
	{
#if DUSB >= 1
		if (debug >= 1)
			Serial.println(F("sendUdp:: ERRO, Udp.beginPacket."));
#endif
		return (0);
	}
	yield();

#ifdef ESP32BUILD
	if (Udp.write(msg, length) != length)
	{
#else
	if (Udp.write((char *)msg, length) != length)
	{
#endif
#if DUSB >= 1
		Serial.println(F("sendUdp:: ERRO de gravação."));
#endif
		Udp.endPacket(); // Fechar UDP.
		return (0);		 // ERRO de retorno.
	}
	yield();

	if (!Udp.endPacket())
	{
#if DUSB >= 1
		if (debug >= 1)
		{
			Serial.println(F("sendUdp:: ERRO, Udp.endPacket."));
			Serial.flush();
		}
#endif
		return (0);
	}
	return (1);
} //sendUDP

// ---------------------------------------------------------------------------------------------------------
// UDPconnect (): conectar-se ao UDP, que é uma coisa local, afinal conexões UDP não existem.
//
// Parâmetros:
// <Nenhum>
// Retorna: Boollean indicando sucesso ou não.
// ---------------------------------------------------------------------------------------------------------
bool UDPconnect()
{

	bool ret = false;
	unsigned int localPort = _LOCUDPPORT; // Para ouvir para retornar mensagens do WiFi.
#if DUSB >= 1
	if (debug >= 1)
	{
		Serial.print(F("PortaLocal UDP = "));
		Serial.print(localPort);
		Serial.println(F("."));
	}
#endif
	if (Udp.begin(localPort) == 1)
	{
#if DUSB >= 1
		if (debug >= 1)
			Serial.println(F("Conexão bem sucedida."));
#endif
		ret = true;
	}
	else
	{
#if DUSB >= 1
		if (debug >= 1)
			Serial.println(F("Conexão falhou."));
#endif
	}
	return (ret);
} //udpConnect

// ---------------------------------------------------------------------------------------------------------
// Envia mensagem pull_DATA periódica para o servidor para manter a conexão ativa e convidar o servidor
// para enviar mensagens descendentes quando elas estiverem disponíveis.
// * 2, par. 5,2.
// - Versão do Protocolo (1 byte).
// - Token Aleatório (2 bytes).
// - Identificador PULL_DATA (1 byte) = 0x02
// - Identificador exclusivo do gateway (8 bytes) = endereço MAC.
// ---------------------------------------------------------------------------------------------------------
void pullData()
{

	uint8_t pullDataReq[12]; // relatório de status como um objeto JSON.
	int pullIndex = 0;
	int i;

	uint8_t token_h = (uint8_t)rand(); // token aleatório.
	uint8_t token_l = (uint8_t)rand(); // token aleatório.

	// preencha o buffer de dados com campos fixos.
	pullDataReq[0] = PROTOCOL_VERSION; // 0x01
	pullDataReq[1] = token_h;
	pullDataReq[2] = token_l;
	pullDataReq[3] = PKT_PULL_DATA; // 0x02
									// LÊ O ENDEREÇO MAC DO ESP32/ESP8266, e retorne o ID exclusivo do Gateway que consiste em endereço MAC e 2bytes 0xFF.
	pullDataReq[4] = MAC_array[0];
	pullDataReq[5] = MAC_array[1];
	pullDataReq[6] = MAC_array[2];
	pullDataReq[7] = 0xFF;
	pullDataReq[8] = 0xFF;
	pullDataReq[9] = MAC_array[3];
	pullDataReq[10] = MAC_array[4];
	pullDataReq[11] = MAC_array[5];
	//pullDataReq[12] = 0/00; // adiciona terminador de string, por segurança.

	pullIndex = 12; // cabeçalho de 12 bytes

	// enviar a atualização.
	uint8_t *pullPtr;
	pullPtr = pullDataReq,
#ifdef _TTNSERVER
	sendUdp(ttnServer, _TTNPORT, pullDataReq, pullIndex);
	yield();

#endif

#if DUSB >= 1
	if (pullPtr != pullDataReq)
	{
		Serial.println(F("pullPtr != pullDatReq"));
		Serial.flush();
	}

#endif
#ifdef _THINGSERVER
	sendUdp(thingServer, _THINGPORT, pullDataReq, pullIndex);
#endif

#if DUSB >= 1
	if (debug >= 2)
	{
		yield();

		Serial.print(F("PKT_PULL_DATA request, len = < "));
		Serial.print(pullIndex);
		Serial.print(F(" >"));

		for (i = 0; i < pullIndex; i++)
		{
			Serial.print(pullDataReq[i], HEX); // DEBUG: exibir o stat JSON.
			Serial.print(':');
		}
		Serial.println();
		if (debug >= 2)
			Serial.flush();
	}
#endif

	return;
} //pullData

// ---------------------------------------------------------------------------------------------------------
// Envia mensagem de status periódica para o servidor, mesmo quando não recebemos nenhum dado.
//
// Parâmetros:
// - <nenhum>
// ---------------------------------------------------------------------------------------------------------
void sendstat()
{

	uint8_t status_report[STATUS_SIZE]; // relatório de status como um objeto JSON.
	char stat_timestamp[32]; // XXX estava 24.
	time_t t;

	char clat[10] = {0};
	char clon[10] = {0};

	int stat_index = 0;
	uint8_t token_h = (uint8_t)rand(); // token aleatório.
	uint8_t token_l = (uint8_t)rand(); // token aleatório.

	// preenche o buffer de dados com campos fixos.
	status_report[0] = PROTOCOL_VERSION; // 0x01
	status_report[1] = token_h;
	status_report[2] = token_l;
	status_report[3] = PKT_PUSH_DATA; // 0x00

	// LÊ O ENDEREÇO MAC DO ESP32/ESP8266, e retorne o ID exclusivo do Gateway que consiste em endereço MAC e 2bytes 0xFF.
	status_report[4] = MAC_array[0];
	status_report[5] = MAC_array[1];
	status_report[6] = MAC_array[2];
	status_report[7] = 0xFF;
	status_report[8] = 0xFF;
	status_report[9] = MAC_array[3];
	status_report[10] = MAC_array[4];
	status_report[11] = MAC_array[5];

	stat_index = 12; // cabeçalho de 12 bytes

	t = now(); // obter registro de data e hora para estatísticas.

	// XXX Usando CET como o fuso horário atual. Mude para o seu fuso horário.
	sprintf(stat_timestamp, "%04d-%02d-%02d %02d:%02d:%02d CET", year(), month(), day(), hour(), minute(), second());
	yield();

	ftoa(lat, clat, 5); // Converte lat (latitude) em um array de "char" com 5 casas decimais.
	ftoa(lon, clon, 5); // Como IDE NÃO PODE imprimir floats. (Faz com a longitude o mesmo que é feito acima com a latitude).

	// Constroe a mensagem Status no formato JSON, XXX Divide esta...
	delay(1);

	int j = snprintf((char *)(status_report + stat_index), STATUS_SIZE - stat_index,
					 "{\"stat\":{\"time\":\"%s\",\"lati\":%s,\"long\":%s,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%u.0,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}",
					 stat_timestamp, clat, clon, (int)alt, cp_nb_rx_rcv, cp_nb_rx_ok, cp_up_pkt_fwd, 0, 0, 0, platform, email, description);
	yield(); // Dá lugar ao serviço de limpeza interno do ESP32/ESP8266.

	stat_index += j;
	status_report[stat_index] = 0; // adiciona terminador de string, por segurança.

	if (debug >= 2)
	{
		Serial.print(F("atualização de estatísticas: <<< "));
		Serial.print(stat_index);
		Serial.print(F(" >>>"));
		Serial.println((char *)(status_report + 12)); // DEBUG: exibir o stat JSON.
	}

	if (stat_index > STATUS_SIZE)
	{
		Serial.println(F("sendstat:: Buffer de ERRO muito grande."));
		return;
	}

	// envia a atualização.
#ifdef _TTNSERVER
	sendUdp(ttnServer, _TTNPORT, status_report, stat_index);
	yield();
#endif

#ifdef _THINGSERVER
	sendUdp(thingServer, _THINGPORT, status_report, stat_index);
#endif
	return;
} //sendstat

// =========================================================================================================
// CÓDIGO DO PROGRAMA PRINCIPAL (setup() e loop() - CONFIGURAÇÃO E LAÇO)
// =========================================================================================================
// ---------------------------------------------------------------------------------------------------------
// Setup code (uma vez).
// _state is S_INIT
// ---------------------------------------------------------------------------------------------------------
void setup()
{

	char MAC_char[19]; // XXX Inacreditável.
	MAC_char[18] = 0;

	Serial.begin(115200); // O mais rápido possível para o ônibus.
	delay(100);
	Serial.flush();
	delay(500);

#if MUTEX_SPI == 1
	CreateMutux(&inSPI);
#endif
#if MUTEX_SPO == 1
	CreateMutux(&inSPO);
#endif
#if MUTEX_INT == 1
	CreateMutux(&inIntr);
#endif
	if (SPIFFS.begin())
		Serial.println(F("SPIFFS Carregado com Sucesso."));
	Serial.println();
	Serial.println(F("Assert - Afirmação. "));
	Serial.println();
#if defined CFG_noassert
	Serial.println(F("No Asserts - Não há afirmação."));
#else
	Serial.println(F("Do Asserts - Preparar afirmação."));
#endif

#if OLED == 1
// Inicializar a interface do usuário também iniciará a exibição.
#ifdef ESP32BUILD
	pinMode(16, OUTPUT);
	digitalWrite(16, LOW); // Define o GPIO16 em nível lógico, baixo para redefinir o OLED.
	delay(50);
	digitalWrite(16, HIGH); // Enquanto o OLED está em execução, deve definir GPIO16 em nível lógico alto.
#endif
	display.init();
	display.flipScreenVertically(); // Coloca o display de ponta cabeça.
	// setFont recebe como parâmetro o tamanho da fonte de escrita:
	// Pode-se utilizar as seguintes fontes:
	// ArialMT_Plain_10
	// ArialMT_Plain_16
	// ArialMT_Plain_24
	display.setFont(ArialMT_Plain_24);
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.drawString(3, 6, "INICIANDO"); // Posição do texto e conteúdo do mesmo.
	display.drawString(3, 36, "LoRa \\('~')/"); // posição do texto na tela. (repeti "\" Para ter efeito).
	display.display(); // Mostra as alterações no display, sem isso não irá mostrar nada!
#endif

	delay(2000);
	//yield();

  display.init();
  display.flipScreenVertically(); // Coloca o display de ponta cabeça.
  // setFont recebe como parâmetro o tamanho da fonte de escrita:
  // Pode-se utilizar as seguintes fontes:
  // ArialMT_Plain_10
  // ArialMT_Plain_16
  // ArialMT_Plain_24
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(5, 6, "The Things"); // Posição do texto e conteúdo do mesmo.
  display.drawString(25, 36, "Network"); // posição do texto na tela. (repeti "\" Para ter efeito).
  display.display(); // Mostra as alterações no display, sem isso não irá mostrar nada!

  delay(2000);
  //yield();

  display.init();
  display.flipScreenVertically(); // Coloca o display de ponta cabeça.
  // setFont recebe como parâmetro o tamanho da fonte de escrita:
  // Pode-se utilizar as seguintes fontes:
  // ArialMT_Plain_10
  // ArialMT_Plain_16
  // ArialMT_Plain_24
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 6, ">> T T N <<"); // Posição do texto e conteúdo do mesmo.
  display.setFont(ArialMT_Plain_16);
  display.drawString(13, 40, "TERESINA-PI"); // posição do texto na tela. (repeti "\" Para ter efeito).
  display.display(); // Mostra as alterações no display, sem isso não irá mostrar nada!

  delay(1000);
  yield();
  
#if DUSB >= 1
	if (debug >= 1)
	{
		Serial.print(F("Nível de depuração = "));
		Serial.print(debug);
		Serial.println(F("."));
		yield();
	}

#endif
	WiFi.mode(WIFI_STA);
	WlanReadWpa(); // Leia as últimas configurações de WiFi de SPIFFS na memória.

	WiFi.macAddress(MAC_array);
	sprintf(MAC_char, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_char[0], MAC_array[1], MAC_char[2], MAC_array[3], MAC_char[4], MAC_array[5]);
	Serial.print(F("MAC: "));
	Serial.print(MAC_char);
	Serial.print(F(", Tamanho = "));
	Serial.print(strlen(MAC_char)); // strlen retorna o tamanho, em caracteres, de uma string dada.
	// Na verdade o strlen() procura o terminador de string e calcula a distância dele ao início da string.
	Serial.println(F("."));

	// Começamos conectando-nos a uma rede WiFi, configuramos o hostname.
	char hostname[12];
#ifdef ESP32BUILD
	sprintf(hostname, "%s%02x%02x%02x", "LoRa_esp32-", MAC_array[3], MAC_array[4], MAC_array[5]);
#else
	sprintf(hostname, "%s%02x%02x%02x", "LoRa_esp8266-", MAC_array[3], MAC_array[4], MAC_array[5]);
#endif

#ifdef ESP32BUILD
	WiFi.setHostname(hostname);
#else
	wifi_station_set_hostname(hostname);
#endif

	// Configuração da conexão WiFi UDP. Dê um tempo e tente novamente 50 vezes.
	while (WlanConnect(50) < 0)
	{
		Serial.println(F("ERRO de conexão de rede Wi-Fi."));
		yield();
	}

	Serial.print(F("Host: "));
#ifdef ESP32BUILD
	Serial.print(WiFi.getHostname());
#else
	Serial.print(wifi_station_get_hostname());
#endif
	Serial.print(F(", Conectado a: "));
	Serial.print(WiFi.SSID());
	Serial.print(F("."));
	Serial.println();
	delay(200);

	// Se estamos aqui, estamos conectados a WLAN.
	// Agora teste a função UDP.
	if (!UDPconnect())
	{
		Serial.println(F("ERRO UDPconnect, testando conexão UDP."));
	}
	delay(200);

	// Pins são definidos e definidos em loraModem.h.
	pinMode(pins.ss, OUTPUT); // Pino 18 Para Heltec, Setado em loraModem.h.
	pinMode(pins.rst, OUTPUT); // Pino 14 Para Heltec, Setado em loraModem.h.

	// Esse pino é de interrupção. GPIO5/D1. Dio0 usado para uma frequência e um SF.
	pinMode(pins.dio0, INPUT); // Pino 26 Para Heltec, Setado em loraModem.h.

	// Este pino é de interrupção. GPIO4/D2. Usado para CAD, pode ou não ser compartilhado com DIO0.
	pinMode(pins.dio1, INPUT); // Pino 33 Para Heltec, Setado em loraModem.h.

	// Este pino estava comentado. GPIO0/D3. Usado para salto de frequência, não importante.
	pinMode(pins.dio2, INPUT); // Pino 32 Para Heltec, Setado em loraModem.h.

// Inicia os pinos da comunição SPI.
#ifdef ESP32BUILD
	// Está setado em loraModem.h.
	SPI.begin(SCK, MISO, MOSI, SS);
#else
	SPI.begin();
#endif

	delay(500);

	// Escolhemos o ID do Gateway para ser o endereço Ethernet do nosso cartão Gateway.
	// exibe resultados de obtenção de endereço de hardware.
	Serial.print(F("Gateway ID: "));
	printHexDigit(MAC_array[0]);
	printHexDigit(MAC_array[1]);
	printHexDigit(MAC_array[2]);
	printHexDigit(0xFF); // é Acrescentado ao meio do MAC da Placa para formar o ID do Gateway.
	printHexDigit(0xFF); // é Acrescentado ao meio do MAC da Placa para formar o ID do Gateway.
	printHexDigit(MAC_array[3]);
	printHexDigit(MAC_array[4]);
	printHexDigit(MAC_array[5]);
	Serial.println(".");
	Serial.print(F("Ouvindo no Spreading Factor (SF): "));
	Serial.print(sf);
	Serial.print(F(" em: "));
	Serial.print((double)freq / 1000000);
	Serial.println(F("Mhz."));

	if (!WiFi.hostByName(NTP_TIMESERVER, ntpServer)) // Obtém o endereço IP do Timeserver (Servidor de tempo).
	{
		die("Configuração:: ERRO em hostByName NTP.");
	};
	delay(100);
#ifdef _TTNSERVER
	if (!WiFi.hostByName(_TTNSERVER, ttnServer)) // Use DNS para obter o IP do servidor uma vez.
	{
		die("Configuração:: ERRO em hostByName TTN.");
	};
	delay(100);
#endif
#ifdef _THINGSERVER
	if (!WiFi.hostByName(_THINGSERVER, thingServer))
	{
		die("Configuração:: ERRO em hostByName THING.");
	}
	delay(100);
#endif

// As atualizações Over the Air (OTAA) são suportadas quando temos uma conexão WiFi.
// Não há necessidade da configuração de hora do NTP ser precisa para que essa função funcione.
#if A_OTA == 1
	setupOta(hostname); // Usa o wwwServer.
#endif

// Definir o horário do NTP
#if NTP_INTR == 1
	setupTime(); // Configura o host e o intervalo de tempo do NTP.
#else
	//setTime((time_t)getNtpTime());
	while (timeStatus() == timeNotSet)
	{
		Serial.println(F("setupTime:: Tempo não definido (ainda)."));
		delay(500);
		time_t newTime;
		newTime = (time_t)getNtpTime();
		if (newTime != 0)
			setTime(newTime);
	}
	Serial.print(F("Data/Hora: "));
	printTime();
	Serial.println(F("."));

	writeGwayCfg(CONFIGFILE);
	Serial.println(F("Configuração do gateway salva."));
#endif

#if A_SERVER == 1
	// Configura o servidor da web.
	setupWWW();
#endif

	delay(100); // Aguarda a configuração.

	// Configura e inicializa a máquina de estado LoRa em _loramModem.ino.
	_state = S_INIT;
	initLoraModem();

	if (_cad)
	{
		_state = S_SCAN;
		cadScanner(); // Começa sempre pelo SF7.
	}
	else
	{
		_state = S_RX;
		rxLoraModem();
	}
	LoraUp.payLength = 0; // Inicia o comprimento como 0.

	// Manipuladores de interrupção de inicialização, que são compartilhados para GPIO15/D8,
	// Ligamos interrupções com nível lógico alto: HIGH.
	if (pins.dio0 == pins.dio1)
	{
		//SPI.usingInterrupt(digitalPinToInterrupt(pins.dio0));
		attachInterrupt(pins.dio0, Interrupt_0, RISING); // Compartilhar interrupções.
	}
	// Ou no caso tradicional de Comresult.
	else
	{
		//SPI.usingInterrupt(digitalPinToInterrupt(pins.dio0));
		//SPI.usingInterrupt(digitalPinToInterrupt(pins.dio1));
		attachInterrupt(pins.dio0, Interrupt_0, RISING); // Separar interrupções.
		attachInterrupt(pins.dio1, Interrupt_1, RISING); // Separar interrupções.
	}

	writeConfig(CONFIGFILE, &gwayConfig); // Escrever configuração.

// Display OLED I2C Azul Amarelo 0.96 Polegadas:
// Ativar display OLED
#if OLED == 1
	// Inicializar a interface do usuário também iniciará a exibição.
	display.clear(); // Limpa o display para exibição.
	// setFont recebe como parâmetro o tamanho da fonte de escrita:
	// Pode-se utilizar as seguintes fontes:
	// ArialMT_Plain_10
	// ArialMT_Plain_16
	// ArialMT_Plain_24
	display.setFont(ArialMT_Plain_24); // Escolha da fonte e tamanho da mesma.
	display.drawString(-1, 10, "OPERANTE"); // Posição do texto e conteúdo do mesmo.
	display.setFont(ArialMT_Plain_16); // Escolha da fonte e tamanho da mesma.
	display.setTextAlignment(TEXT_ALIGN_LEFT); // Alinhamento do texto.
	display.drawString(2, 43, "IP: " + WiFi.localIP().toString()); // Mostra IP da Rede Local.
	display.display(); // Mostra as alterações no display, sem isso não irá mostrar nada!
#endif

	Serial.println(F("<---------------------------------->"));
} // FIM de setup();

// ----------------------------------------------------------------------------
// LOOP
// Este é o programa principal que é executado uma e outra vez.
// Precisamos abrir caminho para o processamento WiFi de backend que ocorre em algum
// lugar no firmware ESP32/ESP8266 e, portanto, incluímos instruções yield () em pontos importantes.
//
// Nota: Se gastarmos muito tempo nas funções de processamento do usuário
// e o sistema backend não pode fazer sua manutenção, o watchdog
// função será executada, o que significa efetivamente que o
// programa falhas.
// Usamos muito yield () para evitar qualquer atividade do cão de guarda do programa.
//
// NOTA2: Para o ESP, certifique-se de não fazer grandes declarações de array em loop ();
// ----------------------------------------------------------------------------
void loop()
{
	uint32_t nowSeconds;
	int packetSize;

	nowTime = micros();
	nowSeconds = (uint32_t)millis() / 1000;

	// Verifica o valor do evento, o que significa que uma interrupção chegou.
	// Neste caso, tratamos da interrupção (por exemplo, mensagem recebida) no userspace em loop().
	if (_event != 0x00)
	{
		stateMachine(); // Inicie a máquina de estado.
		_event = 0; // Valor de reset.
		return; // Loop de reinicialização.
	}

  // Após um período de silêncio, certifique-se de reiniciar o modem.
  // XXX Ainda tem que medir o período silencioso no stat [0];
  // Por enquanto usamos msgTime
	if ((((nowTime - statr[0].tmst) / 1000000) > _MSG_INTERVAL) &&
		(msgTime < statr[0].tmst))
	{
#if DUSB >= 1
		Serial.print("'r' - Após um período de silêncio, certifique-se de reiniciar o modem.");
#endif
		initLoraModem();
		if (_cad)
		{
			_state = S_SCAN;
			cadScanner();
		}
		else
		{
			_state = S_RX;
			rxLoraModem();
		}
		writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t)0x00);
		writeRegister(REG_IRQ_FLAGS, 0xFF); // Redefinir todos os sinalizadores de interrupção.
		msgTime = nowTime;
	}

#if A_OTA == 1
	// Executar a atualização OTA (Over the Air) se ativada e solicitada pelo usuário.
	// É importante colocar esta função no início do loop().
	// ela não é chamado frequentemente, mas deve sempre ser executado quando chamada.
	yield();
	ArduinoOTA.handle();
#endif

#if A_SERVER == 1
	// Lidar com a parte do servidor WiFi deste sketch. Usado principalmente para administração
  // e monitoramento do nó. Esta função é importante por isso é chamada no início da função loop().
	yield();
	server.handleClient();
#endif

	// Se não estivermos conectados, tente se conectar.
  // Não vamos ler o Udp neste ciclo de loop então.
	if (WlanConnect(1) < 0)
	{
#if DUSB >= 1
		Serial.print(F("loop: ERRO reconectar WLAN"));
#endif
		yield();
		return; // Saia do loop se não houver WLAN conectada.
	}

	// Então, se estamos conectados.
  // Receber mensagens UDP PUSH_ACK do servidor. (* 2, par. 3.3)
  // Isso é importante porque o corretor TTN retornará a confirmação
  // mensagens no UDP para cada mensagem enviada pelo gateway. Então nós temos que consumi-los.
  // Como não sabemos quando o servidor responderá, testamos em todos os loops.
	else
	{
		while ((packetSize = Udp.parsePacket()) > 0)
		{ // Comprimento da mensagem UDP em espera.
#if DUSB >= 2
			Serial.println(F("loop:: Chamando leitura UDP."));
#endif
			// O pacote pode ser PKT_PUSH_ACK (0x01), PKT_PULL_ACK (0x03) ou PKT_PULL_RESP (0x04).
      // Este comando é encontrado no byte 4 (buffer [3]).
			if (readUdp(packetSize) <= 0)
			{
#if DUSB >= 1
				if (debug > 0)
					Serial.println(F("Erro de leitura UDP."));
#endif
				break;
			}
			// Agora sabemos que recebemos uma mensagem bem-sucedida do host.
			else
			{
				_event = 1; // Pode ser feito em dobro se mais mensagens forem recebidas.
			}
			//yield();
		}
	}
	yield();

	// A próxima seção é apenas de emergência. Se possível, nós hop() na máquina de estados.
	// Se hopping estiver habilitado, e por falta de timer, nós hop().
	// XXX Experimental, 2,5 ms entre o máximo de hops.

	if ((_hop) && (((long)(nowTime - hopTime)) > 7500))
	{

		if ((_state == S_SCAN) && (sf == SF12))
		{
#if DUSB >= 1
			if (debug >= 1)
				Serial.println(F("loop:: hop - Salto de Frequências."));
#endif
			hop();
		}

		// A seção XXX abaixo não funciona sem mais trabalho. É a seção com a maioria
		// influencia no modo de operação do HOP (o que é um pouco inesperado).
		// Se continuarmos em outro estado, resetar.
		else if (((long)(nowTime - hopTime)) > 100000)
		{

			_state = S_RX;
			rxLoraModem();

			hop();

			if (_cad)
			{
				_state = S_SCAN;
				cadScanner();
			}
		}
		else if (debug >= 3)
		{
			Serial.print(F(" state="));
			Serial.println(_state);
		}
		inHop = false; // Redefinir a proteção de reentrada do HOP.
		yield();
	}

	// stat PUSH_DATA messagem (*2, par. 4).

	if ((nowSeconds - stattime) >= _STAT_INTERVAL)
	{ // Acorde todos os segundos xx
#if DUSB >= 1
		if (debug >= 2)
		{
			Serial.print(F("STAT <"));
			Serial.flush();
		}
#endif
		sendstat(); // Mostrar a mensagem de status e enviar para o servidor.
#if DUSB >= 1
		if (debug >= 2)
		{
			Serial.println(F(">"));
			if (debug >= 2)
				Serial.flush();
		}
#endif

// Se o gateway se comporta como um nó, fazemos de tempos em tempos envia uma mensagem do nó para o servidor backend.
// O emessage do nod Gateway não tem nada a ver com o STAT_INTERVAL mensagem mas agendamos na mesma frequência.
#if GATEWAYNODE == 1
		if (gwayConfig.isNode)
		{
			// Dê lugar ao administrador interno, se necessário.
			yield();

			// Se o gateway 1ch é um sensor, envie os valores do sensor.
			// Pode ser bateria, mas também outras informações de status ou informações do sensor.

			if (sensorPacket() < 0)
			{
				Serial.println(F("sensorPacket: Erro."));
			}
		}
#endif
		stattime = nowSeconds;
	}
	yield();

	// envia PULL_DATA messagem (*2, par. 4)
	nowSeconds = (uint32_t)millis() / 1000;
	if ((nowSeconds - pulltime) >= _PULL_INTERVAL)
	{ // Acorde todos os segundos xx.
#if DUSB >= 1
		if (debug >= 1)
		{
			Serial.print(F("PULL <"));
			if (debug >= 2)
				Serial.flush();
		}
#endif
		pullData(); // Envie a mensagem PULL_DATA para o servidor.
		initLoraModem();
		if (_cad)
		{
			_state = S_SCAN;
			cadScanner();
		}
		else
		{
			_state = S_RX;
			rxLoraModem();
		}
		writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t)0x00);
		writeRegister(REG_IRQ_FLAGS, 0xFF); // Redefinir todos os sinalizadores de interrupção.
#if DUSB >= 1
		if (debug >= 1)
		{
			Serial.println(F(">"));
			if (debug >= 2)
				Serial.flush();
		}
#endif
		pulltime = nowSeconds;
	}

// Se fizermos o nosso próprio manuseio de NTP (aconselhável).
// Não usamos a interrupção do temporizador, mas usamos o tempo do próprio loop(), que é melhor para o SPI.
#if NTP_INTR == 0
	// Defina a hora de forma manual. Não use setSyncProvider.
	// Como esta função pode colidir com o SPI e outras interrupções.
	yield();
	nowSeconds = (uint32_t)millis() / 1000;
	if (nowSeconds - ntptimer >= _NTP_INTERVAL)
	{
		yield();
		time_t newTime;
		newTime = (time_t)getNtpTime();
		if (newTime != 0)
			setTime(newTime);
		ntptimer = nowSeconds;
	}
#endif
}
