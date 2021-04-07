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
// Este arquivo contém o código ota para o ESP Single Channel Gateway.
// Fornece a funcionalidade do servidor OTA para que o gateway 1ch possa ser atualizado pelo ar.
// Este código usa as funções do servidor http do ESP para atualizar o gateway.
// =========================================================================================================

#if A_OTA == 1

//extern ArduinoOTAClass ArduinoOTA;

// Certifique-se de que o servidor da Web esteja em execução antes de continuar.

// --------------------------------------------------------------------------------
// setupOta
// Função para executar na função setup() para inicializar a função de atualização.
// --------------------------------------------------------------------------------
void setupOta(char *hostname)
{

	ArduinoOTA.begin();
	Serial.println(F("setupOta:: Iniciado."));

	// Padrões de nome de host para esp8266- [ChipID].
	ArduinoOTA.setHostname(hostname);

	ArduinoOTA.onStart([]() {
		String type;
		// Versão incorreta da versão XXX de platform.io e ArduinoOtaa.
		// Veja: https://github.com/esp8266/Arduino/issues/3020
		//if (ArduinoOTA.getCommand() == U_FLASH)
		type = "sketch";
		//else // U_SPIFFS
		//	type = "filesystem";

		// NOTA: se atualizar SPIFFS, este seria o local para desmontar SPIFFS usando SPIFFS.end().
		Serial.println("Comece a atualizar " + type);
	});

	ArduinoOTA.onEnd([]() {
		Serial.println("\nFim.");
	});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progresso: %u%%\r", (progress / (total / 100)));
	});

	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Erro[%u]: ", error);
    Serial.printf(".");
		if (error == OTA_AUTH_ERROR)
			Serial.println("Falha na autenticação.");
		else if (error == OTA_BEGIN_ERROR)
			Serial.println("Falha na inicialização.");
		else if (error == OTA_CONNECT_ERROR)
			Serial.println("Falha na conexão.");
		else if (error == OTA_RECEIVE_ERROR)
			Serial.println("Falha no recebimento.");
		else if (error == OTA_END_ERROR)
			Serial.println("Falha no encerramento.");
	});

	Serial.println("Pronto");
	Serial.print("Endereço IP: ");
	Serial.println(WiFi.localIP());

	// Somente se o servidor da Web estiver ativo também.
#if A_SERVER == 2 // Exibido para o momento.
	ESPhttpUpdate.rebootOnUpdate(false);

	server.on("/esp", HTTP_POST, [&]() {
		HTTPUpdateResult ret = ESPhttpUpdate.update(server.arg("firmware"), "1.0.0");

		switch (ret)
		{
		case HTTP_UPDATE_FAILED:
			//PREi::sendJSON(500, "Atualização falhou.");
			Serial.println(F("Atualização falhou."));
			break;
		case HTTP_UPDATE_NO_UPDATES:
			//PREi::sendJSON(304, "Atualização não necessária.");
			Serial.println(F("Atualização não necessária."));
			break;
		case HTTP_UPDATE_OK:
			//PREi::sendJSON(200, "Atualização iniciada.");
			Serial.println(F("Atualização iniciada."));
			ESP.restart();
			break;
		default:
			Serial.println(F("setupOta:: Retorno desconhecido="));
		}
	});
#endif
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void updateOtaa()
{

	String response = "";
	printIP((IPAddress)WiFi.localIP(), '.', response);

	ESPhttpUpdate.update(response, 80, "/arduino.bin");
}

#endif
