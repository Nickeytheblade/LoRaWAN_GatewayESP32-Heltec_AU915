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
// podem ser definidas em (= 1) ou desativadas (= 0).
// A desvantagem do tempo de compilação é menor em comparação com o ganho
// de memória de não ter código demais compilado e carregado em seu ESP8266.
// =========================================================================================================

// Definição do registro de configuração lido na inicialização e escrito quando as configurações são alteradas.
struct espGwayConfig
{
	uint16_t fcnt;   // =0 como o valor do init XXX Pode ser de 32 bits.
	uint16_t boots;  // Número de reinicializações feitas pelo gateway após a redefinição.
	uint16_t resets; // Número de estatísticas redefinidas.
	uint16_t views;  // Número de chamadas sendWebPage().
	uint16_t wifis;  // Número de configurações de WiFi.
	uint16_t reents; // Número de chamadas do manipulador de interrupção reentrantes.
	uint16_t ntpErr; // Número de solicitações UTP que falharam.
	uint16_t ntps;

	uint32_t ntpErrTime; // Registre a hora do último erro NTP.
	uint8_t ch;	// Indexa para a matriz de freqs, freqs [ifreq] = 868100000 padrão (Alterado).
	uint8_t sf; // Varia de SF7 a SF12.
	uint8_t debug; // Faixa 0 a 4.

	bool cad;	// o CAD está ativado?
	bool hop;	// O HOP está ativado (Observação: ele deve ser desativado).
	bool isNode; // O nó do gateway está ativado.
	bool refresh; // A atualização do navegador da Web está ativada?

	String ssid; // SSID da última rede WiFi conectada.
	String pass; // Senha.
} gwayConfig;
