#pragma once

//���嵼����ʽ
#define CHAT_EXPORT __declspec(dllexport) 
#define CHAT_CALL __stdcall

extern "C" {
	CHAT_EXPORT void CHAT_CALL Start(HWND localWnd, HWND remoteWnd);
}
