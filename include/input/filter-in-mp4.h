 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2004 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#ifndef _H_FILTER_IN_MP4_
#define _H_FILTER_IN_MP4_

#include "inputfilter.h"
#include "filter-in-faad2.h"

#ifndef _MSC_VER
#include <stdint.h>
#else
#define int32_t long
#endif

#include <3rdparty/mp4/mp4.h>

class FilterInMP4 : public InputFilter
{
	private:
		MP4FileHandle		 mp4File;
		FilterInFAAD2		*aacFilter;

		Int			 GetAudioTrack();
	public:
					 FilterInMP4(bonkEncConfig *);
					~FilterInMP4();

		bool			 Activate();
		bool			 Deactivate();

		int			 ReadData(unsigned char **, int);

		bonkEncTrack		*GetFileInfo(String);
};

#endif