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
 * $Id: audio_hex.c,v 1.7 2000/07/08 18:34:06 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <stdio.h>
# include <string.h>

# include "mad.h"
# include "audio.h"

static FILE *outfile;
static int stereo;

static
int init(struct audio_init *init)
{
  if (init->path && strcmp(init->path, "-") != 0) {
    outfile = fopen(init->path, "w");
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
  fprintf(outfile, "# %u channel%s, %u Hz\n",
	  config->channels, config->channels == 1 ? "" : "s", config->speed);

  stereo = (config->channels == 2);

  return 0;
}

static inline
signed long scale(mad_fixed_t sample)
{
  /* round */
  sample += 0x00000010L;

  /* scale to signed 24-bit integer value */
  if (sample >= 0x10000000L)		/* +1.0 */
    return 0x7fffffL;
  else if (sample < -0x10000000L)	/* -1.0 */
    return -0x800000L;
  else
    return sample >> 5;
}

static
int play(struct audio_play *play)
{
  mad_fixed_t const *left, *right;
  unsigned int len;

  len   = play->nsamples;
  left  = play->samples[0];
  right = play->samples[1];

  while (len--) {
    signed long sample;

    sample = scale(*left++);
    fprintf(outfile, "%02X%02X%02X\n",
	    (unsigned int) ((sample & 0xff0000L) >> 16),
	    (unsigned int) ((sample & 0x00ff00L) >>  8),
	    (unsigned int) ((sample & 0x0000ffL) >>  0));

    if (stereo) {
      sample = scale(*right++);
      fprintf(outfile, "%02X%02X%02X\n",
	      (unsigned int) ((sample & 0xff0000L) >> 16),
	      (unsigned int) ((sample & 0x00ff00L) >>  8),
	      (unsigned int) ((sample & 0x0000ffL) >>  0));
    }
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

int audio_hex(union audio_control *control)
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
