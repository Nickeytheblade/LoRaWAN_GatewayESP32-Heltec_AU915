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
// O servidor Web ESP trabalha com Strings para exibir o conteúdo html.
// É preciso ter cuidado para que nem todos os dados sejam enviados para o servidor da Web em uma cadeia
// como isso vai usar um monte de memória e possivelmente matar o heap (causa sistema falha ou outro comportamento não confiável.)
// Em vez disso, a saída das várias tabelas na página da Web deve ser exibida em mandris para que as cordas sejam limitadas em tamanho.
// Esteja ciente de que não usar strings, mas somente chamadas sendContent() tem seu próprio desvantagem de que estas chamadas demorem
// muito tempo e façam com que a página seja exibido como uma velha máquina de escrever.
// Então, o truque é fazer mandris que são enviados para o site usando uma string de resposta, mas não torna essas Strings muito grandes.
// =========================================================================================================

// PRINT IP
// Saída do endereço IP de 4 bytes para facilitar a impressão.
// Como esta função também é usada por _otaServer.ino não coloque em #define
// ---------------------------------------------------------------------------------------------------------
static void printIP(IPAddress ipa, const char sep, String &response)
{
	response += (IPAddress)ipa[0];
	response += sep;
	response += (IPAddress)ipa[1];
	response += sep;
	response += (IPAddress)ipa[2];
	response += sep;
	response += (IPAddress)ipa[3];
}

// O restante do arquivo só funciona é A_SERVER = 1 está definido.
#if A_SERVER == 1

// =========================================================================================================
// DECLARAÇÕES DO WEBSERVER

// Nenhum no momento.

// =========================================================================================================
// FUNÇÕES DO WEBSERVER

// ---------------------------------------------------------------------------------------------------------
// Imprimir uma string HEXadecimal de uma cadeia char de 4 bytes.
// ---------------------------------------------------------------------------------------------------------
static void printHEX(char *hexa, const char sep, String &response)
{
	char m;
	m = hexa[0];
	if (m < 016)
		response += '0';
	response += String(m, HEX);
	response += sep;
	m = hexa[1];
	if (m < 016)
		response += '0';
	response += String(m, HEX);
	response += sep;
	m = hexa[2];
	if (m < 016)
		response += '0';
	response += String(m, HEX);
	response += sep;
	m = hexa[3];
	if (m < 016)
		response += '0';
	response += String(m, HEX);
	response += sep;
}

// ---------------------------------------------------------------------------------------------------------
// stringTime
// Somente quando o RTC está presente, imprimimos valores em tempo real
// t contém o número de milli segundos desde o início do sistema que o evento aconteceu.
// Portanto, um valor de 100 significaria que o evento ocorreu em 1 minuto e 40 segundos atrás.
// ---------------------------------------------------------------------------------------------------------
static void stringTime(unsigned long t, String &response)
{

	if (t == 0)
	{
		response += "--";
		return;
	}

	// now() dá segundos desde 1970.
	time_t eventTime = now() - ((millis() - t) / 1000);
	byte _hour = hour(eventTime);
	byte _minute = minute(eventTime);
	byte _second = second(eventTime);

	switch (weekday(eventTime))
	{
	case 1:
		response += "Domingo ";
		break;
	case 2:
		response += "Segunda-Feira ";
		break;
	case 3:
		response += "Terca-Feira ";
		break;
	case 4:
		response += "Quarta-Feira ";
		break;
	case 5:
		response += "Quinta-Feira ";
		break;
	case 6:
		response += "Sexta-Feira ";
		break;
	case 7:
		response += "Sabado ";
		break;
	}
	response += String() + day(eventTime) + "-";
	response += String() + month(eventTime) + "-";
	response += String() + year(eventTime) + " ";

	if (_hour < 10)
		response += "0";
	response += String() + _hour + ":";
	if (_minute < 10)
		response += "0";
	response += String() + _minute + ":";
	if (_second < 10)
		response += "0";
	response += String() + _second;
}

// ---------------------------------------------------------------------------------------------------------
// AJUSTE AS VARIÁVEIS DO WEB SERVER ESP8266
//
// Esta função implementa o servidor Web WiFI (muito simples). O objetivo
// deste servidor é receber comandos admin simples e executá-los
// os resultados são enviados de volta ao cliente web.
// Comandos: DEBUG, ADDRESS, IP, CONFIG, GETTIME, SETTIME
// A página da Web é uma resposta completamente construída e, em seguida, impressa na tela.
// ---------------------------------------------------------------------------------------------------------
static void setVariables(const char *cmd, const char *arg)
{

	// Configurações DEBUG; Estes podem ser usados como um único argumento.
	if (strcmp(cmd, "DEBUG") == 0)
	{ // Definir nível de depuração 0-2.
		if (atoi(arg) == 1)
		{
			debug = (debug + 1) % 4;
		}
		else if (atoi(arg) == -1)
		{
			debug = (debug + 3) % 4;
		}
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
	}

	if (strcmp(cmd, "CAD") == 0)
	{ // Definir -cad on=1 ou off=0.
		_cad = (bool)atoi(arg);
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
	}

	if (strcmp(cmd, "HOP") == 0)
	{ // Definir -hop on=1 ou off=0.
		_hop = (bool)atoi(arg);
		if (!_hop)
		{
			ifreq = 0;
			freq = freqs[0];
			rxLoraModem();
		}
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
	}

	if (strcmp(cmd, "DELAY") == 0)
	{ // Set delay usecs
		txDelay += atoi(arg) * 1000;
	}

	// SF; Lidar com configurações do fator de propagação.
	if (strcmp(cmd, "SF") == 0)
	{
		uint8_t sfi = sf;
		if (atoi(arg) == 1)
		{
			if (sf >= SF12)
				sf = SF7;
			else
				sf = (sf_t)((int)sf + 1);
		}
		else if (atoi(arg) == -1)
		{
			if (sf <= SF7)
				sf = SF12;
			else
				sf = (sf_t)((int)sf - 1);
		}
		rxLoraModem(); // Redefinir o rádio com o novo fator de espalhamento.
		writeGwayCfg(CONFIGFILE); // Salvar a configuração no arquivo.
	}

	// FREQ; Lidar com configurações de frequência.
	if (strcmp(cmd, "FREQ") == 0)
	{
		uint8_t nf = sizeof(freqs) / sizeof(int); // Número de elementos na matriz.

		// Compute índice de frequência.
		if (atoi(arg) == 1)
		{
			if (ifreq == (nf - 1))
				ifreq = 0;
			else
				ifreq++;
		}
		else if (atoi(arg) == -1)
		{
			Serial.println("down");
			if (ifreq == 0)
				ifreq = (nf - 1);
			else
				ifreq--;
		}

		freq = freqs[ifreq];
		rxLoraModem(); // Redefinir o rádio com a nova frequência.
		writeGwayCfg(CONFIGFILE); // Salvar a configuração no arquivo.
	}

	//if (strcmp(cmd, "GETTIME")==0) { Serial.println(F("gettime tbd")); }	// Obtenha a hora local.

	//if (strcmp(cmd, "SETTIME")==0) { Serial.println(F("settime tbd")); }	// Definir a hora local.

	if (strcmp(cmd, "HELP") == 0)
	{
		Serial.println(F("Exibir tópicos da ajuda."));
	}

#if GATEWAYNODE == 1
	if (strcmp(cmd, "NODE") == 0)
	{ // Definir node on=1 ou off=0.
		gwayConfig.isNode = (bool)atoi(arg);
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
	}

	if (strcmp(cmd, "FCNT") == 0)
	{
		frameCount = 0;
		rxLoraModem(); // Redefinir o rádio com a nova frequência.
		writeGwayCfg(CONFIGFILE);
	}
#endif

#if WIFIMANAGER == 1
	if (strcmp(cmd, "NEWSSID") == 0)
	{
		WiFiManager wifiManager;
		strcpy(wpa[0].login, "");
		strcpy(wpa[0].passw, "");
		WiFi.disconnect();
		wifiManager.autoConnect(AP_NAME, AP_PASSWD);
	}
#endif

#if A_OTA == 1
	if (strcmp(cmd, "UPDATE") == 0)
	{
		if (atoi(arg) == 1)
		{
			updateOtaa();
		}
	}
#endif

#if A_REFRESH == 1
	if (strcmp(cmd, "REFR") == 0)
	{ // Definir refresh on=1 ou off=0.
		gwayConfig.refresh = (bool)atoi(arg);
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
	}
#endif
}

// ---------------------------------------------------------------------------------------------------------
// ABRIR PÁGINA DA WEB
// Esta é a função init para abrir a página da web.
// ---------------------------------------------------------------------------------------------------------
static void openWebPage()
{
	++gwayConfig.views; // incremento do número de visualizações.
#if A_REFRESH == 1
	//server.client().stop(); // Experimental, pare o servidor no caso de algo ainda estar em execução!
#endif
	String response = "";

	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");

	// init webserver, preencha a página da web.
  // NOTA: A página é renovada a cada _WWW_INTERVAL segundos, por favor ajuste em ESP32-GatewayLoRaWAN.h.
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/html", "");
#if A_REFRESH == 1
	if (gwayConfig.refresh)
	{
		response += String() + "<!DOCTYPE HTML><HTML><HEAD><meta http-equiv='refresh' content='" + _WWW_INTERVAL + ";http://";
		printIP((IPAddress)WiFi.localIP(), '.', response);
#ifdef ESP32BUILD == 1
		response += "'><TITLE>ESP32 LoRaWANGateway</TITLE>";
#else
		response += "'><TITLE>ESP8266 LoRaWANGateway</TITLE>";
#endif
	}
	else
	{
		response += String() + "<!DOCTYPE HTML><HTML><HEAD><TITLE>ESP32 1ch LoRaGateway</TITLE>";
	}
#else
	response += String() + "<!DOCTYPE HTML><HTML><HEAD><TITLE>ESP8266 1ch LoRaGateway</TITLE>";
#endif
	response += "<META HTTP-EQUIV='CONTENT-TYPE' CONTENT='text/html; charset=UTF-8'>";
	response += "<META NAME='AUTHOR' CONTENT='AdailSilva (adail101@hotmail.com)'>";

	response += "<style>.thead {background-color:green; color:white;}";
	response += ".cell {border: 1px solid black;}";
	response += ".config_table {max_width:100%; min-width:400px; width:95%; border:1px solid black; border-collapse:collapse;}";
	response += "</style></HEAD><BODY>";

	response += "<h1>ESP32 LoRaWAN Gateway - AdailSilva</h1>";
  response += "Código-Fonte Traduzido em: <a href=\"https://github.com/AdailSilva\" target=\"_blank\">https://github.com/AdailSilva</a> :-)<br>";

	response += "Versão: ";
	response += VERSION;

	response += "<br>Data e Hora atual: "; // Hora atual.
	stringTime(millis(), response);
	response += ";";

	response += "<br>Ativo desde: "; // Começou em.
	stringTime(1, response);
	response += ";";

	response += "<br>Tempo de atividade: "; // Tempo de atividade.
	uint32_t secs = millis() / 1000;
	uint16_t days = secs / 86400; // Determinar o número de dias.
	uint8_t _hour = hour(secs);
	uint8_t _minute = minute(secs);
	uint8_t _second = second(secs);
	response += String() + days + "-";
	if (_hour < 10)
		response += "0";
	response += String() + _hour + ":";
	if (_minute < 10)
		response += "0";
	response += String() + _minute + ":";
	if (_second < 10)
		response += "0";
	response += String() + _second;
	response += ".<br>";

	server.sendContent(response);
}

// ---------------------------------------------------------------------------------------------------------
// CONFIGURAÇÃO DE DADOS.
// ---------------------------------------------------------------------------------------------------------
static void configData()
{
	String response = "";
	String bg = "";

	response += "<h2>Configurações do LoRaGateway</h2>";

	response += "<table class=\"config_table\">";
	response += "<tr>";
	response += "<th class=\"thead\">Configuração</th>";
	response += "<th style=\"background-color: green; color: white; width:120px;\">Valor</th>";
	response += "<th colspan=\"2\" style=\"background-color: green; color: white; width:100px;\">Fixar</th>";
	response += "</tr>";

	bg = " background-color: ";
	bg += (_cad ? "LightGreen" : "orange");
	response += "<tr><td class=\"cell\">CAD</td>";
	response += "<td style=\"border: 1px solid black;";
	response += bg;
	response += "\">";
	response += (_cad ? "ON" : "OFF");
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"CAD=1\"><button>ON</button></a></td>";
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"CAD=0\"><button>OFF</button></a></td>";
	response += "</tr>";

	bg = " background-color: ";
	bg += (_hop ? "LightGreen" : "orange");
	response += "<tr><td class=\"cell\">HOP</td>";
	response += "<td style=\"border: 1px solid black;";
	response += bg;
	response += "\">";
	response += (_hop ? "ON" : "OFF");
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"HOP=1\"><button>ON</button></a></td>";
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"HOP=0\"><button>OFF</button></a></td>";
	response += "</tr>";

	response += "<tr><td class=\"cell\">Configuração de SF</td><td class=\"cell\">";
	if (_cad)
	{
		response += "AUTO</td>";
	}
	else
	{
		response += sf;
		response += "<td class=\"cell\"><a href=\"SF=-1\"><button>-</button></a></td>";
		response += "<td class=\"cell\"><a href=\"SF=1\"><button>+</button></a></td>";
	}
	response += "</tr>";

	// Canal.
	response += "<tr><td class=\"cell\">Canal</td>";
	response += "<td class=\"cell\">";
	if (_hop)
	{
		response += "AUTO</td>";
	}
	else
	{
		response += String() + ifreq;
		response += "</td>";
		response += "<td class=\"cell\"><a href=\"FREQ=-1\"><button>-</button></a></td>";
		response += "<td class=\"cell\"><a href=\"FREQ=1\"><button>+</button></a></td>";
	}
	response += "</tr>";

	// Opções de depuração.
	response += "<tr><td class=\"cell\">";
	response += "Nível de depuração</td><td class=\"cell\">";
	response += debug;
	response += "</td>";
	response += "<td class=\"cell\"><a href=\"DEBUG=-1\"><button>-</button></a></td>";
	response += "<td class=\"cell\"><a href=\"DEBUG=1\"><button>+</button></a></td>";
	response += "</tr>";

	// Depuração serial.
	response += "<tr><td class=\"cell\">";
	response += "Depurar USB</td><td class=\"cell\">";
	response += DUSB;
	response += "</td>";
	//response +="<td class=\"cell\"> </td>";
	//response +="<td class=\"cell\"> </td>";
	response += "</tr>";

#if GATEWAYNODE == 1
	response += "<tr><td class=\"cell\">Sensor interno do contador de quadros</td>";
	response += "<td class=\"cell\">";
	response += frameCount;
	response += "</td><td colspan=\"2\" style=\"border: 1px solid black;\">";
	response += "<button><a href=\"/FCNT\">Reiniciar</a></button></td>";
	response += "</tr>";

	bg = " background-color: ";
	bg += ((gwayConfig.isNode == 1) ? "LightGreen" : "orange");
	response += "<tr><td class=\"cell\">Gateway Node</td>";
	response += "<td class=\"cell\" style=\"border: 1px solid black;" + bg + "\">";
	response += ((gwayConfig.isNode == true) ? "ON" : "OFF");
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"NODE=1\"><button>ON</button></a></td>";
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"NODE=0\"><button>OFF</button></a></td>";
	response += "</tr>";
#endif

#if A_REFRESH == 1
	bg = " background-color: ";
	bg += ((gwayConfig.refresh == 1) ? "LightGreen" : "orange");
	response += "<tr><td class=\"cell\">Atualização da GUI</td>";
	response += "<td class=\"cell\" style=\"border: 1px solid black;" + bg + "\">";
	response += ((gwayConfig.refresh == 1) ? "ON" : "OFF");
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"REFR=1\"><button>ON</button></a></td>";
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"REFR=0\"><button>OFF</button></a></td>";
	response += "</tr>";
#endif

#if WIFIMANAGER == 1
	response += "<tr><td>";
	response += "Clique <a href=\"/NEWSSID\">aqui </a> para redefinir o ponto de acesso<br>";
	response += "</td><td></td></tr>";
#endif

	// Repor todas as estatísticas.
#if STATISTICS >= 1
	response += "<tr><td class=\"cell\">Estatisticas</td>";
	response += String() + "<td class=\"cell\">" + statc.resets + "</td>";
	response += "<td colspan=\"2\" class=\"cell\"><a href=\"/RESET\"><button>Reiniciar</button></a></td></tr>";

	response += "<tr><td class=\"cell\">Boots e Redefinições</td>";
	response += String() + "<td class=\"cell\">" + gwayConfig.boots + "</td>";
	response += "<td colspan=\"2\" class=\"cell\"><a href=\"/BOOT\"><button>Reiniciar</button></a></td></tr>";
#endif
	response += "</table>";

	// Atualiza todas as estatísticas do firmware.
	response += "<tr><td class=\"cell\">Atualizar Firmware </td>";
	response += "<td class=\"cell\"></td><td colspan=\"2\" class=\"cell\"><a href=\"/UPDATE=1\"><button>Reiniciar</button></a></td></tr>";
	response += "</table>";

	server.sendContent(response);
}

// ---------------------------------------------------------------------------------------------------------
// INTERRUPÇÃO DE DADOS.
// Exibir dados de interrupção, mas apenas para depuração >= 2.
// ---------------------------------------------------------------------------------------------------------
static void interruptData()
{
	uint8_t flags = readRegister(REG_IRQ_FLAGS);
	uint8_t mask = readRegister(REG_IRQ_FLAGS_MASK);

	if (debug >= 2)
	{
		String response = "";

		response += "<h2>Estado do sistema e interrupção</h2>";

		response += "<table class=\"config_table\">";
		response += "<tr>";
		response += "<th class=\"thead\">Parâmetro</th>";
		response += "<th class=\"thead\">Valor</th>";
		response += "<th colspan=\"2\"  class=\"thead\">Setar</th>";
		response += "</tr>";

		response += "<tr><td class=\"cell\">Estado</td>";
		response += "<td class=\"cell\">";
		switch (_state)
		{ // Veja loraModem.h
		case S_INIT:
			response += "Inicial";
			break;
		case S_SCAN:
			response += "Examinar";
			break;
		case S_CAD:
			response += "CAD";
			break;
		case S_RX:
			response += "RX (Recepção)";
			break;
		case S_TX:
			response += "TX (Transmissão)";
			break;
		default:
			response += "desconhecido.";
			break;
		}
		response += "</td></tr>";

		response += "<tr><td class=\"cell\">Bandeiras (8 bits)</td>";
		response += "<td class=\"cell\">0x";
		if (flags < 16)
			response += "0";
		response += String(flags, HEX);
		response += "</td></tr>";

		response += "<tr><td class=\"cell\">Máscara (8 bits)</td>";
		response += "<td class=\"cell\">0x";
		if (mask < 16)
			response += "0";
		response += String(mask, HEX);
		response += "</td></tr>";

		response += "<tr><td class=\"cell\">Contador Re-Entrante</td>";
		response += "<td class=\"cell\">";
		response += String() + gwayConfig.reents;
		response += "</td></tr>";

		response += "<tr><td class=\"cell\">Contador de Chamada NTP</td>";
		response += "<td class=\"cell\">";
		response += String() + gwayConfig.ntps;
		response += "</td></tr>";

		response += "<tr><td class=\"cell\">Contador de Erros NTP</td>";
		response += "<td class=\"cell\">";
		response += String() + gwayConfig.ntpErr;
		response += "</td>";
		response += "<td colspan=\"2\" style=\"border: 1px solid black;\">";
		stringTime(gwayConfig.ntpErrTime, response);
		response += "</td>";
		response += "</tr>";

		response += "<tr><td class=\"cell\">Correção de Tempo (uSec)</td><td class=\"cell\">";
		response += txDelay;
		response += "</td>";
		response += "<td class=\"cell\"><a href=\"DELAY=-1\"><button>-</button></a></td>";
		response += "<td class=\"cell\"><a href=\"DELAY=1\"><button>+</button></a></td>";
		response += "</tr>";

		response += "</table>";

		server.sendContent(response);
	} // if debug>=2
}

// ---------------------------------------------------------------------------------------------------------
// DADOS DE ESTATÍSTICA.
// ---------------------------------------------------------------------------------------------------------
static void statisticsData()
{
	String response = "";

	response += "<h2>Estatísticas do pacote</h2>";

	response += "<table class=\"config_table\">";
	response += "<tr>";
	response += "<th class=\"thead\">Contador</th>";
	response += "<th class=\"thead\">Pacotes</th>";
	response += "<th class=\"thead\">Pacotes/Hora</th>";
	response += "</tr>";

	response += "<tr><td class=\"cell\">Total de Pacotes de Uplink</td>";
	response += "<td class=\"cell\">" + String(cp_nb_rx_rcv) + "</td>";
	response += "<td class=\"cell\">" + String((cp_nb_rx_rcv * 3600) / (millis() / 1000)) + "</td></tr>";
	response += "<tr><td class=\"cell\">Pacotes Uplink Completados</td><td class=\"cell\">";
	response += cp_nb_rx_ok;
	response += "</tr>";
	response += "<tr><td class=\"cell\">Pacotes de Downlink</td><td class=\"cell\">";
	response += cp_up_pkt_fwd;
	response += "</tr>";

	// Forneça uma tabela com todos os dados do SF, incluindo a porcentagem de mensagens.
#if STATISTICS >= 2
	response += "<tr><td class=\"cell\">SF7 rcvd</td>";
	response += "<td class=\"cell\">";
	response += statc.sf7;
	response += "<td class=\"cell\">";
	response += String(cp_nb_rx_rcv > 0 ? 100 * statc.sf7 / cp_nb_rx_rcv : 0) + " %";
	response += "</td></tr>";
	response += "<tr><td class=\"cell\">SF8 rcvd</td>";
	response += "<td class=\"cell\">";
	response += statc.sf8;
	response += "<td class=\"cell\">";
	response += String(cp_nb_rx_rcv > 0 ? 100 * statc.sf8 / cp_nb_rx_rcv : 0) + " %";
	response += "</td></tr>";
	response += "<tr><td class=\"cell\">SF9 rcvd</td>";
	response += "<td class=\"cell\">";
	response += statc.sf9;
	response += "<td class=\"cell\">";
	response += String(cp_nb_rx_rcv > 0 ? 100 * statc.sf9 / cp_nb_rx_rcv : 0) + " %";
	response += "</td></tr>";
	response += "<tr><td class=\"cell\">SF10 rcvd</td>";
	response += "<td class=\"cell\">";
	response += statc.sf10;
	response += "<td class=\"cell\">";
	response += String(cp_nb_rx_rcv > 0 ? 100 * statc.sf10 / cp_nb_rx_rcv : 0) + " %";
	response += "</td></tr>";
	response += "<tr><td class=\"cell\">SF11 rcvd</td>";
	response += "<td class=\"cell\">";
	response += statc.sf11;
	response += "<td class=\"cell\">";
	response += String(cp_nb_rx_rcv > 0 ? 100 * statc.sf11 / cp_nb_rx_rcv : 0) + " %";
	response += "</td></tr>";
	response += "<tr><td class=\"cell\">SF12 rcvd</td>";
	response += "<td class=\"cell\">";
	response += statc.sf12;
	response += "<td class=\"cell\">";
	response += String(cp_nb_rx_rcv > 0 ? 100 * statc.sf12 / cp_nb_rx_rcv : 0) + " %";
	response += "</td></tr>";
#endif

	response += "</table>";

	server.sendContent(response);
}

// ---------------------------------------------------------------------------------------------------------
// DADOS DO SENSOR.
// Se ativado, exiba o sensorHistory na página do servidor da web atual.
// Parâmetros:
// - <nenhum>
// Retorna:
// - <nenhum>
// ---------------------------------------------------------------------------------------------------------
static void sensorData()
{
#if STATISTICS >= 1
	String response = "";

	response += "<h2>Histórico de Mensagens</h2>";
	response += "<table class=\"config_table\">";
	response += "<tr>";
	response += "<th class=\"thead\">Tempo</th>";
	response += "<th class=\"thead\">Nó</th>";
	response += "<th class=\"thead\" colspan=\"2\">Canal</th>";
	response += "<th class=\"thead\" style=\"width: 50px;\">SF</th>";
	response += "<th class=\"thead\" style=\"width: 50px;\">pRSSI</th>";
#if RSSI == 1
	if (debug > 1)
	{
		response += "<th class=\"thead\" style=\"width: 50px;\">RSSI</th>";
	}
#endif
	response += "</tr>";
	server.sendContent(response);

	for (int i = 0; i < MAX_STAT; i++)
	{
		if (statr[i].sf == 0)
			break;

		response = "";

		response += String() + "<tr><td class=\"cell\">";
		stringTime(statr[i].tmst, response);
		response += "</td>";
		response += String() + "<td class=\"cell\">";
		printHEX((char *)(&(statr[i].node)), ' ', response);
		response += "</td>";
		response += String() + "<td class=\"cell\">" + statr[i].ch + "</td>";
		response += String() + "<td class=\"cell\">" + freqs[statr[i].ch] + "</td>";
		response += String() + "<td class=\"cell\">" + statr[i].sf + "</td>";

		response += String() + "<td class=\"cell\">" + statr[i].prssi + "</td>";
#if RSSI == 1
		if (debug > 1)
		{
			response += String() + "<td class=\"cell\">" + statr[i].rssi + "</td>";
		}
#endif
		response += "</tr>";
		server.sendContent(response);
	}

	server.sendContent("</table>");

#endif
}

// ---------------------------------------------------------------------------------------------------------
// DADOS DO SISTEMA.
// ---------------------------------------------------------------------------------------------------------
static void systemData()
{
	String response = "";
	response += "<h2>Estado do Sistema</h2>";

	response += "<table class=\"config_table\">";
	response += "<tr>";
	response += "<th class=\"thead\">Parâmetro</th>";
	response += "<th class=\"thead\">Valor</th>";
	response += "<th colspan=\"2\" class=\"thead\">Setar</th>";
	response += "</tr>";

	response += "<tr><td style=\"border: 1px solid black; width:120px;\">Identificador do Gateway</td>";
	response += "<td class=\"cell\">";
	if (MAC_array[0] < 0x10)
		response += '0';
	response += String(MAC_array[0], HEX); // O array MAC é sempre retornado em letras minúsculas.
	if (MAC_array[1] < 0x10)
		response += '0';
	response += String(MAC_array[1], HEX);
	if (MAC_array[2] < 0x10)
		response += '0';
	response += String(MAC_array[2], HEX);
	response += "FFFF";
	if (MAC_array[3] < 0x10)
		response += '0';
	response += String(MAC_array[3], HEX);
	if (MAC_array[4] < 0x10)
		response += '0';
	response += String(MAC_array[4], HEX);
	if (MAC_array[5] < 0x10)
		response += '0';
	response += String(MAC_array[5], HEX);
	response += "</tr>";

	response += "<tr><td class=\"cell\">Pilha Livre</td><td class=\"cell\">";
	response += ESP.getFreeHeap();
	response += "</tr>";
	response += "<tr><td class=\"cell\">Frequência da CPU</td><td class=\"cell\">";
	response += ESP.getCpuFreqMHz();
	response += "MHz";
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"SPEED=80\"><button>80</button></a></td>";
	response += "<td style=\"border: 1px solid black; width:40px;\"><a href=\"SPEED=160\"><button>160</button></a></td>";
	response += "</tr>";
#ifdef ESP32BUILD
	{
		char serial[13];
		uint64_t chipid = ESP.getEfuseMac();
		sprintf(serial, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
		response += "<tr><td class=\"cell\">Identificador do Chip ESP</td><td class=\"cell\">";
		response += serial;
		response += "</tr>";
	}
#else
	response += "<tr><td class=\"cell\">Identificador do Chip ESP</td><td class=\"cell\">";
	response += ESP.getChipId();
	response += "</tr>";
#endif
	response += "<tr><td class=\"cell\">Tipo do OLED</td><td class=\"cell\">";
	response += OLED;
	response += "</tr>";

#if STATISTICS >= 1
	response += "<tr><td class=\"cell\">Configurações do WiFi</td><td class=\"cell\">";
	response += gwayConfig.wifis;
	response += "</tr>";
	response += "<tr><td class=\"cell\">Atualização da Página</td><td class=\"cell\">";
	response += gwayConfig.views;
	response += "</tr>";
#endif

	response += "</table>";
	server.sendContent(response);
}

// ---------------------------------------------------------------------------------------------------------
// DADOS WIFI.
// Exibe os parâmetros mais importantes de Wifi reunidos.
// ---------------------------------------------------------------------------------------------------------
static void wifiData()
{
	String response = "";
	response += "<h2>Configurações do WiFi</h2>";

	response += "<table class=\"config_table\">";

	response += "<tr><th class=\"thead\">Parâmetro</th><th class=\"thead\">Valor</th></tr>";

	response += "<tr><td class=\"cell\">Host WiFi</td><td class=\"cell\">";
#ifdef ESP32BUILD
	response += WiFi.getHostname();
	response += "</tr>";
#else
	response += wifi_station_get_hostname();
	response += "</tr>";
#endif

	response += "<tr><td class=\"cell\">WiFi SSID</td><td class=\"cell\">";
	response += WiFi.SSID();
	response += "</tr>";

	response += "<tr><td class=\"cell\">IP Local</td><td class=\"cell\">";
	printIP((IPAddress)WiFi.localIP(), '.', response);
	response += "</tr>";
	response += "<tr><td class=\"cell\">IP Gateway</td><td class=\"cell\">";
	printIP((IPAddress)WiFi.gatewayIP(), '.', response);
	response += "</tr>";
	response += "<tr><td class=\"cell\">Servidor NTP</td><td class=\"cell\">";
	response += NTP_TIMESERVER;
	response += "</tr>";
	response += "<tr><td class=\"cell\">Roteador LoRa</td><td class=\"cell\">";
	response += _TTNSERVER;
	response += "</tr>";
	response += "<tr><td class=\"cell\">IP Roteador LoRa</td><td class=\"cell\">";
	printIP((IPAddress)ttnServer, '.', response);
	response += "</tr>";
#ifdef _THINGSERVER
	response += "<tr><td class=\"cell\">Roteador LoRa</td><td class=\"cell\">";
	response += _THINGSERVER;
	response += String() + ":" + _THINGPORT + "</tr>";
	response += "<tr><td class=\"cell\">IP Roteador LoRa</td><td class=\"cell\">";
	printIP((IPAddress)thingServer, '.', response);
	response += "</tr>";
#endif
	response += "</table>";

	server.sendContent(response);
}

// ---------------------------------------------------------------------------------------------------------
// ENVIAR PÁGINA WEB()
// Ligue para o servidor web e envie o conteúdo padrão e o conteúdo que é passou pelo parâmetro.
//
// NOTA: Este é o único local onde as chamadas yield() ou delay() são usadas.
// ---------------------------------------------------------------------------------------------------------
void sendWebPage(const char *cmd, const char *arg)
{
	openWebPage();
	yield();

	setVariables(cmd, arg);
	yield();

	statisticsData();
	yield(); // Estatísticas de nós.
	sensorData();
	yield(); // Exibe o histórico do sensor, as estatísticas da mensagem.
	systemData();
	yield(); // Estatísticas do sistema, como heap etc.
	wifiData();
	yield(); // Parâmetros específicos de WiFI.

	configData();
	yield(); // Exibir configuração da web.

	interruptData();
	yield(); // Exibir interrompe somente quando depurar >= 2.

	// Feche a conexão do cliente para o servidor.
	server.sendContent(String() + "<br><br />Clique <a href=\"/HELP\">aqui</a> para explicar as opções de Ajuda sobre REST<br>");
	server.sendContent(String() + "</BODY></HTML>");
	server.sendContent("");
	yield();

	server.client().stop();
}

// ---------------------------------------------------------------------------------------------------------
// Usado por funções de tempo limite.
// Esta função exibe apenas a página inicial padrão.
// Nota: Esta função não é utilizada ativamente, pois a página é renovada
// usando uma configuração meta de HTML de 60 segundos.
// ---------------------------------------------------------------------------------------------------------
static void renewWebPage()
{
	//Serial.println(F("Renovar Página da Web."));
	//sendWebPage("","");
	//return;
}

// ---------------------------------------------------------------------------------------------------------
// Função SetupWWW chamada pelo programa principal setup() para configurar o servidor web
// Na verdade, não é muito mais que instalar os manipuladores de retorno de chamada
// para mensagens enviadas para o servidor da web.
//
// Implementado é uma interface como:
// http: // <server> / <Variable> = <value>.
// ---------------------------------------------------------------------------------------------------------
void setupWWW()
{
	server.begin(); // Inicie o servidor da web.

	// ---------------------------------------------------------------------------------------------------------
	// BOTÕES, defina o que deve acontecer com os botões que pressionamos na página inicial.

	server.on("/", []() {
		sendWebPage("", ""); // Envie a string da web.
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	server.on("/HELP", []() {
		sendWebPage("HELP", ""); // Envie a string da WebPage.
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// Redefinir as estatísticas.
	server.on("/RESET", []() {
		Serial.println(F("RESET"));
		cp_nb_rx_rcv = 0;
		cp_nb_rx_ok = 0;
		cp_up_pkt_fwd = 0;
#if STATISTICS >= 1
		for (int i = 0; i < MAX_STAT; i++)
		{
			statr[i].sf = 0;
		}
#if STATISTICS >= 2
		statc.sf7 = 0;
		statc.sf8 = 0;
		statc.sf9 = 0;
		statc.sf10 = 0;
		statc.sf11 = 0;
		statc.sf12 = 0;

		statc.resets = 0;
		writeGwayCfg(CONFIGFILE);
#endif
#endif
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// Redefinir o contador de inicialização.
	server.on("/BOOT", []() {
		Serial.println(F("BOOT"));
#if STATISTICS >= 2
		gwayConfig.boots = 0;
		gwayConfig.wifis = 0;
		gwayConfig.views = 0;
		gwayConfig.ntpErr = 0; // Erros NTP.
		gwayConfig.ntpErrTime = 0; // Hora do último erro do NTP.
		gwayConfig.ntps = 0; // Número de chamadas NTP.
#endif
		gwayConfig.reents = 0; // Reentrado.

		writeGwayCfg(CONFIGFILE);
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	server.on("/NEWSSID", []() {
		sendWebPage("NEWSSID", ""); // Envie a string da web.
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// Definir o parâmetro de depuração.
	server.on("/DEBUG=-1", []() { // Definir debug level 0-2.
		debug = (debug + 3) % 4;
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/DEBUG=1", []() {
		debug = (debug + 1) % 4;
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// Definir atraso em microssegundos.
	server.on("/DELAY=1", []() {
		txDelay += 1000;
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/DELAY=-1", []() {
		txDelay -= 1000;
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// Configuração do fator de espalhamento.
	server.on("/SF=1", []() {
		if (sf >= SF12)
			sf = SF7;
		else
			sf = (sf_t)((int)sf + 1);
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/SF=-1", []() {
		if (sf <= SF7)
			sf = SF12;
		else
			sf = (sf_t)((int)sf - 1);
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// Frequência do nó GateWay.
	server.on("/FREQ=1", []() {
		uint8_t nf = sizeof(freqs) / sizeof(int); // Número de elementos no array.
		if (ifreq == (nf - 1))
			ifreq = 0;
		else
			ifreq++;
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/FREQ=-1", []() {
		uint8_t nf = sizeof(freqs) / sizeof(int); // Número de elementos no array.
		if (ifreq == 0)
			ifreq = (nf - 1);
		else
			ifreq--;
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// Ativar/desativar a função CAD.
	server.on("/CAD=1", []() {
		_cad = (bool)1;
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/CAD=0", []() {
		_cad = (bool)0;
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// GatewayNode
	server.on("/NODE=1", []() {
#if GATEWAYNODE == 1
		gwayConfig.isNode = (bool)1;
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
#endif
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/NODE=0", []() {
#if GATEWAYNODE == 1
		gwayConfig.isNode = (bool)0;
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
#endif
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

#if GATEWAYNODE == 1
	// Contador de quadros do nó Gateway.
	server.on("/FCNT", []() {
		frameCount = 0;
		rxLoraModem(); // Repor o radio com a nova frequência.
		writeGwayCfg(CONFIGFILE);

		//sendWebPage("",""); // Envie a string da WebPage.
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
#endif
	// Função de atualização da página WWW.
	server.on("/REFR=1", []() { // Atualização automática da página WWW ON.
#if A_REFRESH == 1
		gwayConfig.refresh = 1;
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
#endif
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/REFR=0", []() { // Atualização automática da página WWW OFF.
#if A_REFRESH == 1
		gwayConfig.refresh = 0;
		writeGwayCfg(CONFIGFILE); // Salvar configuração no arquivo.
#endif
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

	// Desligue/ligue as funções do HOP.
	server.on("/HOP=1", []() {
		_hop = true;
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/HOP=0", []() {
		_hop = false;
		ifreq = 0;
		freq = freqs[0];
		rxLoraModem();
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

#ifndef ESP32BUILD
	server.on("/SPEED=80", []() {
		system_update_cpu_freq(80);
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
	server.on("/SPEED=160", []() {
		system_update_cpu_freq(160);
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});
#endif

	// Atualize o esboço. Ainda não implementado.
	server.on("/UPDATE=1", []() {
#if A_OTA == 1
		updateOtaa();
#endif
		server.sendHeader("Location", String("/"), true);
		server.send(302, "text/plain", "");
	});

// ---------------------------------------------------------------------------------------------------------
// Esta seção da versão 4.0.7 define qual PARTE a página da web
// é mostrada com base nos botões pressionados pelo usuário.
// Talvez nem todas as informações devam ser colocadas na tela,
// já que pode levar muito tempo para servir todas as informações
// antes de um próximo a interrupção do pacote chega ao gateway.
// ---------------------------------------------------------------------------------------------------------
	Serial.print(F("Servidor WWW iniciado na porta: "));
	Serial.print(A_SERVERPORT);
	Serial.println(F("."));
	return;
}

#endif
