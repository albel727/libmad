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
 * $Id: audio_null.c,v 1.3 2000/03/05 07:31:54 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "libmad.h"
# include "audio.h"

static
int init(struct audio_init *init)
{
  return 0;
}

static
int config(struct audio_config *config)
{
  return 0;
}

static
int play(struct audio_play *play)
{
  return 0;
}

static
int finish(struct audio_finish *finish)
{
  return 0;
}

int audio_null(union audio_control *control)
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
