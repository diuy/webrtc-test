#pragma once
#include "rtc_base/win32.h"

class VideoPlayerGDI {
public:
	VideoPlayerGDI(HWND hwnd);
	virtual ~VideoPlayerGDI();


	 bool Init();
	 bool Show(const BITMAPINFO& bitmapInfo, const uint8_t*buffer);
private:
	void GetRect(const BITMAPINFO& bitmapInfo,RECT * rect);
	HWND _hwnd;
	bool _init;
	HDC _hdc;
	BITMAPINFO _bitmapInfo;
};

