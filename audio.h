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
 * $Id: audio.h,v 1.9 2000/05/09 17:36:27 rob Exp $
 */

# ifndef AUDIO_H
# define AUDIO_H

# include "libmad.h"

# define MAX_NSAMPLES	1152

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
  } play;

  struct audio_finish {
    short command;
  } finish;
};

extern char const *audio_error;

typedef int (*audio_ctlfunc_t)(union audio_control *);

audio_ctlfunc_t audio_output(char const **);

int audio_oss(union audio_control *);
int audio_sun(union audio_control *);
int audio_win32(union audio_control *);

int audio_wav(union audio_control *);
int audio_raw(union audio_control *);
int audio_hex(union audio_control *);
int audio_null(union audio_control *);

# endif

