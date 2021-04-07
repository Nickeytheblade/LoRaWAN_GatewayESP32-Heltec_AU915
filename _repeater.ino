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
// Este arquivo contém código para usar o gateway de canal único também como um nó de sensor.
// Por favor especifique o DevAddr e o AppSKey abaixo (e no seu backend LoRa).
// Além disso, você terá que escolher quais sensores encaminhar ao seu aplicativo.
// =========================================================================================================

#if REPEATER == 1

#define _ICHAN 0
#define _OCHAN 1

#ifdef _TTNSERVER
#error "Por favor, undefined _THINGSERVER, para REAPETR desligamento WiFi."
#endif

// Envia uma mensagem LoRa do transmissor do gateway.
// XXX Talvez devêssemos bloquear o recebimento ontul a mensagem é transmissora.
int sendLora(char *msg, int len)
{
	// Verifique se len não excede o comprimento máximo.
	Serial.print("envioLoRa:: ");

	for (int i = 0; i < len; i++)
	{
		Serial.print(msg[1], HEX);
		Serial.print('.');
	}

	if (debug >= 2)
		Serial.flush();
	return (1);
}

#endif //REPEATER==1
