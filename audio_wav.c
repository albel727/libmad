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
 * $Id: audio_wav.c,v 1.3 2000/03/05 07:31:54 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <stdio.h>
# include <string.h>

# include "libmad.h"
# include "audio.h"

static FILE *outfile;
static unsigned long riff_len, chunk_len;
static long last_chunk;
static struct audio_config configuration;

static
int init(struct audio_init *init)
{
  if (init->path) {
    outfile = fopen(init->path, "wb");
    if (outfile == 0) {
      audio_error = ":";
      return -1;
    }
  }
  else
    outfile = stdout;

  /* RIFF header and (WAVE) data type identifier */

  fwrite("RIFF\0\0\0\0WAVE", 8 + 4, 1, outfile);

  riff_len   = 4;
  last_chunk = 0;

  return 0;
}

static
void int4(unsigned char *ptr, unsigned long num)
{
  *ptr++ = (num & 0x000000ff) >>  0;
  *ptr++ = (num & 0x0000ff00) >>  8;
  *ptr++ = (num & 0x00ff0000) >> 16;
  *ptr++ = (num & 0xff000000) >> 24;
}

static
void int2(unsigned char *ptr, unsigned int num)
{
  *ptr++ = (num & 0x00ff) >> 0;
  *ptr++ = (num & 0xff00) >> 8;
}

static
int patch_length(long address, unsigned long length)
{
  unsigned char dword[4];

  if (fseek(outfile, address, SEEK_SET) == -1)
    return -1;

  int4(dword, length);
  fwrite(dword, 4, 1, outfile);

  if (fseek(outfile, 0, SEEK_END) == -1)
    return -1;

  return 0;
}

# define close_chunk()	patch_length(last_chunk + 4, chunk_len)

static
int config(struct audio_config *config)
{
  unsigned char chunk[24];
  unsigned int channels, block_al;
  unsigned long bytes_ps;

  if (last_chunk)
    close_chunk();

  /* "fmt " chunk */

  channels = config->stereo ? 2 : 1;
  bytes_ps = channels * config->speed * (16 / 8);
  block_al = channels * (16 / 8);

  memcpy(&chunk[0], "fmt ", 4);
  int4(&chunk[4], 16);

  int2(&chunk[8],  0x0001);		/* wFormatTag (WAVE_FORMAT_PCM) */
  int2(&chunk[10], channels);		/* wChannels */
  int4(&chunk[12], config->speed);	/* dwSamplesPerSec */
  int4(&chunk[16], bytes_ps);		/* dwAvgBytesPerSec */
  int2(&chunk[20], block_al);		/* wBlockAlign */

  /* PCM-format-specific */

  int2(&chunk[22], 16);			/* wBitsPerSample */

  fwrite(chunk, 24, 1, outfile);

  /* save current file position for later patching */

  last_chunk = ftell(outfile);
  if (last_chunk == -1)
    last_chunk = 0;

  /* "data" chunk */

  fwrite("data\0\0\0\0", 8, 1, outfile);

  chunk_len = 0;
  riff_len += 24 + 8;

  configuration = *config;

  return 0;
}

static inline
signed short scale(fixed_t sample)
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
  unsigned char data[MAX_NSAMPLES * 2 * 2];
  unsigned char *ptr;
  fixed_t const *left, *right;
  unsigned int len;

  ptr   = data;
  len   = play->nsamples;
  left  = play->samples[0];
  right = play->samples[1];

  while (len--) {
    signed short sample;

    sample = scale(*left++);
    *ptr++ = (sample >> 0) & 0xff;
    *ptr++ = (sample >> 8) & 0xff;

    if (configuration.stereo) {
      sample = scale(*right++);
      *ptr++ = (sample >> 0) & 0xff;
      *ptr++ = (sample >> 8) & 0xff;
    }
  }

  fwrite(data, configuration.stereo ? 4 : 2, play->nsamples, outfile);

  len = play->nsamples * 2;
  if (configuration.stereo)
    len *= 2;

  chunk_len += len;
  riff_len  += len;

  return 0;
}

static
int finish(struct audio_finish *finish)
{
  if (last_chunk)
    close_chunk();

  patch_length(4, riff_len);

  if (outfile != stdout &&
      fclose(outfile) == EOF) {
    audio_error = ":close";
    return -1;
  }

  return 0;
}

int audio_wav(union audio_control *control)
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
