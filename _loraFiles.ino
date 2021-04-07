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
// Este arquivo contém o código específico do sistema de arquivos LoRa.
// =========================================================================================================
//
// =========================================================================================================
// FUNÇÕES LoRa - SISTEMA DE ARQUIVO SPIFFS
//
// As funções de suporte do LoRa estão na seção abaixo.
//
// ---------------------------------------------------------------------------------------------------------
// Listagem de diretório. "s" é uma string contendo código HTML / texto até o momento.
// A listagem de diretórios resultante é anexada a "s" e retornada.
// ---------------------------------------------------------------------------------------------------------
//String espDir(String s) {
//
//	return s;
//}
//
// ---------------------------------------------------------------------------------------------------------
// Leia o arquivo de configuração do gateway.
// ---------------------------------------------------------------------------------------------------------
// =========================================================================================================

int readConfig(const char *fn, struct espGwayConfig *c)
{

	Serial.println("");
	Serial.println(F("Leitura da Configuração:: Iniciando..."));
	Serial.println("");

	if (!SPIFFS.exists(fn))
	{
		Serial.print(F("ERRO:: Leitura da Configuração, arquivo não existe: "));
		Serial.println(fn);
		return (-1);
	}
	File f = SPIFFS.open(fn, "r");
	if (!f)
	{
		Serial.println(F("ERRO:: falhou na abertura da SPIFFS."));
		return (-1);
	}

	while (f.available())
	{

		String id = f.readStringUntil('=');
		String val = f.readStringUntil('\n');

		if (id == "SSID")
		{ // SSID WiFi.
			Serial.print(F("SSID ---------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).ssid = val; // "val" contém "ssid", nós não fazemos check.
		}
		// Comentar:
		if (id == "PASS")
		{ // Senha WiFi.
			Serial.print(F("SENHA --------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).pass = val;
		}
		if (id == "CH")
		{ // Canal de Frequência.
			Serial.print(F("Canal de Frequência (Frequency Channel - CH) -----------------------------> "));
			Serial.println(val);
			(*c).ch = (uint32_t)val.toInt();
		}
		if (id == "SF")
		{ // Fator de espalhamento.
			Serial.print(F("Fator de espalhamento (Spreading Factor - SF) ----------------------------> "));
			Serial.println(val);
			(*c).sf = (uint32_t)val.toInt();
		}
		if (id == "FCNT")
		{ // Contador de quadros.
			Serial.print(F("Contador de quadros (Frame Counter - FCNT) -------------------------------> "));
			Serial.println(val);
			(*c).fcnt = (uint32_t)val.toInt();
		}
		if (id == "DEBUG")
		{ // Contador de quadros.
			Serial.print(F("Nível de Depuração (DEBUG) -----------------------------------------------> "));
			Serial.println(val);
			(*c).debug = (uint8_t)val.toInt();
		}
		if (id == "CAD")
		{ // Configuração CAD.
			Serial.print(F("Detecção de atividade de canal (Channel Activity Detection - CAD) --------> "));
			Serial.println(val);
			(*c).cad = (uint8_t)val.toInt();
		}
		if (id == "HOP")
		{ // Configuração HOP.
			Serial.print(F("Saltos em frequência (Frequency Hopping - HOP) ---------------------------> "));
			Serial.println(val);
			(*c).hop = (uint8_t)val.toInt();
		}
		if (id == "BOOTS")
		{ // Configuração BOOTS.
			Serial.print(F("BOOTS --------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).boots = (uint8_t)val.toInt();
		}
		if (id == "RESETS")
		{ // Configuração RESET.
			Serial.print(F("RESETS -------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).resets = (uint8_t)val.toInt();
		}
		if (id == "WIFIS")
		{ // Configuração WIFIS.
			Serial.print(F("WIFIS --------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).wifis = (uint8_t)val.toInt();
		}
		if (id == "VIEWS")
		{ // Configuração VIEWS.
			Serial.print(F("VISUALIZAÇÕES ------------------------------------------------------------> "));
			Serial.println(val);
			(*c).views = (uint8_t)val.toInt();
		}
		if (id == "NODE")
		{ // Configuração NODE.
			Serial.print(F("NÓ (NODE) ----------------------------------------------------------------> "));
			Serial.println(val);
			(*c).isNode = (uint8_t)val.toInt();
		}
		if (id == "REFR")
		{ // Configuração REFR (REFRESH).
			Serial.print(F("ATUALIZAÇÕES DA PÁGINA ---------------------------------------------------> "));
			Serial.println(val);
			(*c).refresh = (uint8_t)val.toInt();
		}
		if (id == "REENTS")
		{ // Configuração REENTS.
			Serial.print(F("REENTS -------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).reents = (uint8_t)val.toInt();
		}
		if (id == "NTPERR")
		{ // Configuração NTPERR.
			Serial.print(F("NTPERR -------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).ntpErr = (uint8_t)val.toInt();
		}
		if (id == "NTPETIM")
		{ // Configuração NTPERR.
			Serial.print(F("NTPETIM ------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).ntpErrTime = (uint32_t)val.toInt();
		}
		if (id == "NTPS")
		{ // Configuração NTPS.
			Serial.print(F("NTPS ---------------------------------------------------------------------> "));
			Serial.println(val);
			(*c).ntps = (uint8_t)val.toInt();
		}
	}

	Serial.println("");
	f.close();
	return (1);
}

// ----------------------------------------------------------------------------
// Grave a configuração atual do gateway para SPIFFS.
// Primeira cópia de todos os separa itens de dados para a estrutura gwayConfig.
// ----------------------------------------------------------------------------
int writeGwayCfg(const char *fn)
{

	gwayConfig.sf = (uint8_t)sf;
	gwayConfig.ssid = WiFi.SSID();
	//gwayConfig.pass = WiFi.PASS();  // XXX Devemos encontrar uma maneira de armazenar a senha também.
	gwayConfig.ch = ifreq;
	gwayConfig.debug = debug;
	gwayConfig.cad = _cad;
	gwayConfig.hop = _hop;
#if GATEWAYNODE == 1
	gwayConfig.fcnt = frameCount;
#endif

	return (writeConfig(fn, &gwayConfig));
}

// ------------------------------------------------------------------------------------
// Escreva o anúncio de configuração encontrado na estrutura espGwayConfig para SPIFFS.
// ------------------------------------------------------------------------------------
int writeConfig(const char *fn, struct espGwayConfig *c)
{

	if (!SPIFFS.exists(fn))
	{
		Serial.print("AVISO:: writeConfig, arquivo não existe, formatando ");
		SPIFFS.format();
		// XXX faça todas as declarações iniciais aqui se o config vars precisar ter um valor.
		Serial.println(fn);
	}
	File f = SPIFFS.open(fn, "w");
	if (!f)
	{
		Serial.print("ERRO:: Escrita da Configuração, abrir arquivo = ");
		Serial.print(fn);
		Serial.println();
		return (-1);
	}

	f.print("SSID");
	f.print('=');
	f.print((*c).ssid);
	f.print('\n');

	f.print("PASS");
	f.print('=');
	f.print((*c).pass);
	f.print('\n');

	f.print("CH");
	f.print('=');
	f.print((*c).ch);
	f.print('\n');

	f.print("SF");
	f.print('=');
	f.print((*c).sf);
	f.print('\n');

	f.print("FCNT");
	f.print('=');
	f.print((*c).fcnt);
	f.print('\n');

	f.print("DEBUG");
	f.print('=');
	f.print((*c).debug);
	f.print('\n');

	f.print("CAD");
	f.print('=');
	f.print((*c).cad);
	f.print('\n');

	f.print("HOP");
	f.print('=');
	f.print((*c).hop);
	f.print('\n');

	f.print("NODE");
	f.print('=');
	f.print((*c).isNode);
	f.print('\n');

	f.print("BOOTS");
	f.print('=');
	f.print((*c).boots);
	f.print('\n');

	f.print("RESETS");
	f.print('=');
	f.print((*c).resets);
	f.print('\n');

	f.print("WIFIS");
	f.print('=');
	f.print((*c).wifis);
	f.print('\n');

	f.print("VIEWS");
	f.print('=');
	f.print((*c).views);
	f.print('\n');

	f.print("REFR");
	f.print('=');
	f.print((*c).refresh);
	f.print('\n');

	f.print("REENTS");
	f.print('=');
	f.print((*c).reents);
	f.print('\n');

	f.print("NTPETIM");
	f.print('=');
	f.print((*c).ntpErrTime);
	f.print('\n');

	f.print("NTPERR");
	f.print('=');
	f.print((*c).ntpErr);
	f.print('\n');

	f.print("NTPS");
	f.print('=');
	f.print((*c).ntps);
	f.print('\n');

	f.close();
	return (1);
}
