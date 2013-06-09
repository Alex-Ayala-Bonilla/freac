 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2012 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#ifndef H_BONKENC_MAIN_THREADS
#define H_BONKENC_MAIN_THREADS

#include <smooth.h>

using namespace smooth;
using namespace smooth::GUI;

namespace BonkEnc
{
	class LayerThreads : public Layer
	{
		private:
			Text		*text_progress;
			ListBox		*list_threads;

			Text		*text_errors;
			EditBox		*edit_errors;
			ComboBox	*combo_errors;
			Button		*button_details;
		slots:
			Void		 ShowDetails();

			Void		 OnChangeJobs();

			Void		 OnChangeSize(const Size &);
		public:
					 LayerThreads();
					~LayerThreads();
	};
};

#endif