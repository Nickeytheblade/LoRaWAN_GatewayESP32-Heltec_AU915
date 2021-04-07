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

#if GATEWAYNODE == 1

unsigned char DevAddr[4] = _DEVADDR; // Veja ESP32-GatewayLoRaWAN.h

// ---------------------------------------------------------------------------------------------------------
// XXX Sensores Internos de Leitura Experimental.
//
// Você pode monitorar algumas configurações do chip RFM95/SX1276. Por exemplo, a temperatura
// que está definido no REGTEMP no modo FSK (não no LoRa). Ou o valor da bateria.
// Encontre alguns valores de sensor sensíveis para o rádio LoRa e leia-os abaixo em função separada.
// ---------------------------------------------------------------------------------------------------------
//uint8_t readInternal(uint8_t reg) {
//	return 0;
//}

// ---------------------------------------------------------------------------------------------------------
// LoRaSensors() é uma função que coloca valores de sensor no MACPayload e envia esses valores para o servidor.
// Para o servidor, é impossível saber se a mensagem vier de um nó LoRa ou do gateway.
//
// O código de exemplo abaixo adiciona um valor de bateria em lCode (protocolo de codificação), mas
// Claro que você pode adicionar qualquer sequência de bytes que você desejar.
//
// Parâmetros:
// - buf: contém o buffer para colocar os valores do sensor em.
// Retorna:
// - A quantidade de caracteres do sensor colocados no buffer.
// ---------------------------------------------------------------------------------------------------------
static int LoRaSensors(uint8_t *buf)
{

	uint8_t internalSersors;
	//internalSersors = readInternal(0x1A);
	//if (internalSersors > 0) {
	//	return (internalSersors);
	//}

	buf[0] = 0x86; // 134; Código de usuário < lCode + len==3 + Paridade.
	buf[1] = 0x80; // 128; lCode código <bateria>
	buf[2] = 0x3F; //  63; lCode código <valor>
	// Paridade = buf[0]==1 buf[1]=1 buf[2]=0 ==> mesmo, então o último bit do primeiro byte deve ser 0.

	return (3); // retorna o número de bytes adicionados à carga útil (payload).
}

// ---------------------------------------------------------------------------------------------------------
// XOR()
// Executar função x ou para buffer e chave.
// Como fazemos isso APENAS para chaves e X, Y sabemos que precisamos de XOR 16 bytes.
// ---------------------------------------------------------------------------------------------------------
static void mXor(uint8_t *buf, uint8_t *key)
{
	for (uint8_t i = 0; i < 16; ++i)
		buf[i] ^= key[i];
}

// ---------------------------------------------------------------------------------------------------------
// SHIFT-LEFT
// Shift o buf buffer deixado um bit.
// Parâmetros:
// - buf: Uma matriz de uint8_t bytes.
// - len: Comprimento da matriz em bytes.
// ---------------------------------------------------------------------------------------------------------
static void shift_left(uint8_t *buf, uint8_t len)
{
	while (len--)
	{
		uint8_t next = len ? buf[1] : 0; // len 0 to 15

		uint8_t val = (*buf << 1);
		if (next & 0x80)
			val |= 0x01;
		*buf++ = val;
	}
}

// ---------------------------------------------------------------------------------------------------------
// generate_subkey
// RFC 4493, para 2.3
// ---------------------------------------------------------------------------------------------------------
static void generate_subkey(uint8_t *key, uint8_t *k1, uint8_t *k2)
{

	memset(k1, 0, 16); // Preencha a subchave1 com 0x00.

	// Passo 1: Assuma que k1 é um bloco todo zero.
	AES_Encrypt(k1, key);

	// Etapa 2: Analise o resultado da operação Criptografar (em k1), gere k1.
	if (k1[0] & 0x80)
	{
		shift_left(k1, 16);
		k1[15] ^= 0x87;
	}
	else
	{
		shift_left(k1, 16);
	}

	// Passo 3: Gerar k2.
	for (uint8_t i = 0; i < 16; i++)
		k2[i] = k1[i];
	if (k1[0] & 0x80)
	{ // use k1 (== k2) de acordo com rfc.
		shift_left(k2, 16);
		k2[15] ^= 0x87;
	}
	else
	{
		shift_left(k2, 16);
	}

	// passo 4: Feito, retorne k1 e k2.
	return;
}

// ---------------------------------------------------------------------------------------------------------
// ENCODEPACKET (Códificar Pacote)
// No modo Sensor, temos que codificar a carga útil do usuário antes de enviar.
// Os arquivos da biblioteca para AES são adicionados ao diretório da biblioteca no AES.
// Por enquanto usamos a biblioteca AES feita por Ideetron como esta biblioteca
// também é usado na pilha LMIC e é de pequeno em tamanho.
//
// A função abaixo segue exatamente a especificação LoRa.
//
// O número de Bytes resultante é retornado pelas funções. Isso significa 16 bytes por bloco,
// e à medida que adicionamos ao último bloco, também retornamos 16 bytes para o último bloco.
//
// O código LMIC não faz isso, então talvez nós encurtemos o último bloco para apenas
// os bytes significativos no último bloco. Isso significa que o buffer codificado
// é exatamente tão grande quanto a mensagem original.
//
// NOTE: Esteja ciente de que a LICENÇA dos arquivos da biblioteca AES usados
// que chamamos com AES_Encrypt() é GPL3. É usado como é, mas não faz parte deste código.
//
// cmac = aes128_encrypt (K, Block_A [i]).
// ---------------------------------------------------------------------------------------------------------
uint8_t encodePacket(uint8_t *Data, uint8_t DataLength, uint16_t FrameCount, uint8_t Direction)
{

	unsigned char AppSKey[16] = _APPSKEY; // Veja ESP32-GatewayLoRaWAN.h
	uint8_t i, j;
	uint8_t Block_A[16];
	uint8_t bLen = 16; // O tamanho do bloco é 16, exceto pelo último bloco na mensagem.

	uint8_t restLength = DataLength % 16; // Nós trabalhamos em blocos de 16 bytes, este é o resto.
	uint8_t numBlocks = DataLength / 16;  // Número de blocos inteiros para criptografar.
	if (restLength > 0)
		numBlocks++; // E adicione bloco para o resto, se houver.

	for (i = 1; i <= numBlocks; i++)
	{
		Block_A[0] = 0x01;

		Block_A[1] = 0x00;
		Block_A[2] = 0x00;
		Block_A[3] = 0x00;
		Block_A[4] = 0x00;

		Block_A[5] = Direction; // 0 é uplink

		Block_A[6] = DevAddr[3]; // Só funciona para e com a ativação do tipo ABP.
		Block_A[7] = DevAddr[2];
		Block_A[8] = DevAddr[1];
		Block_A[9] = DevAddr[0];

		Block_A[10] = (FrameCount & 0x00FF);
		Block_A[11] = ((FrameCount >> 8) & 0x00FF);
		Block_A[12] = 0x00; // Bytes superiores do contador de quadros.
		Block_A[13] = 0x00; // Estes não são usados, são 0.

		Block_A[14] = 0x00;

		Block_A[15] = i;

		// Criptografar e calcular o S.
		AES_Encrypt(Block_A, AppSKey);

		// Último bloco? definir bLen para descansar.
		if ((i == numBlocks) && (restLength > 0))
			bLen = restLength;

		for (j = 0; j < bLen; j++)
		{
			*Data = *Data ^ Block_A[j];
			Data++;
		}
	}
	//return(numBlocks*16); // Nós realmente queremos retornar todos os 16 bytes no lastblock.
	return (DataLength); // ou apenas 16 * (numBlocks-1) + bLen;
}

// ---------------------------------------------------------------------------------------------------------
// MICPACKET()
// Fornecer um código MIC de 4 bytes válido (par 2.4 da especificação, RFC4493)
// veja também https://tools.ietf.org/html/rfc4493
//
// Embora nosso próprio manipulador possa escolher não interpretar os últimos 4 (MIC) bytes
// de uma mensagem de carga física PHYSPAYLOAD no sensor interno,
// Os backends TTN oficiais (e outros) irão interpretar a mensagem completa e conclui que a mensagem gerada é falsa.
// Então, nós realmente simularemos mensagens internas vindas do gateway de 1 canal
// Para vir de um sensor real e acrescentar 4 bytes MIC a cada mensagem que estiver perfeitamente legitimada.
// Parâmetros:
// - data: uint8_t array de bytes = (MHDR | FHDR | FPort | FRMPayload)
// - len: 8 = comprimento de bit de dados, normalmente menor que 64 bytes.
// - FrameCount: contador de quadros de 16 bits.
// - dir: 0 = up, 1 = down
//
// B0 = (0x49 | 4 x 0x00 | Dir | 4 x DevAddr | 4 x FCnt | 0x00 | len)
// MIC é cmac [0: 3] de (aes128_cmac (NwkSKey, B0 | Data)
// ---------------------------------------------------------------------------------------------------------
uint8_t micPacket(uint8_t *data, uint8_t len, uint16_t FrameCount, uint8_t dir)
{

	unsigned char NwkSKey[16] = _NWKSKEY;
	uint8_t Block_B[16];
	uint8_t X[16];
	uint8_t Y[16];

	// ---------------------------------------------------------------------------------------------------------
	// Constrói o bloco B usado pelo processo MIC.
	Block_B[0] = 0x49; // 1 byte MIC código.

	Block_B[1] = 0x00; // 4 byte 0x00.
	Block_B[2] = 0x00;
	Block_B[3] = 0x00;
	Block_B[4] = 0x00;

	Block_B[5] = dir; // 1 byte Direção.

	Block_B[6] = DevAddr[3]; // 4 byte DevAddr.
	Block_B[7] = DevAddr[2];
	Block_B[8] = DevAddr[1];
	Block_B[9] = DevAddr[0];

	Block_B[10] = (FrameCount & 0x00FF); // 4 byte FCNT.
	Block_B[11] = ((FrameCount >> 8) & 0x00FF);
	Block_B[12] = 0x00; // Bytes superiores do contador de quadros.
	Block_B[13] = 0x00; // Estes não são usados, são 0.

	Block_B[14] = 0x00; // 1 byte 0x00.

	Block_B[15] = len; // 1 byte len.

	// ---------------------------------------------------------------------------------------------------------
	// Etapa 1: gerar as subchaves.
	uint8_t k1[16];
	uint8_t k2[16];
	generate_subkey(NwkSKey, k1, k2);

	// ---------------------------------------------------------------------------------------------------------
	// Copie os dados para um novo buffer que é precedido pelo bloco B0.
	uint8_t micBuf[len + 16]; // B0 | dados.
	for (uint8_t i = 0; i < 16; i++)
		micBuf[i] = Block_B[i];
	for (uint8_t i = 0; i < len; i++)
		micBuf[i + 16] = data[i];

	// ---------------------------------------------------------------------------------------------------------
	// Etapa 2: Calcular o número de blocos para o CMAC.
	uint8_t numBlocks = len / 16 + 1; // Compensar o bloco B0.
	if ((len % 16) != 0)
		numBlocks++; // Se tivermos apenas um bloco de peça, leve tudo.

	// ---------------------------------------------------------------------------------------------------------
	// Etapa 3: Calcular o preenchimento é necessário.
	uint8_t restBits = len % 16; // se numBlocks não for um múltiplo de 16 bytes.

	// ---------------------------------------------------------------------------------------------------------
	// Etapa 4: Se houver um bloco de descanso, retire-o.
	// Último bloco. Nós movemos o Passo 4 para o final, pois precisamos de Y para calcular o último bloco.
	// ---------------------------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------------------------
	// Etapa 5: Faça um buffer de zeros.
	memset(X, 0, 16);

	// ---------------------------------------------------------------------------------------------------------
	// Etapa 6: faça a codificação real de acordo com o RFC.
	for (uint8_t i = 0x0; i < (numBlocks - 1); i++)
	{
		for (uint8_t j = 0; j < 16; j++)
			Y[j] = micBuf[(i * 16) + j];
		mXor(Y, X);
		AES_Encrypt(Y, NwkSKey);
		for (uint8_t j = 0; j < 16; j++)
			X[j] = Y[j];
	}

	// ---------------------------------------------------------------------------------------------------------
	// Passo 4: Se houver um bloco de descanso, retire-o.
	// Último bloco. Nós movemos a Etapa 4 para o final, pois precisamos de Y para calcular o último bloco.
	if (restBits)
	{
		for (uint8_t i = 0; i < 16; i++)
		{
			if (i < restBits)
				Y[i] = micBuf[((numBlocks - 1) * 16) + i];
			if (i == restBits)
				Y[i] = 0x80;
			if (i > restBits)
				Y[i] = 0x00;
		}
		mXor(Y, k2);
	}
	else
	{
		for (uint8_t i = 0; i < 16; i++)
		{
			Y[i] = micBuf[((numBlocks - 1) * 16) + i];
		}
		mXor(Y, k1);
	}
	mXor(Y, X);
	AES_Encrypt(Y, NwkSKey);

	// ---------------------------------------------------------------------------------------------------------
	// Passo 7: pronto, retorne o tamanho do MIC.
	// Somente 4 bytes são retornados (32 bits), o que é menor do que o RFC recomenda.
	// Retornamos adicionando 4 bytes aos dados, portanto, deve haver espaço na matriz de dados.
	data[len + 0] = Y[0];
	data[len + 1] = Y[1];
	data[len + 2] = Y[2];
	data[len + 3] = Y[3];
	return 4;
}

#if _CHECK_MIC == 1
// ---------------------------------------------------------------------------------------------------------
// CHECKMIC
// Função para verificar o MIC calculado para mensagens existentes e para novas mensagens.
// Parâmetros:
// - buf: buffer LoRa para verificar em bytes, os últimos 4 bytes contêm o MIC.
// - len: Comprimento do buffer em bytes.
// - key: Chave para usar no MIC. Normalmente este é o NwkSKey.
// ---------------------------------------------------------------------------------------------------------
static void checkMic(uint8_t *buf, uint8_t len, uint8_t *key)
{
	uint8_t cBuf[len + 1];

	if (debug >= 2)
	{
		Serial.print(F("Antigo = "));
		for (uint8_t i = 0; i < len; i++)
		{
			printHexDigit(buf[i]);
			Serial.print(' ');
		}
		Serial.println();
	}
	for (uint8_t i = 0; i < len - 4; i++)
		cBuf[i] = buf[i];
	len -= 4;

	uint16_t FrameCount = (cBuf[7] * 256) + cBuf[6];
	len += micPacket(cBuf, len, FrameCount, 0);

	if (debug >= 2)
	{
		Serial.print(F("Novo = "));
		for (uint8_t i = 0; i < len; i++)
		{
			printHexDigit(cBuf[i]);
			Serial.print(' ');
		}
		Serial.println();
	}
}
#endif

// ---------------------------------------------------------------------------------------------------------
// SENSORPACKET
// O gateway também pode ter sensores locais que precisam de relatórios.
// Vamos gerar uma mensagem no formato gateway-UDP para mensagens upStream
// para que, para o servidor de backend, pareça que um nó LoRa relatou um valor do sensor.
//
// NOTA: Nós não precisamos de qualquer função LoRa aqui, pois estamos no gateway.
// Nós só precisamos enviar uma mensagem de gateway para o upstream que se parece com uma mensagem do nó.
//
// NOTE: Esta função criptografa o sensorpayload e o backend
// pega tudo bem, pois o decodificador acha que é uma mensagem MAC.
//
// Par 4.0 LoraWan spec:
// PHYPayload = (MHDR | MACPAYLOAD | MIC)
// que é igual a:
// (MHDR | (FHDR | FPORT | FRMPAYLOAD) | MIC)
//
// Essa função faz o totalpackage e calcula MIC
// O tamanho máximo da mensagem é: 12 + (9 + 2 + 64) + 4
// O tamanho da mensagem deve ser menor que 128 bytes se a carga útil for limitada a 64 bytes.
//
// Valor de retorno:
// - On success retorna o número de bytes para enviar.
// - No erro retorna -1.
// ---------------------------------------------------------------------------------------------------------
int sensorPacket()
{

	uint8_t buff_up[512];	  // Declare o buffer aqui para evitar exceções.
	uint8_t message[64] = {0}; // Carga útil, início para 0.
	uint8_t mlength = 0;
	uint32_t tmst = micros();

	// Nos próximos bytes, a falsa mensagem LoRa deve ser colocada.
	// PHYPayload = MHDR | MACPAYLOAD | MIC
	// MHDR, 1 byte
	// MIC, 4 bytes

	// ---------------------------------------------------------------------------------------------------------
	// MHDR (Para 4.2), bit 5-7 MType, bit 2-4 RFU, bit 0-1 Maior.
	message[0] = 0x40; // MHDR 0x40 == mensagem não confirmada,
					   // FRU e maior que 0.

	// ---------------------------------------------------------------------------------------------------------
	// FHDR consiste em 4 bytes addr, 1 byte Fctrl, 2 bytes FCnt, 0-15 bytes FOpts
	// Suportamos endereços ABP somente para gateways.
	message[1] = DevAddr[3]; // Último byte [3] de endereço.
	message[2] = DevAddr[2];
	message[3] = DevAddr[1];
	message[4] = DevAddr[0]; // Primeiro byte [0] de Dev_Addr.

	message[5] = 0x00;				 // FCtrl é normalmente 0.
	message[6] = frameCount % 0x100; // LSB
	message[7] = frameCount / 0x100; // MSB

	// ---------------------------------------------------------------------------------------------------------
	// FPort, 0 ou 1 bytes. Deve ser! = 0 para mensagens não MAC, como carga útil do usuário.
	message[8] = 0x01; // O FPort não deve ser 0.
	mlength = 9;

	// FRMPayload; A carga será codificada com AES128 usando o AppSKey
	// Veja LoRa spec para 4.3.2
	// Você pode adicionar qualquer sequência de bytes abaixo com base na sua escolha pessoal de sensores, etc.
	// Os bytes de payload neste exemplo são codificados no formato LoRaCode(c).
	uint8_t PayLength = LoRaSensors((uint8_t *)(message + mlength));

	// Temos que incluir as funções AES neste estágio para gerar a carga útil LoRa.
	uint8_t CodeLength = encodePacket((uint8_t *)(message + mlength), PayLength, (uint16_t)frameCount, 0);

	mlength += CodeLength; // Comprimento de dados do sensor inclusivo.

	// MIC, Código de integridade da mensagem.
	// Como o MIC é usado pelo TTN (e outros) nós temos que ter certeza que
	// O framecount é válido e a mensagem está corretamente criptografada.
	// Nota: Até que o MIC seja feito corretamente, o TTN não recebe estas mensagens
	// Os últimos 4 bytes são bytes MIC.
	mlength += micPacket((uint8_t *)(message), mlength, (uint16_t)frameCount, 0);

	// Então agora nosso pacote está pronto e podemos enviá-lo através da interface do gateway
  // Nota: Esteja ciente de que a mensagem do sensor (que é bytes) na mensagem deverá
  // ser expandido se o servidor expuser mensagens JSON.
	int buff_index = buildPacket(tmst, buff_up, message, mlength, true);

	frameCount++;

 // Para salvar a memória, só escrevemos o contador de quadros para EEPROM a cada 10 valores.
 // Isso também significa que vamos invalidar. Valor de 10 ao reiniciar o gateway.
	if ((frameCount % 10) == 0)
		writeGwayCfg(CONFIGFILE);

	//yield(); // XXX Podemos remover isso aqui?

	if (buff_index > 512)
	{
		if (debug > 0)
			Serial.println(F("sensorPacket:: Tamanho do buffer de erro muito grande."));
		return (-1);
	}

	if (!sendUdp(ttnServer, _TTNPORT, buff_up, buff_index))
	{
		return (-1);
	}
#ifdef _THINGSERVER
	if (!sendUdp(thingServer, _THINGPORT, buff_up, buff_index))
	{
		return (-1);
	}
#endif

	if (_cad)
	{
		// Defina o estado para a varredura de CAD após o recebimento.
		_state = S_SCAN; // Inicialize o scanner.
		cadScanner();
	}
	else
	{
		// Reiniciar todas as coisas do RX lora.
		_state = S_RX;
		rxLoraModem();
	}

	return (buff_index);
}

#endif //GATEWAYNODE==1
