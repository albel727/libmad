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
 * $Id: audio.c,v 1.1 2000/03/05 05:19:16 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <string.h>

# include "audio.h"

char const *audio_error;

audio_ctlfunc_t audio_output(char const *path)
{
  char const *ext;

  if (path == 0)
    return AUDIO_DEFAULT;

  if (strcmp(path, "/dev/null") == 0)
    return audio_null;

  if (strncmp(path, "/dev/", 5) == 0)
    return AUDIO_DEFAULT;

  ext = strrchr(path, '.');
  if (ext) {
    ++ext;

    if (strcmp(ext, "wav") == 0 ||
	strcmp(ext, "WAV") == 0)
      return audio_wav;
  }

  return 0;
}
