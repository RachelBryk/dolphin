// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/HW/EXI.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceMemoryCard.h"
#include "Core/HW/GCMemcard.h"
#include "Core/HW/GCMemcardDirectory.h"
#include "Core/HW/GCMemcardRaw.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/Sram.h"
#include "DiscIO/NANDContentLoader.h"

#define MC_STATUS_BUSY              0x80
#define MC_STATUS_UNLOCKED          0x40
#define MC_STATUS_SLEEP             0x20
#define MC_STATUS_ERASEERROR        0x10
#define MC_STATUS_PROGRAMEERROR     0x08
#define MC_STATUS_READY             0x01
#define SIZE_TO_Mb (1024 * 8 * 16)

void CEXIMemoryCard::FlushCallback(u64 userdata, int cyclesLate)
{
	// note that userdata is forbidden to be a pointer, due to the implementation of EventDoState
	int card_index = (int)userdata;
	CEXIMemoryCard* pThis = (CEXIMemoryCard*)ExpansionInterface::FindDevice(EXIDEVICE_MEMORYCARD, card_index);
	if (pThis == nullptr)
		pThis = (CEXIMemoryCard*)ExpansionInterface::FindDevice(EXIDEVICE_MEMORYCARDFOLDER, card_index);
	if (pThis && pThis->memorycard)
		pThis->memorycard->Flush();
}

void CEXIMemoryCard::CmdDoneCallback(u64 userdata, int cyclesLate)
{
	int card_index = (int)userdata;
	CEXIMemoryCard* pThis = (CEXIMemoryCard*)ExpansionInterface::FindDevice(EXIDEVICE_MEMORYCARD, card_index);
	if (pThis == nullptr)
		pThis = (CEXIMemoryCard*)ExpansionInterface::FindDevice(EXIDEVICE_MEMORYCARDFOLDER, card_index);
	if (pThis)
		pThis->CmdDone();
}

CEXIMemoryCard::CEXIMemoryCard(const int index, bool gciFolder)
	: card_index(index)
	, m_bDirty(false)
{
	// we're potentially leaking events here, since there's no UnregisterEvent until emu shutdown, but I guess it's inconsequential
	et_this_card = CoreTiming::RegisterEvent((index == 0) ? "memcardFlushA" : "memcardFlushB", FlushCallback);
	et_cmd_done = CoreTiming::RegisterEvent((index == 0) ? "memcardDoneA" : "memcardDoneB", CmdDoneCallback);

	interruptSwitch = 0;
	m_bInterruptSet = 0;
	command = 0;
	status = MC_STATUS_BUSY | MC_STATUS_UNLOCKED | MC_STATUS_READY;
	m_uPosition = 0;
	memset(programming_buffer, 0, sizeof(programming_buffer));
	//Nintendo Memory Card EXI IDs
	//0x00000004 Memory Card 59     4Mbit
	//0x00000008 Memory Card 123    8Mb
	//0x00000010 Memory Card 251    16Mb
	//0x00000020 Memory Card 507    32Mb
	//0x00000040 Memory Card 1019   64Mb
	//0x00000080 Memory Card 2043   128Mb

	//0x00000510 16Mb "bigben" card
	//card_id = 0xc243;
	card_id = 0xc221; // It's a Nintendo brand memcard

	// The following games have issues with memory cards bigger than 16Mb
	// Darkened Skye GDQE6S GDQP6S
	// WTA Tour Tennis GWTEA4 GWTJA4 GWTPA4
	// Disney Sports : Skate Boarding GDXEA4 GDXPA4 GDXJA4
	// Disney Sports : Soccer GDKEA4
	// Use a 16Mb (251 block) memory card for these games
	bool useMC251;
	IniFile gameIni = Core::g_CoreStartupParameter.LoadGameIni();
	gameIni.GetOrCreateSection("Core")->Get("MemoryCard251", &useMC251, false);
	u16 sizeMb = useMC251 ? MemCard251Mb : MemCard2043Mb;

	if (gciFolder)
	{
		setupGciFolder(sizeMb);
	}
	else
	{
		setupRawMemcard(sizeMb);
	}

	memory_card_size = memorycard->GetCardId() * SIZE_TO_Mb;
	u8 header[20] = {0};
	memorycard->Read(0, ArraySize(header), header);
	SetCardFlashID(header, card_index);
}

void CEXIMemoryCard::setupGciFolder(u16 sizeMb)
{

	DiscIO::IVolume::ECountry CountryCode = DiscIO::IVolume::COUNTRY_UNKNOWN;
	auto strUniqueID = Core::g_CoreStartupParameter.m_strUniqueID;

	u32 CurrentGameId = 0;
	if (strUniqueID == TITLEID_SYSMENU_STRING)
	{
		const DiscIO::INANDContentLoader & SysMenu_Loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(TITLEID_SYSMENU, true);
		if (SysMenu_Loader.IsValid())
		{
			CountryCode = DiscIO::CountrySwitch(SysMenu_Loader.GetCountryChar());
		}
	}
	else if (strUniqueID.length() >= 4)
	{
		CountryCode = DiscIO::CountrySwitch(strUniqueID.at(3));
		memcpy((u8 *)&CurrentGameId, strUniqueID.c_str(), 4);
	}
	bool ascii = true;
	std::string strDirectoryName = File::GetUserPath(D_GCUSER_IDX);
	switch (CountryCode)
	{
	case DiscIO::IVolume::COUNTRY_JAPAN:
		ascii = false;
		strDirectoryName += JAP_DIR DIR_SEP;
		break;
	case DiscIO::IVolume::COUNTRY_USA:
		strDirectoryName += USA_DIR DIR_SEP;
		break;
	default:
		CountryCode = DiscIO::IVolume::COUNTRY_EUROPE;
		strDirectoryName += EUR_DIR DIR_SEP;
	}
	strDirectoryName += StringFromFormat("Card %c", 'A' + card_index);

	if (!File::Exists(strDirectoryName)) // first use of memcard folder, migrate automatically
	{
		MigrateFromMemcardFile(strDirectoryName + DIR_SEP, card_index);
	}
	else if (!File::IsDirectory(strDirectoryName))
	{
		if (File::Rename(strDirectoryName, strDirectoryName + ".original"))
		{
			PanicAlertT("%s was not a directory, moved to *.original", strDirectoryName.c_str());
			MigrateFromMemcardFile(strDirectoryName + DIR_SEP, card_index);
		}
		else // we tried but the user wants to crash
		{
			// TODO more user friendly abort
			PanicAlertT("%s is not a directory, failed to move to *.original.\n Verify your write permissions or move "
						"the file outside of dolphin",
						strDirectoryName.c_str());
			exit(0);
		}
	}

	memorycard = std::make_unique<GCMemcardDirectory>(strDirectoryName + DIR_SEP, card_index, sizeMb, ascii,
													  CountryCode, CurrentGameId);
}

void CEXIMemoryCard::setupRawMemcard(u16 sizeMb)
{
	std::string filename =
		(card_index == 0) ? SConfig::GetInstance().m_strMemoryCardA : SConfig::GetInstance().m_strMemoryCardB;
	if (Movie::IsPlayingInput() && Movie::IsConfigSaved() && Movie::IsUsingMemcard(card_index) &&
		Movie::IsStartingFromClearSave())
		filename = File::GetUserPath(D_GCUSER_IDX) + StringFromFormat("Movie%s.raw", (card_index == 0) ? "A" : "B");

	if (sizeMb == MemCard251Mb)
	{
		filename.insert(filename.find_last_of("."), ".251");
	}
	memorycard = std::make_unique<MemoryCard>(filename, card_index, sizeMb);
}

CEXIMemoryCard::~CEXIMemoryCard()
{
	CoreTiming::RemoveEvent(et_this_card);
	memorycard->Flush(true);
	memorycard.reset();
}

bool CEXIMemoryCard::IsPresent()
{
	return true;
}

void CEXIMemoryCard::CmdDone()
{
	status |= MC_STATUS_READY;
	status &= ~MC_STATUS_BUSY;

	m_bInterruptSet = 1;
	m_bDirty = true;
}

void CEXIMemoryCard::CmdDoneLater(u64 cycles)
{
	CoreTiming::RemoveEvent(et_cmd_done);
	CoreTiming::ScheduleEvent((int)cycles, et_cmd_done, (u64)card_index);
}

void CEXIMemoryCard::SetCS(int cs)
{
	// So that memory card won't be invalidated during flushing
	memorycard->joinThread();

	if (cs)  // not-selected to selected
	{
		m_uPosition = 0;
	}
	else
	{
		switch (command)
		{
		case cmdSectorErase:
			if (m_uPosition > 2)
			{
				memorycard->ClearBlock(address & (memory_card_size - 1));
				status |= MC_STATUS_BUSY;
				status &= ~MC_STATUS_READY;

				//???

				CmdDoneLater(5000);
			}
			break;

		case cmdChipErase:
			if (m_uPosition > 2)
			{
				// TODO: Investigate on HW, I (LPFaint99) believe that this only erases the system area (Blocks 0-4)
				memorycard->ClearAll();
				status &= ~MC_STATUS_BUSY;
				m_bDirty = true;
			}
			break;

		case cmdPageProgram:
			if (m_uPosition >= 5)
			{
				int count = m_uPosition - 5;
				int i=0;
				status &= ~0x80;

				while (count--)
				{
					memorycard->Write(address, 1, &(programming_buffer[i++]));
					i &= 127;
					address = (address & ~0x1FF) | ((address+1) & 0x1FF);
				}

				CmdDoneLater(5000);
			}

			// Page written to memory card, not just to buffer - let's schedule a flush 0.5b cycles into the future (1 sec)
			// But first we unschedule already scheduled flushes - no point in flushing once per page for a large write.
			CoreTiming::RemoveEvent(et_this_card);
			CoreTiming::ScheduleEvent(500000000, et_this_card, (u64)card_index);
			break;
		}
	}
}

bool CEXIMemoryCard::IsInterruptSet()
{
	if (interruptSwitch)
		return m_bInterruptSet;
	return false;
}

void CEXIMemoryCard::TransferByte(u8 &byte)
{
	DEBUG_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: > %02x", byte);
	if (m_uPosition == 0)
	{
		command = byte;  // first byte is command
		byte = 0xFF; // would be tristate, but we don't care.

		switch (command) // This seems silly, do we really need it?
		{
		case cmdNintendoID:
		case cmdReadArray:
		case cmdArrayToBuffer:
		case cmdSetInterrupt:
		case cmdWriteBuffer:
		case cmdReadStatus:
		case cmdReadID:
		case cmdReadErrorBuffer:
		case cmdWakeUp:
		case cmdSleep:
		case cmdClearStatus:
		case cmdSectorErase:
		case cmdPageProgram:
		case cmdExtraByteProgram:
		case cmdChipErase:
			INFO_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: command %02x at position 0. seems normal.", command);
			break;
		default:
			WARN_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: command %02x at position 0", command);
			break;
		}
		if (command == cmdClearStatus)
		{
			status &= ~MC_STATUS_PROGRAMEERROR;
			status &= ~MC_STATUS_ERASEERROR;

			status |= MC_STATUS_READY;

			m_bInterruptSet = 0;

			byte = 0xFF;
			m_uPosition = 0;
		}
	}
	else
	{
		switch (command)
		{
		case cmdNintendoID:
			//
			// Nintendo card:
			// 00 | 80 00 00 00 10 00 00 00
			// "bigben" card:
			// 00 | ff 00 00 05 10 00 00 00 00 00 00 00 00 00 00
			// we do it the Nintendo way.
			if (m_uPosition == 1)
				byte = 0x80; // dummy cycle
			else
				byte = (u8)(memorycard->GetCardId() >> (24 - (((m_uPosition - 2) & 3) * 8)));
			break;

		case cmdReadArray:
			switch (m_uPosition)
			{
			case 1: // AD1
				address = byte << 17;
				byte = 0xFF;
				break;
			case 2: // AD2
				address |= byte << 9;
				break;
			case 3: // AD3
				address |= (byte & 3) << 7;
				break;
			case 4: // BA
				address |= (byte & 0x7F);
				break;
			}
			if (m_uPosition > 1) // not specified for 1..8, anyway
			{
				memorycard->Read(address & (memory_card_size - 1), 1, &byte);
				// after 9 bytes, we start incrementing the address,
				// but only the sector offset - the pointer wraps around
				if (m_uPosition >= 9)
					address = (address & ~0x1FF) | ((address+1) & 0x1FF);
			}
			break;

		case cmdReadStatus:
			// (unspecified for byte 1)
			byte = status;
			break;

		case cmdReadID:
			if (m_uPosition == 1) // (unspecified)
				byte = (u8)(card_id >> 8);
			else
				byte = (u8)((m_uPosition & 1) ? (card_id) : (card_id >> 8));
			break;

		case cmdSectorErase:
			switch (m_uPosition)
			{
			case 1: // AD1
				address = byte << 17;
				break;
			case 2: // AD2
				address |= byte << 9;
				break;
			}
			byte = 0xFF;
			break;

		case cmdSetInterrupt:
			if (m_uPosition == 1)
			{
				interruptSwitch = byte;
			}
			byte = 0xFF;
			break;

		case cmdChipErase:
			byte = 0xFF;
			break;

		case cmdPageProgram:
			switch (m_uPosition)
			{
			case 1: // AD1
				address = byte << 17;
				break;
			case 2: // AD2
				address |= byte << 9;
				break;
			case 3: // AD3
				address |= (byte & 3) << 7;
				break;
			case 4: // BA
				address |= (byte & 0x7F);
				break;
			}

			if (m_uPosition >= 5)
				programming_buffer[((m_uPosition - 5) & 0x7F)] = byte; // wrap around after 128 bytes

			byte = 0xFF;
			break;

		default:
			WARN_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: unknown command byte %02x\n", byte);
			byte = 0xFF;
		}
	}
	m_uPosition++;
	DEBUG_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: < %02x", byte);
}

void CEXIMemoryCard::PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock)
	{
		// we don't exactly have anything to pause,
		// but let's make sure the flush thread isn't running.
		memorycard->joinThread();
	}
}

void CEXIMemoryCard::DoState(PointerWrap &p)
{
	// for movie sync, we need to save/load memory card contents (and other data) in savestates.
	// otherwise, we'll assume the user wants to keep their memcards and saves separate,
	// unless we're loading (in which case we let the savestate contents decide, in order to stay aligned with them).
	bool storeContents = (Movie::IsRecordingInput() || Movie::IsPlayingInput());
	p.Do(storeContents);

	if (storeContents)
	{
		p.Do(interruptSwitch);
		p.Do(m_bInterruptSet);
		p.Do(command);
		p.Do(status);
		p.Do(m_uPosition);
		p.Do(programming_buffer);
		p.Do(m_bDirty);
		p.Do(address);
		memorycard->DoState(p);
		p.Do(card_index);
	}
}

IEXIDevice* CEXIMemoryCard::FindDevice(TEXIDevices device_type, int customIndex)
{
	if (device_type != m_deviceType)
		return nullptr;
	if (customIndex != card_index)
		return nullptr;
	return this;
}

// DMA reads are preceded by all of the necessary setup via IMMRead
// read all at once instead of single byte at a time as done by IEXIDevice::DMARead
void CEXIMemoryCard::DMARead(u32 _uAddr, u32 _uSize)
{
	memorycard->Read(address, _uSize, Memory::GetPointer(_uAddr));
}

// DMA write are preceded by all of the necessary setup via IMMWrite
// write all at once instead of single byte at a time as done by IEXIDevice::DMAWrite
void CEXIMemoryCard::DMAWrite(u32 _uAddr, u32 _uSize)
{
	memorycard->Write(address, _uSize, Memory::GetPointer(_uAddr));
}
