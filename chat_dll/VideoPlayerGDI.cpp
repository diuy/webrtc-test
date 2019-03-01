#include "VideoPlayerGDI.h"


VideoPlayerGDI::VideoPlayerGDI(HWND hwnd) 
	:_hwnd(hwnd)
	, _init(false)
	, _hdc(NULL) {
	memset(&_bitmapInfo, 0, sizeof(_bitmapInfo));
}

VideoPlayerGDI::~VideoPlayerGDI() {

	if (_hdc) {
		ReleaseDC(_hwnd,_hdc);
		_hdc = NULL;
	}

	::RedrawWindow(_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

bool VideoPlayerGDI::Init() {

	if (_hwnd == NULL) {
		return false;
	}

	_hdc = GetDC(_hwnd);
	if (_hdc == NULL) {
		return false;
	}
	if (SetStretchBltMode(_hdc, STRETCH_HALFTONE) == 0) {//不设置拉伸质量差
		//LOG_DEBUG("SetStretchBltMode failed:%d", GetLastError());
	}
	_init = true;
	return true;
}

bool VideoPlayerGDI::Show(const BITMAPINFO& bitmapInfo, const uint8_t*buffer) {

	RECT rect = { 0 };
	GetRect(bitmapInfo,&rect);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;

	int ret;

	ret = StretchDIBits(_hdc, rect.left, rect.top, w, h,
		0, 0, bitmapInfo.bmiHeader.biWidth, abs(bitmapInfo.bmiHeader.biHeight), buffer,
		&bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

	
	if (ret ==0) {
		//LOG_DEBUG("StretchDIBits failed:%d", GetLastError());
		return false;
	}

	return true;
}

void VideoPlayerGDI::GetRect(const BITMAPINFO& bitmapInfo, RECT * rect) {
	GetClientRect(_hwnd, rect);
	int width = bitmapInfo.bmiHeader.biWidth;
	int height = abs(bitmapInfo.bmiHeader.biHeight);


	int i = 0;
	int w = rect->right - rect->left;
	int h = rect->bottom - rect->top;
	if (w*width > height*h) {
		i = (h*width / height);
		rect->left += (w - i) / 2;
		rect->right = i + rect->left;
	} else {
		i = (width*w / height);
		rect->top += (h - i) / 2;
		rect->bottom = i + rect->top;
	}
}
