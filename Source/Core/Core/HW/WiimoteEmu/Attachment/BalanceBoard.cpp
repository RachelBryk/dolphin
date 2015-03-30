// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/BalanceBoard.h"
#include "Core/HW/WiimoteEmu/Attachment/Classic.h"

namespace WiimoteEmu
{

static const u8 bb_id[] = { 0x00, 0x00, 0xa4, 0x20, 0x04, 0x02 };
/* Balance Board calibration */
static const u8 bb_calibration[] =
{
	0x11, 0x69, 0x00, 0x00, 0x18, 0x61, 0x4D,
	0x8C, 0x1F, 0x48, 0x42, 0xC0, 0x1F, 0x2D,
	0x54, 0x16, 0x25, 0xe6, 0x49, 0xc0, 0x25,
	0xFD, 0x5A, 0xA7, 0x2C,	0x8F, 0x50, 0xC3
};

static const u16 bb_button_bitmasks[] =
{
	BalanceBoard::BUTTON_A,
};

static const char* const bb_button_names[] =
{
	"A",
};

static const char* const bb_trigger_names[] =
{
	"TR", "TL", "BR", "BL"
};

static const u16 classic_dpad_bitmasks[] =
{
	Classic::PAD_UP, Classic::PAD_DOWN, Classic::PAD_LEFT, Classic::PAD_RIGHT
};

BalanceBoard::BalanceBoard(WiimoteEmu::ExtensionReg& _reg) : Attachment(_trans("Balance Board"), _reg)
{
	// buttons
	groups.emplace_back(m_buttons = new Buttons("Buttons"));
	//for (auto& classic_button_name : bb_button_names)
	//	m_buttons->controls.emplace_back(new ControlGroup::Input(bb_button_names));

	// dpad
	groups.emplace_back(m_dpad = new Buttons("D-Pad"));
	for (auto& named_direction : named_directions)
		m_dpad->controls.emplace_back(new ControlGroup::Input(named_direction));


	// triggers
	groups.emplace_back(m_top_right = new Triggers(_trans("TR")));
	m_top_right->controls.emplace_back(new ControlGroup::Input(_trans("rename this")));

	groups.emplace_back(m_top_left = new Triggers(_trans("TL")));
	m_top_left->controls.emplace_back(new ControlGroup::Input(_trans("rename this")));

	groups.emplace_back(m_bottom_right = new Triggers(_trans("BR")));
	m_bottom_right->controls.emplace_back(new ControlGroup::Input(_trans("rename this")));

	groups.emplace_back(m_bottom_left = new Triggers(_trans("BL")));
	m_bottom_left->controls.emplace_back(new ControlGroup::Input(_trans("rename this")));

	// set up register
	// calibration
	memcpy(calibration, bb_calibration, sizeof(bb_calibration));
	// id
	memcpy(&id, bb_id, sizeof(bb_id));

}

void BalanceBoard::GetState(u8* const data)
{
	wm_bb_extension* const bbdata = (wm_bb_extension*)data;

	u16 blah=0;
	m_dpad->GetState(&blah, classic_dpad_bitmasks);

	// Just uses dpad buttons to switch between 0, 17, 34 kg on each sensor. Silly, but good enough for now.
	if (blah & Classic::PAD_UP)
	{
		// 34
		bbdata->topright = 0xfd25;
		bbdata->bottomright = 0xa75a;
		bbdata->topleft = 0x8f2c;
		bbdata->bottomleft = 0xc350;
	}
	else if (blah & Classic::PAD_DOWN)
	{
		//0
		bbdata->topright = 0x6118;
		bbdata->bottomright = 0x8c4d;
		bbdata->topleft = 0x481f;
		bbdata->bottomleft = 0xc042;
	}
	else
	{
		//17
		bbdata->topright = 0x2d1f;
		bbdata->bottomright = 0x1654;
		bbdata->topleft = 0xe625;
		bbdata->bottomleft = 0xc049;
	}

	// don't know what this is, just read this from my balance board..
	bbdata->unknown[0] = 0x3d;
	bbdata->unknown[1] = 0x86;
	bbdata->unknown[2] = 0x44;
	bbdata->unknown[3] = 0x3d;
	bbdata->unknown[4] = 0x28;
	bbdata->unknown[5] = 0xa2;
	bbdata->unknown[6] = 0xb2;
	bbdata->unknown[7] = 0xd7;
	bbdata->unknown[8] = 0x69;
	bbdata->unknown[9] = 0x86;
	bbdata->unknown[10] = 0x97;
}

void BalanceBoard::Reset()
{
	// set up register
	memset(&reg, 0, WIIMOTE_REG_EXT_SIZE);
	
	memcpy(reg.calibration, bb_calibration, 0x10);
	memcpy(reg.calibration2, bb_calibration+0x10, sizeof(bb_calibration)-0x10);
	// id
	memcpy(&id, bb_id, sizeof(bb_id));
	memcpy(reg.constant_id, bb_id, sizeof(bb_id));

	reg.unknown3[0] = 0x7e;
	reg.unknown3[1] = 0x5b;
	reg.unknown3[2] = 0x68;
	reg.unknown3[3] = 0x59;
	//reg.unknown3[4] = 0x3a;

	// read from my bb, dunno what it is
	reg.unknown4[10] = 0xe5;
	reg.unknown4[11] = 0x5f;
	reg.unknown4[12] = 0xf2;
	reg.unknown4[13] = 0x2e;
	reg.unknown4[14] = 0x79;
	reg.unknown4[15] = 0x20;
	reg.unknown4[16] = 0x57;
	reg.unknown4[17] = 0x81;
	reg.unknown4[18] = 0x8c;
	reg.unknown4[19] = 0x5e;
	reg.unknown4[20] = 0xf2;
	reg.unknown4[21] = 0x2e;
	reg.unknown4[22] = 0x79;
	reg.unknown4[23] = 0x20;
	reg.unknown4[24] = 0x57;
	reg.unknown4[25] = 0x81;
}



}
