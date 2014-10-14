// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <wx/bitmap.h>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/dcmemory.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/layout.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/statbmp.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/validate.h>
#include <wx/window.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Core/Movie.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "DolphinWX/TASInputDlg.h"
#include "InputCommon/GCPadStatus.h"


BEGIN_EVENT_TABLE(TASInputDlg, wxDialog)
EVT_CLOSE(TASInputDlg::OnCloseWindow)
END_EVENT_TABLE()

TASInputDlg::TASInputDlg(wxWindow* parent, wxWindowID id, const wxString& title,
                         const wxPoint& position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	for (unsigned int i = 0; i < 10; ++i)
		Controls[i] = nullptr;
	for (unsigned int i = 0; i < 14; ++i)
		Buttons[i] = nullptr;

	Buttons[0] = &dpad_down;
	Buttons[1] = &dpad_up;
	Buttons[2] = &dpad_left;
	Buttons[3] = &dpad_right;
	Buttons[4] = &A;
	Buttons[5] = &B;
	Controls[0] = &MainStick.xCont;
	Controls[1] = &MainStick.yCont;
	Controls[2] = &CStick.xCont;
	Controls[3] = &CStick.yCont;

	A = CreateButton("A");
	B = CreateButton("B");
	dpad_up = CreateButton("Up");
	dpad_right = CreateButton("Right");
	dpad_down = CreateButton("Down");
	dpad_left = CreateButton("Left");

	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(dpad_up.Checkbox, false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(dpad_left.Checkbox, false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(dpad_right.Checkbox, false);
	buttons_dpad->AddSpacer(20);
	buttons_dpad->Add(dpad_down.Checkbox, false);
	buttons_dpad->AddSpacer(20);
}

const int TASInputDlg::GCPadButtonsBitmask[12] = {
	PAD_BUTTON_DOWN, PAD_BUTTON_UP, PAD_BUTTON_LEFT,PAD_BUTTON_RIGHT, PAD_BUTTON_A, PAD_BUTTON_B,
	PAD_BUTTON_X, PAD_BUTTON_Y, PAD_TRIGGER_Z, PAD_TRIGGER_L, PAD_TRIGGER_R, PAD_BUTTON_START
};

const int TASInputDlg::WiiButtonsBitmask[13] = {
	WiimoteEmu::Wiimote::BUTTON_A, WiimoteEmu::Wiimote::BUTTON_B, WiimoteEmu::Wiimote::PAD_DOWN,
	WiimoteEmu::Wiimote::PAD_UP, WiimoteEmu::Wiimote::PAD_LEFT, WiimoteEmu::Wiimote::PAD_RIGHT,
	WiimoteEmu::Wiimote::BUTTON_ONE, WiimoteEmu::Wiimote::BUTTON_TWO, WiimoteEmu::Wiimote::BUTTON_PLUS,
	WiimoteEmu::Wiimote::BUTTON_MINUS, WiimoteEmu::Wiimote::BUTTON_HOME, 
};

void TASInputDlg::CreateWiiLayout()
{
	if (hasLayout)
		return;

	Buttons[6] = &ONE;
	Buttons[7] = &TWO;
	Buttons[8] = &PLUS;
	Buttons[9] = &MINUS;
	Buttons[10] = &HOME;
	Buttons[11] = &C;
	Buttons[12] = &Z;

	Controls[4] = &xCont;
	Controls[5] = &yCont;
	Controls[6] = &zCont;
	Controls[7] = &nxCont;
	Controls[8] = &nyCont;
	Controls[9] = &nzCont;

	MainStick = CreateStick(ID_MAIN_STICK, 1024, 768, 512, 384, true, false);
	wxStaticBoxSizer* const main_stick = CreateStickLayout(&MainStick, "IR");

	CStick = CreateStick(ID_C_STICK, 255, 255, 128, 128, false, true);
	wxStaticBoxSizer* const nunchuck_stick = CreateStickLayout(&CStick, "Nunchuck stick");


	xCont = CreateControl(wxSL_VERTICAL, -1, 100);
	yCont = CreateControl(wxSL_VERTICAL, -1, 100);
	zCont = CreateControl(wxSL_VERTICAL, -1, 100, false, 255, 154);
	wxStaticBoxSizer* const axisBox = CreateAccellLayout(&xCont, &yCont, &zCont, "Orientation");

	nxCont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 512);
	nyCont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 512);
	nzCont = CreateControl(wxSL_VERTICAL, -1, 100, false, 1023, 512);
	wxStaticBoxSizer* const nunchuckaxisBox = CreateAccellLayout(&nxCont, &nyCont, &nzCont, "Nunchuck orientation");

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (Controls[i] != nullptr)
			Controls[i]->Slider->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(TASInputDlg::OnRightClickSlider), nullptr, this);
	}

	wxStaticBoxSizer* const buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
	wxGridSizer* const buttons_grid = new wxGridSizer(4);

	C = CreateButton("C");
	Z = CreateButton("Z");
	PLUS = CreateButton("+");
	MINUS = CreateButton("-");
	ONE = CreateButton("1");
	TWO = CreateButton("2");
	HOME = CreateButton("HOME");

	for (unsigned int i = 4; i < 14; ++i)
		if (Buttons[i] != nullptr)
			buttons_grid->Add(Buttons[i]->Checkbox, false);
	buttons_grid->AddSpacer(5);

	buttons_box->Add(buttons_grid);
	buttons_box->Add(buttons_dpad);

	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const wiimote_szr = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const ext_szr = new wxBoxSizer(wxHORIZONTAL);
	ext_szr->Add(nunchuck_stick);
	ext_szr->Add(nunchuckaxisBox);

	wiimote_szr->Add(main_stick);
	wiimote_szr->Add(axisBox);
	wiimote_szr->Add(buttons_box);
	main_szr->Add(wiimote_szr);
	main_szr->Add(ext_szr);
	SetSizerAndFit(main_szr);

	hasLayout = true;
	ResetValues();
}

void TASInputDlg::CreateGCLayout()
{
	if (hasLayout)
		return;

	Buttons[6] = &X;
	Buttons[7] = &Y;
	Buttons[8] = &Z;
	Buttons[9] = &L;
	Buttons[10] = &R;
	Buttons[11] = &START;

	Controls[4] = &lCont;
	Controls[5] = &rCont;

	wxBoxSizer* const top_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const bottom_box = new wxBoxSizer(wxHORIZONTAL);
	MainStick = CreateStick(ID_MAIN_STICK, 255, 255, 128, 128, false, true);
	wxStaticBoxSizer* const main_box = CreateStickLayout(&MainStick, "Main Stick");

	CStick = CreateStick(ID_C_STICK, 255, 255, 128, 128, false, true);
	wxStaticBoxSizer* const c_box = CreateStickLayout(&CStick, "C Stick");

	wxStaticBoxSizer* const shoulder_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Shoulder Buttons"));
	lCont = CreateControl(wxSL_VERTICAL, -1, 100, false, 255, 0);
	rCont = CreateControl(wxSL_VERTICAL, -1, 100, false, 255, 0);
	shoulder_box->Add(lCont.Slider, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(lCont.Text, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(rCont.Slider, 0, wxALIGN_CENTER_VERTICAL);
	shoulder_box->Add(rCont.Text, 0, wxALIGN_CENTER_VERTICAL);

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (Controls[i] != nullptr)
			Controls[i]->Slider->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(TASInputDlg::OnRightClickSlider), nullptr, this);
	}

	wxStaticBoxSizer* const buttons_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Buttons"));
	wxGridSizer* const buttons_grid = new wxGridSizer(4);

	X = CreateButton("X");
	Y = CreateButton("Y");
	L = CreateButton("L");
	R = CreateButton("R");
	Z = CreateButton("Z");
	START = CreateButton("Start");

	for (unsigned int i = 4; i < 14; ++i)
		if (Buttons[i] != nullptr)
			buttons_grid->Add(Buttons[i]->Checkbox, false);
	buttons_grid->AddSpacer(5);

	buttons_box->Add(buttons_grid);
	buttons_box->Add(buttons_dpad);

	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);

	top_box->AddSpacer(5);
	top_box->Add(main_box);
	top_box->AddSpacer(5);
	top_box->Add(c_box);
	top_box->AddSpacer(5);
	bottom_box->AddSpacer(5);
	bottom_box->Add(shoulder_box);
	bottom_box->AddSpacer(5);
	bottom_box->Add(buttons_box);
	bottom_box->AddSpacer(5);
	main_szr->Add(top_box);
	main_szr->Add(bottom_box);
	SetSizerAndFit(main_szr);

	hasLayout = true;
	ResetValues();
}


TASInputDlg::Control TASInputDlg::CreateControl(long style, int width, int height, bool reverse, u32 range, u32 defaultValue)
{
	Control tempCont;
	tempCont.range = range;
	tempCont.defaultValue = defaultValue;
	tempCont.Slider = new wxSlider(this, eleID++, defaultValue, 0, range, wxDefaultPosition, wxDefaultSize, style);
	tempCont.Slider->SetMinSize(wxSize(width, height));
	tempCont.Slider->Connect(wxEVT_SLIDER, wxCommandEventHandler(TASInputDlg::UpdateFromSliders), nullptr, this);
	tempCont.Text = new wxTextCtrl(this, eleID++, std::to_string(defaultValue), wxDefaultPosition, wxSize(40, 20));
	tempCont.Text->SetMaxLength(range > 999 ? 4 : 3);
	tempCont.Text_ID = eleID - 1;
	tempCont.Text->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(TASInputDlg::UpdateFromText), nullptr, this);
	tempCont.Slider_ID = eleID - 2;
	tempCont.reverse = reverse;
	return tempCont;
}

TASInputDlg::Stick TASInputDlg::CreateStick(int id_stick, int xRange, int yRange, u32 defaultX, u32 defaultY, bool reverseX, bool reverseY)
{
	Stick tempStick;
	tempStick.bitmap = new wxStaticBitmap(this, id_stick, CreateStickBitmap(128,128), wxDefaultPosition, wxDefaultSize);
	tempStick.bitmap->Connect(wxEVT_MOTION, wxMouseEventHandler(TASInputDlg::OnMouseDownL), nullptr, this);
	tempStick.bitmap->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(TASInputDlg::OnMouseDownL), nullptr, this);
	tempStick.bitmap->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(TASInputDlg::OnMouseUpR), nullptr, this);
	tempStick.xCont = CreateControl(wxSL_HORIZONTAL | (reverseX ? wxSL_INVERSE : 0), 120, -1, reverseX, xRange, defaultX);
	tempStick.yCont = CreateControl(wxSL_VERTICAL | (reverseY ?  wxSL_INVERSE : 0), -1, 120, reverseY, yRange, defaultY);
	return tempStick;
}

wxStaticBoxSizer* TASInputDlg::CreateStickLayout(Stick* tempStick, std::string title)
{
	wxStaticBoxSizer* const temp_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _(title));
	wxBoxSizer* const temp_xslider_box = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* const temp_yslider_box = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const temp_stick_box = new wxBoxSizer(wxVERTICAL);

	temp_xslider_box->Add(tempStick->xCont.Slider, 0, wxALIGN_TOP);
	temp_xslider_box->Add(tempStick->xCont.Text, 0, wxALIGN_TOP);
	temp_stick_box->Add(temp_xslider_box);
	temp_stick_box->Add(tempStick->bitmap,0, wxALL|wxCenter,3);
	temp_box->Add(temp_stick_box);
	temp_yslider_box->Add(tempStick->yCont.Slider, 0, wxALIGN_CENTER_VERTICAL);
	temp_yslider_box->Add(tempStick->yCont.Text, 0, wxALIGN_CENTER_VERTICAL);
	temp_box->Add(temp_yslider_box);
	return temp_box;
}

wxStaticBoxSizer* TASInputDlg::CreateAccellLayout(Control* x, Control* y, Control* z, std::string title)
{
	wxStaticBoxSizer* const temp_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _(title));
	wxStaticBoxSizer* const xBox = new wxStaticBoxSizer(wxVERTICAL, this, _("X"));
	wxStaticBoxSizer* const yBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Y"));
	wxStaticBoxSizer* const zBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Z"));

	xBox->Add(x->Slider, 0, wxALIGN_CENTER_VERTICAL);
	xBox->Add(x->Text, 0, wxALIGN_CENTER_VERTICAL);
	yBox->Add(y->Slider, 0, wxALIGN_CENTER_VERTICAL);
	yBox->Add(y->Text, 0, wxALIGN_CENTER_VERTICAL);
	zBox->Add(z->Slider, 0, wxALIGN_CENTER_VERTICAL);
	zBox->Add(z->Text, 0, wxALIGN_CENTER_VERTICAL);
	temp_box->Add(xBox);
	temp_box->Add(yBox);
	temp_box->Add(zBox);
	return temp_box;
}

TASInputDlg::Button TASInputDlg::CreateButton(const std::string& name)
{
	Button temp;
	wxCheckBox* checkbox = new wxCheckBox(this, eleID++, name, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxCheckBoxNameStr);
	checkbox->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(TASInputDlg::SetTurbo), nullptr, this);
	checkbox->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(TASInputDlg::SetTurbo), nullptr, this);
	temp.Checkbox = checkbox;
	temp.ID = eleID - 1;
	return temp;
}

void TASInputDlg::ResetValues()
{
	if (!hasLayout)
		return;

	for (unsigned int i = 0; i < 14; ++i)
	{
		if (Buttons[i] != nullptr)
			Buttons[i]->Checkbox->SetValue(false);
	}

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (Controls[i] != nullptr)
		{
			Controls[i]->value = Controls[i]->defaultValue;
			Controls[i]->Slider->SetValue(Controls[i]->defaultValue);
			Controls[i]->Text->SetValue(std::to_string(Controls[i]->defaultValue));
		}
	}
}

void TASInputDlg::SetStickValue(bool* ActivatedByKeyboard, int* AmountPressed, wxTextCtrl* Textbox, int CurrentValue, int center)
{
	if (CurrentValue != center)
	{
		*AmountPressed = CurrentValue;
		*ActivatedByKeyboard = true;
		Textbox->SetValue(std::to_string(*AmountPressed));
	}
	else if (*ActivatedByKeyboard)
	{
		*AmountPressed = center;
		*ActivatedByKeyboard = false;
		Textbox->SetValue(std::to_string(*AmountPressed));
	}
}

void TASInputDlg::SetSliderValue(Control* control, int CurrentValue, int defaultValue)
{
	if (CurrentValue != defaultValue)
	{
		control->value = CurrentValue;
		control->SetByKeyboard = true;
		control->Text->SetValue(std::to_string(CurrentValue));
	}
	else if (control->SetByKeyboard)
	{
		control->value = defaultValue;
		control->SetByKeyboard = false;
		control->Text->SetValue(std::to_string(defaultValue));
	}
}

void TASInputDlg::SetButtonValue(Button* button, bool CurrentState)
{
	if (CurrentState)
	{
		button->SetByKeyboard = true;
		button->Checkbox->SetValue(CurrentState);
	}
	else if (button->SetByKeyboard)
	{
		button->SetByKeyboard = false;
		button->Checkbox->SetValue(CurrentState);
	}
}

void TASInputDlg::SetWiiButtons(wm_core* butt)
{
	for (unsigned int i = 0; i < 10; ++i)
	{
		if (Buttons[i] != nullptr)
			*butt |= (Buttons[i]->Checkbox->IsChecked()) ? WiiButtonsBitmask[i] : 0;
	}
	ButtonTurbo();
}

void TASInputDlg::GetKeyBoardInput(GCPadStatus* PadStatus)
{
	SetStickValue(&MainStick.xCont.SetByKeyboard, &MainStick.xCont.value, MainStick.xCont.Text, PadStatus->stickX);
	SetStickValue(&MainStick.yCont.SetByKeyboard, &MainStick.yCont.value, MainStick.yCont.Text, PadStatus->stickY);

	SetStickValue(&CStick.xCont.SetByKeyboard, &CStick.xCont.value, CStick.xCont.Text, PadStatus->substickX);
	SetStickValue(&CStick.yCont.SetByKeyboard, &CStick.yCont.value, CStick.yCont.Text, PadStatus->substickY);

	for (unsigned int i = 0; i < 14; ++i)
	{
		if (Buttons[i] != nullptr)
			SetButtonValue(Buttons[i], ((PadStatus->button & GCPadButtonsBitmask[i]) != 0));
	}
	SetButtonValue(&L, ((PadStatus->triggerLeft) != 0) || ((PadStatus->button & PAD_TRIGGER_L) != 0));
	SetButtonValue(&R, ((PadStatus->triggerRight) != 0) || ((PadStatus->button & PAD_TRIGGER_R) != 0));
}

void TASInputDlg::GetKeyBoardInput(u8* data, WiimoteEmu::ReportFeatures rptf, int ext, const struct wiimote_key key)
{
	u8* const coreData = rptf.core ? (data + rptf.core) : nullptr;
	u8* const accelData = rptf.accel ? (data + rptf.accel) : nullptr;
	u8* const irData = rptf.ir ? (data + rptf.ir) : nullptr;
	u8* const extData = rptf.ext ? (data + rptf.ext) : nullptr;

	if (coreData)
	{
		for (unsigned int i = 0; i < 10; ++i)
		{
			if (Buttons[i] != nullptr)
				SetButtonValue(Buttons[i], (*(wm_core*)coreData & WiiButtonsBitmask[i]) != 0);
		}
	}
	if (accelData)
	{
		wm_accel* dt = (wm_accel*)accelData;

		SetSliderValue(&xCont, dt->x);
		SetSliderValue(&yCont, dt->y);
		SetSliderValue(&zCont, dt->z, 154);
	}

	// I don't think this can be made to work in a sane manner.
	//if (irData)
	//{
	//	u16 x = 1023 - (irData[0] | ((irData[2] >> 4 & 0x3) << 8));
	//	u16 y = irData[1] | ((irData[2] >> 6 & 0x3) << 8);

	//	SetStickValue(&MainStick.xCont.SetByKeyboard, &MainStick.xCont.value, MainStick.xCont.Text, x, 561);
	//	SetStickValue(&MainStick.yCont.SetByKeyboard, &MainStick.yCont.value, MainStick.yCont.Text, y, 486);
	//}

	if (extData)
	{
		wm_nc& nunchuck = *(wm_nc*)extData;
		WiimoteDecrypt(&key, (u8*)&nunchuck, 0, sizeof(wm_nc));
		nunchuck.bt.hex = nunchuck.bt.hex ^ 0x3;
		SetButtonValue(Buttons[11], nunchuck.bt.c != 0);
		SetButtonValue(Buttons[12], nunchuck.bt.z != 0);
	}
}

void TASInputDlg::GetValues(u8* data, WiimoteEmu::ReportFeatures rptf, int ext, const struct wiimote_key key)
{
	if (!IsShown())
		return;

	GetKeyBoardInput(data, rptf, ext, key);

	u8* const coreData = rptf.core ? (data + rptf.core) : nullptr;
	u8* const accelData = rptf.accel ? (data + rptf.accel) : nullptr;
	u8* const irData = rptf.ir ? (data + rptf.ir) : nullptr;
	u8* const extData = rptf.ext ? (data + rptf.ext) : nullptr;
	u8 size = rptf.size;

	if (coreData)
		SetWiiButtons((wm_core*)coreData);

	if (accelData)
	{
		wm_accel* dt = (wm_accel*)accelData;
		dt->x = xCont.value;
		dt->y = yCont.value;
		dt->z = zCont.value;
	}
	if (irData)
	{
		u16 x[4];
		u16 y;

		x[0] = MainStick.xCont.value;
		y = MainStick.yCont.value;
		x[1] = x[0] + 100;
		x[2] = x[0] - 10;
		x[3] = x[1] + 10;

		u8 mode;
		// Mode 5 not supported in core anyway.
		if (rptf.ext)
			mode = (rptf.ext - rptf.ir) == 10 ? 1 : 3;
		else
			mode = (rptf.size - rptf.ir) == 10 ? 1 : 3;

		if (mode == 1)
		{
			memset(irData, 0xFF, sizeof(wm_ir_basic) * 2);
			wm_ir_basic* data = (wm_ir_basic*)irData;
			for (unsigned int i = 0; i < 2; ++i)
			{
				if (x[i*2] < 1024 && y < 768)
				{
					data[i].x1 = static_cast<u8>(x[i*2]);
					data[i].x1hi = x[i*2] >> 8;

					data[i].y1 = static_cast<u8>(y);
					data[i].y1hi = y >> 8;
				}
				if (x[i*2+1] < 1024 && y < 768)
				{
					data[i].x2 = static_cast<u8>(x[i*2+1]);
					data[i].x2hi = x[i*2+1] >> 8;

					data[i].y2 = static_cast<u8>(y);
					data[i].y2hi = y >> 8;
				}
			}
		}
		else
		{
			memset(data, 0xFF, sizeof(wm_ir_extended) * 4);
			wm_ir_extended* const data = (wm_ir_extended*)irData;
			for (unsigned int i = 0; i < 4; ++i)
			{
				if (x[i] < 1024 && y < 768)
				{
					data[i].x = static_cast<u8>(x[i]);
					data[i].xhi = x[i] >> 8;

					data[i].y = static_cast<u8>(y);
					data[i].yhi = y >> 8;

					data[i].size = 10;
				}
			}
		}
	}
	if (extData && ext == 1)
	{
		wm_nc& nunchuck = *(wm_nc*)extData;

		nunchuck.jx = CStick.xCont.value;
		nunchuck.jy = CStick.yCont.value;

		nunchuck.ax    = nxCont.value >> 2;
		nunchuck.bt.acc_x_lsb = nxCont.value & 0x3;
		nunchuck.ay    = nyCont.value >> 2;
		nunchuck.bt.acc_y_lsb = nyCont.value & 0x3;
		nunchuck.az    = nzCont.value >> 2;
		nunchuck.bt.acc_z_lsb = nzCont.value & 0x3;

		nunchuck.bt.hex |= (Buttons[11]->Checkbox->IsChecked()) ? WiimoteEmu::Nunchuk::BUTTON_C : 0;
		nunchuck.bt.hex |= (Buttons[12]->Checkbox->IsChecked()) ? WiimoteEmu::Nunchuk::BUTTON_Z : 0;
		nunchuck.bt.hex = nunchuck.bt.hex ^ 0x3;
		WiimoteEncrypt(&key, (u8*)&nunchuck, 0, sizeof(wm_nc));
	}

	if (extData && ext == 2)
	{
		// TODO
		wm_classic_extension& cc = *(wm_classic_extension*)extData;
		WiimoteDecrypt(&key, (u8*)&cc, 0, sizeof(wm_classic_extension));



		WiimoteEncrypt(&key, (u8*)&cc, 0, sizeof(wm_classic_extension));
	}
}

void TASInputDlg::GetValues(GCPadStatus* PadStatus)
{
	if (!IsShown())
		return;

	//TODO:: Make this instant not when polled.
	GetKeyBoardInput(PadStatus);

	PadStatus->stickX = MainStick.xCont.value;
	PadStatus->stickY = MainStick.yCont.value;
	PadStatus->substickX = CStick.xCont.value;
	PadStatus->substickY = CStick.yCont.value;
	PadStatus->triggerLeft = L.Checkbox->GetValue() ? 255 : lCont.Slider->GetValue();
	PadStatus->triggerRight = R.Checkbox->GetValue() ? 255 : rCont.Slider->GetValue();

	for (unsigned int i = 0; i < 14; ++i)
	{
		if (Buttons[i] != nullptr)
		{
			if (Buttons[i]->Checkbox->IsChecked())
				PadStatus->button |= GCPadButtonsBitmask[i];
			else
				PadStatus->button &= ~GCPadButtonsBitmask[i];
		}
	}

	if (A.Checkbox->IsChecked())
		PadStatus->analogA = 0xFF;
	else
		PadStatus->analogA = 0x00;

	if (B.Checkbox->IsChecked())
		PadStatus->analogB = 0xFF;
	else
		PadStatus->analogB = 0x00;

	ButtonTurbo();
}

void TASInputDlg::UpdateFromSliders(wxCommandEvent& event)
{
	wxTextCtrl* text;

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (Controls[i] != nullptr && event.GetId() == Controls[i]->Slider_ID)
			text = Controls[i]->Text;
	}
	int value = ((wxSlider*) event.GetEventObject())->GetValue();
	text->SetValue(std::to_string(value));
}

void TASInputDlg::UpdateFromText(wxCommandEvent& event)
{
	int v;
	unsigned long value;

	if (!((wxTextCtrl*) event.GetEventObject())->GetValue().ToULong(&value))
		return;

	for (unsigned int i = 0; i < 10; ++i)
	{
		if (Controls[i] != nullptr && event.GetId() == Controls[i]->Text_ID)
		{
			v = value > Controls[i]->range ? Controls[i]->range : value;
			Controls[i]->Slider->SetValue(v);
			Controls[i]->value = v;
		}
	}

	int x = CStick.xCont.value;
	int y = CStick.yCont.value;

	if (CStick.xCont.reverse)
		x = CStick.xCont.range - CStick.xCont.value;
	if (CStick.yCont.reverse)
		y = CStick.yCont.range - CStick.yCont.value;


	CStick.bitmap->SetBitmap(CreateStickBitmap(x, y));

	x = (u8)((double)MainStick.xCont.value / (double)MainStick.xCont.range * 255.0);
	y = (u8)((double)MainStick.yCont.value / (double)MainStick.yCont.range * 255.0);
	if (MainStick.xCont.reverse)
		x = 255 - (u8)x;
	if (MainStick.yCont.reverse)
		y = 255 - (u8)y;
	MainStick.bitmap->SetBitmap(CreateStickBitmap(x, y));
}

void TASInputDlg::OnCloseWindow(wxCloseEvent& event)
{
	if (event.CanVeto())
	{
		event.Skip(false);
		this->Show(false);
		ResetValues();
	}
}

bool TASInputDlg::TASHasFocus()
{
	if (!hasLayout)
		return false;
	//allows numbers to be used as hotkeys
	for (unsigned int i = 0; i < 10; ++i)
	{
		if (Controls[i] != nullptr && wxWindow::FindFocus() == Controls[i]->Text)
			return false;
	}

	if (wxWindow::FindFocus() == this)
		return true;
	else if (wxWindow::FindFocus() != nullptr && wxWindow::FindFocus()->GetParent() == this)
		return true;
	else
		return false;
}

void TASInputDlg::OnMouseUpR(wxMouseEvent& event)
{
	Stick* stick = nullptr;
	if (event.GetId() == ID_MAIN_STICK)
		stick = &MainStick;
	else if (event.GetId() == ID_C_STICK)
		stick = &CStick;

	if (stick == nullptr)
		return;

	stick->xCont.value = stick->xCont.defaultValue;
	stick->yCont.value = stick->yCont.defaultValue;
	stick->bitmap->SetBitmap(CreateStickBitmap(128,128));
	stick->xCont.Text->SetValue(std::to_string(stick->xCont.defaultValue));
	stick->yCont.Text->SetValue(std::to_string(stick->yCont.defaultValue));
	stick->xCont.Slider->SetValue(stick->xCont.defaultValue);
	stick->yCont.Slider->SetValue(stick->yCont.defaultValue);

	event.Skip(true);
}

void TASInputDlg::OnRightClickSlider(wxMouseEvent& event)
{
	for (unsigned int i = 0; i < 10; ++i)
	{
		if (Controls[i] != nullptr && event.GetId() == Controls[i]->Slider_ID)
		{
			Controls[i]->value = Controls[i]->defaultValue;
			Controls[i]->Slider->SetValue(Controls[i]->defaultValue);
			Controls[i]->Text->SetValue(std::to_string(Controls[i]->defaultValue));
		}
	}
}

void TASInputDlg::OnMouseDownL(wxMouseEvent& event)
{
	if (!event.LeftIsDown())
		return;

	Stick* stick;
	if (event.GetId() == ID_MAIN_STICK)
		stick = &MainStick;
	else if (event.GetId() == ID_C_STICK)
		stick = &CStick;
	else
		return;

	wxPoint ptM(event.GetPosition());
	stick->xCont.value = ptM.x * stick->xCont.range / 127;
	stick->yCont.value = ptM.y * stick->yCont.range / 127;

	if ((unsigned)stick->yCont.value > stick->yCont.range)
		stick->yCont.value = stick->yCont.range;
	if ((unsigned)stick->xCont.value > stick->xCont.range)
		stick->xCont.value = stick->xCont.range;

	if (stick->yCont.reverse)
		stick->yCont.value = stick->yCont.range - (u16)stick->yCont.value;
	if (stick->xCont.reverse)
		stick->xCont.value = stick->xCont.range - (u16)stick->xCont.value;

	stick->xCont.value = (unsigned)stick->xCont.value > stick->xCont.range ? stick->xCont.range : stick->xCont.value;
	stick->yCont.value = (unsigned)stick->yCont.value > stick->yCont.range ? stick->yCont.range : stick->yCont.value;
	stick->xCont.value = (unsigned)stick->xCont.value < 0 ? 0 : stick->xCont.value;
	stick->yCont.value = (unsigned)stick->yCont.value < 0 ? 0 : stick->yCont.value;

	int x = (u8)((double)stick->xCont.value / (double)stick->xCont.range * 255.0);
	int y = (u8)((double)stick->yCont.value / (double)stick->yCont.range * 255.0);

	stick->bitmap->SetBitmap(CreateStickBitmap(ptM.x*2, ptM.y*2));

	stick->xCont.Text->SetValue(std::to_string(stick->xCont.value));
	stick->yCont.Text->SetValue(std::to_string(stick->yCont.value));

	stick->xCont.Slider->SetValue(stick->xCont.value);
	stick->yCont.Slider->SetValue(stick->yCont.value);
	event.Skip(true);
}

void TASInputDlg::SetTurbo(wxMouseEvent& event)
{
	Button* button;
	for (unsigned int i = 0; i < 14; ++i)
	{
		if (Buttons[i] != nullptr && event.GetId() == Buttons[i]->ID)
			button = Buttons[i];
	}
	if (event.LeftDown())
	{
		button->TurboOn = false;
		event.Skip(true);
		return;
	}
	button->Checkbox->SetValue(true);
	button->TurboOn = !button->TurboOn;
	event.Skip(true);
}

void TASInputDlg::ButtonTurbo()
{
	static int frame = Movie::g_currentFrame;
	if (abs((int)(frame - (int)Movie::g_currentFrame)) > 2)
		frame = Movie::g_currentFrame;

	if (frame < Movie::g_currentFrame)
	{
		frame = Movie::g_currentFrame;
		for (unsigned int i = 0; i < 14; ++i)
		{
			if (Buttons[i] != nullptr && Buttons[i]->TurboOn)	
				Buttons[i]->Checkbox->SetValue(!Buttons[i]->Checkbox->GetValue());
		}
	}
}

wxBitmap TASInputDlg::CreateStickBitmap(int x, int y)
{
	x = x / 2;
	y = y / 2;

	wxMemoryDC memDC;
	wxBitmap bitmap(129, 129);
	memDC.SelectObject(bitmap);
	memDC.SetBackground(*wxLIGHT_GREY_BRUSH);
	memDC.Clear();
	memDC.SetBrush(*wxWHITE_BRUSH);
	memDC.DrawCircle(65, 65, 64);
	memDC.SetBrush(*wxRED_BRUSH);
	memDC.DrawLine(64, 64, x, y);
	memDC.DrawLine(63, 64, x - 1, y);
	memDC.DrawLine(65, 64, x + 1 , y);
	memDC.DrawLine(64, 63, x, y - 1);
	memDC.DrawLine(64, 65, x, y + 1);
	memDC.SetPen(*wxBLACK_PEN);
	memDC.CrossHair(64, 64);
	memDC.SetBrush(*wxBLUE_BRUSH);
	memDC.DrawCircle(x, y, 5);
	memDC.SelectObject(wxNullBitmap);
	return bitmap;
}
