﻿#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <cstdio>

// =============================================================================
// CONSTANTES
// =============================================================================

// Valor de municao maximo gravado diretamente na memoria do processo.
// 0x0902FFF0 interpretado pelo jogo como quantidade de municao "infinita"
// (bem acima do limite normal de qualquer arma).
static const DWORD VALOR_MUNICAO_MAX = 0x0902FFF0;

// Teclas de atalho (nenhuma dessas e usada pelo RE4 classico)
static const int TECLA_SAIR    = VK_F10; // Encerrar o trainer
static const int TECLA_MUNICAO = VK_F5;  // Ligar/desligar municao infinita
static const int TECLA_FOV     = VK_F6;  // Ligar/desligar FOV alterado

// Campo de visao (FOV) em graus, armazenado como float no endereco estatico
// bio4.exe + 0x86CA1C. O jogo usa ~65.0 como padrao.
static const float FOV_PADRAO   = 65.0f;
static const float FOV_ALTERADO = 90.0f; // campo de visao mais aberto

// =============================================================================
// FUNCAO AUXILIAR: obter PID do processo pelo nome
// =============================================================================

DWORD ObterPIDPorNome(const wchar_t* nomeProcesso) {
	DWORD pid = 0;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 entrada;
		entrada.dwSize = sizeof(entrada);
		if (Process32First(snapshot, &entrada)) {
			do {
				if (_wcsicmp(entrada.szExeFile, nomeProcesso) == 0) {
					pid = entrada.th32ProcessID;
					break;
				}
			} while (Process32Next(snapshot, &entrada));
		}
		CloseHandle(snapshot);
	}
	return pid;
}

// =============================================================================
// MAIN
// =============================================================================

int main()
{
	// --- Localizar o processo do RE4 ---
	DWORD PID = ObterPIDPorNome(L"bio4.exe");
	if (PID == 0) {
		printf("Processo do Resident Evil 4 nao encontrado!\n");
		return 1;
	}

	// Abrir o processo com permissao de leitura e escrita na memoria
	HANDLE processoResidentEvil = OpenProcess(
		PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
		FALSE, PID
	);
	if (processoResidentEvil == NULL) {
		printf("Nao foi possivel abrir o processo!\n");
		return 1;
	}

	// --- Obter o endereco base do modulo bio4.exe ---
	// EnumProcessModules retorna o primeiro modulo carregado, que e sempre
	// o executavel principal (bio4.exe).
	HMODULE moduloResidentEvil = NULL;
	DWORD cbNecessario = 0;
	if (!EnumProcessModules(processoResidentEvil, &moduloResidentEvil, sizeof(moduloResidentEvil), &cbNecessario)) {
		printf("Nao foi possivel enumerar os modulos!\n");
		CloseHandle(processoResidentEvil);
		return 1;
	}

	MODULEINFO infoModuloResidentEvil;
	if (!GetModuleInformation(processoResidentEvil, moduloResidentEvil, &infoModuloResidentEvil, sizeof(infoModuloResidentEvil))) {
		printf("Nao foi possivel obter as informacoes do modulo!\n");
		CloseHandle(processoResidentEvil);
		return 1;
	}

	// memoriaBase = endereco onde bio4.exe foi carregado na RAM do processo alvo
	uintptr_t memoriaBase = (uintptr_t)infoModuloResidentEvil.lpBaseOfDll;

	// =========================================================================
	// MUNICAO INFINITA - cadeia de ponteiros (explicacao)
	//
	// O RE4 nao armazena a municao em um endereco fixo. Em vez disso, usa um
	// ponteiro intermediario que sempre aponta para a arma ATUALMENTE equipada:
	//
	//   bio4.exe + 0x870FE0  -->  [ponteiro_da_arma]  -->  +0x08 = municao
	//
	// Passo 1: ler o valor em (memoriaBase + 0x870FE0).
	//          Esse valor e um endereco que aponta para a estrutura da arma ativa.
	// Passo 2: somar +0x08 ao endereco lido.
	//          Esse offset dentro da estrutura e o campo de municao.
	// Passo 3: escrever VALOR_MUNICAO_MAX nesse campo.
	//
	// Como o ponteiro em 0x870FE0 e atualizado pelo jogo toda vez que o jogador
	// troca de arma, re-ler esse ponteiro a cada ciclo garante que QUALQUER
	// arma equipada receba municao maxima automaticamente.
	// =========================================================================
	uintptr_t enderecoArmaAtual = memoriaBase + 0x00870FE0;

	// =========================================================================
	// FOV - endereco estatico
	//
	// bio4.exe + 0x86CA1C e um endereco fixo que armazena o campo de visao
	// da camera como float. Nao requer desvio de ponteiro; basta escrever
	// diretamente nesse endereco para alterar o FOV instantaneamente.
	// =========================================================================
	uintptr_t enderecoFOV = memoriaBase + 0x86CA1C;

	// =========================================================================
	// LOOP PRINCIPAL
	// =========================================================================

	// Flags de estado de cada funcionalidade (toggle on/off)
	bool municaoAtiva = false;
	bool fovAtivo     = false;

	// Controle de debounce: evita que um unico pressionamento de tecla
	// dispare o toggle multiplas vezes (o loop roda 10x por segundo).
	bool teclaMunicaoPressionada = false;
	bool teclaFOVPressionada     = false;

	// Rastreia o estado anterior do FOV para restaurar o valor padrao
	// exatamente uma vez ao desativar.
	bool fovAtivoAnterior = false;

	printf("========================================\n");
	printf("  Trainer - Resident Evil 4 (bio4.exe)\n");
	printf("========================================\n");
	printf("  F5  = Municao Infinita  [OFF]\n");
	printf("  F6  = FOV Alterado      [OFF]\n");
	printf("  F10 = Encerrar\n");
	printf("========================================\n\n");

	while (1) {

		// --- Verificar tecla de saida (F10) ---
		if (GetAsyncKeyState(TECLA_SAIR) & 0x8000) {
			printf("\nF10 pressionado. Encerrando trainer...\n");
			break;
		}

		// --- Toggle: Municao Infinita (F5) ---
		// GetAsyncKeyState retorna o bit 15 setado enquanto a tecla esta pressionada.
		// O debounce garante que o toggle so muda na BORDA de pressionar,
		// e nao fica alternando enquanto a tecla permanece segurada.
		if (GetAsyncKeyState(TECLA_MUNICAO) & 0x8000) {
			if (!teclaMunicaoPressionada) {
				municaoAtiva = !municaoAtiva;
				teclaMunicaoPressionada = true;
				printf("Municao Infinita: %s\n", municaoAtiva ? "[ON] " : "[OFF]");
			}
		}
		else {
			teclaMunicaoPressionada = false;
		}

		// --- Toggle: FOV (F6) ---
		if (GetAsyncKeyState(TECLA_FOV) & 0x8000) {
			if (!teclaFOVPressionada) {
				fovAtivo = !fovAtivo;
				teclaFOVPressionada = true;
				printf("FOV Alterado:     %s\n", fovAtivo ? "[ON] " : "[OFF]");
			}
		}
		else {
			teclaFOVPressionada = false;
		}

		// --- Aplicar: Municao Infinita ---
		if (municaoAtiva) {
			// Re-ler o ponteiro intermediario a cada ciclo.
			// O jogo atualiza esse ponteiro ao trocar de arma, portanto
			// re-le-lo garante que a arma equipada NESTE momento receba municao maxima.
			uintptr_t ponteiroDaArma = 0;
			if (ReadProcessMemory(processoResidentEvil, (LPCVOID)enderecoArmaAtual,
				&ponteiroDaArma, sizeof(ponteiroDaArma), NULL)) {
				// ponteiroDaArma = endereco da estrutura da arma ativa.
				// Offset +0x08 dentro dessa estrutura = campo de municao.
				void* ptrMunicao = (void*)(ponteiroDaArma + 0x8);
				WriteProcessMemory(processoResidentEvil, ptrMunicao,
					&VALOR_MUNICAO_MAX, sizeof(VALOR_MUNICAO_MAX), NULL);
			}
		}

		// --- Aplicar: FOV ---
		if (fovAtivo) {
			// Sobrescrever o FOV a cada ciclo (o jogo pode tentar restaurar o valor).
			WriteProcessMemory(processoResidentEvil, (LPVOID)enderecoFOV,
				&FOV_ALTERADO, sizeof(FOV_ALTERADO), NULL);
		}
		else if (fovAtivo != fovAtivoAnterior) {
			// FOV foi desativado neste ciclo: restaurar o valor padrao uma unica vez.
			WriteProcessMemory(processoResidentEvil, (LPVOID)enderecoFOV,
				&FOV_PADRAO, sizeof(FOV_PADRAO), NULL);
		}
		fovAtivoAnterior = fovAtivo;

		// Aguarda 100ms antes do proximo ciclo (10 verificacoes por segundo)
		Sleep(100);
	}

	CloseHandle(processoResidentEvil);
	return 0;
}
