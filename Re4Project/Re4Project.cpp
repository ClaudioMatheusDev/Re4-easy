#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <cstdio>

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

int main()
{
	DWORD PID = ObterPIDPorNome(L"bio4.exe"); // Nome do executavel do Resident Evil 4
	DWORD valorMunicao = 151126256; //Valor da municao total

	if (PID == 0) {
		printf("Processo do Resident Evil 4 nao encontrado!\n");
		return 1;
	}

	HANDLE processoResidentEvil = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, PID);
	HMODULE moduloResidetEvil = NULL;
	DWORD obrigatorio;

	if (processoResidentEvil == NULL) {
		printf("Nao foi possivel abrir o processo!\n");
		return 1;
	}

	if (EnumProcessModules(processoResidentEvil, &moduloResidetEvil, sizeof(moduloResidetEvil), &obrigatorio)) {
		MODULEINFO infoModuloResidentEvil;
		if (GetModuleInformation(processoResidentEvil, moduloResidetEvil, &infoModuloResidentEvil, sizeof(infoModuloResidentEvil))) {

			DWORD MemoriaBase = (DWORD)infoModuloResidentEvil.lpBaseOfDll;
			DWORD segundaPosicao = MemoriaBase + 0x00870FE0;
			DWORD valorSegundaPosicao;

			if (ReadProcessMemory(processoResidentEvil, (LPCVOID)segundaPosicao, &valorSegundaPosicao, sizeof(valorSegundaPosicao), NULL)) {

				int* ptr = (int*)valorSegundaPosicao;
				ptr = (int*)((char*)ptr + 0x8);

				while (1) {
					if (WriteProcessMemory(processoResidentEvil, ptr, &valorMunicao, sizeof(valorMunicao), NULL)) {
						printf("Valor da municao alterado com sucesso!\n");
					}
					else
					{
						printf("Nao foi possivel alterar o valor da memoria!\n");
						CloseHandle(processoResidentEvil);
						return 1;
					}
					Sleep(100);
				}
			}
			else
			{
				printf("Nao foi possivel ler a memoria!\n");
				CloseHandle(processoResidentEvil);
				return 1;
			}
		}
		else
		{
			printf("Nao foi possivel obter as informacoes do modulo!\n");
			CloseHandle(processoResidentEvil);
			return 1;
		}
	}
	else
	{
		printf("Nao foi possivel obter as informacoes do modulo!\n");
		CloseHandle(processoResidentEvil);
		return 1;
	}

	CloseHandle(processoResidentEvil);
	return 0;
}
