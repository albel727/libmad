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
 * $Id: audio.h,v 1.16 2000/09/24 17:49:25 rob Exp $
 */

# ifndef AUDIO_H
# define AUDIO_H

# include "mad.h"

# define MAX_NSAMPLES	(1152 * 3)	/* allow for resampled frame */

enum {
  audio_cmd_init,
  audio_cmd_config,
  audio_cmd_play,
  audio_cmd_finish
};

union audio_control {
  short command;

  struct audio_init {
    short command;
    char const *path;
  } init;

  struct audio_config {
    short command;
    unsigned short channels;
    unsigned int speed;
  } config;

  struct audio_play {
    short command;
    unsigned short nsamples;
    mad_fixed_t const *samples[2];
    int mode;
  } play;

  struct audio_finish {
    short command;
  } finish;
};

extern char const *audio_error;

typedef int audio_ctlfunc_t(union audio_control *);

audio_ctlfunc_t *audio_output(char const **);

audio_ctlfunc_t audio_oss;
audio_ctlfunc_t audio_empeg;
audio_ctlfunc_t audio_sun;
audio_ctlfunc_t audio_win32;

audio_ctlfunc_t audio_raw;
audio_ctlfunc_t audio_wave;
audio_ctlfunc_t audio_au;
audio_ctlfunc_t audio_hex;
audio_ctlfunc_t audio_null;

signed long audio_linear_round(unsigned int, mad_fixed_t);
signed long audio_linear_dither(unsigned int, mad_fixed_t, mad_fixed_t *);

unsigned int audio_pcm_u8(unsigned char *, unsigned int,
			  mad_fixed_t const *, mad_fixed_t const *, int);
unsigned int audio_pcm_s16le(unsigned char *, unsigned int,
			     mad_fixed_t const *, mad_fixed_t const *, int);
unsigned int audio_pcm_s16be(unsigned char *, unsigned int,
			     mad_fixed_t const *, mad_fixed_t const *, int);
unsigned int audio_pcm_s32le(unsigned char *, unsigned int,
			     mad_fixed_t const *, mad_fixed_t const *, int);
unsigned int audio_pcm_s32be(unsigned char *, unsigned int,
			     mad_fixed_t const *, mad_fixed_t const *, int);

unsigned char audio_mulaw_round(mad_fixed_t);
unsigned char audio_mulaw_dither(mad_fixed_t, mad_fixed_t *);

unsigned int audio_pcm_mulaw(unsigned char *, unsigned int,
			     mad_fixed_t const *, mad_fixed_t const *, int);

# define AUDIO_MODE_ROUND	0x0001
# define AUDIO_MODE_DITHER	0x0002

# endif

