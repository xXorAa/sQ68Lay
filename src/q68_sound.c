/*
 * Copyright (c) 2023 Graeme Gregory
 *
 * SPDX: GPL-2.0-only
 */

#include <SDL3/SDL.h>

#include "emulator_logging.h"
#include "emulator_options.h"

SDL_AudioDeviceID audio_dev = 0;
SDL_AudioStream *sss_stream = NULL;

bool q68InitSound(void)
{
	SDL_AudioSpec audio_spec, spec;

	SDL_LogDebug(Q68_LOG_SOUND, "Init SSS sound");
	audio_dev =
		SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	if (!audio_dev) {
		SDL_LogError(Q68_LOG_SOUND, "Couldn't open audio device: %s",
			     SDL_GetError());
		return false;
	}

	if (!SDL_GetAudioDeviceFormat(audio_dev, &audio_spec, NULL)) {
		SDL_LogError(QLAY_LOG_SOUND,
			     "Couldn't get audio device format: %s",
			     SDL_GetError());
		SDL_CloseAudioDevice(audio_dev);
		return false;
	}

	spec.channels = 2;
	spec.format = SDL_AUDIO_U8;
	spec.freq = 20000;

	double gain = emulatorOptionInt("sssvol");
	if (gain < 0.0) {
		gain = 0.0;
	} else if (gain > 10.0) {
		gain = 10.0;
	}

	gain /= 10.0; // Normalize gain to 0.0 - 1.0

	sss_stream = SDL_CreateAudioStream(&spec, &audio_spec);
	if (!sss_stream) {
		SDL_LogError(Q68_LOG_SOUND, "Couldn't create audio stream: %s",
			     SDL_GetError());
		return false;
	}

	// Set the volume for this stream
	SDL_SetAudioStreamGain(sss_stream, gain);

	if (!SDL_BindAudioStream(audio_dev, sss_stream)) {
		SDL_LogError(QLAY_LOG_SOUND, "Couldn't bind audio stream: %s",
			     SDL_GetError());
		SDL_DestroyAudioStream(sss_stream);
		return false;
	}

	return true;
}

static Uint8 sound_buffer[2] = { 0 };

void q68PlayByte(int channel, Uint8 byte)
{
	switch (channel) {
	case 0:
		if (sss_stream) {
			SDL_LogDebug(Q68_LOG_SOUND, "Left SSS sound %02x",
				     byte);
			sound_buffer[1] = byte;
			SDL_PutAudioStreamData(sss_stream, sound_buffer,
					       sizeof(sound_buffer));
		}
		break;
	case 1:
		if (sss_stream) {
			SDL_LogDebug(Q68_LOG_SOUND, "Right SSS sound %02x",
				     byte);
			sound_buffer[0] = byte;
		}
		break;
	default:
		SDL_LogError(Q68_LOG_SOUND, "Unknown SSS channel %d", channel);
		break;
	}
}
