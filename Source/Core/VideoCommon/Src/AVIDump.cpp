// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#if defined(__FreeBSD__)
#define __STDC_CONSTANT_MACROS 1
#endif 

#include "AVIDump.h"
#include "HW/VideoInterface.h" //for TargetRefreshRate
#include "VideoConfig.h"

#include "..\..\AudioCommon\Src\AudioCommon.h" // for m_DumpAudioToAVI
#include "..\..\Core\Src\ConfigManager.h" // for EuRGB60

#ifdef _WIN32

#include "tchar.h"

#include <cstdio>
#include <cstring>
#include <vfw.h>
#include <winerror.h>

#include "FileUtil.h"
#include "CommonPaths.h"
#include "Log.h"

HWND m_emuWnd;
LONG m_byteBuffer;
LONG m_frameCount;
LONG m_frameCountNoSplit; // tracks across 2gig splits
LONG m_totalBytes;
PAVIFILE m_file;
int m_width;
int m_height;
int m_fileCount;
int m_samplesSound;
PAVISTREAM m_stream;
PAVISTREAM m_streamCompressed;
PAVISTREAM m_streamSound;
int framerate;

AVISTREAMINFO m_header;
AVISTREAMINFO m_soundHeader;
AVICOMPRESSOPTIONS m_options;
AVICOMPRESSOPTIONS *m_arrayOptions[1];
BITMAPINFOHEADER m_bitmap;
std::FILE *timecodes;


bool AVIDump::Start(HWND hWnd, int w, int h)
{
	m_emuWnd = hWnd;
	m_fileCount = 0;

	m_width = w;
	m_height = h;

	m_frameCountNoSplit = 0;

	if (SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.E60"))
		framerate = 60; // always 60, for either pal60 or ntsc
	else
		framerate = VideoInterface::TargetRefreshRate; // 50 or 60, depending on region

	if (!ac_Config.m_DumpAudioToAVI)
	{
		timecodes = std::fopen ((File::GetUserPath (D_DUMPFRAMES_IDX) + "timecodes.txt").c_str (), "w");
		if (!timecodes)
			return false;
		std::fprintf (timecodes, "# timecode format v2\n");
	}

	// clear CFR frame cache on start, not on file create (which is also segment switch)
	SetBitmapFormat ();
	if (ac_Config.m_DumpAudioToAVI)
		StoreFrame (NULL);

	return CreateFile();
}

bool AVIDump::CreateFile()
{
	m_totalBytes = 0;
	m_frameCount = 0;
	m_samplesSound = 0;
	char movie_file_name[255];
	sprintf(movie_file_name, "%sframedump%d.avi", File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), m_fileCount);
	// Create path
	File::CreateFullPath(movie_file_name);

	// Ask to delete file
	if (File::Exists(movie_file_name))
	{
		if (AskYesNoT("Delete the existing file '%s'?", movie_file_name))
			File::Delete(movie_file_name);
	}

	AVIFileInit();
	NOTICE_LOG(VIDEO, "Opening AVI file (%s) for dumping", movie_file_name);
	// TODO: Make this work with AVIFileOpenW without it throwing REGDB_E_CLASSNOTREG
	HRESULT hr = AVIFileOpenA(&m_file, movie_file_name, OF_WRITE | OF_CREATE, NULL);
	if (FAILED(hr))
	{
		if (hr == AVIERR_BADFORMAT) NOTICE_LOG(VIDEO, "The file couldn't be read, indicating a corrupt file or an unrecognized format."); 
		if (hr == AVIERR_MEMORY)  NOTICE_LOG(VIDEO, "The file could not be opened because of insufficient memory."); 
		if (hr == AVIERR_FILEREAD) NOTICE_LOG(VIDEO, "A disk error occurred while reading the file."); 
		if (hr == AVIERR_FILEOPEN) NOTICE_LOG(VIDEO, "A disk error occurred while opening the file.");
		if (hr == REGDB_E_CLASSNOTREG) NOTICE_LOG(VIDEO, "AVI class not registered");
		Stop();
		return false;
	}

	SetBitmapFormat();

	NOTICE_LOG(VIDEO, "Setting video format...");
	if (!SetVideoFormat())
	{
		NOTICE_LOG(VIDEO, "Setting video format failed");
		Stop();
		return false;
	}

	if (!m_fileCount) {
		if (!SetCompressionOptions()) {
			NOTICE_LOG(VIDEO, "SetCompressionOptions failed");
			Stop();
			return false;
		}
	}

	if (FAILED(AVIMakeCompressedStream(&m_streamCompressed, m_stream, &m_options, NULL)))
	{
		NOTICE_LOG(VIDEO, "AVIMakeCompressedStream failed");
		Stop();
		return false;
	}

	if (FAILED(AVIStreamSetFormat(m_streamCompressed, 0, &m_bitmap, m_bitmap.biSize)))
	{
		NOTICE_LOG(VIDEO, "AVIStreamSetFormat failed");
		Stop();
		return false;
	}

	if (ac_Config.m_DumpAudioToAVI)
	{
		WAVEFORMATEX wfex;
		wfex.cbSize = sizeof (wfex);
		wfex.nAvgBytesPerSec = 48000 * 4;
		wfex.nBlockAlign = 4;
		wfex.nChannels = 2;
		wfex.nSamplesPerSec = 48000;
		wfex.wBitsPerSample = 16;
		wfex.wFormatTag = WAVE_FORMAT_PCM;

		ZeroMemory (&m_soundHeader, sizeof (AVISTREAMINFO));
        m_soundHeader.fccType         = streamtypeAUDIO;
        m_soundHeader.dwQuality       = (DWORD)-1;
        m_soundHeader.dwScale         = wfex.nBlockAlign;
        m_soundHeader.dwInitialFrames = 1;
        m_soundHeader.dwRate       = wfex.nAvgBytesPerSec;
        m_soundHeader.dwSampleSize = wfex.nBlockAlign;

		if (FAILED(AVIFileCreateStream (m_file, &m_streamSound, &m_soundHeader)))
		{
			NOTICE_LOG(VIDEO, "AVIFileCreateStream failed (sound)");
			Stop();
			return false;
		}
        if (FAILED(AVIStreamSetFormat(m_streamSound, 0, &wfex, sizeof(WAVEFORMATEX))))
		{
			NOTICE_LOG(VIDEO, "AVIStreamSetFormat failed (sound)");
			Stop();
			return false;
		}
	}
	return true;
}

void AVIDump::CloseFile()
{
	if (timecodes)
	{
		std::fclose (timecodes);
		timecodes = 0;
	}
	if (m_streamCompressed)
	{
		AVIStreamClose(m_streamCompressed);
		m_streamCompressed = NULL;
	}

	if (m_stream)
	{
		AVIStreamClose(m_stream);
		m_stream = NULL;
	}

	if (m_streamSound)
	{
		AVIStreamClose (m_streamSound);
		m_streamSound = NULL;
	}

	if (m_file)
	{
		AVIFileRelease(m_file);
		m_file = NULL;
	}

	AVIFileExit();
}

void AVIDump::Stop()
{
	// flush any leftover sound data
	if (m_streamSound)
		AddSoundInternal (NULL, 0);
	// store one copy of the last video frame, CFR case
	if (ac_Config.m_DumpAudioToAVI && m_streamCompressed)
		AVIStreamWrite(m_streamCompressed, m_frameCount++, 1, GetFrame (), m_bitmap.biSizeImage, AVIIF_KEYFRAME, NULL, &m_byteBuffer);

	CloseFile();
	m_fileCount = 0;
	NOTICE_LOG(VIDEO, "Stop");
}

// DUMP HACK
#include "CoreTiming.h"
#include "HW\SystemTimers.h"

void AVIDump::AddSoundBE (const short *data, int nsamp, int rate)
{
	/* do these checks in AddSound
	if (!m_streamSound)
		return;
	*/

	static short *buff = NULL;
	static int nsampf = 0;
	if (nsampf < nsamp)
	{
		buff = (short *) realloc (buff, nsamp * 4);
		nsampf = nsamp;
	}
	if (buff)
	{
		for (int i = 0; i < nsamp * 2; i++)
			buff[i] = Common::swap16 (data[i]);
		AVIDump::AddSound (buff, nsamp, rate);
	}
}

// interleave is extra big (10s) because the buffer must be able to store potentially quite a bit of data,
// audio before the video dump starts
const int soundinterleave = 480000;

void AVIDump::AddSoundInternal (const short *data, int nsamp)
{
	// each write to avi takes packet overhead.  so writing every 8 samples wastes lots of space
	static short *buff = NULL;
	if (!buff)
		buff = (short *) malloc (soundinterleave * 4);
	if (!buff)
		return;

	static int buffpos = 0;

	if (data)
	{
		while (nsamp)
		{
			while (buffpos < soundinterleave * 2 && nsamp)
			{
				buff[buffpos++] = *data++;
				buff[buffpos++] = *data++;
				nsamp--;
			}
			if (buffpos == soundinterleave * 2)
			{
				if (m_streamSound)
					AVIStreamWrite (m_streamSound, m_samplesSound, soundinterleave, buff, soundinterleave * 4, 0, NULL, &m_byteBuffer);
				else
					; // audio stream should have been ready by now, nothing to be done
				
				m_totalBytes += m_byteBuffer;
				m_samplesSound += soundinterleave;
				buffpos = 0;
			}
		}
	}
	else if (buffpos)// data = NULL: flush
	{
		if (m_streamSound)
			AVIStreamWrite (m_streamSound, m_samplesSound, buffpos / 2, buff, buffpos * 2, 0, NULL, &m_byteBuffer);
		else
			; // shouldn't happen?
		m_totalBytes += m_byteBuffer;
		m_samplesSound += buffpos / 2;
		buffpos = 0;
	}

}


void AVIDump::AddSound (const short *data, int nsamp, int rate)
{
	/* can't do this.  when dumping sound to avi, the first sound can arrive before the video stream is prepped
	if (!m_streamSound) */
	if (!g_ActiveConfig.bDumpFrames)
		return;

	static short *buff = NULL;
	static int nsampf = 0;

	static short old[2] = {0, 0};

	// resample
	if (rate == 32000)
	{
		if (nsamp % 2)
			return;

		// 2->3
		if (nsampf < nsamp * 3 / 2)
		{
			buff = (short *) realloc (buff, nsamp * 3 / 2 * 4);
			nsampf = nsamp * 3 / 2;
		}
		if (!buff)
			return;

		for (int i = 0; i < nsamp / 2; i++)
		{
			buff[6 * i + 0] = (int) data[4 * i + 0] * 2 / 3 + old[0] * 1 / 3;
			buff[6 * i + 1] = (int) data[4 * i + 1] * 2 / 3 + old[1] * 1 / 3;
			buff[6 * i + 2] = (int) data[4 * i + 0] * 2 / 3 + data[4 * i + 2] * 1 / 3;
			buff[6 * i + 3] = (int) data[4 * i + 1] * 2 / 3 + data[4 * i + 3] * 1 / 3;
			buff[6 * i + 4] = data[4 * i + 2];
			buff[6 * i + 5] = data[4 * i + 3];

			old[0] = data[4 * i + 2];
			old[1] = data[4 * i + 3];
		}

		nsamp = nsamp * 3 / 2;
		data = buff;
	}

	AddSoundInternal (data, nsamp);

}

// the CFR dump design doesn't let you dump until you know the NEXT timecode.
// so we have to save a frame and always be behind

void *storedframe = 0;
unsigned storedframesz = 0;


void AVIDump::StoreFrame (const void *data)
{
	if (m_bitmap.biSizeImage > storedframesz)
	{
		storedframe = realloc (storedframe, m_bitmap.biSizeImage);
		storedframesz = m_bitmap.biSizeImage;
		memset (storedframe, 0, m_bitmap.biSizeImage);
	}
	if (storedframe)
	{
		if (data)
			memcpy (storedframe, data, m_bitmap.biSizeImage);
		else // pitch black frame
			memset (storedframe, 0, m_bitmap.biSizeImage);
	}

}

void *AVIDump::GetFrame (void)
{
	return storedframe;
}


void AVIDump::AddFrame(char *data)
{

	if (!ac_Config.m_DumpAudioToAVI)
	{
		// write timecode
		u64 now = CoreTiming::GetTicks ();

		u64 persec = SystemTimers::GetTicksPerSecond ();


		u64 num = now * 1000 / persec;

		now -= num * (persec / 1000);

		u64 den = ((now * 1000000000 + persec / 2) / persec) % 1000000;

		std::fprintf (timecodes, "%I64u.%06I64u\n", num, den);


		AVIStreamWrite(m_streamCompressed, m_frameCount++, 1, (LPVOID) data, m_bitmap.biSizeImage, AVIIF_KEYFRAME, NULL, &m_byteBuffer);
		m_totalBytes += m_byteBuffer;
		// Close the recording if the file is more than 2gb
		// VfW can't properly save files over 2gb in size, but can keep writing to them up to 4gb.
		if (m_totalBytes >= 2000000000)
		{
			CloseFile();
			m_fileCount++;
			CreateFile();
		}
		
	}
	else
	{
		static std::FILE *fff = 0;
		
		if (!fff)
		{
			fff = fopen ("cfrdbg.txt", "w");
			fprintf (fff, "now,oneCFR,CFRstart\n");
		}
		
		// no timecodes, instead dump each frame as many/few times as needed to keep sync

		u64 now = CoreTiming::GetTicks ();
		fprintf (fff, "%I64u,", now);

		u64 oneCFR = SystemTimers::GetTicksPerSecond () / framerate;

		fprintf (fff, "%I64u,", oneCFR);

		u64 CFRstart = oneCFR * m_frameCountNoSplit;

		fprintf (fff,"%I64u\n", CFRstart);

		int nplay = 0;

		s64 delta = now - CFRstart;

		// try really hard to place one copy of frame in stream (otherwise it's dropped)
		if (delta > (s64) oneCFR * 3 / 10) // place if 3/10th of a frame space
		{
			delta -= oneCFR;
			nplay++;
		}
		// try not nearly so hard to place additional copies of the frame
		while (delta > (s64) oneCFR * 8 / 10) // place if 8/10th of a frame space
		{
			delta -= oneCFR;
			nplay++;
		}

		while (nplay--)
		{
			m_frameCountNoSplit++;
			AVIStreamWrite(m_streamCompressed, m_frameCount++, 1, GetFrame (), m_bitmap.biSizeImage, AVIIF_KEYFRAME, NULL, &m_byteBuffer);
			m_totalBytes += m_byteBuffer;
			// Close the recording if the file is more than 2gb
			// VfW can't properly save files over 2gb in size, but can keep writing to them up to 4gb.
			if (m_totalBytes >= 2000000000)
			{
				CloseFile();
				m_fileCount++;
				CreateFile();
			}
		}
		StoreFrame (data);
	}
}

void AVIDump::SetBitmapFormat()
{
	memset(&m_bitmap, 0, sizeof(m_bitmap));
	m_bitmap.biSize = 0x28;
	m_bitmap.biPlanes = 1;
	m_bitmap.biBitCount = 24;
	m_bitmap.biWidth = m_width;
	m_bitmap.biHeight = m_height;
	m_bitmap.biSizeImage = 3 * m_width * m_height;
}

bool AVIDump::SetCompressionOptions()
{
	memset(&m_options, 0, sizeof(m_options));
	m_arrayOptions[0] = &m_options;

	return (AVISaveOptions(m_emuWnd, 0, 1, &m_stream, m_arrayOptions) != 0);
}

bool AVIDump::SetVideoFormat()
{
	memset(&m_header, 0, sizeof(m_header));
	m_header.fccType = streamtypeVIDEO;
	m_header.dwScale = 1;
	m_header.dwRate = framerate;
	m_header.dwSuggestedBufferSize  = m_bitmap.biSizeImage;

	return SUCCEEDED(AVIFileCreateStream(m_file, &m_stream, &m_header));
}

#else

#include "FileUtil.h"
#include "StringUtil.h"
#include "Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

AVFormatContext *s_FormatContext = NULL;
AVStream *s_Stream = NULL;
AVFrame *s_BGRFrame = NULL, *s_YUVFrame = NULL;
uint8_t *s_YUVBuffer = NULL;
uint8_t *s_OutBuffer = NULL;
int s_width;
int s_height;
int s_size;

static void InitAVCodec()
{
	static bool first_run = true;
	if (first_run)
	{
		av_register_all();
		first_run = false;
	}
}

bool AVIDump::Start(int w, int h)
{
	s_width = w;
	s_height = h;

	InitAVCodec();
	return CreateFile();
}

bool AVIDump::CreateFile()
{
	AVCodec *codec = NULL;

	s_FormatContext = avformat_alloc_context();
	snprintf(s_FormatContext->filename, sizeof(s_FormatContext->filename), "%s",
			(File::GetUserPath(D_DUMPFRAMES_IDX) + "framedump0.avi").c_str());
	File::CreateFullPath(s_FormatContext->filename);

	if (!(s_FormatContext->oformat = av_guess_format("avi", NULL, NULL)) ||
			!(s_Stream = av_new_stream(s_FormatContext, 0)))
	{
		CloseFile();
		return false;
	}

	s_Stream->codec->codec_id =
		g_Config.bUseFFV1 ?  CODEC_ID_FFV1 : s_FormatContext->oformat->video_codec;
	s_Stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
	s_Stream->codec->bit_rate = 400000;
	s_Stream->codec->width = s_width;
	s_Stream->codec->height = s_height;
	s_Stream->codec->time_base = (AVRational){1, static_cast<int>(VideoInterface::TargetRefreshRate)};
	s_Stream->codec->gop_size = 12;
	s_Stream->codec->pix_fmt = g_Config.bUseFFV1 ? PIX_FMT_BGRA : PIX_FMT_YUV420P;

	if (!(codec = avcodec_find_encoder(s_Stream->codec->codec_id)) ||
			(avcodec_open(s_Stream->codec, codec) < 0))
	{
		CloseFile();
		return false;
	}

	s_BGRFrame = avcodec_alloc_frame();
	s_YUVFrame = avcodec_alloc_frame();

	s_size = avpicture_get_size(s_Stream->codec->pix_fmt, s_width, s_height);

	s_YUVBuffer = new uint8_t[s_size];
	avpicture_fill((AVPicture *)s_YUVFrame, s_YUVBuffer, s_Stream->codec->pix_fmt, s_width, s_height);

	s_OutBuffer = new uint8_t[s_size];

	NOTICE_LOG(VIDEO, "Opening file %s for dumping", s_FormatContext->filename);
	if (avio_open(&s_FormatContext->pb, s_FormatContext->filename, AVIO_FLAG_WRITE) < 0)
	{
		WARN_LOG(VIDEO, "Could not open %s", s_FormatContext->filename);
		CloseFile();
		return false;
	}

	avformat_write_header(s_FormatContext, NULL);

	return true;
}

void AVIDump::AddFrame(uint8_t *data, int width, int height)
{
	avpicture_fill((AVPicture *)s_BGRFrame, data, PIX_FMT_BGR24, width, height);

	// Convert image from BGR24 to desired pixel format, and scale to initial
	// width and height
	struct SwsContext *s_SwsContext;
	if ((s_SwsContext = sws_getContext(width, height, PIX_FMT_BGR24, s_width, s_height,
					s_Stream->codec->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL)))
	{
		sws_scale(s_SwsContext, s_BGRFrame->data, s_BGRFrame->linesize, 0,
				height, s_YUVFrame->data, s_YUVFrame->linesize);
		sws_freeContext(s_SwsContext);
	}

	// Encode and write the image
	int outsize = avcodec_encode_video(s_Stream->codec, s_OutBuffer, s_size, s_YUVFrame);
	while (outsize > 0)
	{
		AVPacket pkt;
		av_init_packet(&pkt);

		if (s_Stream->codec->coded_frame->pts != (unsigned int)AV_NOPTS_VALUE)
			pkt.pts = av_rescale_q(s_Stream->codec->coded_frame->pts,
					s_Stream->codec->time_base, s_Stream->time_base);
		if(s_Stream->codec->coded_frame->key_frame)
			pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = s_Stream->index;
		pkt.data = s_OutBuffer;
		pkt.size = outsize;

		// Write the compressed frame in the media file
		av_interleaved_write_frame(s_FormatContext, &pkt);

		// Encode delayed frames
		outsize = avcodec_encode_video(s_Stream->codec, s_OutBuffer, s_size, NULL);
	}
}

void AVIDump::Stop()
{
	av_write_trailer(s_FormatContext);
	CloseFile();
	NOTICE_LOG(VIDEO, "Stopping frame dump");
}

void AVIDump::CloseFile()
{
	if (s_Stream)
	{
		if (s_Stream->codec)
			avcodec_close(s_Stream->codec);
		av_free(s_Stream);
		s_Stream = NULL;
	}

	if (s_YUVBuffer)
		delete[] s_YUVBuffer;
	s_YUVBuffer = NULL;

	if (s_OutBuffer)
		delete[] s_OutBuffer;
	s_OutBuffer = NULL;

	if (s_BGRFrame)
		av_free(s_BGRFrame);
	s_BGRFrame = NULL;

	if (s_YUVFrame)
		av_free(s_YUVFrame);
	s_YUVFrame = NULL;

	if (s_FormatContext)
	{
		if (s_FormatContext->pb)
			avio_close(s_FormatContext->pb);
		av_free(s_FormatContext);
		s_FormatContext = NULL;
	}
}

#endif
