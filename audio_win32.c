/*
 * mad - MPEG audio decoder
 * Copyright (C) 2000 Robert Leslie
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: audio_win32.c,v 1.22 2000/05/11 04:17:30 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <windows.h>
# include <mmsystem.h>

# include "libmad.h"
# include "audio.h"

static HWAVEOUT wave_handle;

# define NBUFFERS  3

static struct buffer {
  WAVEHDR wave_header;
  HANDLE event_handle;
  unsigned char pcm_data[MAX_NSAMPLES * 2 * 2];
} output[NBUFFERS];

static int buffer;
static int stereo;

static
char const *error_text(MMRESULT result)
{
  static char buffer[MAXERRORLENGTH];

  if (waveOutGetErrorText(result, buffer, sizeof(buffer)) != MMSYSERR_NOERROR)
    return "error getting error text";

  return buffer;
}

static
int init(struct audio_init *init)
{
  int i;

  wave_handle = 0;

  for (i = 0; i < NBUFFERS; ++i) {
    output[i].event_handle = CreateEvent(0, FALSE /* auto reset */,
					 TRUE /* initial state */, 0);
    if (output[i].event_handle == NULL) {
      while (i--)
	CloseHandle(output[i].event_handle);

      audio_error = "failed to create synchronization object";
      return -1;
    }
  }

  buffer = 0;
  stereo = 0;

  return 0;
}

static
void CALLBACK callback(HWAVEOUT handle, UINT message, DWORD data,
		       DWORD param1, DWORD param2)
{
  WAVEHDR *header;
  struct buffer *buffer;

  switch (message) {
  case WOM_DONE:
    header = (WAVEHDR *) param1;
    buffer = (struct buffer *) header->dwUser;

    if (SetEvent(buffer->event_handle) == 0) {
      /* error */
    }
    break;

  case WOM_OPEN:
  case WOM_CLOSE:
    break;
  }
}

static
int open_dev(HWAVEOUT *handle, unsigned short channels, unsigned int speed)
{
  WAVEFORMATEX format;
  unsigned int block_al;
  unsigned long bytes_ps;
  MMRESULT error;
  int i;

  block_al = channels * (16 / 8);
  bytes_ps = speed * block_al;

  format.wFormatTag      = WAVE_FORMAT_PCM;
  format.nChannels       = channels;
  format.nSamplesPerSec  = speed;
  format.nAvgBytesPerSec = bytes_ps;
  format.nBlockAlign     = block_al;
  format.wBitsPerSample  = 16;
  format.cbSize          = 0;

  error = waveOutOpen(handle, WAVE_MAPPER, &format,
		      (DWORD) callback, 0, CALLBACK_FUNCTION);
  if (error != MMSYSERR_NOERROR) {
    audio_error = error_text(error);
    return -1;
  }

  for (i = 0; i < NBUFFERS; ++i) {
    output[i].wave_header.lpData         = output[i].pcm_data;
    output[i].wave_header.dwBufferLength = sizeof(output[i].pcm_data);
    output[i].wave_header.dwUser         = (DWORD) &output[i];
    output[i].wave_header.dwFlags        = 0;

    error = waveOutPrepareHeader(*handle, &output[i].wave_header,
				 sizeof(output[i].wave_header));
    if (error != MMSYSERR_NOERROR) {
      audio_error = error_text(error);

      while (i--) {
	waveOutUnprepareHeader(*handle, &output[i].wave_header,
			       sizeof(output[i].wave_header));
      }

      waveOutClose(*handle);
      *handle = 0;

      return -1;
    }
  }

  return 0;
}

static
int close_dev(HWAVEOUT *handle)
{
  MMRESULT error;
  int i;

  for (i = 0; i < NBUFFERS; ++i) {
    error = waveOutUnprepareHeader(*handle, &output[i].wave_header,
				   sizeof(output[i].wave_header));
    if (error != MMSYSERR_NOERROR) {
      audio_error = error_text(error);
      return -1;
    }
  }

  error = waveOutClose(*handle);

  if (error != MMSYSERR_NOERROR) {
    audio_error = error_text(error);
    return -1;
  }

  *handle = 0;

  return 0;
}

static
int flush(struct buffer *buffer)
{
  switch (WaitForSingleObject(buffer->event_handle, INFINITE)) {
  case WAIT_OBJECT_0:
    return 0;

  case WAIT_ABANDONED:
    audio_error = "wait abandoned";
    return -1;

  case WAIT_TIMEOUT:
    audio_error = "wait timeout";
    return -1;

  case WAIT_FAILED:
  default:
    audio_error = "wait failed";
    return -1;
  }
}

static
int config(struct audio_config *config)
{
  if (wave_handle) {
    int i;

    for (i = 0; i < NBUFFERS; ++i) {
      if (flush(&output[i]) == -1)
	return -1;
    }

    if (close_dev(&wave_handle) == -1)
      return -1;
  }

  stereo = (config->channels == 2);

  return open_dev(&wave_handle, config->channels, config->speed);
}

static inline
signed short scale(mad_fixed_t sample)
{
  /* round */
  sample += 0x00001000L;

  /* scale to signed 16-bit integer value */
  if (sample >= 0x10000000L)		/* +1.0 */
    return 0x7fff;
  else if (sample <= -0x10000000L)	/* -1.0 */
    return -0x8000;
  else
    return sample >> 13;
}

static
int play(struct audio_play *play)
{
  unsigned char *ptr;
  mad_fixed_t const *left, *right;
  unsigned int len;
  MMRESULT error;

  /* wait for previous block to finish */

  if (flush(&output[buffer]) == -1)
    return -1;

  /* prepare new block */

  ptr   = output[buffer].pcm_data;
  len   = play->nsamples;
  left  = play->samples[0];
  right = play->samples[1];

  while (len--) {
    signed short sample;

    /* little-endian */

    sample = scale(*left++);
    *ptr++ = (sample >> 0) & 0xff;
    *ptr++ = (sample >> 8) & 0xff;

    if (stereo) {
      sample = scale(*right++);
      *ptr++ = (sample >> 0) & 0xff;
      *ptr++ = (sample >> 8) & 0xff;
    }
  }

  len = play->nsamples * 2;
  if (stereo)
    len *= 2;

  /* output new block */

  output[buffer].wave_header.dwBufferLength = len;

  error = waveOutWrite(wave_handle, &output[buffer].wave_header,
		       sizeof(output[buffer].wave_header));
  if (error != MMSYSERR_NOERROR) {
    audio_error = error_text(error);
    return -1;
  }

  buffer = (buffer + 1) % NBUFFERS;

  return 0;
}

static
int finish(struct audio_finish *finish)
{
  int i, result = 0;

  if (wave_handle) {
    for (i = 0; i < NBUFFERS; ++i) {
      if (flush(&output[i]) == -1)
	result = -1;
    }

    if (close_dev(&wave_handle) == -1)
      result = -1;
  }

  for (i = 0; i < NBUFFERS; ++i) {
    if (CloseHandle(output[i].event_handle) == 0 && result == 0) {
      audio_error = "failed to close synchronization object";
      result = -1;
    }
  }

  return result;
}

int audio_win32(union audio_control *control)
{
  audio_error = 0;

  switch (control->command) {
  case audio_cmd_init:
    return init(&control->init);
  case audio_cmd_config:
    return config(&control->config);
  case audio_cmd_play:
    return play(&control->play);
  case audio_cmd_finish:
    return finish(&control->finish);
  }

  return 0;
}
