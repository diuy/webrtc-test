﻿
// chatDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "chat.h"
#include "chatDlg.h"
#include "afxdialogex.h"
#include "stdint.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include "chat_dll/chat_webrtc.h"
// ChatDlg 对话框



ChatDlg::ChatDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CHAT_DIALOG, pParent) {
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void ChatDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_LOCAL, editLocal);
	DDX_Control(pDX, IDC_EDIT_REMOTE, editRemote);
}

BEGIN_MESSAGE_MAP(ChatDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_START, &ChatDlg::OnBnClickedBtnStart)
END_MESSAGE_MAP()


// ChatDlg 消息处理程序
static HWND hwndlocal;
static HWND hwndRemote;
BOOL ChatDlg::OnInitDialog() {
	CDialogEx::OnInitDialog();
	hwndlocal = editLocal.m_hWnd;
	hwndRemote = editRemote.m_hWnd;

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void ChatDlg::OnPaint() {
	if (IsIconic()) {
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else {
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR ChatDlg::OnQueryDragIcon() {
	return static_cast<HCURSOR>(m_hIcon);
}



void ChatDlg::OnBnClickedBtnStart() {
	Start(editLocal.m_hWnd, editRemote.m_hWnd);
}
