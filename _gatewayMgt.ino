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
// Este arquivo contém as funções para fazer gerenciamento sobre UDP, poderíamos usar a função de mensagem
// LoRa para o sensor e Gateway em si. No entanto, as funções definidas neste arquivo não são funções de sensor
// e ativando-as através da interface LoRa adicionariam nenhum valor e tornaria o código mais complexo.
//
// Vantagem: Simples, e não há necessidade de alterações com a configuração TTN.
//
// Desvantagem: É claro que você precisa configurar sua própria função de backend
// para trocar mensagens com o gateway, pois o TTN não fará isso.
//
// XXX Mas, se necessário, podemos sempre adicionar isso mais tarde.
// =========================================================================================================

#if GATEWAYMGT == 1

#if !defined _THINGPORT
#error "The management functions needs _THINGPORT defined (and not over _TTNPORT)"
#endif

// ----------------------------------------------------------------------------
// A função gateway_mgt é chamada na função UDP Receive depois
// todas as mensagens conhecidas do LoRa Gateway são verificadas.
//
// Como parte desta função, vamos ouvir outro conjunto de mensagens
// isso é definido em loraModem.h.
// Todos os opCodes começam com 0x1y para deixar opcodes 0x00 a 0x0F para o
// protocolo de gateway puro.
//
// Formato de mensagens recebidas:
// buf [0] -buf [2], estes são 0x00 ou não se importam
// buf [3], contém opcode
// buf [4] -buf [7], contém o parâmetro max. 4 bytes.
//
// Formato de mensagem upstream:
// ----------------------------------------------------------------------------
void gateway_mgt(uint8_t size, uint8_t *buff)
{

	uint8_t opcode = buff[3];

	switch (opcode)
	{
	case MGT_RESET:
		Serial.println(F("gateway_mgt:: RESET"));
		// Não há mais parâmetros, apenas redefinir (Resetar) o gateway.
		setup(); // Chame a função de configuração de esboço.
   // Enviar confirmação ao servidor.

		break;
	case MGT_SET_SF:
		Serial.println(F("gateway_mgt:: SET SF"));
		// byte [4] contém o código SF desejado (7 para SF7 e 12 para SF12).
		break;
	case MGT_SET_FREQ:
		Serial.println(F("gateway_mgt:: SET FREQ"));
		// Byte [4] contém índice de frequência.
		break;
	default:
		Serial.print(F("gateway_mgt:: Código UDP desconhecido = "));
		Serial.println(opcode);
		return;
		break;
	}
}

#endif //GATEWAYMGT==1
