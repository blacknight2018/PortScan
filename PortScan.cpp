#include <WinSock2.h>
#include <Windows.h>
#include <string>
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)//关闭不安全函数警告

using namespace std;


// 全局变量:
HINSTANCE hInst;                                // 当前实例
HWND Edit_IP_hWnd = 0;
HWND Edit_Port1_hWnd = 0;
HWND Edit_Port2_hWnd = 0;
HWND Button_Start_hWnd = 0;
HWND Button_Stop_hWnd = 0;
HWND Edit_Result_hWnd = 0;
int g_flag = 0;									//停止标记 1为停止
int g_Threads = 0;								//记录线程数量的 属于共享资源
CRITICAL_SECTION lpCriticalSection;				//临界区 防止多线程对共享资源抢夺出问题

//IP和端口结构体 用于线程传参
struct IpAndPort
{
	char Ip[20];
	USHORT Port;
};


// 此代码模块中包含的函数的前向声明:
VOID				AddResult(char*);			//添加一条内容到结果编辑框
VOID				Create(HWND);				//创建窗口界面的组件 传入父窗口句柄
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void				TestDestPort(IpAndPort* t);
void				StartScandThread();

//按钮控件的ID的宏 接收消息的适合拿来判断的
#define BUTN_STAT 1
#define BUTN_STOP 2
#define MAXTHREAD 1000 //最大线程数  可以增大此参数提高扫描速度
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);


	MyRegisterClass(hInstance);

	// 执行应用程序初始化:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}


	MSG msg;

	// 主消息循环:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, 0, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = "Port";
	wcex.hIconSm = 0;

	return RegisterClassEx(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // 将实例句柄存储在全局变量中

	HWND hWnd = CreateWindowA("Port", "简易端口扫描器", WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_BORDER | WS_DLGFRAME | WS_SYSMENU | WS_GROUP | WS_POPUPWINDOW,
		CW_USEDEFAULT, 0, 340, 426, nullptr, nullptr, hInstance, nullptr);

	//因为要用到WinSock 所以需要初始化
	WSADATA wsaData;
	WORD wsaVersion = MAKEWORD(2, 2);
	if (0 != WSAStartup(wsaVersion, &wsaData))
	{
		MessageBoxA(0, "初始化网络库失败", "提示", MB_OK);

	}



	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int P1, P2, startPort = 0, i;
	char IPBuf[20] = { 0 }, Port1Buf[20] = { 0 }, Port2Buf[20] = { 0 };
	switch (message)
	{
		//当窗口创建完毕后收到这个消息
	case WM_CREATE:
	{
		Create(hWnd);//父窗口创建完 创建各种组件

		break;
	}
	case WM_COMMAND://按钮被单击的适合会接收到这个消息
	{
		switch (LOWORD(wParam))//wParam的低16位保存着控件的ID 表明是哪个触发的
		{
		case BUTN_STAT:
			g_flag = 0;
			CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)StartScandThread, NULL, NULL, NULL);
			break;
		case BUTN_STOP:

			g_flag = 1;//停止
			break;
		default:
			break;
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		const char* scandTip = "扫描地址", * scandPortTips = "扫描范围";

		TextOutA(hdc, 18, 23, scandTip, strlen(scandTip));
		TextOutA(hdc, 18, 23 + 43, scandPortTips, strlen(scandPortTips));
		TextOutA(hdc, 90 + 80 + 24, 22 + 40, "-", 1);
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


//创建组件
VOID				Create(HWND hParent)
{

	Edit_IP_hWnd = CreateWindowA("Edit", "127.0.0.1", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER, 90, 22, 212, 22, hParent, NULL, hInst, NULL);
	Edit_Port1_hWnd = CreateWindowA("Edit", "19730", WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER, 90, 22 + 40, 80, 22, hParent, NULL, hInst, NULL);
	Edit_Port2_hWnd = CreateWindowA("Edit", "19731", WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER, 90 + 80 + 52, 22 + 40, 80, 22, hParent, 0, hInst, NULL);
	Button_Start_hWnd = CreateWindowA("Button", "扫描", WS_VISIBLE | WS_CHILD, 18, 22 + 40 + 32, 80, 40, hParent, (HMENU)BUTN_STAT, hInst, NULL);
	Button_Stop_hWnd = CreateWindowA("Button", "停止", WS_VISIBLE | WS_CHILD, 18 + 22 + 40 + 32 + 10 + 30 + 70, 22 + 40 + 32, 80, 40, hParent, (HMENU)BUTN_STOP, hInst, NULL);
	Edit_Result_hWnd = CreateWindowA("Edit", "", WS_VSCROLL | WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOVSCROLL | ES_LEFT | ES_MULTILINE | ES_WANTRETURN, 18, 150, 284, 204, hParent, NULL, hInst, NULL);

}

//添加一条内容到结果EDIT
VOID				AddResult(char* Text)
{
	if (Edit_Result_hWnd)
	{
		int srcTextLen = 0, newTextLen = 0;
		char* pNewText = 0;
		srcTextLen = GetWindowTextLengthA(Edit_Result_hWnd);
		newTextLen = strlen(Text) + srcTextLen + 1 + 1 + 1;//新的编辑框内容长度等于原来的长度加上新的文本的长度
		pNewText = new char[newTextLen]();//动态分配内存
		if (pNewText)
		{
			//先把原来的文本放到缓冲区里
			GetWindowTextA(Edit_Result_hWnd, pNewText, newTextLen);
			pNewText[newTextLen - 1] = 0;

			//再接上新的文本
			strcat(pNewText, Text);
			strcat(pNewText, "\r\n");

			//重新设置
			SetWindowTextA(Edit_Result_hWnd, pNewText);

			//释放掉分配的内存

			delete[]pNewText;
		}

	}
}

//尝试与目标端口进行连接  连接成功返回真   否则失败
void				TestDestPort(IpAndPort* t)
{
	int states = 0;
	char msgBuf[50] = { 0 }, Buf2[40];
	char* DestIp = t->Ip;
	USHORT DestPort = t->Port;

	SOCKET m_Client;
	m_Client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (m_Client < 0)
	{
		return;
	}

	sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons(DestPort);
	addr.sin_addr.S_un.S_addr = inet_addr(DestIp);

	states = connect(m_Client, (const sockaddr*)& addr, sizeof(sockaddr_in));


	closesocket(m_Client);


	//192.168.1.1-1972-存在
	strcpy(msgBuf, DestIp);
	itoa(DestPort, Buf2, 10);
	strcat(msgBuf, "-");
	strcat(msgBuf, Buf2);
	if (0 == states)
		strcat(msgBuf, "-存在");
	else
		strcat(msgBuf, "-不存在");

	EnterCriticalSection(&lpCriticalSection);//进去临界区
	g_Threads--;//线程数量-1
	//把结果反馈到界面
	//如果没有的端口干脆不显示吧
	if (0 == states)
		AddResult(msgBuf);

	LeaveCriticalSection(&lpCriticalSection);
	delete t;
	return;
}
void				StartScandThread()
{
	int P1, P2, startPort = 0, i;
	char IPBuf[20] = { 0 }, Port1Buf[20] = { 0 }, Port2Buf[20] = { 0 };
	//当单击开始的时候获取编辑框的IP 和 端口号 判断合法性 再进行下一步

	GetWindowTextA(Edit_IP_hWnd, IPBuf, 20);
	GetWindowTextA(Edit_Port1_hWnd, Port1Buf, 20);
	GetWindowTextA(Edit_Port2_hWnd, Port2Buf, 20);

	//判断参数合法性

	P1 = atoi(Port1Buf);
	P2 = atoi(Port2Buf);

	if (strlen(IPBuf) < 7 || (P1 < 1 || P1>65536) || (P2 < 1 || P2>65536) || (P1 >= P2)) {
		MessageBoxA(0, "参数非法", "提示", MB_OK);
		;
	}
	//多线程扫描 

	//初始化创建一个临界区
	InitializeCriticalSection(&lpCriticalSection);

	//已开启线程数量为0
	g_Threads = 0;

	//参数没问题开始扫描
	for (startPort = P1; startPort <= P2; startPort++)
	{

		//每次开启线程前 看下当前线程的情况 不然如果端口范围太大1-65535 一直开 会出事
		do
		{
			EnterCriticalSection(&lpCriticalSection);//进去临界区
			i = g_Threads;
			LeaveCriticalSection(&lpCriticalSection);
			Sleep(1);
		} while (i > MAXTHREAD);//若当前线程数量大于这个数则等待

		//当线程数量少于200后准备开启新线程
		IpAndPort* pTmp = new IpAndPort();//申请用来传递的空间 释放工作则交给线程
		if (pTmp)
		{
			strcpy(pTmp->Ip, IPBuf);
			pTmp->Port = startPort;
			HANDLE hThread = 0;
			hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)TestDestPort, (LPVOID)pTmp, NULL, NULL);
			//如果创建线程成功了
			if (hThread != 0)
			{
				EnterCriticalSection(&lpCriticalSection);//进去临界区
				g_Threads++;//线程数量+1
				LeaveCriticalSection(&lpCriticalSection);
			}
		}
		//检查标记
		if (g_flag == 1)
		{
			break;
		}

	}

	//等待所有线程结束
	do
	{
		EnterCriticalSection(&lpCriticalSection);//进去临界区
		i = g_Threads;
		LeaveCriticalSection(&lpCriticalSection);
		Sleep(100);
	} while (i != 0);
	DeleteCriticalSection(&lpCriticalSection);
	AddResult((char*)"扫描完成");
}