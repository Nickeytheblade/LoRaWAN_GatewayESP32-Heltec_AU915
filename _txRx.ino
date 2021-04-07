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
// Este arquivo contém o código específico do modem LoRa que permite receber e transmitir pacotes/mensagens.
// =========================================================================================================

// ---------------------------------------------------------------------------------------------------------
// DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN
// Envie DOWN um pacote LoRa pelo ar para o nó.
// Esta função faz toda a decodificação da mensagem do servidor e prepara um buffer de Payload.
// A carga útil é realmente transmitida pela função sendPkt().
// Esta função é usada para mensagens downstream regulares e para mensagens JOIN_ACCEPT.
//
// NOTA: Esta não é uma função de interrupção, mas é iniciada por loop().
// ---------------------------------------------------------------------------------------------------------
int sendPacket(uint8_t *buf, uint8_t length)
{
	// Pacote recebido com metadados:
	// codr	: "4/5"
	// data	: "Kuc5CSwJ7/a5JgPHrP29X9K6kf/Vs5kU6g==" // por exemplo.
	// freq	: 868.1 // 868100000
	// ipol	: true/false
	// modu : "LORA"
	// powe	: 14 // Definir por padrão.
	// rfch : 0 // Definir por padrão.
	// size	: 21
	// tmst : 1800642 // por exemplo.
	// datr	: "SF7BW125"

	// 12-byte header;
	//		HDR (1 byte)
  //
	// Resposta de dados para JOIN_ACCEPT enviada pelo servidor:
	// AppNonce (3 byte)
	// NetID (3 byte)
	// DevAddr (4 byte) [ 31..25]:NwkID , [24..0]:NwkAddr
	// DLSettings (1 byte)
	// RxDelay (1 byte)
	// CFList (fill to 16 bytes)

	int i = 0;
	StaticJsonBuffer<312> jsonBuffer;
	char *bufPtr = (char *)(buf);
	buf[length] = 0;

#if DUSB >= 1
	if (debug >= 2)
	{
		Serial.println((char *)buf);
		Serial.print(F("<"));
		Serial.flush();
	}
#endif
  // Use JSON para decodificar a string após os primeiros 4 bytes.
  // Os dados para o nó estão no campo "data". Esta função destrói o buffer original.
	JsonObject &root = jsonBuffer.parseObject(bufPtr);

	if (!root.success())
	{
#if DUSB >= 1
		Serial.print(F("sendPacket:: ERRO na decodificação do JSON."));
		if (debug >= 2)
		{
			Serial.print(':');
			Serial.println(bufPtr);
		}
		Serial.flush();
#endif
		return (-1);
	}
	delay(1);
	// Metadados enviados pelo servidor (exemplo).
	// {"txpk":{"codr":"4/5","data":"YCkEAgIABQABGmIwYX/kSn4Y","freq":868.1,"ipol":true,"modu":"LORA","powe":14,"rfch":0,"size":18,"tmst":1890991792,"datr":"SF7BW125"}}

	// Usado no protocolo:
	const char *data = root["txpk"]["data"];
	uint8_t psize = root["txpk"]["size"];
	bool ipol = root["txpk"]["ipol"];
	uint8_t powe = root["txpk"]["powe"];
	uint32_t tmst = (uint32_t)root["txpk"]["tmst"].as<unsigned long>();

	// Não usado no protocolo:
	const char *datr = root["txpk"]["datr"]; // eg "SF7BW125".
	const float ff = root["txpk"]["freq"]; // eg 869.525.
	const char *modu = root["txpk"]["modu"]; // =="LORA".
	const char *codr = root["txpk"]["codr"];
	//if (root["txpk"].containsKey("imme") ) {
	//	const bool imme = root["txpk"]["imme"]; // Transmissão Imediata (tmst não se importe).
	//}

	if (data != NULL)
	{
#if DUSB >= 1
		if (debug >= 2)
		{
			Serial.print(F("data: "));
			Serial.println((char *)data);
			if (debug >= 2)
				Serial.flush();
		}
#endif
	}
	else
	{
#if DUSB >= 1
		Serial.println(F("Enviar Pacote:: ERRO: os dados são NULOS."));
		if (debug >= 2)
			Serial.flush();
#endif
		return (-1);
	}

	uint8_t iiq = (ipol ? 0x40 : 0x27); // se ipol==true 0x40 se não 0x27.
	uint8_t crc = 0x00; // desligue o CRC para TX.
	uint8_t payLength = base64_dec_len((char *)data, strlen(data));

	// Encha a carga útil com a mensagem decodificada.
	base64_decode((char *)payLoad, (char *)data, strlen(data));

	// Calcular o tempo de espera em microssegundos.
	uint32_t w = (uint32_t)(tmst - micros());

#if _STRICT_1CH == 1
	// Use o intervalo de tempo RX1 como esta é a nossa frequência.
	// Não use RX2 ou JOIN2, pois eles contêm outras frequências.
	if ((w > 1000000) && (w < 3000000))
	{
		tmst -= 1000000;
	}
	else if ((w > 6000000) && (w < 7000000))
	{
		tmst -= 1000000;
	}

	const uint8_t sfTx = sfi; // Tome cuidado, TX sf não deve ser misturado com o SCAN.
	const uint32_t fff = freq;
#else
	const uint8_t sfTx = atoi(datr + 2); // Converte "SF9BW125" para 9
	// converter frequência dupla (MHz) em frequência uint32_t em Hz.
	const uint32_t fff = (uint32_t)((uint32_t)((ff + 0.000035) * 1000)) * 1000;
#endif

	// Todos os dados estão em Payload e em parâmetros e precisam ser transmitidos.
  // A função é chamada no espaço do usuário.
	_state = S_TX; // _state definido para transmitir.

	LoraDown.payLoad = payLoad;
	LoraDown.payLength = payLength;
	LoraDown.tmst = tmst;
	LoraDown.sfTx = sfTx;
	LoraDown.powe = powe;
	LoraDown.fff = fff;
	LoraDown.crc = crc;
	LoraDown.iiq = iiq;

	Serial.println(F("Enviar Pacote:: LoraDown preenchido."));

#if DUSB >= 1
	if (debug >= 2)
	{
		Serial.print(F("Requisição:: "));
		Serial.print(F(" tmst="));
		Serial.print(tmst);
		Serial.print(F(" aguardar="));
		Serial.println(w);

		Serial.print(F(" strict="));
		Serial.print(_STRICT_1CH);
		Serial.print(F(" datr="));
		Serial.println(datr);
		Serial.print(F(" freq="));
		Serial.print(freq);
		Serial.print(F(" ->"));
		Serial.println(fff);
		Serial.print(F(" sf  ="));
		Serial.print(sf);
		Serial.print(F(" ->"));
		Serial.print(sfTx);

		Serial.print(F(" modu="));
		Serial.print(modu);
		Serial.print(F(" powe="));
		Serial.print(powe);
		Serial.print(F(" codr="));
		Serial.println(codr);

		Serial.print(F(" ipol="));
		Serial.println(ipol);
		Serial.println(); // linha vazia entre mensagens.
	}
#endif

	if (payLength != psize)
	{
#if DUSB >= 1
		Serial.print(F("Envio de Pacote:: AVISO Comprimento de carga útil: "));
		Serial.print(payLength);
		Serial.print(F(", psize="));
		Serial.println(psize);
		if (debug >= 2)
			Serial.flush();
#endif
	}
#if DUSB >= 1
	else if (debug >= 2)
	{
		for (i = 0; i < payLength; i++)
		{
			Serial.print(payLoad[i], HEX);
			Serial.print(':');
		}
		Serial.println();
		if (debug >= 2)
			Serial.flush();
	}
#endif
	cp_up_pkt_fwd++;

#if DUSB >= 1
	Serial.println(F("Envio de pacote:: Concluído OK"));
#endif
	return 1;
} //sendPacket

// ---------------------------------------------------------------------------------------------------------
// UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP
// Baseado na informação lida do transceptor LoRa (ou mensagem falsa)
// cria uma mensagem de gateway para enviar upstream.
//
// Parâmetros:
// tmst: registro de data e hora para incluir na mensagem ascendente.
// buff_up: O buffer que é gerado para o desenvolvedor.
// message: A mensagem de payload para incluir no buff_up.
// messageLength: O número de bytes recebidos pelo transceptor LoRa.
// internal: valor booleano para indicar se o sensor local é processado.
// ---------------------------------------------------------------------------------------------------------
int buildPacket(uint32_t tmst, uint8_t *buff_up, struct LoraUp LoraUp, bool internal)
{
	long SNR;
	int rssicorr;
	int prssi; // pacote rssi.

	char cfreq[12] = {0}; // Array de caracteres para manter a frequência em MHz.
	lastTmst = tmst; // Seguindo/de acordo com especificação.
	int buff_index = 0;
	char b64[256];

	uint8_t *message = LoraUp.payLoad;
	char messageLength = LoraUp.payLength;

#if _CHECK_MIC == 1
	unsigned char NwkSKey[16] = _NWKSKEY;
	checkMic(message, messageLength, NwkSKey);
#endif

	// Leia SNR e RSSI do registro. Nota: Não para sensores internos!
  // Para sensor interno nós falsificamos esses valores, pois não podemos ler um registrador.
	if (internal)
	{
		SNR = 12;
		prssi = 50;
		rssicorr = 157;
	}
	else
	{
		SNR = LoraUp.snr;
		prssi = LoraUp.prssi; // leia o registrador 0x1A, pacote rssi.
		rssicorr = LoraUp.rssicorr;
	}

#if STATISTICS >= 1
	// Receber estatísticas.
	for (int m = (MAX_STAT - 1); m > 0; m--)
		statr[m] = statr[m - 1];
	statr[0].tmst = millis();
	statr[0].ch = ifreq;
	statr[0].prssi = prssi - rssicorr;
#if RSSI == 1
	statr[0].rssi = _rssi - rssicorr;
#endif
	statr[0].sf = LoraUp.sf;
	statr[0].node = (message[1] << 24 | message[2] << 16 | message[3] << 8 | message[4]);

#if STATISTICS >= 2
	switch (statr[0].sf)
	{
	case SF7:
		statc.sf7++;
		break;
	case SF8:
		statc.sf8++;
		break;
	case SF9:
		statc.sf9++;
		break;
	case SF10:
		statc.sf10++;
		break;
	case SF11:
		statc.sf11++;
		break;
	case SF12:
		statc.sf12++;
		break;
	}
#endif
#endif

#if DUSB >= 1
	if (debug >= 1)
	{
		Serial.print(F("pRSSI: "));
		Serial.print(prssi - rssicorr);
		Serial.print(F(" RSSI: "));
		Serial.print(_rssi - rssicorr);
		Serial.print(F(" SNR: "));
		Serial.print(SNR);
		Serial.print(F(" Length: "));
		Serial.print((int)messageLength);
		Serial.print(F(" -> "));
		int i;
		for (i = 0; i < messageLength; i++)
		{
			Serial.print(message[i], HEX);
			Serial.print(' ');
		}
		Serial.println();
		yield();
	}
#endif

// Mostrar status da mensagem recebida no display OLED.
#if OLED == 1
	display.clear();
	display.setFont(ArialMT_Plain_16);
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	char timBuff[20];
	sprintf(timBuff, "%02i:%02i:%02i", hour(), minute(), second());
	display.drawString(0, 0, "HORA ");
	display.drawString(56, 0, timBuff);
	display.drawString(0, 16, "RSSI ");
	display.drawString(43, 16, String(prssi - rssicorr));
	display.drawString(73, 16, "SNR ");
	display.drawString(111, 16, String(SNR));

	display.drawString(0, 32, "Nó > ");

	if (message[4] < 0x10)
		display.drawString(35, 32, "0" + String(message[4], HEX));
	else
		display.drawString(35, 32, String(message[4], HEX));
	if (message[3] < 0x10)
		display.drawString(57, 32, "0" + String(message[3], HEX));
	else
		display.drawString(57, 32, String(message[3], HEX));
	if (message[2] < 0x10)
		display.drawString(77, 32, "0" + String(message[2], HEX));
	else
		display.drawString(77, 32, String(message[2], HEX));
	if (message[1] < 0x10)
		display.drawString(99, 32, "0" + String(message[1], HEX));
	else
		display.drawString(99, 32, String(message[1], HEX));

  display.drawString(115, 32, " <");

	display.drawString(0, 49, "Pacote ");
	display.drawString(54, 49, String((int)messageLength));
  display.drawString(75, 49, "BYTES");
	display.display();
	yield();
#endif

	int j;

// A biblioteca XXX Base64 é nopad.
// Portanto, talvez tenhamos que adicionar caracteres de preenchimento até que o Comprimento da mensagem seja múltiplo de 4!
// Codifica a mensagem com messageLength em b64.
	int encodedLen = base64_enc_len(messageLength); // máximo 341.
#if DUSB >= 1
	if ((debug >= 1) && (encodedLen > 255))
	{
		Serial.println(F("buildPacket:: b64 erro."));
		if (debug >= 2)
			Serial.flush();
	}
#endif
	base64_encode(b64, (char *)message, messageLength); // máximo 341.

	// Preencha o buffer de dados com campos fixos.
	buff_up[0] = PROTOCOL_VERSION; // 0x01 entretanto.
	buff_up[3] = PKT_PUSH_DATA;	// 0x00

	// LEIA MAC ENDEREÇO DO ESP8266, e insira 0xFF 0xFF no meio.
	buff_up[4] = MAC_array[0];
	buff_up[5] = MAC_array[1];
	buff_up[6] = MAC_array[2];
	buff_up[7] = 0xFF;
	buff_up[8] = 0xFF;
	buff_up[9] = MAC_array[3];
	buff_up[10] = MAC_array[4];
	buff_up[11] = MAC_array[5];

	// Começa a compor o datagrama com o cabeçalho.
	uint8_t token_h = (uint8_t)rand(); // token aleatório.
	uint8_t token_l = (uint8_t)rand(); // token aleatório.
	buff_up[1] = token_h;
	buff_up[2] = token_l;
	buff_index = 12; // Cabeçalho binário de 12 bytes (!).

	// Início da estrutura JSON que fará a carga útil.
	memcpy((void *)(buff_up + buff_index), (void *)"{\"rxpk\":[", 9);
	buff_index += 9;
	buff_up[buff_index] = '{';
	++buff_index;
	j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, "\"tmst\":%u", tmst);
#if DUSB >= 1
	if ((j < 0) && (debug >= 1))
	{
		Serial.println(F("buildPacket:: Erro."));
	}
#endif
	buff_index += j;
	ftoa((double)freq / 1000000, cfreq, 6); // XXX Isso pode ser feito melhor.
	j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"chan\":%1u,\"rfch\":%1u,\"freq\":%s", 0, 0, cfreq);
	buff_index += j;
	memcpy((void *)(buff_up + buff_index), (void *)",\"stat\":1", 9);
	buff_index += 9;
	memcpy((void *)(buff_up + buff_index), (void *)",\"modu\":\"LORA\"", 14);
	buff_index += 14;

	/* Taxa de dados e largura de banda do Lora, 16-19 gráficos úteis. */
	switch (LoraUp.sf)
	{
	case SF6:
		memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF6", 12);
		buff_index += 12;
		break;
	case SF7:
		memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF7", 12);
		buff_index += 12;
		break;
	case SF8:
		memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF8", 12);
		buff_index += 12;
		break;
	case SF9:
		memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF9", 12);
		buff_index += 12;
		break;
	case SF10:
		memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF10", 13);
		buff_index += 13;
		break;
	case SF11:
		memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF11", 13);
		buff_index += 13;
		break;
	case SF12:
		memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF12", 13);
		buff_index += 13;
		break;
	default:
		memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF?", 12);
		buff_index += 12;
	}
	memcpy((void *)(buff_up + buff_index), (void *)"BW125\"", 6);
	buff_index += 6;
	memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"4/5\"", 13);
	buff_index += 13;
	j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"lsnr\":%li", SNR);
	buff_index += j;
	j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"rssi\":%d,\"size\":%u", prssi - rssicorr, messageLength);
	buff_index += j;
	memcpy((void *)(buff_up + buff_index), (void *)",\"data\":\"", 9);
	buff_index += 9;

	// Use a biblioteca gBase64 para preencher a string de dados.
	encodedLen = base64_enc_len(messageLength); // máximo 341.
	j = base64_encode((char *)(buff_up + buff_index), (char *)message, messageLength);

	buff_index += j;
	buff_up[buff_index] = '"';
	++buff_index;

	// Fim da serialização de pacotes.
	buff_up[buff_index] = '}';
	++buff_index;
	buff_up[buff_index] = ']';
	++buff_index;

	// Fim da carga útil do datagrama JSON. */
	buff_up[buff_index] = '}';
	++buff_index;
	buff_up[buff_index] = 0; // Adicione o terminador de string, por segurança.
#if DUSB >= 1
	if (debug >= 2)
	{
		Serial.print(F("RXPK:: "));
		Serial.println((char *)(buff_up + 12)); // DEBUG: exibe payload JSON.
	}
	if (debug >= 2)
	{
		Serial.print(F("RXPK:: comprimento do pacote = "));
		Serial.println(buff_index);
	}
#endif
	return (buff_index);
} // buildPacket

// ---------------------------------------------------------------------------------------------------------
// UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP UP
// Receber um pacote LoRa pelo ar, LoRa.
//
// Recebe uma mensagem LoRa e preenche o buffer de buff_up char.
// Retorna valores:
// - retorna o tamanho da string retornada em buff_up.
// - retorna -1 quando nenhuma mensagem chegou.
//
// Esta é a função "highlevel" chamada por loop().
// _state é S_RX ao iniciar e _state é S_STANDBY ao sair da função.
// ---------------------------------------------------------------------------------------------------------
int receivePacket()
{
	uint8_t buff_up[TX_BUFF_SIZE]; // buffer para compor o pacote upstream para o servidor backend.
	long SNR;
	uint8_t message[128] = {0x00}; // O tamanho do MSG é de 128 bytes para rx.
	uint8_t messageLength = 0;

  // Mensagem regular recebida, consulte a tabela de especificações SX1276 18.
  // Próximo comando também pode ser um "while" para combinar várias mensagens recebidas
  // em uma mensagem UDP, pois a especificação Semtech Gateway permite isso.
  // XXX Ainda não suportado

  // Pegue o timestamp o mais rápido possível, para ter um timestamp de recepção preciso.
  // TODO: tmst pode pular se estouro micros().
  uint32_t tmst = (uint32_t)micros(); // Apenas microssegundos, rollover em.
	lastTmst = tmst; // Seguindo/de acordo com especificação.

	// Manipule os dados físicos lidos no LoraUp.
	if (LoraUp.payLength > 0)
	{

		// Pacote recebido externamente, então o último parâmetro é falso (== LoRa externo).
		int build_index = buildPacket(tmst, buff_up, LoraUp, false);

		// REPEATER é uma função especial em que retransmitimos a mensagem recebida em _ICHANN para _OCHANN.
		// Nota: No momento, o OCHANN não pode ser o mesmo que o _ICHANN.
#if REPEATER == 1
		if (!sendLora(LoraUp.payLoad, LoraUp.payLength))
		{
			return (-3);
		}
#endif

		LoraUp.payLength = 0;
		LoraUp.payLoad[0] = 0x00;

		// Esta é uma das possíveis áreas problemáticas.
		// Se possível, o tráfego USB deve ficar de fora das rotinas de interrupção
		// rxpk PUSH_DATA recebido do nó é rxpk (* 2, par. 3.2).
#ifdef _TTNSERVER
		if (!sendUdp(ttnServer, _TTNPORT, buff_up, build_index))
		{
			return (-1); // Recebeu uma mensagem.
		}
		yield();
#endif

#ifdef _THINGSERVER
		if (!sendUdp(thingServer, _THINGPORT, buff_up, build_index))
		{
			return (-2); // Recebeu uma mensagem.
		}
#endif
		return (build_index);
	}

	return (0); // Falha nenhuma mensagem lida.

} //receivePacket
