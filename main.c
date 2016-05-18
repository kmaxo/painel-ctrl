/*
* CtrlBomba_V_3_0.c
*
* Created: 04/02/2016 07:30:00
* Author : kmaxo
*/

// build 0518
// todo : versao 3
// todo : V3.01 * Erros na selecao do motor, seleção de motor
// Inclusao do mapa de portas 


// Buid 18-01 sem problema
// todo : versao 3
// todo : V3 * Montar um mapa para ser passado como Modbus e/ou TXT plano
// todo : V3 * Qtdes de partida do motor1
// todo : V3 * Qtdes de partida do motor2
// todo : V3 * faltas de Agua no reservatorio inferior
// todo : V3 * Faltas de fluxo de agua no sistema
// todo : V3
// todo : Versao 2.0 - novas funcionalidades Solicitadas
// Todo : V2 * Criar um sensor nivel superior e um sensor nivel inferior
// Todo : V2 * Se o sensor nivel inferior armar, parar todas as bombas
// Todo : V2 * Qdo os 2 motores estiverem ativos, nenhum liga no Manual
// Todo : V2 * Incluir o sensor de Fluxo
// Todo : V2 * MUDANCAS NA IHM
// Todo : V2 * No led sensor teremos : (apagado, caixas cheias e tudo ok, acesso falta dagua no reservatorio superior, bomba funcionando,
// Todo : V2 *   piscando .. falta agua no reservatorio inferior
// Todo : V2 * No motor, se o led de sensor estiver acesso, falta agua no reservatorio superior , e nao tem fluxo na tubulação depois da bomba
// Todo : V2 *   Significa que o motor nao esta mandando agua...
// Todo : V2 *
// Todo : V2.1 * - Falha na selecao de motores
// Todo : V2 * Motor1 e motor 2 nao estao sendo selecionados de acordo no IHM
// Todo : V2 *

/* Requerimentos
* Mapa das portas
* porta D
*   Bit 0 - nao usado
*   Bit 1 - nao usado
*   Bit 2 - Liga/Desliga Motor 1
*   Bit 3 - Liga/Desliga Motor 2
*   Bit 4 - Automatico Ligado (Acesso Automatico / Apagado Manual )
*   Bit 5 - Falta de Agua no reservatorio inferior
*   Bit 6 - Indicador Motor 1 (Apagado Inativo, Acesso Ligado, piscando Ativo porem desligado)
*   Bit 7 - Indicador Motor 2 (Apagado Inativo, Acesso Ligado, piscando Ativo porem desligado)
* porta B -
*   Bit 0 - Superior
*   Bit 1 - Inferior
*   Bit 2 - NA
*   Bit 3 - Motor 1
*   Bit 4 - Motor 2
* porta C - Lado Analogico (ao lado do CI)
*   Bit 0 - Modo Automatico/Manual

*/


#define F_CPU 16000000
#define MANUAL 0
#define AUTOMATICO 1
#define CHEIO 1
#define VAZIO 0
#define DESLIGADO 0
#define LIGADO 1
#define MOTOR1 0
#define MOTOR2 1
#define CONFIDENCE_LEVEL 50
#define BOTAO_CONFIDENCE_LEVEL 10
#define M_CONFIDENCE_LEVEL 10
#define MUDOU 1
#define NAOMUDOU 0

#include <avr/io.h>
#include <util/delay.h>

// Variaveis Globais
unsigned char m1_ativo=0, m1_ligado, m2_ativo=0, m2_ligado = 0, modo = 0, fluxo = 0, ns = 0 , ni = 0, tempo_fluxo=0, pisca = 0;
unsigned char ihm = 0;
unsigned char n_pos=4;
unsigned char motor = 0;
unsigned char ns_confidence_level = 0, ni_confidence_level = 0;
unsigned char ns_release_level = 0, ni_release_level = 0;
unsigned char m1_select_level = 0, m2_select_level = 0;
unsigned char m1_libera_level = 0, m2_libera_level = 0;
unsigned char modo_confidence_level = 0, modo_release_level = 0;
unsigned char sensor_mudou=MUDOU;
unsigned char botao_mudou=MUDOU;


/*
* m1_ligado    - motor desligado(0), ligado (1)
* m2_ligado    - motor desligado(0), ligado (1)
* m1_Ativo     - motor em manutenção(0), ativo (1)
* m2_ativo     - motor em manutenção(0), ativo (1)
* modo  - Modo manual(0), Automatico (1)
* fluxo - sem fluxo de agua(0), com fluxo de agua (1)
* na - sensor na Aberto(0), sensor na fechado (1)
* nf - sensor nf fechado(0), sensor nf aberto (1)
* tempo - Tempo para ser considerado sem fluxo de agua
* pisca - Mascara para pisca IHM
*/

void AvaliaBotoes(); // Avalia os botoes modo, m1, m2
void AvaliaSensores(); // Avalia sensores ns e ni
void AvaliaFluxo(); // Avalia se temos fluxo de agua apos 10 segundos
void AtualizaIHM();
void MostraIHM();
void LigaMotor();
void DesligaMotor();
void ExecutaProcesso() ; // Apos a avaliação de sensores e motores, atualiza as saidas M1 e M2
void ExecutaAutomatico() ; // Executa modo automtico
void ExecutaManual() ; // Executa Modo Manual

int main(void)
{
	/* Replace with your application code */
	DDRD = 0b11111100; // 0 - Para Entrada, 1 para saida
	DDRB = 0b00100000; // 1 - para o led 13 da placa arduino
	DDRC = 0b00000000; // Para a Porta analogica

	PORTD = 0b00000000; // todos os leds desligados
	PORTB = 0b00011111; // todas as chaves ligadas em 5v
	PORTC = 0b00000001; // Somente o modo manual automtico como chave
	
	DesligaMotor();
	
	while (1)
	{
		AvaliaBotoes();
		AvaliaSensores();
		
		if (modo == AUTOMATICO)
		{ // modo Automatico
			ExecutaAutomatico();
		} else
		{
			// Modo Manual
			ExecutaManual();
		}
		if ((sensor_mudou == MUDOU) ||
		(botao_mudou == MUDOU) )
		{
			ExecutaProcesso();
			sensor_mudou = NAOMUDOU;
			botao_mudou = NAOMUDOU;
		}
		//
		//  Atualiza IHM, sensores e motores
		AtualizaIHM();

		MostraIHM();
		
	}
}

void AvaliaBotoes() // Avalia os botoes modo, m1, m2
{

	unsigned char modo_antigo = modo;
	unsigned char m1_ativo_antigo = m1_ativo;
	unsigned char m2_ativo_antigo = m2_ativo;
	
	//botao_mudou = NAOMUDOU;
	
	if (bit_is_clear(PINC, 0)){ // Verifica modo
		if (modo == AUTOMATICO) {
			modo_release_level = 0;
			modo_confidence_level ++;
			if (modo_confidence_level > BOTAO_CONFIDENCE_LEVEL)
			{
				modo = MANUAL; // Manual
				modo_confidence_level = 0;
			}
		}
		} else {
		if (modo == MANUAL)
		{
			modo_release_level++;
			modo_confidence_level = 0;
			if (modo_release_level > BOTAO_CONFIDENCE_LEVEL)
			{
				modo = AUTOMATICO; // Automtico
				modo_release_level = 0;
			}
		}

	}
	if bit_is_clear(PINB, PINB3) { // Verifica motor 1
		if (m1_ativo == LIGADO) {
			m1_select_level = 0;
			m1_libera_level++;
			if (m1_libera_level > M_CONFIDENCE_LEVEL) {
				m1_ativo = DESLIGADO; // motor desligado
				motor = MOTOR2;
				m1_libera_level=0;
			}
		}
		} else {
		if (m1_ativo == DESLIGADO) {
			m1_libera_level = 0;
			m1_select_level++;
			if (m1_select_level > M_CONFIDENCE_LEVEL) {
				m1_ativo = LIGADO;
				m1_select_level=0;
			}
		}

	}


	if bit_is_clear(PINB, PINB4) { // valida motor 2
		if (m2_ativo == LIGADO) {
			m2_select_level = 0;
			m2_libera_level++;
			if (m2_libera_level > M_CONFIDENCE_LEVEL) {
				m2_ativo = DESLIGADO; // motor desligado
				motor = MOTOR1;
				m2_libera_level=0;
			}
		}
		} else {
		if (m2_ativo == DESLIGADO) {
			m2_libera_level = 0;
			m2_select_level++;
			if (m2_select_level > M_CONFIDENCE_LEVEL) {
				m2_ativo = LIGADO;
				m2_select_level=0;
			}
		}
	}

	if ((m1_ativo == DESLIGADO) && (m2_ativo == DESLIGADO) ) {
		motor = 2;
		DesligaMotor();
	}

	if ((modo_antigo != modo) ||
	(m1_ativo_antigo != m1_ativo) ||
	(m2_ativo_antigo != m2_ativo)
	)
	{
		botao_mudou = MUDOU;
	}
	if (modo_antigo != modo)
	{
		DesligaMotor();
	}


}

void AvaliaSensores() // Avalia sensores na e nf
{

	// sensor_mudou = NAOMUDOU;
	if (bit_is_clear(PINB, PINB0)) // nivel superior
	{
		ns_confidence_level++;
		if (ns_confidence_level > CONFIDENCE_LEVEL)
		{
			if (ns == VAZIO)
			{
				ns = CHEIO;
				ns_release_level = 0;
				sensor_mudou = MUDOU;
			}
			ns_confidence_level = 0;
		}
	} else
	{
		ns_release_level++;
		if (ns_release_level > CONFIDENCE_LEVEL)
		{
			if (ns == CHEIO)
			{
				ns = VAZIO;
				ns_confidence_level = 0;
				sensor_mudou = MUDOU;
			}
			ns_release_level = 0;
		}
	}


	if (bit_is_clear(PINB, PINB1)) { // nivel Inferior
		ni_confidence_level++;
		if (ni_confidence_level > CONFIDENCE_LEVEL) {
			if (ni == VAZIO) {
				ni = CHEIO;
				ni_release_level = 0;
				sensor_mudou = MUDOU;
			}
			ni_confidence_level = 0;
		}
	} else
	{
		ni_release_level++;
		if (ni_release_level > CONFIDENCE_LEVEL)
		{
			if (ni == CHEIO)
			{
				ni = VAZIO;
				ni_confidence_level = 0;
				sensor_mudou = MUDOU;
			}
			ni_release_level = 0;
		}
	}

}

void AvaliaFluxo() // Avalia se temos fluxo de agua apos 10 segundos
{
	// todo : Avaliando o fluxo de agua apos um periodo
	// if fluxo mais 10 seg
	fluxo = LIGADO;
	// else
	//fluxo = 0;


}
void MostraIHM()
{
	PORTD &= 0b00001111; // zera todos os leds
	//	ihm = 1 << n_pos;
	//	n_pos ++;
	//	if (n_pos > 7) {
	//		n_pos = 2;
	//	}
	PORTD |= ihm;
	PORTB ^= 1<< PINB5;
	_delay_ms(100);
	
	
}
void AtualizaIHM()
{

	ihm = PORTD;
	if (modo == MANUAL) {
		ihm &= 0b11101111;
		} else {
		ihm |= 1 << 4;
	}
	
	if (m1_ativo == LIGADO) {
		if (m1_ligado == LIGADO)
		{
			ihm |= 0b01000000;
		}
		else
		{
			ihm ^= 1<< 6;
		}
	}
	else
	{
		ihm &= 0b10111111;
	}
	if (m2_ativo == LIGADO) {
		if (m2_ligado == LIGADO)
		{
			ihm |= 0b10000000;
		}
		else
		{
			ihm ^= 1<< 7;
		}
	}
	else
	{
		ihm &= 0b01111111;
	}
	
	
	if ((ns == CHEIO) &&
	(ni == CHEIO))
	{
		ihm &= 0b11011111;
	}
	else
	{
		if ((ns == VAZIO) && (ni == CHEIO))
		{
			ihm |= 1<< 5;
		}
		else
		{
			if (ni == VAZIO)
			{
				ihm ^= 1 << 5; // pisca o led qdo o reservatorio inferior estiver vazio
			}
			
		}
		
	}
	

	
}
void LigaMotor()
{
	if (motor == MOTOR1) { // Liga motor 1
		m1_ligado = LIGADO;
		m2_ligado = DESLIGADO;
	}
	if (motor == MOTOR2) {
		m1_ligado = DESLIGADO;
		m2_ligado = LIGADO;
	}
}
void DesligaMotor()
{
	m1_ligado = DESLIGADO;
	m2_ligado = DESLIGADO;
	motor++;
	if (motor > MOTOR2) {
		motor = MOTOR1;
	}
	if ((motor == MOTOR1) && (m1_ativo == DESLIGADO)) { // m1 selecionado mas inativo, muda para 2
		motor = MOTOR2;
	}
	if ((motor == MOTOR2) && (m2_ativo == DESLIGADO)) { // m2 selecionado mas inativo muda para 1
		motor = MOTOR1;
	}
	if ((m1_ativo == DESLIGADO) && (m2_ativo == DESLIGADO)) { // se ambos estao inativos, nao liga nada motor 2 = erro
		motor = 2;
	}
	
}

void ExecutaProcesso()
{
	if (m1_ligado == LIGADO) {
		PORTD |= 1<< PIND2;
		} else {
		PORTD &= 0b11111011;
	}
	if (m2_ligado == LIGADO) {
		PORTD |= 1<< PIND3;
		} else {
		PORTD &= 0b11110111;
	}
}

void ExecutaAutomatico()  // Executa modo automtico
{
	if (sensor_mudou == MUDOU)
	{
		if (ni == CHEIO)
		{
			if (ns == VAZIO)
			{
				LigaMotor();
			} else
			{
				DesligaMotor();
			}
		} else
		{ // ni  vazio
			DesligaMotor();
		}
	}
	
	
	
}
void ExecutaManual()  // Executa Modo Manual
{
	/* Modo Manual
	* 1) So liga se houver agua no reservatorio inferior
	* 2) se os 2 motores estiverem ativos, desliga os motores
	* 3)
	*/
	if (ni == VAZIO)// faltar agua no reservaotrio inferior desliga o motor
	{
		DesligaMotor();
	}
	else
	{
		if (((m1_ativo == LIGADO) && (m2_ativo == LIGADO)) ||
		((m1_ativo == DESLIGADO) && (m2_ativo == DESLIGADO)) ) // por default, sempre desliga os motores
		{
			DesligaMotor();
		}
		else
		{
			LigaMotor(); // liga o motor ativo
		}
		
	}
	
}