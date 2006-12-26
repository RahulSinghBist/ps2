#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <boost/bind.hpp>
#include "../PS2VM.h"
#include "../Config.h"
#include "../MIPSAssembler.h"
#include "win32/InputBox.h"
#include "Debugger.h"
#include "resource.h"
#include "PtrMacro.h"

#define CLSNAME			_X("CDebugger")

#define WM_EXECUNLOAD	(WM_USER + 0)
#define WM_EXECCHANGE	(WM_USER + 1)

using namespace Framework;
using namespace std;
using namespace boost;

CDebugger::CDebugger()
{
	RECT rc;

	RegisterPreferences();

	if(!DoesWindowClassExist(CLSNAME))
	{
		WNDCLASSEX wc;
		memset(&wc, 0, sizeof(WNDCLASSEX));
		wc.cbSize			= sizeof(WNDCLASSEX);
		wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground	= (HBRUSH)GetStockObject(GRAY_BRUSH); 
		wc.hInstance		= GetModuleHandle(NULL);
		wc.lpszClassName	= CLSNAME;
		wc.lpfnWndProc		= CWindow::WndProc;
		RegisterClassEx(&wc);
	}
	
	SetRect(&rc, 0, 0, 640, 480);

	Create(NULL, CLSNAME, _X(""), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, &rc, NULL, NULL);
	SetClassPtr();

	SetMenu(LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_DEBUGGER)));

	CreateClient(NULL);

	//Show(SW_MAXIMIZE);

	SetRect(&rc, 0, 0, 320, 240);

	//ELF View Initialization
	m_pELFView = new CELFView(m_pMDIClient->m_hWnd);
	m_pELFView->Show(SW_HIDE);

	//Functions View Initialization
	m_pFunctionsView = new CFunctionsView(m_pMDIClient->m_hWnd, &CPS2VM::m_EE);
	m_pFunctionsView->Show(SW_HIDE);
	m_pFunctionsView->m_OnFunctionDblClick.InsertHandler(new CEventHandlerMethod<CDebugger, uint32>(this, &CDebugger::OnFunctionViewFunctionDblClick));
	m_pFunctionsView->m_OnFunctionsStateChange.InsertHandler(new CEventHandlerMethod<CDebugger, int>(this, &CDebugger::OnFunctionViewFunctionsStateChange));

	//Debug Views Initialization
	m_nCurrentView = -1;

	m_pView[DEBUGVIEW_EE]	= new CDebugView(m_pMDIClient->m_hWnd, &CPS2VM::m_EE, "EmotionEngine");
	m_pView[DEBUGVIEW_VU0]	= NULL;
	m_pView[DEBUGVIEW_VU1]	= new CDebugView(m_pMDIClient->m_hWnd, &CPS2VM::m_VU1, "Vector Unit 1");

	CPS2OS::m_OnExecutableChange.connect(bind(&CDebugger::OnExecutableChange, this));
	CPS2OS::m_OnExecutableUnloading.connect(bind(&CDebugger::OnExecutableUnloading, this));

	ActivateView(DEBUGVIEW_EE);
	LoadSettings();

	if(GetDisassemblyWindow()->IsVisible())
	{
		GetDisassemblyWindow()->SetFocus();
	}

	UpdateLoggingMenu();
	CreateAccelerators();
}

CDebugger::~CDebugger()
{
	DestroyAccelerators();

	SaveSettings();

	DELETEPTR(m_pELFView);
	DELETEPTR(m_pFunctionsView);
}

HACCEL CDebugger::GetAccelerators()
{
	return m_nAccTable;
}

void CDebugger::RegisterPreferences()
{
	CConfig* pConfig;
	pConfig = CConfig::GetInstance();

	pConfig->RegisterPreferenceInteger("debugger.log.posx",				0);
	pConfig->RegisterPreferenceInteger("debugger.log.posy",				0);
	pConfig->RegisterPreferenceInteger("debugger.log.sizex",			0);
	pConfig->RegisterPreferenceInteger("debugger.log.sizey",			0);
	pConfig->RegisterPreferenceBoolean("debugger.log.visible",			true);

	pConfig->RegisterPreferenceInteger("debugger.disasm.posx",			0);
	pConfig->RegisterPreferenceInteger("debugger.disasm.posy",			0);
	pConfig->RegisterPreferenceInteger("debugger.disasm.sizex",			0);
	pConfig->RegisterPreferenceInteger("debugger.disasm.sizey",			0);
	pConfig->RegisterPreferenceBoolean("debugger.disasm.visible",		true);

	pConfig->RegisterPreferenceInteger("debugger.regview.posx",			0);
	pConfig->RegisterPreferenceInteger("debugger.regview.posy",			0);
	pConfig->RegisterPreferenceInteger("debugger.regview.sizex",		0);
	pConfig->RegisterPreferenceInteger("debugger.regview.sizey",		0);
	pConfig->RegisterPreferenceBoolean("debugger.regview.visible",		true);

	pConfig->RegisterPreferenceInteger("debugger.memoryview.posx",		0);
	pConfig->RegisterPreferenceInteger("debugger.memoryview.posy",		0);
	pConfig->RegisterPreferenceInteger("debugger.memoryview.sizex",		0);
	pConfig->RegisterPreferenceInteger("debugger.memoryview.sizey",		0);
	pConfig->RegisterPreferenceBoolean("debugger.memoryview.visible",	true);

	pConfig->RegisterPreferenceInteger("debugger.callstack.posx",		0);
	pConfig->RegisterPreferenceInteger("debugger.callstack.posy",		0);
	pConfig->RegisterPreferenceInteger("debugger.callstack.sizex",		0);
	pConfig->RegisterPreferenceInteger("debugger.callstack.sizey",		0);
	pConfig->RegisterPreferenceBoolean("debugger.callstack.visible",	true);
}

void CDebugger::UpdateLoggingMenu()
{
	HMENU hMenu;
	MENUITEMINFO mii;
	bool nState[6];

	hMenu = GetMenu(m_hWnd);

	hMenu = GetSubMenu(hMenu, 2);

	nState[0] = CPS2VM::m_Logging.GetGSLoggingStatus();
	nState[1] = CPS2VM::m_Logging.GetDMACLoggingStatus();
	nState[2] = CPS2VM::m_Logging.GetIPULoggingStatus();
	nState[3] = CPS2VM::m_Logging.GetOSLoggingStatus();
	nState[4] = CPS2VM::m_Logging.GetSIFLoggingStatus();
	nState[5] = CPS2VM::m_Logging.GetIOPLoggingStatus();

	for(unsigned int i = 0; i < 6; i++)
	{
		memset(&mii, 0, sizeof(MENUITEMINFO));
		mii.cbSize		= sizeof(MENUITEMINFO);
		mii.fMask		= MIIM_STATE;
		mii.fState		= nState[i] ? MFS_CHECKED : 0;

		SetMenuItemInfo(hMenu, i, TRUE, &mii);
	}
}

void CDebugger::UpdateTitle()
{
	xchar sTitle[256];
	xchar sConvert[256];

	xstrcpy(sTitle, xcond("Purei! - Debugger", L"プレイ! - Debugger"));
	
	if(GetCurrentView() != NULL)
	{
		xconvert(sConvert, GetCurrentView()->GetName(), 256);
		xstrcat(sTitle, _X(" - [ "));
		xstrcat(sTitle, sConvert);
		xstrcat(sTitle, _X(" ]"));
	}

	SetText(sTitle);
}

void CDebugger::LoadSettings()
{
	LoadViewLayout();
}

void CDebugger::SaveSettings()
{
	SaveViewLayout();
}

void CDebugger::SerializeWindowGeometry(CWindow* pWindow, const char* sPosX, const char* sPosY, const char* sSizeX, const char* sSizeY, const char* sVisible)
{
	RECT rc;
	CConfig* pConfig;

	pConfig = CConfig::GetInstance();

	pWindow->GetWindowRect(&rc);
	ScreenToClient(m_pMDIClient->m_hWnd, (POINT*)&rc + 0);
	ScreenToClient(m_pMDIClient->m_hWnd, (POINT*)&rc + 1);

	pConfig->SetPreferenceInteger(sPosX, rc.left);
	pConfig->SetPreferenceInteger(sPosY, rc.top);

	if(sSizeX != NULL && sSizeY != NULL)
	{
		pConfig->SetPreferenceInteger(sSizeX, (rc.right - rc.left));
		pConfig->SetPreferenceInteger(sSizeY, (rc.bottom - rc.top));
	}

	pConfig->SetPreferenceBoolean(sVisible, pWindow->IsVisible());
}

void CDebugger::UnserializeWindowGeometry(CWindow* pWindow, const char* sPosX, const char* sPosY, const char* sSizeX, const char* sSizeY, const char* sVisible)
{
	CConfig* pConfig;

	pConfig = CConfig::GetInstance();

	pWindow->SetPosition(pConfig->GetPreferenceInteger(sPosX), pConfig->GetPreferenceInteger(sPosY));
	pWindow->SetSize(pConfig->GetPreferenceInteger(sSizeX), pConfig->GetPreferenceInteger(sSizeY));

	if(!pConfig->GetPreferenceBoolean(sVisible))
	{
		pWindow->Show(SW_HIDE);
	}
	else
	{
		pWindow->Show(SW_SHOW);
	}
}

void CDebugger::Resume()
{
	CPS2VM::Resume();
}

void CDebugger::StepCPU1()
{
	if(CPS2VM::m_nStatus == PS2VM_STATUS_RUNNING)
	{
		MessageBeep(-1);
		return;
	}
	
	if(::GetParent(GetFocus()) != GetDisassemblyWindow()->m_hWnd)
	{
		GetDisassemblyWindow()->SetFocus();
	}

	GetContext()->Step();
	CPS2VM::m_OnMachineStateChange();
}

void CDebugger::FindValue()
{
	uint32 nValue;
	const xchar* sValue;
	unsigned int i;
	Win32::CInputBox Input(_X("Find Value in Memory"), _X("Enter value to find:"), _X("00000000"));
	
	sValue = Input.GetValue(m_hWnd);
	if(sValue == NULL) return;

	xsscanf(sValue, _X("%x"), &nValue);
	if(nValue == 0) return;

	printf("Search results for 0x%0.8X\r\n", nValue);
	printf("-----------------------------\r\n");
	for(i = 0; i < CPS2VM::RAMSIZE; i += 4)
	{
		if(*(uint32*)&CPS2VM::m_pRAM[i] == nValue)
		{
			printf("0x%0.8X\r\n", i);
		}
	}
}

void CDebugger::AssembleJAL()
{
	uint32 nValueTarget, nValueAssemble;
	const xchar* sTarget;
	const xchar* sAssemble;

	Win32::CInputBox InputTarget(_X("Assemble JAL"), _X("Enter jump target:"), _X("00000000"));
	Win32::CInputBox InputAssemble(_X("Assemble JAL"), _X("Enter address to assemble JAL to:"), _X("00000000"));

	sTarget = InputTarget.GetValue(m_hWnd);
	if(sTarget == NULL) return;

	sAssemble = InputAssemble.GetValue(m_hWnd);
	if(sAssemble == NULL) return;

	xsscanf(sTarget, _X("%x"), &nValueTarget);
	xsscanf(sAssemble, _X("%x"), &nValueAssemble);

	*(uint32*)&CPS2VM::m_pRAM[nValueAssemble] = 0x0C000000 | (nValueTarget / 4);
}

void CDebugger::Layout1024()
{
	GetDisassemblyWindow()->SetPosition(0, 0);
	GetDisassemblyWindow()->SetSize(700, 435);
	GetDisassemblyWindow()->Show(SW_SHOW);

	GetRegisterViewWindow()->SetPosition(700, 0);
	GetRegisterViewWindow()->SetSize(324, 572);
	GetRegisterViewWindow()->Show(SW_SHOW);

	GetMemoryViewWindow()->SetPosition(0, 435);
	GetMemoryViewWindow()->SetSize(700, 265);
	GetMemoryViewWindow()->Show(SW_SHOW);

	GetCallStackWindow()->SetPosition(700, 572);
	GetCallStackWindow()->SetSize(324, 128);
	GetCallStackWindow()->Show(SW_SHOW);
}

void CDebugger::Layout1280()
{
	GetDisassemblyWindow()->SetPosition(0, 0);
	GetDisassemblyWindow()->SetSize(900, 540);
	GetDisassemblyWindow()->Show(SW_SHOW);

	GetRegisterViewWindow()->SetPosition(900, 0);
	GetRegisterViewWindow()->SetSize(380, 784);
	GetRegisterViewWindow()->Show(SW_SHOW);

	GetMemoryViewWindow()->SetPosition(0, 540);
	GetMemoryViewWindow()->SetSize(900, 416);
	GetMemoryViewWindow()->Show(SW_SHOW);

	GetCallStackWindow()->SetPosition(900, 784);
	GetCallStackWindow()->SetSize(380, 172);
	GetCallStackWindow()->Show(SW_SHOW);
}

void CDebugger::InitializeConsole()
{
	AllocConsole();

	CONSOLE_SCREEN_BUFFER_INFO ScreenBufferInfo;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ScreenBufferInfo);
	ScreenBufferInfo.dwSize.Y = 1000;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), ScreenBufferInfo.dwSize);

	(*stdout) = *_fdopen(_open_osfhandle(
		reinterpret_cast<intptr_t>(GetStdHandle(STD_OUTPUT_HANDLE)),
		_O_TEXT), "w");

	setvbuf(stdout, NULL, _IONBF, 0);
	ios::sync_with_stdio();	
}

void CDebugger::ActivateView(unsigned int nView)
{
	if(m_nCurrentView == nView) return;

	if(m_nCurrentView != -1)
	{
		SaveViewLayout();
		GetCurrentView()->Hide();
	}

	m_nCurrentView = nView;
	LoadViewLayout();
	UpdateTitle();

	if(GetDisassemblyWindow()->IsVisible())
	{
		GetDisassemblyWindow()->SetFocus();
	}
}

void CDebugger::SaveViewLayout()
{
	SerializeWindowGeometry(GetDisassemblyWindow(), \
		"debugger.disasm.posx", \
		"debugger.disasm.posy", \
		"debugger.disasm.sizex", \
		"debugger.disasm.sizey", \
		"debugger.disasm.visible");

	SerializeWindowGeometry(GetRegisterViewWindow(), \
		"debugger.regview.posx", \
		"debugger.regview.posy", \
		"debugger.regview.sizex", \
		"debugger.regview.sizey", \
		"debugger.regview.visible");

	SerializeWindowGeometry(GetMemoryViewWindow(), \
		"debugger.memoryview.posx", \
		"debugger.memoryview.posy", \
		"debugger.memoryview.sizex", \
		"debugger.memoryview.sizey", \
		"debugger.memoryview.visible");

	SerializeWindowGeometry(GetCallStackWindow(), \
		"debugger.callstack.posx", \
		"debugger.callstack.posy", \
		"debugger.callstack.sizex", \
		"debugger.callstack.sizey", \
		"debugger.callstack.visible");
}

void CDebugger::LoadViewLayout()
{
	UnserializeWindowGeometry(GetDisassemblyWindow(), \
		"debugger.disasm.posx", \
		"debugger.disasm.posy", \
		"debugger.disasm.sizex", \
		"debugger.disasm.sizey", \
		"debugger.disasm.visible");

	UnserializeWindowGeometry(GetRegisterViewWindow(), \
		"debugger.regview.posx", \
		"debugger.regview.posy", \
		"debugger.regview.sizex", \
		"debugger.regview.sizey", \
		"debugger.regview.visible");

	UnserializeWindowGeometry(GetMemoryViewWindow(), \
		"debugger.memoryview.posx", \
		"debugger.memoryview.posy", \
		"debugger.memoryview.sizex", \
		"debugger.memoryview.sizey", \
		"debugger.memoryview.visible");

	UnserializeWindowGeometry(GetCallStackWindow(), \
		"debugger.callstack.posx", \
		"debugger.callstack.posy", \
		"debugger.callstack.sizex", \
		"debugger.callstack.sizey", \
		"debugger.callstack.visible");
}

CDebugView* CDebugger::GetCurrentView()
{
	if(m_nCurrentView == -1) return NULL;
	return m_pView[m_nCurrentView];
}

CMIPS* CDebugger::GetContext()
{
	return GetCurrentView()->GetContext();
}

CDisAsmWnd* CDebugger::GetDisassemblyWindow()
{
	return GetCurrentView()->GetDisassemblyWindow();
}

CMemoryViewMIPSWnd* CDebugger::GetMemoryViewWindow()
{
	return GetCurrentView()->GetMemoryViewWindow();
}

CRegViewWnd* CDebugger::GetRegisterViewWindow()
{
	return GetCurrentView()->GetRegisterViewWindow();
}

CCallStackWnd* CDebugger::GetCallStackWindow()
{
	return GetCurrentView()->GetCallStackWindow();
}

void CDebugger::CreateAccelerators()
{
	ACCEL Accel[8];

	Accel[0].cmd	= ID_VM_SAVESTATE;
	Accel[0].key	= VK_F7;
	Accel[0].fVirt	= FVIRTKEY;

	Accel[1].cmd	= ID_VM_LOADSTATE;
	Accel[1].key	= VK_F8;
	Accel[1].fVirt	= FVIRTKEY;

	Accel[2].cmd	= ID_VIEW_FUNCTIONS;
	Accel[2].key	= 'F';
	Accel[2].fVirt	= FCONTROL | FVIRTKEY;

	Accel[3].cmd	= ID_VM_STEP1;
	Accel[3].key	= VK_F10;
	Accel[3].fVirt	= FVIRTKEY;

	Accel[4].cmd	= ID_VM_RESUME;
	Accel[4].key	= VK_F5;
	Accel[4].fVirt	= FVIRTKEY;

	Accel[5].cmd	= ID_VIEW_CALLSTACK;
	Accel[5].key	= 'A';
	Accel[5].fVirt	= FCONTROL | FVIRTKEY;

	Accel[6].cmd	= ID_VIEW_EEVIEW;
	Accel[6].key	= '1';
	Accel[6].fVirt	= FALT | FVIRTKEY;

	Accel[7].cmd	= ID_VIEW_VU1VIEW;
	Accel[7].key	= '3';
	Accel[7].fVirt	= FALT | FVIRTKEY;

	m_nAccTable = CreateAcceleratorTable(Accel, sizeof(Accel) / sizeof(ACCEL));
}

void CDebugger::DestroyAccelerators()
{
	DestroyAcceleratorTable(m_nAccTable);
}

long CDebugger::OnCommand(unsigned short nID, unsigned short nMsg, HWND hFrom)
{
	switch(nID)
	{
	case ID_VM_STEP1:
		StepCPU1();
		break;
	case ID_VM_RESUME:
		Resume();
		break;
	case ID_VM_SAVESTATE:
		CPS2VM::SaveState("./config/state.sta");
		break;
	case ID_VM_LOADSTATE:
		CPS2VM::LoadState("./config/state.sta");
		break;
	case ID_VM_DUMPTHREADS:
		CPS2VM::DumpEEThreadSchedule();
		break;
	case ID_VM_DUMPINTCHANDLERS:
		CPS2VM::DumpEEIntcHandlers();
		break;
	case ID_VM_DUMPDMACHANDLERS:
		CPS2VM::DumpEEDmacHandlers();
		break;
	case ID_VM_ASMJAL:
		AssembleJAL();
		break;
	case ID_VM_TEST_SHIFT:
		StartShiftOpTest();
		break;
	case ID_VM_TEST_SHIFT64:
		StartShift64OpTest();
		break;
	case ID_VM_TEST_SPLITLOAD:
		StartSplitLoadOpTest();
		break;
	case ID_VM_TEST_ADD64:
		StartAddition64OpTest();
		break;
	case ID_VM_TEST_SLT:
		StartSetLessThanOpTest();
		break;
	case ID_VM_FINDVALUE:
		FindValue();
		break;
	case ID_VIEW_MEMORY:
		GetMemoryViewWindow()->Show(SW_SHOW);
		GetMemoryViewWindow()->SetFocus();
		return FALSE;
		break;
	case ID_VIEW_CALLSTACK:
		GetCallStackWindow()->Show(SW_SHOW);
		GetCallStackWindow()->SetFocus();
		return FALSE;
		break;
	case ID_VIEW_FUNCTIONS:
		m_pFunctionsView->Show(SW_SHOW);
		m_pFunctionsView->SetFocus();
		return FALSE;
		break;
	case ID_VIEW_ELF:
		m_pELFView->Show(SW_SHOW);
		m_pELFView->SetFocus();
		return FALSE;
		break;
	case ID_VIEW_DISASSEMBLY:
		GetDisassemblyWindow()->Show(SW_SHOW);
		GetDisassemblyWindow()->SetFocus();
		return FALSE;
		break;
	case ID_VIEW_EEVIEW:
		ActivateView(DEBUGVIEW_EE);
		break;
	case ID_VIEW_VU1VIEW:
		ActivateView(DEBUGVIEW_VU1);
		break;
	case ID_LOGGING_GS:
		CPS2VM::m_Logging.SetGSLoggingStatus(!CPS2VM::m_Logging.GetGSLoggingStatus());
		UpdateLoggingMenu();
		break;
	case ID_LOGGING_DMAC:
		CPS2VM::m_Logging.SetDMACLoggingStatus(!CPS2VM::m_Logging.GetDMACLoggingStatus());
		UpdateLoggingMenu();
		break;
	case ID_LOGGING_IPU:
		CPS2VM::m_Logging.SetIPULoggingStatus(!CPS2VM::m_Logging.GetIPULoggingStatus());
		UpdateLoggingMenu();
		break;
	case ID_LOGGING_OS:
		CPS2VM::m_Logging.SetOSLoggingStatus(!CPS2VM::m_Logging.GetOSLoggingStatus());
		UpdateLoggingMenu();
		break;
	case ID_LOGGING_SIF:
		CPS2VM::m_Logging.SetSIFLoggingStatus(!CPS2VM::m_Logging.GetSIFLoggingStatus());
		UpdateLoggingMenu();
		break;
	case ID_LOGGING_IOP:
		CPS2VM::m_Logging.SetIOPLoggingStatus(!CPS2VM::m_Logging.GetIOPLoggingStatus());
		UpdateLoggingMenu();
		break;
	case ID_WINDOW_CASCAD:
		m_pMDIClient->Cascade();
		return FALSE;
		break;
	case ID_WINDOW_TILEHORIZONTAL:
		m_pMDIClient->TileHorizontal();
		return FALSE;
		break;
	case ID_WINDOW_TILEVERTICAL:
		m_pMDIClient->TileVertical();
		return FALSE;
		break;
	case ID_WINDOW_LAYOUT1024:
		Layout1024();
		return FALSE;
		break;
	case ID_WINDOW_LAYOUT1280:
		Layout1280();
		return FALSE;
		break;
	}
	return TRUE;
}

long CDebugger::OnSysCommand(unsigned int nCmd, LPARAM lParam)
{
	switch(nCmd)
	{
	case SC_CLOSE:
		Show(SW_HIDE);
		return FALSE;
	}
	return TRUE;
}

long CDebugger::OnWndProc(unsigned int nMsg, WPARAM wParam, LPARAM lParam)
{
	switch(nMsg)
	{
	case WM_EXECUNLOAD:
		OnExecutableUnloadingMsg();
		return FALSE;
		break;
	case WM_EXECCHANGE:
		OnExecutableChangeMsg();
		return FALSE;
		break;
	}
	return CMDIFrame::OnWndProc(nMsg, wParam, lParam);
}

void CDebugger::OnFunctionViewFunctionDblClick(uint32 nAddress)
{
	GetDisassemblyWindow()->SetAddress(nAddress);
	//GetDisassemblyWindow()->SetFocus();
}

void CDebugger::OnFunctionViewFunctionsStateChange(int nNothing)
{
	GetDisassemblyWindow()->Refresh();
}

void CDebugger::OnExecutableChange()
{
	SendMessage(m_hWnd, WM_EXECCHANGE, 0, 0);
}

void CDebugger::OnExecutableUnloading()
{
	SendMessage(m_hWnd, WM_EXECUNLOAD, 0, 0);
}

void CDebugger::OnExecutableChangeMsg()
{
	m_pELFView->SetELF(CPS2OS::GetELF());
	m_pFunctionsView->SetELF(CPS2OS::GetELF());

	GetDisassemblyWindow()->Refresh();
	m_pFunctionsView->Refresh();
}

void CDebugger::OnExecutableUnloadingMsg()
{
	m_pELFView->SetELF(NULL);
	m_pFunctionsView->SetELF(NULL);
}

void CDebugger::StartShiftOpTest()
{
	const int nBaseAddress = 0x00100000;

	CMIPSAssembler Assembler((uint32*)&CPS2VM::m_pRAM[nBaseAddress]);

	Assembler.LUI(CMIPS::T0, 0x8080);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x80FF);

	//SLLV
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.ADDIU(CMIPS::T1, CMIPS::R0, i);
		Assembler.SLLV(CMIPS::T2, CMIPS::T0, CMIPS::T1);
	}

	//SRAV
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.ADDIU(CMIPS::T1, CMIPS::R0, i);
		Assembler.SRAV(CMIPS::T2, CMIPS::T0, CMIPS::T1);
	}

	//SRLV
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.ADDIU(CMIPS::T1, CMIPS::R0, i);
		Assembler.SRLV(CMIPS::T2, CMIPS::T0, CMIPS::T1);
	}

	//SLL
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.SLL(CMIPS::T2, CMIPS::T0, i);
	}
	
	//SRA
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.SRA(CMIPS::T2, CMIPS::T0, i);
	}

	//SRL
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.SRL(CMIPS::T2, CMIPS::T0, i);
	}

	CPS2VM::m_EE.m_State.nPC = nBaseAddress;
}

void CDebugger::StartShift64OpTest()
{
	const int nBaseAddress = 0x00100000;

	CMIPSAssembler Assembler((uint32*)&CPS2VM::m_pRAM[nBaseAddress]);

	Assembler.LUI(CMIPS::T0, 0x8080);
	Assembler.ADDIU(CMIPS::T0, CMIPS::T0, 0xFFFF);
	Assembler.DSLL32(CMIPS::T0, CMIPS::T0, 0);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x80FF);

	//DSLLV
	for(unsigned int i = 0; i <= 64; i += 4)
	{
		Assembler.ADDIU(CMIPS::T1, CMIPS::R0, i);
		Assembler.DSLLV(CMIPS::T2, CMIPS::T0, CMIPS::T1);
	}

	//DSRAV
	//for(unsigned int i = 0; i <= 64; i += 4)
	//{
	//	Assembler.ADDIU(CMIPS::T1, CMIPS::R0, i);
	//	Assembler.DSRAV(CMIPS::T2, CMIPS::T0, CMIPS::T1);
	//}

	//DSRLV
	for(unsigned int i = 0; i <= 64; i += 4)
	{
		Assembler.ADDIU(CMIPS::T1, CMIPS::R0, i);
		Assembler.DSRLV(CMIPS::T2, CMIPS::T0, CMIPS::T1);
	}

	//DSRA32
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.DSRA32(CMIPS::T2, CMIPS::T0, i);
	}

	//DSRL32
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.DSRL32(CMIPS::T2, CMIPS::T0, i);
	}

	//DSLL32
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.DSLL32(CMIPS::T2, CMIPS::T0, i);
	}

	//DSLL
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.DSLL(CMIPS::T2, CMIPS::T0, i);
	}
	
	//DSRA
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.DSRA(CMIPS::T2, CMIPS::T0, i);
	}

	//DSRL
	for(unsigned int i = 0; i <= 32; i += 4)
	{
		Assembler.DSRL(CMIPS::T2, CMIPS::T0, i);
	}

	CPS2VM::m_EE.m_State.nPC = nBaseAddress;
}

void CDebugger::StartSplitLoadOpTest()
{
	const int nBaseAddress = 0x00100000;

	CMIPSAssembler Assembler((uint32*)&CPS2VM::m_pRAM[nBaseAddress]);

	Assembler.LUI(CMIPS::T0, 0x0302);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x0100);
	Assembler.SW(CMIPS::T0, 0x00, CMIPS::R0);

	Assembler.LUI(CMIPS::T0, 0x0706);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x0504);
	Assembler.SW(CMIPS::T0, 0x04, CMIPS::R0);

	Assembler.LUI(CMIPS::T0, 0x0B0A);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x0908);
	Assembler.SW(CMIPS::T0, 0x08, CMIPS::R0);

	Assembler.LUI(CMIPS::T0, 0x0F0E);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x0D0C);
	Assembler.SW(CMIPS::T0, 0x0C, CMIPS::R0);

	for(unsigned int i = 0; i < 4; i++)
	{
		Assembler.ADDIU(CMIPS::T0, CMIPS::R0, i);
		Assembler.LWL(CMIPS::T1, 3, CMIPS::T0);
		Assembler.LWR(CMIPS::T1, 0, CMIPS::T0);
	}

	for(unsigned int i = 0; i < 8; i++)
	{
		Assembler.ADDIU(CMIPS::T0, CMIPS::R0, i);
		Assembler.LDL(CMIPS::T1, 0x7, CMIPS::T0);
		Assembler.LDR(CMIPS::T1, 0x0, CMIPS::T0);
	}

	CPS2VM::m_EE.m_State.nPC = nBaseAddress;
}

void CDebugger::StartAddition64OpTest()
{
	const int nBaseAddress = 0x00100000;

	CMIPSAssembler Assembler((uint32*)&CPS2VM::m_pRAM[nBaseAddress]);

	Assembler.DADDU(CMIPS::T0, CMIPS::R0, CMIPS::R0);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0xFFFF);
	Assembler.DSLL(CMIPS::T0, CMIPS::T0, 16);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0xFFF0);
	Assembler.DSLL(CMIPS::T0, CMIPS::T0, 16);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0xFFFF);
	Assembler.DSLL(CMIPS::T0, CMIPS::T0, 16);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0xFFFF);

	Assembler.DADDIU(CMIPS::T1, CMIPS::T0, 1);
	Assembler.ADDIU(CMIPS::T2, CMIPS::R0, 1);
	Assembler.DADDU(CMIPS::T0, CMIPS::T2, CMIPS::T0);

	Assembler.DSUBU(CMIPS::T0, CMIPS::T0, CMIPS::T2);
	Assembler.DADDIU(CMIPS::T1, CMIPS::T1, 0xFFFF);

	Assembler.DADDU(CMIPS::T0, CMIPS::R0, CMIPS::R0);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x8FFF);
	Assembler.DSLL(CMIPS::T0, CMIPS::T0, 16);
	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0xFFFF);
	Assembler.DSLL32(CMIPS::T0, CMIPS::T0, 0);
//	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x0000);
//	Assembler.DSLL(CMIPS::T0, CMIPS::T0, 16);
//	Assembler.ORI(CMIPS::T0, CMIPS::T0, 0x0000);

	//Assembler.BLTZ(CMIPS::T0, 0xFFFF);
	//Assembler.BGEZ(CMIPS::T0, 0xFFFF);
	Assembler.BLEZ(CMIPS::T0, 0xFFFF);

	CPS2VM::m_EE.m_State.nPC = nBaseAddress;
}

void CDebugger::StartSetLessThanOpTest()
{
	const int nBaseAddress = 0x00100000;

	CMIPSAssembler Assembler((uint32*)&CPS2VM::m_pRAM[nBaseAddress]);

	Assembler.LUI(CMIPS::K0, 0xFFFF);
	Assembler.DSLL(CMIPS::K1, CMIPS::K0, 16);
	Assembler.DSRL32(CMIPS::GP, CMIPS::K0, 16);
	Assembler.DADDIU(CMIPS::V0, CMIPS::R0, CMIPS::R0);

	Assembler.SLTU(CMIPS::T0, CMIPS::K0, CMIPS::V0);
	Assembler.SLTU(CMIPS::T0, CMIPS::K1, CMIPS::V0);
	Assembler.SLTU(CMIPS::T0, CMIPS::GP, CMIPS::V0);
	Assembler.SLTU(CMIPS::T0, CMIPS::GP, CMIPS::K0);

	CPS2VM::m_EE.m_State.nPC = nBaseAddress;
}
