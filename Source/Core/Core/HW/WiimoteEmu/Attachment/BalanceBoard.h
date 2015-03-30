// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

namespace WiimoteEmu
{

class BalanceBoard : public Attachment
{
public:
	BalanceBoard(WiimoteEmu::ExtensionReg& _reg);
	void GetState(u8* const data) override;
	void Reset() override;

	enum
	{
		BUTTON_A     = 0x4,
	};

	static const u8 LEFT_STICK_CENTER_X = 0x20;
	static const u8 LEFT_STICK_CENTER_Y = 0x20;
	static const u8 LEFT_STICK_RADIUS = 0x1F;

	u8 calibration[0x1C];

private:
	Buttons*       m_buttons;
	Buttons*       m_dpad;
	MixedTriggers* m_triggers;
	Triggers*      m_top_right;
	Triggers*      m_top_left;
	Triggers*      m_bottom_right;
	Triggers*      m_bottom_left;
};

}
