/*
 * Copyright 2003-2016 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "FakeDecoderAPI.hxx"
#include "decoder/DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "Compiler.h"

#include <stdexcept>

#include <unistd.h>
#include <stdio.h>

void
decoder_initialized(Decoder &decoder,
		    const AudioFormat audio_format,
		    gcc_unused bool seekable,
		    SignedSongTime duration)
{
	struct audio_format_string af_string;

	assert(!decoder.initialized);
	assert(audio_format.IsValid());

	fprintf(stderr, "audio_format=%s duration=%f\n",
		audio_format_to_string(audio_format, &af_string),
		duration.ToDoubleS());

	decoder.initialized = true;
}

DecoderCommand
decoder_get_command(gcc_unused Decoder &decoder)
{
	return DecoderCommand::NONE;
}

void
decoder_command_finished(gcc_unused Decoder &decoder)
{
}

SongTime
decoder_seek_time(gcc_unused Decoder &decoder)
{
	return SongTime();
}

uint64_t
decoder_seek_where_frame(gcc_unused Decoder &decoder)
{
	return 1;
}

void
decoder_seek_error(gcc_unused Decoder &decoder)
{
}

InputStreamPtr
decoder_open_uri(Decoder &decoder, const char *uri)
{
	return InputStream::OpenReady(uri, decoder.mutex, decoder.cond);
}

size_t
decoder_read(gcc_unused Decoder *decoder,
	     InputStream &is,
	     void *buffer, size_t length)
{
	try {
		return is.LockRead(buffer, length);
	} catch (const std::runtime_error &) {
		return 0;
	}
}

bool
decoder_read_full(Decoder *decoder, InputStream &is,
		  void *_buffer, size_t size)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	while (size > 0) {
		size_t nbytes = decoder_read(decoder, is, buffer, size);
		if (nbytes == 0)
			return false;

		buffer += nbytes;
		size -= nbytes;
	}

	return true;
}

bool
decoder_skip(Decoder *decoder, InputStream &is, size_t size)
{
	while (size > 0) {
		char buffer[1024];
		size_t nbytes = decoder_read(decoder, is, buffer,
					     std::min(sizeof(buffer), size));
		if (nbytes == 0)
			return false;

		size -= nbytes;
	}

	return true;
}

void
decoder_timestamp(gcc_unused Decoder &decoder,
		  gcc_unused double t)
{
}

DecoderCommand
decoder_data(gcc_unused Decoder &decoder,
	     gcc_unused InputStream *is,
	     const void *data, size_t datalen,
	     gcc_unused uint16_t kbit_rate)
{
	static uint16_t prev_kbit_rate;
	if (kbit_rate != prev_kbit_rate) {
		prev_kbit_rate = kbit_rate;
		fprintf(stderr, "%u kbit/s\n", kbit_rate);
	}

	gcc_unused ssize_t nbytes = write(1, data, datalen);
	return DecoderCommand::NONE;
}

DecoderCommand
decoder_tag(gcc_unused Decoder &decoder,
	    gcc_unused InputStream *is,
	    Tag &&tag)
{
	fprintf(stderr, "TAG: duration=%f\n", tag.duration.ToDoubleS());

	for (const auto &i : tag)
		fprintf(stderr, "  %s=%s\n", tag_item_names[i.type], i.value);

	return DecoderCommand::NONE;
}

void
decoder_replay_gain(gcc_unused Decoder &decoder,
		    const ReplayGainInfo *rgi)
{
	const ReplayGainTuple *tuple = &rgi->tuples[REPLAY_GAIN_ALBUM];
	if (tuple->IsDefined())
		fprintf(stderr, "replay_gain[album]: gain=%f peak=%f\n",
			tuple->gain, tuple->peak);

	tuple = &rgi->tuples[REPLAY_GAIN_TRACK];
	if (tuple->IsDefined())
		fprintf(stderr, "replay_gain[track]: gain=%f peak=%f\n",
			tuple->gain, tuple->peak);
}

void
decoder_mixramp(gcc_unused Decoder &decoder, gcc_unused MixRampInfo &&mix_ramp)
{
	fprintf(stderr, "MixRamp: start='%s' end='%s'\n",
		mix_ramp.GetStart(), mix_ramp.GetEnd());
}
