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
 * $Id: audio.c,v 1.5 2000/05/24 05:06:24 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <string.h>

# include "audio.h"

char const *audio_error;

audio_ctlfunc_t *audio_output(char const **path)
{
  char const *ext;

  if (path == 0)
    return AUDIO_DEFAULT;

  ext = strchr(*path, ':');
  if (ext) {
    char const *type;

    type  = *path;
    *path = ext + 1;

    if (strncmp(type, "null", ext - type) == 0 ||
	strncmp(type, "NULL", ext - type) == 0) {
      *path = ext + 1;
      return audio_null;
    }

    if (strncmp(type, "wav", ext - type) == 0 ||
	strncmp(type, "WAV", ext - type) == 0) {
      *path = ext + 1;
      return audio_wav;
    }

    if (strncmp(type, "raw", ext - type) == 0 ||
	strncmp(type, "RAW", ext - type) == 0 ||
	strncmp(type, "pcm", ext - type) == 0 ||
	strncmp(type, "PCM", ext - type) == 0) {
      *path = ext + 1;
      return audio_raw;
    }

# ifdef DEBUG
    if (strncmp(type, "hex", ext - type) == 0 ||
	strncmp(type, "HEX", ext - type) == 0) {
      *path = ext + 1;
      return audio_hex;
    }
# endif
  }

  if (strcmp(*path, "/dev/null") == 0)
    return audio_null;

  if (strncmp(*path, "/dev/", 5) == 0)
    return AUDIO_DEFAULT;

  ext = strrchr(*path, '.');
  if (ext) {
    ++ext;

    if (strcmp(ext, "wav") == 0 ||
	strcmp(ext, "WAV") == 0)
      return audio_wav;

    if (strcmp(ext, "raw") == 0 ||
	strcmp(ext, "RAW") == 0 ||
	strcmp(ext, "pcm") == 0 ||
	strcmp(ext, "PCM") == 0)
      return audio_raw;

# ifdef DEBUG
    if (strcmp(ext, "hex") == 0 ||
	strcmp(ext, "HEX") == 0)
      return audio_hex;
# endif
  }

  return 0;
}
