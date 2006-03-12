//-----------------------------------------------------------------------------
// File: tir2joy.cpp
//
// Desc: The Mouse sample show how to use a DirectInput mouse device and 
//       the differences between cooperative levels and data styles. 
//
// Copyright (c) 1999-2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#include "stdafx.h"
#include <windows.h>
#include <tchar.h>
#include <basetsd.h>
#include <dinput.h>
#include <stdio.h>
#include <iostream.h>
#include <winioctl.h>
#include "NPClient.h"
#include "NPClientWraps.h"
#include "PPJIoctl.h"
#include "resource.h"




//-----------------------------------------------------------------------------
// Function-prototypes
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam );
HRESULT OnInitDialog( HWND hDlg );
VOID    UpdateUI( HWND hDlg );
HRESULT OnCreateDevice( HWND hDlg );
HRESULT ReadImmediateTrackIRData( HWND hDlg );
HRESULT ReadImmediateMouseData( HWND hDlg );
HRESULT ReadImmediateKeyboardData( HWND hDlg );
VOID    FreeDirectInput();
CString GetDllLocation();




//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

LPDIRECTINPUT8       g_pDI       = NULL;
LPDIRECTINPUTDEVICE8 g_pMouse    = NULL;
LPDIRECTINPUTDEVICE8 g_pKeyboard = NULL; 
bool                 useTrackIR = false;

long posx = (PPJOY_AXIS_MIN + PPJOY_AXIS_MAX) / 2;
long posy = (PPJOY_AXIS_MIN + PPJOY_AXIS_MAX) / 2;

int keycommand;

#define	NUM_ANALOG	8		/* Number of analog values which we will provide */
#define	NUM_DIGITAL	16		/* Number of digital values which we will provide */

#pragma pack(push,1)		/* All fields in structure must be byte aligned. */
typedef struct
{
 unsigned long	Signature;				/* Signature to identify packet to PPJoy IOCTL */
 char			NumAnalog;				/* Num of analog values we pass */
 long			Analog[NUM_ANALOG];		/* Analog values */
 char			NumDigital;				/* Num of digital values we pass */
 char			Digital[NUM_DIGITAL];	/* Digital values */
}	JOYSTICK_STATE;
#pragma pack(pop)

HANDLE				ppjHandle;
JOYSTICK_STATE		JoyState;
unsigned long	    NPFrameSignature;

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for 
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, int )
{
	// we need to run at same priority as NR2003
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // Display the main dialog box.
	DialogBox( hInst, MAKEINTRESOURCE(IDD_MOUSE), NULL, MainDlgProc );

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: MainDlgProc()
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
INT_PTR CALLBACK MainDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch(msg) {
        case WM_INITDIALOG:
            OnInitDialog( hDlg );
            break;
        
       case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
                case IDCANCEL:
                    EndDialog( hDlg, 0 ); 
                    break;

                default:
                    return FALSE; // Message not handled 
            }       
            break;

        case WM_ACTIVATE:
            if (WA_INACTIVE != wParam && g_pMouse) {
                // Make sure the device is acquired, if we are gaining focus.
                g_pMouse->Acquire();
            }
			if (WA_INACTIVE != wParam && g_pKeyboard) {
				g_pKeyboard->Acquire();
			}
            break;
        
        case WM_TIMER:
            // Update the input device every timer message
			if (useTrackIR == true) {
				if (FAILED(ReadImmediateTrackIRData(hDlg))) {
					KillTimer(hDlg, 0);    
					MessageBox(NULL, _T("Error Reading Input State. ")
						             _T("The sample will now exit. "), 
							         _T("Mouse"), MB_ICONERROR | MB_OK);
					EndDialog(hDlg, TRUE); 
				}
			} else {
				if (FAILED(ReadImmediateMouseData(hDlg))) {
					KillTimer(hDlg, 0);    
					MessageBox(NULL, _T("Error Reading Input State. ")
						             _T("The sample will now exit. "), 
							         _T("Mouse"), MB_ICONERROR | MB_OK);
					EndDialog(hDlg, TRUE); 
				}
			}
			if (FAILED(ReadImmediateKeyboardData(hDlg))) {
				KillTimer(hDlg, 0);    
                MessageBox(NULL, _T("Error Reading Input State. ")
                                 _T("The sample will now exit. "), 
                                 _T("Keyboard"), MB_ICONERROR | MB_OK);
                EndDialog(hDlg, TRUE); 
            }
            break;
        
        case WM_DESTROY:
            // Cleanup everything
            KillTimer(hDlg, 0);    
            FreeDirectInput();    
            break;

        default:
            return FALSE; // Message not handled 
    }

    return TRUE; // Message handled 
}




//-----------------------------------------------------------------------------
// Name: OnInitDialog()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
HRESULT OnInitDialog( HWND hDlg )
{
    // Load the icon
#ifdef _WIN64
    HINSTANCE hInst = (HINSTANCE) GetWindowLongPtr( hDlg, GWLP_HINSTANCE );
#else
    HINSTANCE hInst = (HINSTANCE) GetWindowLong( hDlg, GWL_HINSTANCE );
#endif
    HICON hIcon = LoadIcon( hInst, MAKEINTRESOURCE( IDI_MAIN ) );

    // Set the icon for this dialog.
    PostMessage( hDlg, WM_SETICON, ICON_BIG,   (LPARAM) hIcon );  // Set big icon
    PostMessage( hDlg, WM_SETICON, ICON_SMALL, (LPARAM) hIcon );  // Set small icon

    if(FAILED(OnCreateDevice(hDlg))) {
		MessageBox(hDlg, _T("CreateDevice() failed. ")
                         _T("The sample will now exit. "), 
                         _T("Mouse"), MB_ICONERROR | MB_OK );
        FreeDirectInput();
    }
    SetFocus(GetDlgItem(hDlg, IDC_CREATEDEVICE));

    UpdateUI(hDlg);

	// get key command id from ini file
	keycommand = GetPrivateProfileInt("commands", "resetview", 88, ".\\tir2joy.ini");

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateUI()
// Desc: Enables/disables the UI, and sets the dialog behavior text based on the UI
//-----------------------------------------------------------------------------
VOID UpdateUI( HWND hDlg )
{
    if(g_pMouse) {
       // SetDlgItemText(hDlg, IDC_DATA, TEXT(""));
		SetDlgItemText(hDlg, IDC_DATA, "Tracking");
    } else {
        SetDlgItemText(hDlg, IDC_DATA, TEXT("Device not created."));   
    }
}

//-----------------------------------------------------------------------------
// Name: OnCreateNPDevice()
// Desc: Setup the TrackIR Driver for enhanced mode
//-----------------------------------------------------------------------------
HRESULT OnCreateNPDevice( HWND hDlg )
{
	NPRESULT result;


	// Initialize the NPClient interface
	result = NPClient_Init(GetDllLocation());
	if (NP_OK != result) {
		MessageBox(hDlg, _T("Register with TrackIR failed. ")
                         _T("Could not find TrackIR DLL, now trying mouse mode. "), 
                         _T("Mouse"), MB_ICONERROR | MB_OK );
		return S_FALSE;
	}

	// Register the app's window handle
	result = NP_RegisterWindowHandle(hDlg);
	if (NP_OK != result) {
		MessageBox(hDlg, _T("Register with TrackIR failed. ")
                         _T("Error registering window handle, now trying mouse mode. "), 
                         _T("Mouse"), MB_ICONERROR | MB_OK );
		return S_FALSE;
	}

	// Query the NaturalPoint software version
	unsigned short wNPClientVer;
	result = NP_QueryVersion( &wNPClientVer );
	if (NP_OK != result) {
		MessageBox(hDlg, _T("Register with TrackIR failed. ")
                         _T("Could not read TrackIR version, now using mouse mode. "), 
                         _T("Mouse"), MB_ICONERROR | MB_OK );
		return S_FALSE;
	}
	
	// More NP API calls can go here -- StartData, StopData, etc....
	unsigned int DataFields = 0;
	DataFields |= NPPitch;
	DataFields |= NPYaw;

	NP_RequestData(DataFields);

	NP_StopCursor();
	NP_StartDataTransmission();

	useTrackIR = true;
	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
// Desc: Setups a the mouse device using the flags from the dialog.
//-----------------------------------------------------------------------------
HRESULT OnCreateDevice( HWND hDlg )
{
    HRESULT hr;
    DWORD   dwCoopFlags;
	char	devName[256];

    // Cleanup any previous call first
    KillTimer( hDlg, 0 );    
    FreeDirectInput();

	dwCoopFlags = DISCL_NONEXCLUSIVE | DISCL_BACKGROUND;
    
	OnCreateNPDevice(hDlg);

	// Create a DInput object
    if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_pDI, NULL))) {
        return hr;
	}

	// Obtain an interface to the system keyboard device.
    if (FAILED(hr = g_pDI->CreateDevice(GUID_SysKeyboard, &g_pKeyboard, NULL))) {
        return hr;
	}

	// Set the data format to "keyboard format" - a predefined data format
	if (FAILED(hr = g_pKeyboard->SetDataFormat(&c_dfDIKeyboard))) {
        return hr;
	}

	// Set the cooperativity level to let DirectInput know how
    // this device should interact with the system and with other
    // DirectInput applications.
	hr = g_pKeyboard->SetCooperativeLevel(hDlg, dwCoopFlags);
    if (hr == DIERR_UNSUPPORTED) {
        FreeDirectInput();
        MessageBox(hDlg, _T("SetCooperativeLevel() returned DIERR_UNSUPPORTED.\n")
                         _T("For security reasons, background exclusive keyboard\n")
                         _T("access is not allowed."), _T("Keyboard"), MB_OK );
        return S_OK;
    }

	if (useTrackIR == false) {
		// Obtain an interface to the system mouse device.
		if (FAILED(hr = g_pDI->CreateDevice(GUID_SysMouse, &g_pMouse, NULL))) {
			return hr;
		}
    
		// Set the data format to "mouse format" - a predefined data format 
		if (FAILED(hr = g_pMouse->SetDataFormat(&c_dfDIMouse2))) {
			return hr;
		}
    
		// Set the cooperativity level to let DirectInput know how
		// this device should interact with the system and with other
		// DirectInput applications.
		hr = g_pMouse->SetCooperativeLevel(hDlg, dwCoopFlags);
		if (hr == DIERR_UNSUPPORTED) {
			FreeDirectInput();
			MessageBox( hDlg, _T("SetCooperativeLevel() returned DIERR_UNSUPPORTED.\n")
				              _T("For security reasons, background exclusive mouse\n")
							_T("access is not allowed."), 
							_T("Mouse"), MB_OK);
			return S_OK;
		}

		if (FAILED(hr)) {
			return hr;
		}
	}

	// Open a handle to the control device for the first virtual joystick
	for (int i = 1; i < 17; i++) {
		sprintf(devName, "\\\\.\\PPJoyIOCTL%d", i);
		ppjHandle = CreateFile(devName, GENERIC_WRITE,FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (ppjHandle != INVALID_HANDLE_VALUE) {
			break;
		}
	}
	if (ppjHandle == INVALID_HANDLE_VALUE) {
		MessageBox(hDlg, _T("CreateFile() failed. ")
                         _T("Unable to connect to virtual joystick driver. "), 
                         _T("Mouse"), MB_ICONERROR | MB_OK );
	}

	JoyState.Signature= JOYSTICK_STATE_V1;
	JoyState.NumAnalog= NUM_ANALOG;
	JoyState.NumDigital= NUM_DIGITAL;

    // Acquire the newly created device
	if (useTrackIR == false) {
		g_pMouse->Acquire();
	}
	g_pKeyboard->Acquire();

    // Set a timer to go off 50 times a second, to read input
    SetTimer(hDlg, 0, 1000 / 50, NULL);

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: ReadImmediateTrackIRData()
// Desc: Read the input device's state when in immediate mode and display it.
//-----------------------------------------------------------------------------
HRESULT ReadImmediateTrackIRData( HWND hDlg )
{
    TCHAR         strNewText[256] = TEXT("");
	long		  *analog;
	char		  *digital;
	DWORD		  RetSize;
	TRACKIRDATA   tid;

	analog = JoyState.Analog;
	digital = JoyState.Digital;
	analog[0] = analog[1] = analog[2] = analog[3] = analog[4] = analog[5] = analog[6] = analog[7] = (PPJOY_AXIS_MIN + PPJOY_AXIS_MAX) / 2;
    ZeroMemory(digital, sizeof(JoyState.Digital));

	// Get the old text in the text box
	TCHAR strOldText[128];
	GetDlgItemText( hDlg, IDC_DATA, strOldText, 127 );

    NPRESULT result = NP_GetData(&tid);
    if (NP_OK == result) {
		if (tid.wNPStatus == NPSTATUS_REMOTEACTIVE) {
			if (NPFrameSignature != tid.wPFrameSignature) {
				analog[0] = (PPJOY_AXIS_MIN + PPJOY_AXIS_MAX) / 2 - long(tid.fNPYaw);
				analog[1] = long(tid.fNPPitch) + (PPJOY_AXIS_MIN + PPJOY_AXIS_MAX) / 2;
				sprintf(strNewText, "TIR(X=% 3.3d, Y=% 3.3d)", 0 - long(tid.fNPYaw), 0 - long(tid.fNPPitch));

				if (!DeviceIoControl(ppjHandle, IOCTL_PPORTJOY_SET_STATE, &JoyState, sizeof(JoyState), NULL, 0, &RetSize, NULL)) {
					sprintf(strNewText, "Device error: %d", GetLastError());
				}
				if(0 != lstrcmp(strOldText, strNewText)) {
					SetDlgItemText(hDlg, IDC_DATA, strNewText);
				}
				NPFrameSignature = tid.wPFrameSignature;
				return S_OK;
			} else {
				// Either there is no tracking data or the user has
				// paused the trackIR
				sprintf(strNewText, "No Data");
				if(0 != lstrcmp(strOldText, strNewText)) {
					SetDlgItemText(hDlg, IDC_DATA, strNewText);
				}
				return S_OK;
			}
		} else 	{
			// The user has set the device out of trackIR Enhanced Mode
			// and into Mouse Emulation mode with the hotkey
			sprintf(strNewText, "User Disabled");
			if(0 != lstrcmp(strOldText, strNewText)) {
				SetDlgItemText(hDlg, IDC_DATA, strNewText);
			}
			return S_OK;
		}

	} // if( got data to process )
	
	return S_FALSE;
}

//-----------------------------------------------------------------------------
// Name: ReadImmediateMouseData()
// Desc: Read the input device's state when in immediate mode and display it.
//-----------------------------------------------------------------------------
HRESULT ReadImmediateMouseData( HWND hDlg )
{
    HRESULT       hr;
    TCHAR         strNewText[256] = TEXT("");
    DIMOUSESTATE2 dims2;      // DirectInput mouse state structure
	long		  *analog;
	char		  *digital;
	DWORD		  RetSize;

	analog = JoyState.Analog;
	digital = JoyState.Digital;
	analog[0] = analog[1] = analog[2] = analog[3] = analog[4] = analog[5] = analog[6] = analog[7] = (PPJOY_AXIS_MIN + PPJOY_AXIS_MAX) / 2;
    ZeroMemory(digital,sizeof(JoyState.Digital));

    if (NULL == g_pMouse || NULL == g_pKeyboard) {
        return S_OK;
	}
    
    // Get the input's device state, and put the state in dims
    ZeroMemory(&dims2, sizeof(dims2));
    hr = g_pMouse->GetDeviceState(sizeof(DIMOUSESTATE2), &dims2);

    if (FAILED(hr)) {
        // If input is lost then acquire and keep trying 
        hr = g_pMouse->Acquire();
        while(hr == DIERR_INPUTLOST) {
            hr = g_pMouse->Acquire();
		}

        // Update the dialog text 
        if(hr == DIERR_OTHERAPPHASPRIO || hr == DIERR_NOTACQUIRED) {
            SetDlgItemText(hDlg, IDC_DATA, "Unacquired");
		}

        // hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
        // may occur when the app is minimized or in the process of 
        // switching, so just try again later 
        return S_OK; 
    }
    
    // The dims structure now has the state of the mouse, so 
    // display mouse coordinates (x, y, z) and buttons.
    sprintf(strNewText, "(X=% 3.3d, Y=% 3.3d)", dims2.lX, dims2.lY);

    // Get the old text in the text box
    TCHAR strOldText[128];
    GetDlgItemText( hDlg, IDC_DATA, strOldText, 127 );
    
	posx += dims2.lX * 20;
	posy += dims2.lY * 20;

	if (posx < PPJOY_AXIS_MIN) {
		posx = PPJOY_AXIS_MIN;
	} else if (posx > PPJOY_AXIS_MAX) {
		posx = PPJOY_AXIS_MAX;
	}

	if (posy < PPJOY_AXIS_MIN) {
		posy = PPJOY_AXIS_MIN;
	} else if (posy > PPJOY_AXIS_MAX) {
		posy = PPJOY_AXIS_MAX;
	}

	// set joystick values
	analog[0] = posx;
	analog[1] = posy;

	if (!DeviceIoControl(ppjHandle, IOCTL_PPORTJOY_SET_STATE, &JoyState, sizeof(JoyState), NULL, 0, &RetSize, NULL)) {
		sprintf(strNewText, "Device error: %d", GetLastError());
	}

    // If nothing changed then don't repaint - avoid flicker
    if(0 != lstrcmp(strOldText, strNewText)) {
        SetDlgItemText(hDlg, IDC_DATA, strNewText);
	}

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: ReadImmediateData()
// Desc: Read the input device's state when in immediate mode and display it.
//-----------------------------------------------------------------------------
HRESULT ReadImmediateKeyboardData(HWND hDlg)
{
    HRESULT hr;
    TCHAR   strNewText[256*5 + 1] = TEXT(""); 
    BYTE    diks[256];   // DirectInput keyboard state buffer 

    if (NULL == g_pKeyboard) {
        return S_OK;
	}
    
    // Get the input's device state, and put the state in dims
    ZeroMemory(&diks, sizeof(diks));
    hr = g_pKeyboard->GetDeviceState(sizeof(diks), &diks);
    if (FAILED(hr)) {
        // If input is lost then acquire and keep trying 
        hr = g_pKeyboard->Acquire();
        while(hr == DIERR_INPUTLOST) {
            hr = g_pKeyboard->Acquire();
		}

        // Update the dialog text 
        if(hr == DIERR_OTHERAPPHASPRIO || hr == DIERR_NOTACQUIRED) {
            SetDlgItemText(hDlg, IDC_DATA, "Unacquired");
		}

        return S_OK; 
    }
    
    // check if reset key was pressed
    if(diks[keycommand] & 0x80 ) {
		posx = (PPJOY_AXIS_MIN + PPJOY_AXIS_MAX) / 2;
		posy = (PPJOY_AXIS_MIN + PPJOY_AXIS_MAX) / 2;
		SetDlgItemText(hDlg, IDC_DATA, "Resetting");
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
VOID FreeDirectInput()
{
    // Unacquire the device one last time just in case 
    // the app tried to exit while the device is still acquired.
	if (useTrackIR == true) {
		NP_StopDataTransmission();
		NP_UnregisterWindowHandle();
	} else {
		if (g_pMouse) {
			g_pMouse->Unacquire();
		}
	}
	
	if (g_pKeyboard) {
		g_pKeyboard->Unacquire();
	}

    // close the virtual joystick device
	CloseHandle(ppjHandle);

    // Release any DirectInput objects.
	if (useTrackIR == false) {
		SAFE_RELEASE(g_pMouse);
	}
	SAFE_RELEASE(g_pKeyboard);
    SAFE_RELEASE(g_pDI);
}


CString GetDllLocation()
{
	//find path to NPClient.dll
	HKEY pKey = NULL;
	//open the registry key 
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\NaturalPoint\\NATURALPOINT\\NPClient Location", 0, KEY_READ, &pKey) != ERROR_SUCCESS) {
		return "";
	}

	//get the value from the key
	unsigned char *szValue;
	DWORD dwSize;
	//first discover the size of the value
	if (RegQueryValueEx(pKey, "Path", NULL, NULL, NULL, &dwSize) == ERROR_SUCCESS) {
		//allocate memory for the buffer for the value
		szValue = (unsigned char *)malloc(dwSize);
		if (szValue == NULL) {
			return "";
		}
		//now get the value
        if (RegQueryValueEx(pKey, "Path", NULL, NULL, szValue, &dwSize) == ERROR_SUCCESS) {
			//everything worked
			::RegCloseKey(pKey);
			CString LValue(szValue);
			return LValue;
		} else {
			return "";
		}
			
	}
	RegCloseKey(pKey);
	return "Error";
}