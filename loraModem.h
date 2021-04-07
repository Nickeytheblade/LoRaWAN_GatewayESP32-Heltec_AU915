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
// Este arquivo contém várias configurações e declarações em tempo de compilação
// específico para o rádio LoRa rfm95, sx1276, sx1272 do gateway.
// =========================================================================================================

// Nosso código deve corrigir o tempo do servidor.
long txDelay = 0x00; // Tempo de atraso no topo do servidor TMST.

#define SPISPEED 8000000 // Estava 50000/50KHz < 10MHz.

// Frequências:
// Definir a frequência central. Em caso de dúvida, escolha o primeiro, comente todos os outros
// Cada gateway "real" deve suportar as primeiras 3 frequências de acordo com a especificação LoRa.
// NOTA: Isso significa que você deve especificar pelo menos 3 frequências aqui para o gateway de canal único funcionar.

#if _LFREQ==868
// Este é o formato EU868 usado na maior parte da Europa.
// Também é o padrão para a maioria do trabalho de gateway de canal único.
int freqs [] = { 
	868100000, 									// Channel 0, 868.1 MHz/125 primary
	868300000, 									// Channel 1, 868.3 MHz mandatory
	868500000, 									// Channel 2, 868.5 MHz mandatory
	867100000, 									// Channel 3, 867.1 MHz Optional
	867300000, 									// Channel 4, 867.3 MHz Optional
	867500000,  								// Channel 5, 867.5 MHz Optional
	867700000,  								// Channel 6, 867.7 MHz Optional 
	867900000,  								// Channel 7, 867.9 MHz Optional 
	868800000,   								// Channel 8, 868.9 MHz/125 Optional
	869525000									  // Channel 9, 869.5 MHz/125 for RX2 responses SF9(10%)
	// O TTN define um canal adicional a 869.525Mhz usando o SF9 para a classe B. Não usado.
};
#elif _LFREQ==433
// As seguintes 3 frequências devem ser definidas/usadas em um ambiente EU433.
int freqs [] = {
	433175000, 									// Channel 0, 433.175 MHz/125 primary
	433375000, 									// Channel 1, 433.375 MHz primary
	433575000, 									// Channel 2, 433.575 MHz primary
	433775000, 									// Channel 3, 433.775 MHz primary
	433975000, 									// Channel 4, 433.975 MHz primary
	434175000, 									// Channel 5, 434.175 MHz primary
	434375000, 									// Channel 6, 434.375 MHz primary
	434575000, 									// Channel 7, 434.575 MHz primary
	434775000 									// Channel 8, 434.775 MHz primary
};
#elif _LFREQ==915
// US902=928
/*
int freqs [] = {
	// Uplink
	903900000, 									// Channel 0, SF7BW125 to SF10BW125 primary
	904100000, 									// Ch 1, SF7BW125 to SF10BW125
	904300000, 									// Ch 2, SF7BW125 to SF10BW125
	904500000, 									// Ch 3, SF7BW125 to SF10BW125
	904700000, 									// Ch 4, SF7BW125 to SF10BW125
	904900000, 									// Ch 5, SF7BW125 to SF10BW125
	905100000, 									// Ch 6, SF7BW125 to SF10BW125
	905300000, 									// Ch 7, SF7BW125 to SF10BW125
	904600000 									// Ch 8, SF8BW500 
	// Downlink
	// Devemos especificar frequências de downlink aqui.											
											        // SFxxxBW500
};
*/

/*
 * PLANO DE FREQUÊNCIAS
 * US902-928
 * Usado nos EUA, Canadá e América do Sul.
 * 
 * Uplink:
 * 903.9 - SF7BW125 to SF10BW125
 * 904.1 - SF7BW125 to SF10BW125
 * 904.3 - SF7BW125 to SF10BW125
 * 904.5 - SF7BW125 to SF10BW125
 * 904.7 - SF7BW125 to SF10BW125
 * 904.9 - SF7BW125 to SF10BW125
 * 905.1 - SF7BW125 to SF10BW125
 * 905.3 - SF7BW125 to SF10BW125
 * 904.6 - SF8BW500
 * 
 * Downlink: 
 * 923.3 - SF7BW500 to SF12BW500
 * 923.9 - SF7BW500 to SF12BW500
 * 924.5 - SF7BW500 to SF12BW500
 * 925.1 - SF7BW500 to SF12BW500
 * 925.7 - SF7BW500 to SF12BW500
 * 926.3 - SF7BW500 to SF12BW500
 * 926.9 - SF7BW500 to SF12BW500
 * 927.5 - SF7BW500 to SF12BW500
 */

// AU915-928
int freqs [] = {
  // Uplink
  916800000,                  	// Channel 0, SF7BW125 to SF10BW125 primary
  917000000,                  	// Ch 1, SF7BW125 to SF10BW125
  917200000,                  	// Ch 2, SF7BW125 to SF10BW125
  917400000,                  	// Ch 3, SF7BW125 to SF10BW125
  917600000,                  	// Ch 4, SF7BW125 to SF10BW125
  917800000,                  	// Ch 5, SF7BW125 to SF10BW125
  918000000,                  	// Ch 6, SF7BW125 to SF10BW125
  918200000,                  	// Ch 7, SF7BW125 to SF10BW125
  917500000                   	// Ch 8, SF8BW500 
  // Downlink
  // Devemos especificar frequências de downlink aqui
				                        // SFxxxBW500
};

/*
 * PLANO DE FREQUÊNCIAS
 * AU915-928
 * 
 * Uplink:
 * 916.8 - SF7BW125 to SF10BW125
 * 917.0 - SF7BW125 to SF10BW125
 * 917.2 - SF7BW125 to SF10BW125
 * 917.4 - SF7BW125 to SF10BW125
 * 917.6 - SF7BW125 to SF10BW125
 * 917.8 - SF7BW125 to SF10BW125
 * 918.0 - SF7BW125 to SF10BW125
 * 918.2 - SF7BW125 to SF10BW125
 * 917.5 SF8BW500
 * 
 * Downlink:
 * 923.3 - SF7BW500 to SF12BW500
 * 923.9 - SF7BW500 to SF12BW500
 * 924.5 - SF7BW500 to SF12BW500
 * 925.1 - SF7BW500 to SF12BW500
 * 925.7 - SF7BW500 to SF12BW500
 * 926.3 - SF7BW500 to SF12BW500
 * 926.9 - SF7BW500 to SF12BW500
 * 927.5 - SF7BW500 to SF12BW500
 * 
 * Note que The Things Network usa apenas a segunda sub-banda (canais 8 a 15 e 65).
 * Você precisará programar os canais específicos nos dispositivos para que eles funcionem com o TTN.
 */

#else
int freqs [] = {
			// Imprimir um erro, não suportado.
#error "Desculpe, mas o seu plano de frequência não é suportado."
};
#endif
uint32_t  freq = freqs[0];
uint8_t	 ifreq = 0;	// Índice de Canal.

// Definir a estrutura do fator de espalhamento.
enum sf_t
{
	SF6 = 6,
	SF7,
	SF8,
	SF9,
	SF10,
	SF11,
	SF12
};

// O estado do receptor. Veja a folha de dados Semtech (rev 4, março de 2015) página 43.
// O _state é do tipo enum (e deve ser convertido quando usado como um número).
enum state_t
{
	S_INIT = 0,
	S_SCAN,
	S_CAD,
	S_RX,
	S_TX,
	S_TXDONE
};

volatile state_t _state;
volatile uint8_t _event = 0;

// rssi é medido em momentos específicos e relatado em outros,
// então precisamos armazenar o valor atual que gostamos de trabalhar.
uint8_t _rssi;

// Para tornar o comportamento do CAD dinâmico, definimos uma variável
// quando as funções do CAD são definidas. Valor de 3 é frequências mínimas a
// o gateway deve suportar para ser totalmente compatível com o LoRa.
#define NUM_HOPS 3

bool _cad = (bool)_CAD; // Defina como verdadeiro para Detecção de atividade do canal, somente quando o dio 1 estiver conectado.
bool _hop = false; // Experimental; salto de frequência. Use somente quando o dio2 estiver conectado.
bool inHop = false;
unsigned long nowTime = 0;
unsigned long hopTime = 0;
unsigned long msgTime = 0;

#if _PIN_OUT == 1
// Definição dos pinos GPIO usados pelo Gateway para placas tipo Hallard.
struct pins
{
	uint8_t dio0 = 15; // GPIO15  / D8. Para o conselho de Hallard compartilhado entre DIO0/DIO1/DIO2.
	uint8_t dio1 = 15; // GPIO15  / D8. Usado para CAD, pode ou não ser compartilhado com DIO0.
	uint8_t dio2 = 15; // GPIO15  / D8. Usado para salto de frequência, não se importe.
	uint8_t ss = 16;   // GPIO16  / D0. Selecione o pino conectado a GPIO16 / D0.
	uint8_t rst = 0;   // GPIO0   / D3. Repor o pino não usado.
					           // MISO 12 / D6.
					           // MOSI 13 / D7.
					           // CLK  14 / D5.
} pins;
#elif _PIN_OUT == 2
// Para o PCB do gateway ComResult, use as seguintes configurações.
struct pins
{
	uint8_t dio0 = 5;  // GPIO5   / D1. Dio0 usado para uma frequência e um SF.
	uint8_t dio1 = 4;  // GPIO4   / D2. Usado para CAD, pode ou não ser compartilhado com DIO0.
	uint8_t dio2 = 0;  // GPIO0   / D3. Usado para salto de frequência, não se importe.
	uint8_t ss = 15;   // GPIO15  / D8. Selecione o pino conectado a GPIO15.
	uint8_t rst = 0;   // GPIO0   / D3. Repor o pino não usado.
} pins;
#elif _PIN_OUT == 3
// Para o PCB do gateway ComResult, use as seguintes configurações.
struct pins
{
	uint8_t dio0 = 26; // GPIO5   / D1. Dio0 usado para uma frequência e um SF.
	uint8_t dio1 = 33; // GPIO4   / D2. Usado para CAD, pode ou não ser compartilhado com DIO0.
	uint8_t dio2 = 32; // GPIO0   / D3. Usado para salto de frequência, não importa.
	uint8_t ss = 18;   // GPIO15  / D8. Selecione o pino conectado ao GPIO15.
	uint8_t rst = 14;  // GPIO0   / D3. Repor o pino não usado.
// Pin definição de WIFI LoRa 32
// HelTec Automação 2017 support@heltec.cn
#define SCK 5        // GPIO5  -- SX127x's SCK.
#define MISO 19      // GPIO19 -- SX127x's MISO.
#define MOSI 27      // GPIO27 -- SX127x's MOSI.
#define SS 18        // GPIO18 -- SX127x's CS.
#define RST 14       // GPIO14 -- SX127x's RESET.
#define DI00 26      // GPIO26 -- SX127x's IRQ(Interrupt Request).
} pins;
#else
// Use suas próprias definições de pin e descomente a linha #error abaixo.
// MISO 12 / D6
// MOSI 13 / D7
// CLK  14 / D5
// SS   16 / D0
#error "Definições de PIN _PIN_OUT deve ser 1 (HALLARD) ou 2 (COMRESULT)"
#endif

// STATR contém os statictis que são mantidos por mensagem.
// A cada hora que uma mensagem é recebida ou enviada, as estatísticas são atualizadas.
// No caso de STATISTICS == 1, definimos as últimas mensagens MAX_STAT como estatísticas.
struct stat_t
{
	unsigned long tmst; // Tempo desde 1970 em milissegundos.
	unsigned long node; // DEVaddr de 4 bytes (o único conhecido pelo gateway).
	uint8_t ch; // Índice do canal para a matriz de frequências.
	uint8_t sf;
#if RSSI == 1
	int8_t rssi; // XXX Pode ser < -128.
#endif
	int8_t prssi; // XXX Pode ser < -128.
} stat_t;

#if STATISTICS >= 1
// Histórico de mensagens de uplink recebidas de nós.
struct stat_t statr[MAX_STAT];

// STATC contém a estatística que é relacionada ao gateway e não por mensagem.
// Exemplo: Número de mensagens recebidas no SF7 ou número de (re) botas.
// Então, onde statr contém as estatísticas reunidas por pacote, o statc contém estática geral do nó.
#if STATISTICS >= 2 // Somente se explicitamente configuramos mais.
struct stat_c
{
	unsigned long sf7;  // Fator de espalhamento 7.
	unsigned long sf8;  // Fator de espalhamento 8.
	unsigned long sf9;  // Fator de espalhamento 9.
	unsigned long sf10; // Fator de espalhamento 10.
	unsigned long sf11; // Fator de espalhamento 11.
	unsigned long sf12; // Fator de espalhamento 12.

	uint16_t boots; // Número de reinicios.
	uint16_t resets;
} stat_c;
struct stat_c statc;
#endif
#else // STATISTICS==0
struct stat_t statr[1]; // Sempre tenha pelo menos um elemento para armazenar.
#endif

// Definir a estrutura de payload usada para separar as interrupções e as SPI.
// processamento da parte loop().
uint8_t payLoad[128]; // Carga útil i.
struct LoraBuffer
{
	uint8_t *payLoad;
	uint8_t payLength;
	uint32_t tmst;
	uint8_t sfTx;
	uint8_t powe;
	uint32_t fff;
	uint8_t crc;
	uint8_t iiq;
} LoraDown;

// Up buffer (do Lora para o UDP).
struct LoraUp
{
	uint8_t payLoad[128];
	uint8_t payLength;
	int prssi;
	long snr;
	int rssicorr;
	uint8_t sf;
} LoraUp;

// ---------------------------------------------------------------------------------------------------------
// Usado por REG_PAYLOAD_LENGTH para definir o recebimento do payload len.
#define PAYLOAD_LENGTH 0x40	// 64 bytes.
#define MAX_PAYLOAD_LENGTH 0x80 // 128 bytes.

// Não altere essas configurações para detecção de RSSI. Eles são usados para CAD.
// Dado o fator de correção de 157, podemos chegar a -120dB com essa classificação.
#define RSSI_LIMIT 37 // Estava 39.
#define RSSI_LIMIT_DOWN 34 // Estava 34.

// Quanto tempo esperar no modo LoRa antes de usar o valor RSSI.
// Este período deve ser o mais curto possível, mas suficiente.
#define RSSI_WAIT 15 // Foi 100 obras, 50 obras, 40 obras, 25 obras.
#define RSSI_WAIT_DOWN 10 // Foram 75 obras, 35 obras, 30 obras, 20 obras.

// =========================================================================================================
// Defina todas as definições para o Gateway.
// =========================================================================================================
// Registra definições. Estes são os endereços do TFM95, SX1276 que precisamos definir no programa.

#define REG_FIFO 0x00
#define REG_OPMODE 0x01
// O registro 2 a 5 não é usado para o LoRa.
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PAC 0x09
#define REG_PARAMP 0x0A
#define REG_LNA 0x0C
#define REG_FIFO_ADDR_PTR 0x0D
#define REG_FIFO_TX_BASE_AD 0x0E
#define REG_FIFO_RX_BASE_AD 0x0F

#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS_MASK 0x11
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_PKT_SNR_VALUE 0x19
#define REG_PKT_RSSI 0x1A // último pacote.
#define REG_RSSI 0x1B // RSSI atual, seção 6.4 ou 5.5.5.
#define REG_HOP_CHANNEL 0x1C
#define REG_MODEM_CONFIG1 0x1D
#define REG_MODEM_CONFIG2 0x1E
#define REG_SYMB_TIMEOUT_LSB 0x1F

#define REG_PAYLOAD_LENGTH 0x22
#define REG_MAX_PAYLOAD_LENGTH 0x23
#define REG_HOP_PERIOD 0x24
#define REG_MODEM_CONFIG3 0x26
#define REG_RSSI_WIDEBAND 0x2C

#define REG_INVERTIQ 0x33
#define REG_DET_TRESH 0x37 // SF6.
#define REG_SYNC_WORD 0x39
#define REG_TEMP 0x3C

#define REG_DIO_MAPPING_1 0x40
#define REG_DIO_MAPPING_2 0x41
#define REG_VERSION 0x42

#define REG_PADAC 0x5A
#define REG_PADAC_SX1272 0x5A
#define REG_PADAC_SX1276 0x4D

// ---------------------------------------------------------------------------------------------------------
// opModes
#define SX72_MODE_SLEEP 0x80
#define SX72_MODE_STANDBY 0x81
#define SX72_MODE_FSTX 0x82
#define SX72_MODE_TX 0x83 // 0x80 | 0x03
#define SX72_MODE_RX_CONTINUOS 0x85

// ---------------------------------------------------------------------------------------------------------
// Constantes LMIC para registros de rádio.
#define OPMODE_LORA 0x80
#define OPMODE_MASK 0x07
#define OPMODE_SLEEP 0x00
#define OPMODE_STANDBY 0x01
#define OPMODE_FSTX 0x02
#define OPMODE_TX 0x03
#define OPMODE_FSRX 0x04
#define OPMODE_RX 0x05
#define OPMODE_RX_SINGLE 0x06
#define OPMODE_CAD 0x07

// ----------------------------------------------------------------------------------------------------------
// AMPLIFICADOR DE BAIXO RUÍDO.

#define LNA_MAX_GAIN 0x23
#define LNA_OFF_GAIN 0x00
#define LNA_LOW_GAIN 0x20

// CONF REG
#define REG1 0x0A
#define REG2 0x84

// ---------------------------------------------------------------------------------------------------------
// MC1 sx1276 RegModemConfig1
#define SX1276_MC1_BW_125 0x70
#define SX1276_MC1_BW_250 0x80
#define SX1276_MC1_BW_500 0x90
#define SX1276_MC1_CR_4_5 0x02
#define SX1276_MC1_CR_4_6 0x04
#define SX1276_MC1_CR_4_7 0x06
#define SX1276_MC1_CR_4_8 0x08
#define SX1276_MC1_IMPLICIT_HEADER_MODE_ON 0x01

#define SX72_MC1_LOW_DATA_RATE_OPTIMIZE 0x01 // Obrigatório para SF11 e SF12.

// ---------------------------------------------------------------------------------------------------------
// MC2 definições.
#define SX72_MC2_FSK 0x00
#define SX72_MC2_SF7 0x70 // SF7 == 0x07, so (SF7<<4) == SX7_MC2_SF7.
#define SX72_MC2_SF8 0x80
#define SX72_MC2_SF9 0x90
#define SX72_MC2_SF10 0xA0
#define SX72_MC2_SF11 0xB0
#define SX72_MC2_SF12 0xC0

// ---------------------------------------------------------------------------------------------------------
// MC3
#define SX1276_MC3_LOW_DATA_RATE_OPTIMIZE 0x08
#define SX1276_MC3_AGCAUTO 0x04

// ---------------------------------------------------------------------------------------------------------
// FRF
//#define FRF_MSB 0xD9 // 868.1 Mhz.
//#define FRF_MID 0x06
//#define FRF_LSB 0x66

#define FRF_MSB 0xE1 // 916.8 Mhz /61.035 = 15007782 = 0xE50026 or
#define FRF_MID 0xF9 // 903.9 MHz /61.035 = 14809535 = 0xE1F9BF
#define FRF_LSB 0xBF

// ---------------------------------------------------------------------------------------------------------
// DIO mapeamentos de funções D0D1D2D3.
#define MAP_DIO0_LORA_RXDONE 0x00     // 00------ bit 7 e 6.
#define MAP_DIO0_LORA_TXDONE 0x40     // 01------
#define MAP_DIO0_LORA_CADDONE 0x80    // 10------
#define MAP_DIO0_LORA_NOP 0xC0	      // 11------

#define MAP_DIO1_LORA_RXTOUT 0x00     // --00---- bit 5 e 4
#define MAP_DIO1_LORA_FCC 0x10        // --01----
#define MAP_DIO1_LORA_CADDETECT 0x20  // --10----
#define MAP_DIO1_LORA_NOP 0x30		    // --11----

#define MAP_DIO2_LORA_FCC0 0x00       // ----00-- bit 3 e 2
#define MAP_DIO2_LORA_FCC1 0x04       // ----01-- bit 3 e 2
#define MAP_DIO2_LORA_FCC2 0x08       // ----10-- bit 3 e 2
#define MAP_DIO2_LORA_NOP 0x0C        // ----11-- bit 3 e 2

#define MAP_DIO3_LORA_CADDONE 0x00    // ------00 bit 1 e 0
#define MAP_DIO3_LORA_NOP 0x03        // ------11

#define MAP_DIO0_FSK_READY 0x00       // 00------ (pacote enviado / payload pronto).
#define MAP_DIO1_FSK_NOP 0x30         // --11----
#define MAP_DIO2_FSK_TXNOP 0x04       // ----01--
#define MAP_DIO2_FSK_TIMEOUT 0x08     // ----10--

// ---------------------------------------------------------------------------------------------------------
// Bits mascarando os IRQs correspondentes do rádio.
#define IRQ_LORA_RXTOUT_MASK 0x80
#define IRQ_LORA_RXDONE_MASK 0x40
#define IRQ_LORA_CRCERR_MASK 0x20
#define IRQ_LORA_HEADER_MASK 0x10
#define IRQ_LORA_TXDONE_MASK 0x08
#define IRQ_LORA_CDDONE_MASK 0x04
#define IRQ_LORA_FHSSCH_MASK 0x02
#define IRQ_LORA_CDDETD_MASK 0x01

// ---------------------------------------------------------------------------------------------------------
// Definições para a mensagem UDP que chega do servidor.
#define PROTOCOL_VERSION 0x01
#define PKT_PUSH_DATA 0x00
#define PKT_PUSH_ACK 0x01
#define PKT_PULL_DATA 0x02
#define PKT_PULL_RESP 0x03
#define PKT_PULL_ACK 0x04
#define PKT_TX_ACK 0x05

#define MGT_RESET 0x15 // Não é uma mensagem de especificação do Gateway LoRa.
#define MGT_SET_SF 0x16
#define MGT_SET_FREQ 0x17
