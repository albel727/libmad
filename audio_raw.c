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
 * $Id: audio_raw.c,v 1.1 2000/03/19 06:43:38 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <stdio.h>
# include <string.h>

# include "libmad.h"
# include "audio.h"

static FILE *outfile;
static int stereo;

static
int init(struct audio_init *init)
{
  if (init->path && strcmp(init->path, "-") != 0) {
    outfile = fopen(init->path, "wb");
    if (outfile == 0) {
      audio_error = ":";
      return -1;
    }
  }
  else
    outfile = stdout;

  return 0;
}

static
int config(struct audio_config *config)
{
  stereo = (config->channels == 2);

  return 0;
}

static inline
signed long scale(fixed_t sample)
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
  unsigned char data[MAX_NSAMPLES * 2 * 2], *ptr;
  fixed_t const *left, *right;
  unsigned int len;

  ptr   = data;
  len   = play->nsamples;
  left  = play->samples[0];
  right = play->samples[1];

  while (len--) {
    signed long sample;

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

  if (fwrite(data, stereo ? 4 : 2,
	     play->nsamples, outfile) != play->nsamples) {
    audio_error = ":fwrite";
    return -1;
  }

  return 0;
}

static
int finish(struct audio_finish *finish)
{
  if (outfile != stdout &&
      fclose(outfile) == EOF) {
    audio_error = ":close";
    return -1;
  }

  return 0;
}

int audio_raw(union audio_control *control)
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
