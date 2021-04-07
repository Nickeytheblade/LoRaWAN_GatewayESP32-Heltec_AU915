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
//
// === MÁQUINA DE ESTADO ===
// O programa usa a seguinte máquina de estados (em _state), todos os estados são feitos na rotina de interrupção,
// somente o acompanhamento do S-RXDONE é feito no programa principal loop(). Isso ocorre porque, de outra forma,
// o processamento de interrupção levaria muito tempo para terminar.
//
// S-INIT = 0, Os comandos neste estado são executados apenas uma vez.
// - Ir para o S_SCAN
// S-SCAN, parte do CadScanner()
// - Após CDDONE (int0) obteve S_CAD, S-CAD,
// - Após CDDECT (int1) ir para S_RX,
// - Após CDDONE (int0), vá para S_SCAN, S-RX, CDDECT recebido, mensagem detectada, ciclo de RX iniciado.
// - Após RXDONE (int0) ler ok ir para S_RX ou S_SCAN,
// - Sobre RXTOUT (int1) goto S_SCAN, S-RXDONE, lê o buffer
// - Aguarde a leitura em loop()
// - Na mensagem enviada para o servidor goto S_SCAN, S-TX Transmitindo uma mensagem,
// - No TX, vá para o S_SCAN.
//
// SPI E INTERRUPÇÕES
// As comunicações RFM96/SX1276 com o ESP32/ESP8266 por meio de interrupções e interface SPI.
// A interface SPI é bidirecional e permite tanto partes para escrever e ler simultaneamente para registradores.
// A grande desvantagem é que o acesso não é protegido por interrupções e não interromper o acesso significa que
// quando um programa em loop() e um programa na interrupção, acesse o readregister e a função writeRegister() ao
// mesmo tempo que provavelmente ocorrerá um erro.
// Portanto, é melhor não usar interrupções (como LMIC) ou use somente essas funções em inteiros
// e para processar mais no programa principal loop().
// =========================================================================================================

// ---------------------------------------------------------------------------------------------------------
// Definição de mutex.
// ---------------------------------------------------------------------------------------------------------
#if MUTEX == 1
void CreateMutux(int *mutex)
{
	*mutex = 1;
}

#define LIB_MUTEX 1
#if LIB_MUTEX == 1
bool GetMutex(int *mutex)
{
	//noInterrupts();
	if (*mutex == 1)
	{
		*mutex = 0;
		//interrupts();
		return (true);
	}
	//interrupts();
	return (false);
}
#else
bool GetMutex(int *mutex)
{

	int iOld = 1, iNew = 0;

	asm volatile(
		"rsil a15, 1\n"	// Leia e defina o nível de interrupção para 1.
		"l32i %0, %1, 0\n" // Carrega o valor do mutex.
		"bne %0, %2, 1f\n" // Compare com Old, branch se não for igual.
		"s32i %3, %1, 0\n" // Armazenar iNew em mutex.
		"1:\n" // Alvo de filial.
		"wsr.ps a15\n" // Restaurar o estado do programa.
		"rsync\n"
		: "=&r"(iOld)
		: "r"(mutex), "r"(iOld), "r"(iNew)
		: "a15", "memory");
	return (bool)iOld;
}
#endif

void ReleaseMutex(int *mutex)
{
	*mutex = 1;
}

#endif //MUTEX==1

// ---------------------------------------------------------------------------------------------------------
// Lê um valor de byte, par addr é o endereço
// Retorna o valor do registrador (addr)
//
// O pino SS (Chip select) é usado para garantir que o RFM95 esteja selecionado.
// Também usamos um mutexSPI volátil para informar se a interrupção está em uso.
// A variável é válida por razões óbvias para o tráfego de leitura e gravação no mesmo tempo.
// Já que ler e escrever significam que escrevemos para a interface SPI.
//
// Parâmetros:
//
// Endereço: Endereço SPI para ler. Digite unint8_t
// Retorna: Valor lido do endereço.
// ---------------------------------------------------------------------------------------------------------

// define as configurações do SPI para ler mensagens.
SPISettings readSettings(SPISPEED, MSBFIRST, SPI_MODE0);

uint8_t readRegister(uint8_t addr)
{
	//noInterrupts(); // XXX
#if MUTEX_SPI == 1
	if (!GetMutex(&mutexSPI))
	{
#if DUSB >= 1
		if (debug >= 0)
		{
			gwayConfig.reents++;
			Serial.print(F("Leitura do registro :: leia reentrada."));
			printTime();
			Serial.println();
			if (debug >= 2)
				Serial.flush();
			delayMicroseconds(50);
			initLoraModem();
		}
#endif
		return 0;
	}
#endif

	SPI.beginTransaction(readSettings);

	digitalWrite(pins.ss, LOW); // Selecione o receptor.
	SPI.transfer(addr & 0x7F);
	uint8_t res = (uint8_t)SPI.transfer(0x00);
	digitalWrite(pins.ss, HIGH); // Desmarcar Receptor.
	SPI.endTransaction();

#if MUTEX_SPI == 1
	ReleaseMutex(&mutexSPI);
#endif
	//interrupts(); // XXX
	return ((uint8_t)res);
}

// ---------------------------------------------------------------------------------------------------------
// Escreve valor para um registrador com endereço addr.
// Função escreve um byte de cada vez.
// Parâmetros:
//
// addr: Endereço SPI para gravar.
// value: O valor para gravar no endereço.
// Retorna: <void>.
// ---------------------------------------------------------------------------------------------------------

// Define as configurações para a escrita do SPI.
SPISettings writeSettings(SPISPEED, MSBFIRST, SPI_MODE0);

void writeRegister(uint8_t addr, uint8_t value)
{
	//noInterrupts(); // XXX
#if MUTEX_SPO == 1
	if (!GetMutex(&mutexSPI))
	{
#if DUSB >= 1
		if (debug >= 0)
		{
			gwayConfig.reents++;
			Serial.print(F("Escrita do registro :: escrever reentrada."));
			printTime();
			Serial.println();
			delayMicroseconds(50);
			initLoraModem();
			if (debug >= 2)
				Serial.flush();
		}
#endif
		return;
	}
#endif

	SPI.beginTransaction(writeSettings);
	digitalWrite(pins.ss, LOW); // Selecione o receptor.

	SPI.transfer((addr | 0x80) & 0xFF);
	SPI.transfer(value & 0xFF);
	//delayMicroseconds(10);

	digitalWrite(pins.ss, HIGH); // Desmarcar Receptor.

	SPI.endTransaction();

#if MUTEX_SPO == 1
	ReleaseMutex(&mutexSPI);
#endif
	//interrupts();
}

// ---------------------------------------------------------------------------------------------------------
// Escreva um buffer para um registrador com endereço addr.
// Função escreve um byte de cada vez.
//
// Parâmetros:
//
// addr: Endereço SPI para gravar.
// value: O valor para gravar no endereço.
// Retorna: <void>.
// ---------------------------------------------------------------------------------------------------------

void writeBuffer(uint8_t addr, uint8_t *buf, uint8_t len)
{
	//noInterrupts(); // XXX
#if MUTEX_SPO == 1
	if (!GetMutex(&mutexSPI))
	{
#if DUSB >= 1
		if (debug >= 0)
		{
			gwayConfig.reents++;
			Serial.print(F("Escrita do buffer:: escrever reentrada."));
			printTime();
			Serial.println();
			delayMicroseconds(50);
			initLoraModem();
			if (debug >= 2)
				Serial.flush();
		}
#endif
		return;
	}
#endif

	SPI.beginTransaction(writeSettings);
	digitalWrite(pins.ss, LOW); // Selecione o receptor.

	SPI.transfer((addr | 0x80) & 0xFF); // Escreva o endereço do buffer.
	for (uint8_t i = 0; i < len; i++)
	{
		SPI.transfer(buf[i] & 0xFF);
	}
	digitalWrite(pins.ss, HIGH); // Desmarque o receptor.

	SPI.endTransaction();

#if MUTEX_SPI == 1
	ReleaseMutex(&mutexSPI);
#endif
	//interrupts();
}

// ---------------------------------------------------------------------------------------------------------
// setRate está definindo taxa e fator de espalhamento e CRC etc, para transmissão.
// Modem Config 1 (MC1) == 0x72 para sx1276
// Modem Config 2 (MC2) == (CRC_ON) | (sf << 4)
// Modem Config 3 (MC3) == 0x04 | (opcionais SF11 / 12 OTIMIZAR BAIXA DE DADOS 0x08)
// sf == SF7 default 0x07, (SF7 << 4) == SX72_MC2_SF7
// bw == 125 == 0x70
// cr == CR4 / 5 == 0x02
// CRC_ON == 0x04
// ---------------------------------------------------------------------------------------------------------

void setRate(uint8_t sf, uint8_t crc)
{
	uint8_t mc1 = 0, mc2 = 0, mc3 = 0;
#if DUSB >= 2
	if ((sf < SF7) || (sf > SF12))
	{
		if (debug >= 1)
		{
			Serial.print(F("setRate:: SF = "));
			Serial.println(sf);
		}
		return;
	}
#endif
	// Definir taxa com base no fator de propagação etc.
	if (sx1272)
	{
		mc1 = 0x0A; // SX1276_MC1_BW_250 0x80 | SX1276_MC1_CR_4_5 0x02
		mc2 = ((sf << 4) | crc) % 0xFF;
		// SX1276_MC1_BW_250 0x80 | SX1276_MC1_CR_4_5 0x02 | SX1276_MC1_IMPLICIT_HEADER_MODE_ON 0x01
		if (sf == SF11 || sf == SF12)
		{
			mc1 = 0x0B;
		}
	}
	else
	{
		mc1 = 0x72; // SX1276_MC1_BW_125==0x70 | SX1276_MC1_CR_4_5==0x02
		mc2 = ((sf << 4) | crc) & 0xFF; // crc is 0x00 or 0x04==SX1276_MC2_RX_PAYLOAD_CRCON
		mc3 = 0x04;	// 0x04; SX1276_MC3_AGCAUTO
		if (sf == SF11 || sf == SF12)
		{
			mc3 |= 0x08;
		} // 0x08 | 0x04
	}

	// Cabeçalho Implícito (IH), para beacons de classe b (&& SF6).
	//if (getIh(LMIC.rps)) {
	// mc1 |= SX1276_MC1_IMPLICIT_HEADER_MODE_ON;
	// writeRegister(REG_PAYLOAD_LENGTH, getIh(LMIC.rps)); // comprimento requerido.
	//}

	writeRegister(REG_MODEM_CONFIG1, (uint8_t)mc1);
	writeRegister(REG_MODEM_CONFIG2, (uint8_t)mc2);
	writeRegister(REG_MODEM_CONFIG3, (uint8_t)mc3);

	// Configurações de tempo limite de símbolo.
	if (sf == SF10 || sf == SF11 || sf == SF12)
	{
		writeRegister(REG_SYMB_TIMEOUT_LSB, (uint8_t)0x05);
	}
	else
	{
		writeRegister(REG_SYMB_TIMEOUT_LSB, (uint8_t)0x08);
	}
	return;
}

// ---------------------------------------------------------------------------------------------------------
// Definir a frequência do nosso gateway.
// A função não possui nenhum parâmetro diferente da configuração de frequência usada no init.
// Como estamos usando um gateway 1ch, esse valor é definido como fixo.
// ---------------------------------------------------------------------------------------------------------

void setFreq(uint32_t freq)
{
	// Definir frequência.
	uint64_t frf = ((uint64_t)freq << 19) / 32000000;
	writeRegister(REG_FRF_MSB, (uint8_t)(frf >> 16));
	writeRegister(REG_FRF_MID, (uint8_t)(frf >> 8));
	writeRegister(REG_FRF_LSB, (uint8_t)(frf >> 0));

	return;
}

// ---------------------------------------------------------------------------------------------------------
// Definir potência para o nosso gateway.
// ---------------------------------------------------------------------------------------------------------
void setPow(uint8_t powe)
{
	if (powe >= 16)
		powe = 15;
	//if (powe >= 15) powe = 14;
	else if (powe < 2)
		powe = 2;

	ASSERT((powe >= 2) && (powe <= 15));

	uint8_t pac = (0x80 | (powe & 0xF)) & 0xFF;
	writeRegister(REG_PAC, (uint8_t)pac); // Define 0x09 para pac.

	// XXX Configurações de energia para CFG_sx1272 são diferentes.

	return;
}

// ---------------------------------------------------------------------------------------------------------
// Usado para definir o rádio para o modo LoRa (transmissor)
// Note que este modo só pode ser definido no modo SLEEP e não em espera.
// Também não deve haver necessidade de reinicializar, este modo é definido na função setup().
// Para altas frequências (> 860 MHz) precisamos & com 0x08, caso contrário, com 0x00.
// ---------------------------------------------------------------------------------------------------------

//void ICACHE_RAM_ATTR opmodeLora()
//{
//#ifdef CFG_sx1276_radio
//       uint8_t u = OPMODE_LORA | 0x80;  // TBD: SX1276 alta frequência.
//#else // SX-1272
//	    uint8_t u = OPMODE_LORA | 0x08;
//#endif
//    writeRegister(REG_OPMODE, (uint8_t) u);
//}

// ---------------------------------------------------------------------------------------------------------
// Defina o opmode para um valor conforme definido no topo;
// Os valores são 0x00 a 0x07.
// O valor é definido para os 3 bits mais baixos, os outros bits são como antes.
// ---------------------------------------------------------------------------------------------------------
void opmode(uint8_t mode)
{
	if (mode == OPMODE_LORA)
		writeRegister(REG_OPMODE, (uint8_t)mode);
	else
		writeRegister(REG_OPMODE, (uint8_t)((readRegister(REG_OPMODE) & ~OPMODE_MASK) | mode));
}

// ---------------------------------------------------------------------------------------------------------
// Pule para a próxima frequência conforme definido por NUM_HOPS.
// Esta função só deve ser usada para operação do receptor. A atual frequência
// do receptor é determinada pelo índice ifreq da seguinte forma: freqs [ifreq].
// ---------------------------------------------------------------------------------------------------------
void hop()
{
	// Se já estamos em uma função hop, não prossiga.
	if (!inHop)
	{

		inHop = true;
		opmode(OPMODE_STANDBY);
		ifreq = (ifreq + 1) % NUM_HOPS;
		setFreq(freqs[ifreq]);
		hopTime = micros(); // registrar o momento do HOP.
		opmode(OPMODE_CAD);
		inHop = false;
	}
#if DUSB >= 2
	else
	{
		if (debug >= 3)
		{
			Serial.println(F("Hop:: tentar re-entrada."));
		}
	}
#endif
}

// ---------------------------------------------------------------------------------------------------------
// Esta função LoRa lê uma mensagem do transceptor LoRa.
// No sucesso: Retorna o comprimento da mensagem lido quando a mensagem foi recebida corretamente.
// No falha: Retorna um valor negativo no erro (por exemplo, erro de CRC).
// Função UP, esta é a função de recebimento "lowlevel" chamada por stateMachine()
// Lidando com as funções LoRa específicas do rádio.
// ---------------------------------------------------------------------------------------------------------
uint8_t receivePkt(uint8_t *payload)
{
	uint8_t irqflags = readRegister(REG_IRQ_FLAGS); // 0x12; bandeiras lidas de volta.

	cp_nb_rx_rcv++; // Receber contador de estatísticas.

	// Verifique se há payload IRQ_LORA_CRCERR_MASK = conjunto 0x20.
	if ((irqflags & IRQ_LORA_CRCERR_MASK) == IRQ_LORA_CRCERR_MASK)
	{
#if DUSB >= 2
		Serial.println(F("CRC"));
#endif
		// Redefinir o sinalizador CRC 0x20.
		writeRegister(REG_IRQ_FLAGS, (uint8_t)(IRQ_LORA_CRCERR_MASK || IRQ_LORA_RXDONE_MASK)); // 0x12; limpar CRC (== 0x20) bandeira.
		return 0;
	}
	else
	{
		cp_nb_rx_ok++; // Receber contador de estatísticas OK.

		uint8_t currentAddr = readRegister(REG_FIFO_RX_CURRENT_ADDR); // 0x10.
		uint8_t receivedCount = readRegister(REG_RX_NB_BYTES); // 0x13; Quantos bytes foram lidos.
#if DUSB >= 2
		if ((debug >= 0) && (currentAddr > 64))
		{
			Serial.print(F("receivePkt:: Rx addr>64"));
			Serial.println(currentAddr);
			if (debug >= 2)
				Serial.flush();
		}
#endif
		writeRegister(REG_FIFO_ADDR_PTR, (uint8_t)currentAddr); // 0x0D

		if (receivedCount > 64)
		{
#if DUSB >= 2
			Serial.print(F("receivePkt:: Contagem recebida = "));
			Serial.println(receivedCount);
			if (debug >= 2)
				Serial.flush();
#endif
			receivedCount = 64;
		}
#if DUSB >= 2
		else if (debug >= 2)
		{
			Serial.print(F("ReceivePkt:: addr = "));
			Serial.print(currentAddr);
			Serial.print(F(", len = "));
			Serial.println(receivedCount);
			if (debug >= 2)
				Serial.flush();
		}
#endif
		for (int i = 0; i < receivedCount; i++)
		{
			payload[i] = readRegister(REG_FIFO); // 0x00
		}

		writeRegister(REG_IRQ_FLAGS, (uint8_t)0xFF); // Redefinir todas as interrupções.
		return (receivedCount);
	}

	writeRegister(REG_IRQ_FLAGS, (uint8_t)IRQ_LORA_RXDONE_MASK); // 0x12; Limpar RxDone IRQ_LORA_RXDONE_MASK.
	return 0;
}

// ---------------------------------------------------------------------------------------------------------
// Esta função DOWN envia uma carga para o nó LoRa pelo ar.
// O rádio deve voltar ao modo de espera assim que a transmissão terminar.
//
// NOTA: as funções writeRegister não devem ser usadas fora de interrupções.
// ---------------------------------------------------------------------------------------------------------
bool sendPkt(uint8_t *payLoad, uint8_t payLength)
{
#if DUSB >= 2
	if (payLength >= 128)
	{
		if (debug >= 1)
		{
			Serial.print("sendPkt:: len = ");
			Serial.println(payLength);
		}
		return false;
	}
#endif
	writeRegister(REG_FIFO_ADDR_PTR, (uint8_t)readRegister(REG_FIFO_TX_BASE_AD)); // 0x0D, 0x0E.

	writeRegister(REG_PAYLOAD_LENGTH, (uint8_t)payLength); // 0x22.
	//for(int i = 0; i < payLength; i++)
	//{
	//    writeRegister(REG_FIFO, (uint8_t) payLoad[i]);  // 0x00.
	//}
	writeBuffer(REG_FIFO, (uint8_t *)payLoad, payLength);
	return true;
}

// ---------------------------------------------------------------------------------------------------------
// loraWait()
// Esta função implementa o protocolo de espera necessário para transmissões downstream.
//
// Nota: O tempo das mensagens downstream e JoinAccept é MUITO crítico.
//
// Como o watchdog ESP32/ESP8266 não vai gostar de esperar mais que algumas centenas de
// milissegundos (ou vai entrar), temos que implementar uma maneira simples de esperar o
// horário em caso de ter que esperar segundos antes de enviar mensagens (por exemplo, para OTAA 5 ou 6 segundos).
// Sem ele, o sistema é conhecido por falhar em metade dos casos que ele tem que esperar mensagens JOIN-ACCEPT para enviar.
//
// Essa função usa uma combinação de instruções delay() e delayMicroseconds().
// Como usamos delay() somente quando ainda há tempo suficiente para esperar e usamos micros()
// para ter certeza de que delay() não demorou muito para que isso funcione.
//
// Parâmetro: uint32-t tmst fornece o valor micros() quando a transmissão deve iniciar.
// ---------------------------------------------------------------------------------------------------------

void loraWait(uint32_t tmst)
{
	uint32_t startTime = micros(); // Início da função loraWait.
	tmst += txDelay;
	uint32_t waitTime = tmst - micros();

	while (waitTime > 16000)
	{
		delay(15); // ms atraso incluindo rendimento, ligeiramente mais curto.
		waitTime = tmst - micros();
	}
	if (waitTime > 0)
		delayMicroseconds(waitTime);
#if DUSB >= 2
	else if ((waitTime + 20) < 0)
	{
		Serial.println(F("LoRa Espere ATÉ TARDE."));
	}

	if (debug >= 1)
	{
		Serial.print(F("Começo: "));
		Serial.print(startTime);
		Serial.print(F(", Fim: "));
		Serial.print(tmst);
		Serial.print(F(", Esperou: "));
		Serial.print(tmst - startTime);
		Serial.print(F(", delay - Atraso = "));
		Serial.print(txDelay);
		Serial.println();
		if (debug >= 2)
			Serial.flush();
	}
#endif
}

// ---------------------------------------------------------------------------------------------------------
// txLoraModem
// Inicia o transmissor e transmite o buffer.
// Após a transmissão bem-sucedida (dio0 == 1) o TxDone reinicia o receptor.
//
// crc está definido como 0x00 para TX.
// iiq é configurado para 0x27 (ou 0x40 com base no valor de ipol em txpkt).
//
// 1. opmode Lora (apenas no modo Sleep).
// 2. standby do opmode.
// 3. Configurar Modem.
// 4. Configurar Canal.
// 5. Escrever PA Ramp.
// 6. Config Power.
// 7. RegLoRaSyncWord LORA_MAC_PREAMBLE.
// 8. Gravar o mapeamento dio do REG (dio0).
// 9. Gravar sinalizadores REG IRQ.
// 10. Escreva a máscara REG IRQ.
// 11. Escreva REG LoRa Fifo Base Address.
// 12. Escreva REG LoRa Fifo Addr Ptr.
// 13. Escreva REG LoRa Payload Length.
// 14. Buffer de gravação (byte por byte).
// 15. Espere até o momento certo para transmitir chegou.
// 16. opmode TX.
// ---------------------------------------------------------------------------------------------------------

void txLoraModem(uint8_t *payLoad, uint8_t payLength, uint32_t tmst, uint8_t sfTx,
				 uint8_t powe, uint32_t freq, uint8_t crc, uint8_t iiq)
{
#if DUSB >= 2
	if (debug >= 1)
	{
		// Certifique-se de que todo o material serial seja feito antes de continuar.
		Serial.print(F("txLoraModem::"));
		Serial.print(F("  Potência: "));
		Serial.print(powe);
		Serial.print(F(", Frequência: "));
		Serial.print(freq);
		Serial.print(F(", CRC: "));
		Serial.print(crc);
		Serial.print(F(", IIQ: 0X"));
		Serial.print(iiq, HEX);
		Serial.println();
		if (debug >= 2)
			Serial.flush();
	}
#endif
	_state = S_TX;

	// 1. Selecione o modem LoRa no modo de espera.
	//opmode(OPMODE_LORA);  // Ajusta o registrador 0x01 para 0x80.

	// Afirme o valor do modo atual.
	ASSERT((readRegister(REG_OPMODE) & OPMODE_LORA) != 0);

	// 2. Entrar no modo de espera (necessário para o carregamento FIFO)).
	opmode(OPMODE_STANDBY); // Defina 0x01 para 0x01.

	// 3. Fator de propagação de inicialização e outra configuração de modem.
	setRate(sfTx, crc);

	// Frequency hopping (HOP) - Frequência Hopping. Saltos de frequências.
	//writeRegister(REG_HOP_PERIOD, (uint8_t) 0x00);  // Defina 0x24 para 0x00 apenas para receptores.

	// 4. Frequência de inicialização, canal de configuração.
	setFreq(freq);

	// 6. Definir nível de energia, REG_PAC.
	setPow(powe);

	// 7. Impedir a comunicação nó a nó.
	writeRegister(REG_INVERTIQ, (uint8_t)iiq); // 0x33, (0x27 ou 0x40).

	// 8. Definir o mapeamento de IRQ DIO0 = TxDone DIO1 = NOP DIO2 = NOP (ou menos para o gateway de 1 canal).
	writeRegister(REG_DIO_MAPPING_1, (uint8_t)(MAP_DIO0_LORA_TXDONE | MAP_DIO1_LORA_NOP | MAP_DIO2_LORA_NOP));

	// 9. Limpar todos os sinalizadores de IRQ de rádio.
	writeRegister(REG_IRQ_FLAGS, (uint8_t)0xFF);

	// 10. Mascarar todos os IRQs, exceto o TxDone.
	writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t)~IRQ_LORA_TXDONE_MASK);

	// txLora
	opmode(OPMODE_FSTX); // Defina 0x01 para 0x02 (o valor atual se torna 0x82).

	// 11, 12, 13, 14. Escreva o buffer para o FiFo.
	sendPkt(payLoad, payLength);

	// 15. Aguarde o atraso extra. O timer de delayMicroseconds é preciso até 16383 uSec.
	loraWait(tmst);

	// Definir o endereço base do buffer de transmissão no FIFO.
	writeRegister(REG_FIFO_ADDR_PTR, (uint8_t)readRegister(REG_FIFO_TX_BASE_AD)); // Defina 0x0D para 0x0F (contém 0x80);

	// Para TX, temos que definir o valor de PAYLOAD_LENGTH.
	writeRegister(REG_PAYLOAD_LENGTH, (uint8_t)payLength); // Defina 0x22, max 0x40 == 64Byte long.

	//For TX we have to set the MAX_PAYLOAD_LENGTH
	writeRegister(REG_MAX_PAYLOAD_LENGTH, (uint8_t)MAX_PAYLOAD_LENGTH); // Defina 0x22, max 0x40 == 64Byte long.

	// Redefinir o registro de IRQ.
	//writeRegister(REG_IRQ_FLAGS, 0xFF); // Definir 0x12 para 0xFF.
	writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t)0x00); // Limpe a máscara.
	writeRegister(REG_IRQ_FLAGS, (uint8_t)IRQ_LORA_TXDONE_MASK); // Defina 0x12 para 0x08.

	// 16. Inicie a transmissão real do FiFo.
	opmode(OPMODE_TX); // Defina 0x01 como 0x03 (o valor real se torna 0x83).

} // txLoraModem

// ---------------------------------------------------------------------------------------------------------
// Configure o receptor LoRa no transceptor conectado.
// - Determine o tipo de transceptor correto (sx1272 / RFM92 ou sx1276 / RFM95).
// - Defina a frequência para ouvir (lembre-se de 1 canal).
// - Defina Fator de espalhamento (padrão SF7).
// O pino RST de reset pode não ser necessário para pelo menos o transceptor RGM95.
//
// 1. Coloque o rádio no modo LoRa.
// 2. Coloque o modem em repouso ou em espera.
// 3. Definir Frequência.
// 4. Fator Distribuidor.
// 5. Definir máscara de interrupção.
// 6. Limpar todos os sinalizadores de interrupção.
// 7. Defina opmode para OPMODE_RX.
// 8. Definir _state para S_RX.
// 9. Redefinir todas as interrupções.
// ---------------------------------------------------------------------------------------------------------

void rxLoraModem()
{
	// 1. Coloque o sistema no modo LoRa.
	//opmode(OPMODE_LORA);

	// 2. Coloque o rádio no modo de suspensão.
	opmode(OPMODE_STANDBY);
	//opmode(OPMODE_SLEEP); // Defina 0x01 para 0x00.

	// 3. Definir frequência com base no valor em freq.
	setFreq(freqs[ifreq]); // Definido como 868.1MHz (Alterado).

	// 4. Definir fator de propagação e CRC.
	setRate(sf, 0x04);

	// Impedir a comunicação do nó para o nó.
	writeRegister(REG_INVERTIQ, (uint8_t)0x27); // 0x33, 0x27; para redefinir a partir de TX.

	// O tamanho máximo da carga depende do buffer de 256 bytes.
  // Na inicialização, o TX começa em 0x80 e o RX em 0x00. RX, portanto, maximizado em 128 bytes.
  // Para o TX, temos que definir o PAYLOAD_LENGTH.
	//writeRegister(REG_PAYLOAD_LENGTH, (uint8_t) PAYLOAD_LENGTH); // Defina 0x22, 0x40==64Byte long.

	// Definir proteção CRC ou proteção de carga MAX.
	//writeRegister(REG_MAX_PAYLOAD_LENGTH, (uint8_t) MAX_PAYLOAD_LENGTH);	// Defina 0x23 para 0x80==128.

	// Definir o endereço inicial para o FiFO (que deve ser 0).
	writeRegister(REG_FIFO_ADDR_PTR, (uint8_t)readRegister(REG_FIFO_RX_BASE_AD)); // Defina 0x0D para 0x0F (contém 0x00);

	// Amplificador de baixo ruído usado no receptor.
	writeRegister(REG_LNA, (uint8_t)LNA_MAX_GAIN); // 0x0C, 0x23.

	// Não aceite interrupções, exceto RXDONE, RXTOUT e RXCRC.
	writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t) ~(IRQ_LORA_RXDONE_MASK | IRQ_LORA_RXTOUT_MASK | IRQ_LORA_CRCERR_MASK));

	// Defina o salto de frequência.
	if (_hop)
	{
#if DUSB >= 1
		if (debug >= 1)
		{
			Serial.print(F("rxLoRaModem:: Hop, canal = "));
			Serial.println(ifreq);
		}
#endif
		writeRegister(REG_HOP_PERIOD, 0x01); // 0x24, 0x01 estava 0xFF.
		// Definir interrupção RXDONE para dio0.
		writeRegister(REG_DIO_MAPPING_1, (uint8_t)(MAP_DIO0_LORA_RXDONE | MAP_DIO1_LORA_RXTOUT | MAP_DIO1_LORA_FCC));
	}
	else
	{
		writeRegister(REG_HOP_PERIOD, 0x00); // 0x24, 0x00 estava 0xFF.
		// Definir a interrupção RXDONE para dio0.
		writeRegister(REG_DIO_MAPPING_1, (uint8_t)(MAP_DIO0_LORA_RXDONE | MAP_DIO1_LORA_RXTOUT));
	}

	// Defina o opmode para recebimento único ou contínuo. O primeiro é usado quando
  // cada mensagem pode vir em um SF diferente, o segundo quando tivermos corrigido SF.
	if (_cad)
	{
	  // cad Configuração do scanner, configure _state para S_RX.
    // Definir modo de recepção único, entra no modo STANDBY após o recebimento.
		_state = S_RX;
		opmode(OPMODE_RX_SINGLE); // 0x80 | 0x06 (ouça uma mensagem).
	}
	else
	{
		// Defina o modo de recepção contínua, útil se ficarmos em um SF.
		_state = S_RX;
		opmode(OPMODE_RX); // 0x80 | 0x05 (ouça)
	}

	// 9. Limpe todos os sinalizadores de IRQ de rádio.
	writeRegister(REG_IRQ_FLAGS, 0xFF);

	return;
} // rxLoraModem

// ---------------------------------------------------------------------------------------------------------
// function cadScanner ()
//
// O CAD Scanner fará a varredura no canal fornecido para um sinal válido de símbolo/preâmbulo.
// Então, ao invés de receber contínuo em uma dada combinação de canal/sf
// vamos esperar no canal em questão e procurar por um preâmbulo. Uma vez recebido
// vamos configurar o radio para o SF com o melhor rssi (indicando recepção nesse sf).
// A função define o _state para S_SCAN
//
// NOTA: Não defina a frequência aqui, mas use o alimentador de frequência.
// ---------------------------------------------------------------------------------------------------------
void cadScanner()
{
	// 1. Coloque o sistema no modo LoRa (que destrói todos os outros nós.
	//opmode (OPMODE_LORA);

	// 2. Coloque o rádio no modo de suspensão.
	opmode(OPMODE_STANDBY); // Era valor antigo.
  //opmode(OPMODE_SLEEP);	// defina 0x01 para 0x00.

  // Como podemos voltar do S_TX com outras frequências e SF.
  // redefine ambos para bons valores para o cadScanner.

	// 3. Defina a frequência com base no valor em freq. // XXX Novo, pode ser necessário ao receber para baixo.
#if DUSB >= 2
	if ((debug >= 1) && (ifreq > 9))
	{
		Serial.print(F("cadScanner:: E Freq="));
		Serial.println(ifreq);
		if (debug >= 2)
			Serial.flush();
		ifreq = 0;
	}
#endif
	setFreq(freqs[ifreq]);

	// Para cada vez que nós determinamos o scanner, configuramos o SF para o valor inicial.
	sf = SF7; // Nós fazemos SF mais baixo, isso é mais rápido!

	// 4. Definir fator de propagação e CRC.
	setRate(sf, 0x04);

	// Ouça a LORA_MAC_PREAMBLE.
	writeRegister(REG_SYNC_WORD, (uint8_t)0x34); // Defina reg 0x39 para 0x34.

	// Defina as interrupções que queremos ouvir top.
	writeRegister(REG_DIO_MAPPING_1,
				  (uint8_t)(MAP_DIO0_LORA_CADDONE | MAP_DIO1_LORA_CADDETECT | MAP_DIO2_LORA_NOP | MAP_DIO3_LORA_NOP));

	// Defina a máscara para interrupções (não queremos ouvir), exceto para:
	writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t) ~(IRQ_LORA_CDDONE_MASK | IRQ_LORA_CDDETD_MASK | IRQ_LORA_CRCERR_MASK));

	// Defina o opMode para CAD.
	opmode(OPMODE_CAD);

	// Limpe todas as interrupções relevantes.
	//writeRegister(REG_IRQ_FLAGS, (uint8_t) 0xFF ); // Pode funcionar melhor, limpar TODAS as interrupções.

	// Se estamos aqui. ou podemos ter definido o SF ou temos um tempo limite em que
  // caso o recebimento seja iniciado da mesma forma normal.
	return;
} // cadScanner

// ---------------------------------------------------------------------------------------------------------
// Primeira inicialização do modem LoRa.
// Alterações subsequentes no estado do modem, etc. feitas por txLoraModem ou rxLoraModem.
// Após a inicialização, o modem é colocado no modo rx (listen).
// ---------------------------------------------------------------------------------------------------------
void initLoraModem()
{
	_state = S_INIT;
	// Redefinir o chip do transceptor com um pulso de 10 mSec.
#ifdef ESP32BUILD
	digitalWrite(pins.rst, LOW);
	delayMicroseconds(10000);
	digitalWrite(pins.rst, HIGH);
	delayMicroseconds(10000);
#else
	digitalWrite(pins.rst, HIGH);
	delayMicroseconds(10000);
	digitalWrite(pins.rst, LOW);
	delayMicroseconds(10000);
#endif
	digitalWrite(pins.ss, HIGH);

	// Verifique a versão do chip primeiro.
	uint8_t version = readRegister(REG_VERSION); // Leia o ID da versão do chip LoRa.
	if (version == 0x22)
	{
		// sx1272
#if DUSB >= 2
		Serial.println(F("AVISO:: SX1272 detectado."));
#endif
		sx1272 = true;
	}
	else if (version == 0x12)
	{
		// sx1276?
#if DUSB >= 2
		if (debug >= 1)
			Serial.println(F("SX1276 iniciando."));
#endif
		sx1272 = false;
	}
	else
	{
#if DUSB >= 2
		Serial.print(F("Transceptor desconhecido = "));
		Serial.println(version, HEX);
#endif
		die("");
	}

	// 2. Definir rádio para dormir.
	opmode(OPMODE_SLEEP); // defina o registrador 0x01 para 0x00.

	// 1 Set LoRa Mode
	opmode(OPMODE_LORA); // defina o registrador 0x01 para 0x80.

	// 3. Definir frequência com base no valor em freq.
	ifreq = 0;
	freq = freqs[0];
	setFreq(freq); // definido como 868.1MHz (Alterado).

	// 4. Defina o fator de espalhamento.
	setRate(sf, 0x04);

	// Amplificador de baixo ruído usado no receptor.
	writeRegister(REG_LNA, (uint8_t)LNA_MAX_GAIN); // 0x0C, 0x23.

	// 7. Definir palavra de sincronização.
	writeRegister(REG_SYNC_WORD, (uint8_t)0x34); // defina 0x39 para 0x34 LORA_MAC_PREAMBLE.

	// Impedir a comunicação do nó para o nó.
	writeRegister(REG_INVERTIQ, 0x27); // 0x33, 0x27; para redefinir a partir do TX.

	// O tamanho máximo da carga depende do buffer de 256 bytes. Na inicialização TX começa em
  // 0x80 e RX em 0x00. RX, portanto, maximizado em 128 bytes.
	writeRegister(REG_MAX_PAYLOAD_LENGTH, MAX_PAYLOAD_LENGTH); // defina 0x23 para 0x80 == 128 bytes.
	writeRegister(REG_PAYLOAD_LENGTH, PAYLOAD_LENGTH); // 0x22, 0x40==64Byte long.

	writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_BASE_AD)); // definir reg 0x0D para 0x0F.
	writeRegister(REG_HOP_PERIOD, 0x00); // reg 0x24, definido como 0x00 era 0xFF.
	if (_hop)
	{
		writeRegister(REG_HOP_CHANNEL, 0x00); // reg 0x1C, definido como 0x00.
	}

	// 5. Config PA Ramp up time // definir reg 0x0A.
	writeRegister(REG_PARAMP, (readRegister(REG_PARAMP) & 0xF0) | 0x08); // definir tempo de aceleração PA 50 uSec.

	// Configura 0x4D PADAC para SX1276; O registro XXX é 0x5a para sx1272.
	writeRegister(REG_PADAC_SX1276, 0x84); // defina 0x4D (PADAC) para 0x84.
	//writeRegister(REG_PADAC, readRegister(REG_PADAC)|0x4);

	// Redefinir a máscara de interrupção, ative todas as interrupções.
	writeRegister(REG_IRQ_FLAGS_MASK, 0x00);

	// 9. Limpar todos os sinalizadores de IRQ de rádio.
	writeRegister(REG_IRQ_FLAGS, 0xFF);
} // initLoraModem

// ---------------------------------------------------------------------------------------------------------
// manipulador stateMachine da máquina de estado.
// Usamos uma máquina de estado para todos os tipos de interrupções.
// Isso garante que tomemos a ação correta ao receber uma interrupção.
//
// MÁQUINA DE ESTADO
// O programa usa a seguinte máquina de estados (em _state), todos os estados
// são feitos na rotina de interrupção, somente o acompanhamento do S-RXDONE é feito
// no programa principal loop(). Isso ocorre porque, de outra forma, o processamento de interrupção
// levaria muito tempo para terminar.
//
// S-INIT = 0, Os comandos neste estado são executados apenas uma vez.
// - Ir para o S_SCAN
//
// S-SCAN, parte do CadScanner ()
// - Após CDDECT (int1) ir para S_RX,
// - Após CDDONE (int0) ter S_CAD, percorrer todos os SF até CDDETD.
// - Else fica no estado SCAN.
//
// S-CAD,
// - Após CDDECT (int1) ir para S_RX,
// - Após CDDONE (int0) ir ao S_SCAN, comece com o reconhecimento do SF7 novamente.
//
// S-RX, CDDECT recebido, mensagem detectada, ciclo de RX iniciado.
// - Após a leitura do pacote RXDONE (int0). Se ler ok continue,
// - sobre RXTOUT (int1) goto S_SCAN.
//
// A leitura do buffer S-RXDONE está concluída.
// - Aguarde a leitura em loop().
// - Na mensagem enviada para o servidor goto S_SCAN.
//
// S-TX Transmitindo uma mensagem.
// - No TX, vá para o S_SCAN.
//
// S-TXDONE Transmissão completada por loop() agora novamente em interrupção.
// - Definir a máscara.
// - Redefine as bandeiras.
// - Ir para SCAN ou RX.
//
// Esta rotina de interrupção foi mantida tão simples e curta quanto possível.
// Se recebermos uma interrupção que não esteja abaixo de um estado _, então imprimamos um erro.
//
// NOTA: Podemos limpar a interrupção, mas deixar a bandeira no momento.
// O eventHandler deve cuidar da reparação de sinalizadores entre interrupções.
// ---------------------------------------------------------------------------------------------------------

void stateMachine()
{
	// Fazer uma espécie de mutex usando uma variável volátil.
#if MUTEX_INT == 1
	if (!GetMutex(&inIntr))
	{
#if DUSB >= 1
		if (debug >= 0)
		{
			Serial.println(F("eInt:: Mutex - Exclusão mútua"));
			if (debug >= 2)
				Serial.flush();
		}
#endif
		return;
	}
#endif
	// Determine quais flags de interrupção estão definidas.
	uint8_t flags = readRegister(REG_IRQ_FLAGS);
	uint8_t mask = readRegister(REG_IRQ_FLAGS_MASK);
	uint8_t intr = flags & (~mask); // Reaja apenas em interrupções não mascaradas.
	uint8_t rssi;

	if (intr == 0x00)
	{
#if DUSB >= 1
		// Algo estranho aconteceu: Houve um evento e não temos um valor para interrupção.
		if (debug >= 1)
			Serial.println(F("stateMachine:: NO intr - não temos um valor para interrupção."));
#endif

			// Talvez espere um pouco antes de redefinir todos.
#if MUTEX_INT == 1
		ReleaseMutex(&inIntr);
#endif
		//_state = S_SCAN;
		writeRegister(REG_IRQ_FLAGS, 0xFF); // Limpar TODAS as interrupções.
		_event = 0;
		return;
	}

	// Máquina de estado pequena dentro do manipulador de interrupção, pois as próximas ações dependem do estado em que estamos.
	switch (_state)
	{

	// ---------------------------------------------------------------------------------------------------------
	// Se o estado for init, estamos iniciando.
  // A função initLoraModem() já é chamada ini setup();
	case S_INIT:
#if DUSB >= 2
		if (debug >= 1)
		{
			Serial.println(F("S_INIT"));
		}
#endif
		// Novo estado, necessário para iniciar o rádio (para S_SCAN)
		writeRegister(REG_IRQ_FLAGS, 0xFF); // Limpar TODAS as interrupções.
		break;

	// ---------------------------------------------------------------------------------------------------------
	// Em S_SCAN nós medimos um RSSI alto isso significa que existe (provavelmente) uma mensagem
  // chegando a essa frequência. Mas não necessariamente no SF atual.
  // Se assim for, encontre o SF correto com o CDDETD.
	case S_SCAN:
		// Intr=IRQ_LORA_CDDETD_MASK.
		// Detectamos uma mensagem nesta freqüência e SF ao escanear.
    // Nós limpamos o CDDETD e o CDDONE e mudamos para o estado de leitura.
		if (intr & IRQ_LORA_CDDETD_MASK)
		{
#if DUSB >= 2
			if (debug >= 3)
			{
				Serial.println(F("SCAN:: CADDETD, "));
			}
#endif

			_state = S_RX; // Definir estado para receber.
			opmode(OPMODE_RX_SINGLE); // defina reg 0x01 como 0x06.

			// Definir interrupção RXDONE para dio0, RXTOUT para dio1.
			writeRegister(REG_DIO_MAPPING_1, (MAP_DIO0_LORA_RXDONE | MAP_DIO1_LORA_RXTOUT));

			// Como o novo estado é S_RX, não aceite interrupções, exceto RXDONE ou RXTOUT.
			writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t) ~(IRQ_LORA_RXDONE_MASK | IRQ_LORA_RXTOUT_MASK));

			delayMicroseconds(RSSI_WAIT_DOWN); // Espere alguns microsegundos menos.
			// A partir da versão 5.0.1, o tempo de espera depende do SF.
			// Assim, para o SF12, esperamos mais (2 ^ 7 == 128 uSec) e para o SF7 4 uSec.
			//delayMicroseconds( (0x01 << ((uint8_t)sf - 5 )) );
			rssi = readRegister(REG_RSSI); // Leia o RSSI.
			_rssi = rssi; // Leia o RSSI na variável de estado.

			writeRegister(REG_IRQ_FLAGS, 0xFF); // Redefinir todos os sinalizadores de interrupção.
		}										//if

		// CDDONE
		// Recebemos um CDDONE int dizendo que recebemos uma mensagem sobre isso frequência e possivelmente em um de seus SF.
    // Se assim for, mudamos para o estado do CAD, onde apenas ouvimos o evento CDDETD.
		else if (intr & IRQ_LORA_CDDONE_MASK)
		{

			opmode(OPMODE_CAD);

			rssi = readRegister(REG_RSSI); // Leia o RSSI.

			// Escolhemos o RSSI genérico como um mecanismo de classificação para pacotes/mensagens.
      // O pRSSI (pacote RSSI) é calculado após a recepção bem-sucedida da mensagem.
      // Portanto, esperamos que este valor faça pouco sentido no momento com o CDDONE.
      // Defina o rssi tão baixo quanto o piso de ruído. Valores inferiores não são reconhecidos então.
			if (rssi > RSSI_LIMIT)
			{ // Está definido para 37 (27/08/2017).
				_state = S_CAD; // XXX invocar o manipulador de interrupção novamente?
			}

			// Limpe o sinalizador CADDONE.
			//writeRegister(REG_IRQ_FLAGS, IRQ_LORA_CDDONE_MASK);
			writeRegister(REG_IRQ_FLAGS, 0xFF);
		}

		// Se não CDDETC e não CDDONE e sf == 12 nós temos que hop.
		else if ((_hop) && (sf == 12))
		{
			hop();
			sf = (sf_t)7;
		}

		// Se não mudarmos para o S_CAD, temos que pular.
    // Em vez de esperar por uma interrupção, fazemos isso com base no temporizador (mais regular).
		else
		{
			_state = S_SCAN;
			cadScanner();
			writeRegister(REG_IRQ_FLAGS, 0xFF);
		}
		// mais continue a digitalizar.
		break; // S_SCAN

	// ---------------------------------------------------------------------------------------------------------
	// S_CAD: No modo CAD, nós escaneamos cada SF por um RSSI alto até termos um DETECT.
  // Motivo é que recebemos uma interrupção do CADDONE, então sabemos que uma mensagem foi recebida na frequência, mas pode estar em outro SF.
  //
  // Se a mensagem tiver a frequência correta e SF, IRQ_LORA_CDDETD_MSAK interromper é gerado, indicando que podemos começar a ler a mensagem da SPI.
  //
  // DIO0 interrompe IRQ_LORA_CDDONE_MASK no estado S_CAD == 2 significa que podemos ter um bloqueio no Freq mas não no SF correto. Então aumentamos o SF.
	case S_CAD:

		// Intr=IRQ_LORA_CDDETD_MASK
		// Temos que configurar o sf com base em um RSSI forte para este canal.
		if (intr & IRQ_LORA_CDDETD_MASK)
		{

			_state = S_RX; // Definir estado para começar a receber.
			opmode(OPMODE_RX_SINGLE); // configure reg 0x01 para 0x06, inicie o LER (a leitura).

			// Defina a interrupção RXDONE para dio0, RXTOUT para dio1.
			writeRegister(REG_DIO_MAPPING_1, (MAP_DIO0_LORA_RXDONE | MAP_DIO1_LORA_RXTOUT));

			// Não aceite interrupções, exceto RXDONE ou RXTOUT.
			writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t) ~(IRQ_LORA_RXDONE_MASK | IRQ_LORA_RXTOUT_MASK));

			delayMicroseconds(RSSI_WAIT_DOWN); // Espere alguns microsegundos menos.
			//delayMicroseconds( (0x01 << ((uint8_t)sf - 5 )) );
			rssi = readRegister(REG_RSSI); // Leia o RSSI.
			_rssi = rssi; // Leia o RSSI na variável de estado.

			//writeRegister(REG_IRQ_FLAGS, IRQ_LORA_CDDETD_MASK | IRQ_LORA_RXDONE_MASK);
			writeRegister(REG_IRQ_FLAGS, 0xFF); // Redefinir todos os sinalizadores de interrupção Detectar CAD.
		}										// CDDETD

		// Intr == CADDONE
		// Então nós digitalizamos este SF e se não for alto o suficiente ... próximo.
		else if (intr & IRQ_LORA_CDDONE_MASK)
		{
			// Se isso não for SF12, incremente o SF e tente novamente.
      // Esperamos que em outro SF receba CDDETD.
			if (((uint8_t)sf) < SF12)
			{
				sf = (sf_t)((uint8_t)sf + 1); // XXX Isso significaria SF7 nunca usado.
				setRate(sf, 0x04); // Definir SF com CRC == on.

				opmode(OPMODE_CAD); // Modo de digitalização.

				delayMicroseconds(RSSI_WAIT);
				rssi = readRegister(REG_RSSI); // Leia o RSSI.

				// Redefinir sinalizadores de interrupção para o CAD Done.
				writeRegister(REG_IRQ_FLAGS, IRQ_LORA_CDDONE_MASK | IRQ_LORA_CDDETD_MASK);
				//writeRegister(REG_IRQ_FLAGS, 0xFF );	// XXX isso impedirá que o CDDETD seja lido.
			}
			// Se chegarmos ao SF12, devemos voltar ao estado SCAN.
			else
			{
				if (_hop)
				{
					hop();
				} // Se HOP começarmos na próxima frequência.
				_state = S_SCAN;
				cadScanner(); // Que irá redefinir SF para SF7.
				// Repor as interrupções.
				writeRegister(REG_IRQ_FLAGS, IRQ_LORA_CDDONE_MASK);
			}
		} //CADDONE

		// se esta interrupção não é CDECT ou CDDONE, então a interrupção é
    // é desconhecido neste estado. Então, nós limpamos a interrupção e damos um aviso.
		else
		{
#if DUSB >= 2
			if (debug >= 1)
			{
				Serial.println(F("CAD:: Interrupção desconhecida."));
			}
#endif
			_state = S_SCAN;
			cadScanner();
			writeRegister(REG_IRQ_FLAGS, (uint8_t)0xFF); // Redefinir todas as interrupções.
		}
		break; //S_CAD

	// ---------------------------------------------------------------------------------------------------------
	// Se recebermos uma interrupção no estado dio0 == S_RX.
  // deve ser uma interrupção RxDone.
  // Então devemos lidar com a mensagem recebida.
	case S_RX:

		if (intr & IRQ_LORA_RXDONE_MASK)
		{

			// Temos que verificar o erro de CRC, que será visível APÓS o RXDONE ser definido.
      // Erros de CRC podem indicar que a recepção não está correta.
      // Pode ser um erro de CRC ou uma mensagem muito grande.
      // Verificação de erros de CRC requer DIO3
			if (intr & IRQ_LORA_CRCERR_MASK)
			{
#if DUSB >= 2
				Serial.println(F("CRC erro"));
				if (debug >= 2)
					Serial.flush();
#endif
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

				writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t)0x00); // Reponha a máscara de interrupção.
				// Redefinir interrupções.
				writeRegister(REG_IRQ_FLAGS, (uint8_t)(IRQ_LORA_CRCERR_MASK | IRQ_LORA_RXDONE_MASK | IRQ_LORA_RXTOUT_MASK));
				break;
			}

			if ((LoraUp.payLength = receivePkt(LoraUp.payLoad)) <= 0)
			{
#if DUSB >= 1
				if (debug >= 0)
				{
					Serial.println(F("sMachine:: Erro S-RX"));
				}
#endif
			}

			// Fazer todo o processamento de registro nesta seção (interrupção).
			uint8_t value = readRegister(REG_PKT_SNR_VALUE); // 0x19;
			if (value & 0x80)
			{ // O bit de sinal SNR é 1.

				value = ((~value + 1) & 0xFF) >> 2; // Inverta e divida por 4.
				LoraUp.snr = -value;
			}
			else
			{
				// Divida por 4.
				LoraUp.snr = (value & 0xFF) >> 2;
			}

			LoraUp.prssi = readRegister(REG_PKT_RSSI); // Leia o registrador 0x1A, pacote rssi.

			// Correção do valor de RSSI baseado no chip usado.
			if (sx1272)
			{ // É um rádio sx1272?
				LoraUp.rssicorr = 139;
			}
			else
			{ // Provavelmente SX1276 ou RFM95.
				LoraUp.rssicorr = 157;
			}

			LoraUp.sf = readRegister(REG_MODEM_CONFIG2) >> 4;

			if (receivePacket() <= 0)
			{ // ler não é bem sucedido.
#if DUSB >= 1
				Serial.println(F("sMach:: Erro no recebimendo do Pacote."));
#endif
			}
#if DUSB >= 2
			else if (debug >= 2)
			{
				Serial.println(F("sMach:: recebimento do Pacote OK."));
			}
#endif

			// Configure o modem para receber ANTES de voltar ao espaço do usuário.
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
			writeRegister(REG_IRQ_FLAGS, 0xFF); // Reponha a máscara de interrupção.
		}

		// Receba o tempo limite da mensagem.
		else if (intr & IRQ_LORA_RXTOUT_MASK)
		{

			// Define o modem para a próxima ação de recebimento. Isso deve ser feito antes
      // a varredura ocorre porque não podemos fazer isso uma vez que o RXDETTD esteja configurado.
			if (_cad)
			{
				// Defina o estado para a digitalização CAD.
				_state = S_SCAN;
				cadScanner(); // Inicie o scanner após o RXTOUT.
			}
			else
			{
				_state = S_RX; // XXX 170828, por quê?
				rxLoraModem();
			}
			writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t)0x00);
			writeRegister(REG_IRQ_FLAGS, (uint8_t)0xFF); // Redefinir todas as interrupções.
		}

		// A interrupção recebida não é RXDONE nem RXTOUT.
    // portanto, reiniciamos a sequência de varredura (captura tudo).
		else
		{
#if DUSB >= 2
			if (debug >= 3)
			{
				Serial.println(F("S_RX:: Não RXDONE/RXTOUT, "));
			}
#endif
			initLoraModem(); // Repor toda a comunicação, 3.
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
			writeRegister(REG_IRQ_FLAGS_MASK, 0x00); // Redefinir todas as máscaras.
			writeRegister(REG_IRQ_FLAGS, 0xFF); // Redefinir todas as interrupções.
		}
		break; // S_RX

	// ---------------------------------------------------------------------------------------------------------
  
	case S_TX:

		// Iniciar a transmissão do buffer (no espaço de interrupção).
		// Nós reagimos em TODAS as interrupções se estamos no estado de TEXAS.

		txLoraModem(
			LoraDown.payLoad,
			LoraDown.payLength,
			LoraDown.tmst,
			LoraDown.sfTx,
			LoraDown.powe,
			LoraDown.fff,
			LoraDown.crc,
			LoraDown.iiq);

#if DUSB >= 2
		if (debug >= 0)
		{
			Serial.println(F("S_TX, "));
		}
#endif
		_state = S_TXDONE;
		writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t)0x00);
		writeRegister(REG_IRQ_FLAGS, (uint8_t)0xFF); // Redefinir sinalizadores de interrupção.

		break; // S_TX

	// ---------------------------------------------------------------------------------------------------------
	// Após a transmissão ser completada pelo hardware,
  // a interrupção TXDONE é levantada nos dizendo que a tranmissão foi bem sucedida.
  // Se recebermos uma interrupção no dio0 _state == S_TX, é uma interrupção do TxDone.
  // Não faça nada com a interrupção, é apenas uma indicação.
  // sendPacket alterna de volta para o modo scanner depois que a transmissão terminar OK.
	case S_TXDONE:
		if (intr & IRQ_LORA_TXDONE_MASK)
		{
#if DUSB >= 1
			Serial.println(F("TXFeito interromper."));
#endif
			// Após a transmissão, reinicie o receptor.
			if (_cad)
			{
				// Defina o estado para a digitalização CAD.
				_state = S_SCAN;
				cadScanner(); // Inicie o scanner após o ciclo de TX.
			}
			else
			{
				_state = S_RX;
				rxLoraModem();
			}

			writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t)0x00);
			writeRegister(REG_IRQ_FLAGS, (uint8_t)0xFF); // Redefinir sinalizadores de interrupção.
#if DUSB >= 1
			if (debug >= 1)
			{
				Serial.println(F("TXFeito tratado."));
				if (debug >= 2)
					Serial.flush();
			}
#endif
		}
		else
		{
#if DUSB >= 1
			if (debug >= 0)
			{
				Serial.println(F("TXFeito interrupção desconhecida."));
				if (debug >= 2)
					Serial.flush();
			}
#endif
		}
		break; // S_TXDONE

	// ---------------------------------------------------------------------------------------------------------
	// If _STATE está em estado indefinido.
  // Se tal coisa acontecer, devemos reinicializar a interface e certifique-se de que pegaremos a próxima interrupção.
	default:
#if DUSB >= 2
		if (debug >= 2)
		{
			Serial.print("E state=");
			Serial.println(_state);
		}
#endif
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
		writeRegister(REG_IRQ_FLAGS, (uint8_t)0xFF); // Repor todas as interrupções.
		break;
	}

#if MUTEX_INT == 1
	ReleaseMutex(&inIntr);
#endif
	_event = 0;
	return;
}

// ---------------------------------------------------------------------------------------------------------
// Interruptor_0 Manipulador.
// Ambas as interrupções DIO0 e DIO1 são mapeadas no GPIO15. Se nós temos que olhar
// a interrupção sinaliza para ver quais interrupções são chamadas.
//
// NOTE: Este método pode funcionar não tão bem quanto usar apenas mais pinos GPIO
// o ESP8266 mcu. Mas na prática funciona bem o suficiente.
// ---------------------------------------------------------------------------------------------------------
void ICACHE_RAM_ATTR Interrupt_0()
{
	_event = 1;
}

// ---------------------------------------------------------------------------------------------------------
// Interromper o manipulador para DIO1 com valor alto.
// Como DIO0 e DIO1 podem ser multiplexados em um manipulador de interrupções GPIO
// (como fazemos) temos que tomar cuidado apenas para chamar a Interrupção correta_x
// manipulador e limpe as interrupções correspondentes para esse dio.
// NOTA: Certifique-se de que toda a comunicação Serial seja apenas para o nível de depuração 3 e superior.
// Manipulador para:
// - CDDETD.
// - RXTIMEOUT.
// - (apenas erro RXDONE).
// ---------------------------------------------------------------------------------------------------------
void ICACHE_RAM_ATTR Interrupt_1()
{
	_event = 1;
}

// ---------------------------------------------------------------------------------------------------------
// Canal de salto de frequência (FHSS) dio2.
// ---------------------------------------------------------------------------------------------------------
void ICACHE_RAM_ATTR Interrupt_2()
{
	_event = 1;
}
