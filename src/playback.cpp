 /* fre:ac - free audio converter
  * Copyright (C) 2001-2017 Robert Kausch <robert.kausch@freac.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU General Public License as
  * published by the Free Software Foundation, either version 2 of
  * the License, or (at your option) any later version.
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#include <playback.h>
#include <config.h>
#include <utilities.h>
#include <freac.h>

#include <jobs/engine/convert.h>

#include <engine/decoder.h>
#include <engine/processor.h>

using namespace BoCA;
using namespace BoCA::AS;

freac::Playback	*freac::Playback::instance = NIL;

freac::Playback::Playback()
{
	playing	    = False;
	paused	    = False;

	stop	    = False;

	output	    = NIL;

	newPosition = -1;
}

freac::Playback::~Playback()
{
}

freac::Playback *freac::Playback::Get()
{
	if (instance == NIL)
	{
		instance = new Playback();
	}

	return instance;
}

Void freac::Playback::Free()
{
	if (instance != NIL)
	{
		delete instance;

		instance = NIL;
	}
}

Void freac::Playback::Play(const Track &iTrack)
{
	if (JobConvert::IsConverting())
	{
		BoCA::Utilities::ErrorMessage("Cannot play a file while encoding!");

		return;
	}

	if (playing && paused && track.GetTrackID() == iTrack.GetTrackID())
	{
		Resume();

		return;
	}

	if (playing) Stop();

	track	= iTrack;

	playing	= True;
	paused	= False;

	stop	= False;

	NonBlocking0<>(&Playback::PlayThread, this).Call();
}

Int freac::Playback::PlayThread()
{
	BoCA::Config	*config = BoCA::Config::Copy();

	/* Get config values.
	 */
	Bool	 processPlayback = config->GetIntValue(Config::CategoryProcessingID, Config::ProcessingProcessPlaybackID, Config::ProcessingProcessPlaybackDefault);

	/* Create decoder.
	 */
	Decoder	*decoder = new Decoder(config);

	if (!decoder->Create(track.origFilename, track))
	{
		delete decoder;

		BoCA::Config::Free(config);

		playing = False;

		return Error();
	}

	/* Create processor.
	 */
	Track		 trackToPlay = track;
	Processor	*processor   = new Processor(config);

	if (processPlayback)
	{
		if (!processor->Create(track))
		{
			delete decoder;
			delete processor;

			BoCA::Config::Free(config);

			playing = False;

			return Error();
		}

		trackToPlay.SetFormat(processor->GetFormatInfo());
	}

	/* Create output component.
	 */
	Registry	&boca = Registry::Get();

	for (Int i = 0; i < boca.GetNumberOfComponents(); i++)
	{
		if (boca.GetComponentType(i) != BoCA::COMPONENT_TYPE_OUTPUT) continue;

		output = (OutputComponent *) boca.CreateComponentByID(boca.GetComponentID(i));

		if (output != NIL) break;
	}

	if (output == NIL)
	{
		delete decoder;
		delete processor;

		BoCA::Config::Free(config);

		playing = false;

		return Error();
	}

	output->SetConfiguration(config);
	output->SetAudioTrackInfo(trackToPlay);

	if (output->Activate())
	{
		/* Enter playback loop.
		 */
		if (!output->GetErrorState()) Loop(decoder, processor);

		/* Clean up.
		 */
		output->Deactivate();
	}

	boca.DeleteComponent(output);

	delete decoder;
	delete processor;

	BoCA::Config::Free(config);

	playing = false;

	return Success();
}

Void freac::Playback::Loop(Decoder *decoder, Processor *processor)
{
	BoCA::I18n	*i18n = BoCA::I18n::Get();

	/* Notify application about track playback.
	 */
	onPlay.Emit(track);

	/* Enter playback loop.
	 */
	const Format		&format		= track.GetFormat();

	Int64			 position	= 0;
	UnsignedLong		 samplesSize	= format.rate / 4;

	Int			 bytesPerSample = format.bits / 8;
	Buffer<UnsignedByte>	 buffer(samplesSize * bytesPerSample * format.channels);

	while (!stop)
	{
		/* Seek if requested.
		 */
		if (newPosition >= 0)
		{
			if	(track.length	    >= 0) position = track.length		/ 1000 * newPosition;
			else if (track.approxLength >= 0) position = track.approxLength		/ 1000 * newPosition;
			else				  position = (Int64(240) * format.rate) / 1000 * newPosition;

			decoder->Seek(position);

			newPosition = -1;
		}

		/* Find step size.
		 */
		Int	 step = samplesSize;

		if (track.length >= 0)
		{
			if (position >= track.length) break;

			if (position + step > track.length) step = track.length - position;
		}

		buffer.Resize(step * bytesPerSample * format.channels);

		/* Read samples from decoder.
		 */
		Int	 bytes = decoder->Read(buffer);

		if (bytes == 0) break;

		/* Transform samples using processor.
		 */
		if (processor != NIL) processor->Transform(buffer);

		/* Update position and write data.
		 */
		position += (bytes / bytesPerSample / format.channels);

		if	(track.length	    >= 0) onProgress.Emit(i18n->IsActiveLanguageRightToLeft() ? 1000 - 1000.0 / track.length	    * position : 1000.0 / track.length	      * position);
		else if (track.approxLength >= 0) onProgress.Emit(i18n->IsActiveLanguageRightToLeft() ? 1000 - 1000.0 / track.approxLength  * position : 1000.0 / track.approxLength  * position);
		else				  onProgress.Emit(i18n->IsActiveLanguageRightToLeft() ? 1000 - 1000.0 / (240 * format.rate) * position : 1000.0 / (240 * format.rate) * position);

		while (output->CanWrite() < buffer.Size() && !stop) S::System::System::Sleep(10);

		if (!stop) output->WriteData(buffer);
	}

	/* Finish sample transformations.
	 */
	buffer.Resize(0);

	if (processor != NIL) processor->Finish(buffer);

	/* Pass remaining samples to output.
	 */
	while (output->CanWrite() < buffer.Size() && !stop) S::System::System::Sleep(10);

	if (!stop) output->WriteData(buffer);
	if (!stop) output->Finish();

	while (!stop && output->IsPlaying()) S::System::System::Sleep(20);

	/* Notify application about finished playback.
	 */
	stop = True;

	onFinish.Emit(track);
}

Void freac::Playback::Pause()
{
	if (!playing) return;

	output->SetPause(True);

	paused = True;
}

Void freac::Playback::Resume()
{
	if (!playing) return;

	output->SetPause(False);

	paused = False;
}

Void freac::Playback::SetPosition(Int position)
{
	if (!playing) return;

	newPosition = position;
}

Void freac::Playback::Stop()
{
	if (!playing) return;

	if (!stop)
	{
		stop = True;

		while (playing) S::System::System::Sleep(10);
	}
}
