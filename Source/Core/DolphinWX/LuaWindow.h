// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/imaglist.h>
#include <wx/treectrl.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/mimetype.h>
#include <wx/colour.h>
#include <wx/listbox.h>
#include <string>

#include "Common/IniFile.h"
#include "DiscIO/Filesystem.h"

class wxLuaWindow : public wxFrame
{
	public:

		wxLuaWindow(wxFrame* parent, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);

		void PrintMessage(const char *text);

		void OnStop();

		virtual ~wxLuaWindow();

	protected:
		static int luaCount;
		int luaID;
		bool bScriptRunning;
		wxString currentScript;

		// Event Table
		DECLARE_EVENT_TABLE();

		// --- GUI Controls ---

		wxPanel *m_Tab_Log;

		wxButton *m_Button_Close, *m_Button_LoadScript, *m_Button_Run, 
			     *m_Button_Stop, *m_Button_Clear;

		wxTextCtrl *m_TextCtrl_Log;

		// GUI IDs
		enum
		{
			ID_TAB_LOG,
			ID_BUTTON_CLOSE,
			ID_BUTTON_LOAD,
			ID_BUTTON_RUN,
			ID_BUTTON_STOP,
			ID_BUTTON_CLEAR,
			ID_TEXTCTRL_LOG
		};

		void InitGUIControls();


		// --- Wx Events Handlers ---
		// $ Window
		void OnEvent_Window_Resize(wxSizeEvent& event);
		void OnEvent_Window_Close(wxCloseEvent& event);

		// $ Buttons
		void OnEvent_ButtonClose_Press(wxCommandEvent& event);
		void OnEvent_ScriptLoad_Press(wxCommandEvent& event);
		void OnEvent_ScriptRun_Press(wxCommandEvent& event);
		void OnEvent_ScriptStop_Press(wxCommandEvent& event);
		void OnEvent_ButtonClear_Press(wxCommandEvent& event);

		// -- CoreTiming-style event handlers --
		static void LuaOpenCallback(u64 userdata, int cyclesLate);
		static void LuaCloseCallback(u64 userdata, int cyclesLate);
		static void LuaStartCallback(u64 userdata, int cyclesLate);
		static void LuaStopCallback(u64 userdata, int cyclesLate);

		static void LuaWindow_InitFirstTime();

};
