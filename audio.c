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
 * $Id: audio.c,v 1.12 2000/09/17 18:49:32 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <string.h>

# include "audio.h"
# include "mad.h"

char const *audio_error;

/*
 * NAME:	audio_output()
 * DESCRIPTION: choose an audio output module from a specifier pathname
 */
audio_ctlfunc_t *audio_output(char const **path)
{
  char const *ext;

  if (path == 0)
    return AUDIO_DEFAULT;

  /* check for prefix specifier */

  ext = strchr(*path, ':');
  if (ext) {
    char const *type;

    type  = *path;
    *path = ext + 1;

    if (strncmp(type, "raw", ext - type) == 0 ||
	strncmp(type, "RAW", ext - type) == 0 ||
	strncmp(type, "pcm", ext - type) == 0 ||
	strncmp(type, "PCM", ext - type) == 0)
      return audio_raw;

    if (strncmp(type, "wave", ext - type) == 0 ||
	strncmp(type, "WAVE", ext - type) == 0 ||
	strncmp(type, "wav",  ext - type) == 0 ||
	strncmp(type, "WAV",  ext - type) == 0)
      return audio_wave;

# ifdef DEBUG
    if (strncmp(type, "hex", ext - type) == 0 ||
	strncmp(type, "HEX", ext - type) == 0)
      return audio_hex;
# endif

    if (strncmp(type, "null", ext - type) == 0 ||
	strncmp(type, "NULL", ext - type) == 0)
      return audio_null;

    *path = type;
    return 0;
  }

  if (strcmp(*path, "/dev/null") == 0)
    return audio_null;

  if (strncmp(*path, "/dev/", 5) == 0)
    return AUDIO_DEFAULT;

  /* check for file extension specifier */

  ext = strrchr(*path, '.');
  if (ext) {
    ++ext;

    if (strcmp(ext, "wav") == 0 ||
	strcmp(ext, "WAV") == 0)
      return audio_wave;

    if (strcmp(ext, "raw") == 0 ||
	strcmp(ext, "RAW") == 0 ||
	strcmp(ext, "pcm") == 0 ||
	strcmp(ext, "PCM") == 0 ||
	strcmp(ext, "out") == 0 ||
	strcmp(ext, "OUT") == 0)
      return audio_raw;

# ifdef DEBUG
    if (strcmp(ext, "hex") == 0 ||
	strcmp(ext, "HEX") == 0 ||
	strcmp(ext, "txt") == 0 ||
	strcmp(ext, "TXT") == 0)
      return audio_hex;
# endif
  }

  return AUDIO_DEFAULT;
}

/*
 * NAME:	audio_round()
 * DESCRIPTION:	generic sample quantize routine
 */
inline
signed long audio_round(unsigned int bits, mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - bits));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - bits);
}

/*
 * NAME:	audio_dither()
 * DESCRIPTION:	generic sample quantize and dither routine
 */
inline
signed long audio_dither(unsigned int bits, mad_fixed_t sample,
			 mad_fixed_t *error)
{
  mad_fixed_t quantized;

  /* dither */
  sample += *error;

  /* clip */
  if (sample >= MAD_F_ONE)
    quantized = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    quantized = -MAD_F_ONE;
  else
    quantized = sample;

  /* quantize */
  quantized &= ~((1L << (MAD_F_FRACBITS + 1 - bits)) - 1);

  /* error */
  *error = sample - quantized;

  return quantized >> (MAD_F_FRACBITS + 1 - bits);
}

/*
 * NAME:	audio_pcm_u8()
 * DESCRIPTION:	write a block of unsigned 8-bit PCM samples
 */
unsigned int audio_pcm_u8(unsigned char *data, unsigned int nsamples,
			  mad_fixed_t const *left, mad_fixed_t const *right,
			  int mode)
{
  static mad_fixed_t left_err, right_err;
  unsigned int len;

  len = nsamples;

  if (right) {  /* stereo */
    switch (mode) {
    case AUDIO_MODE_ROUND:
      while (len--) {
	*data++ = audio_round(8, *left++)  + 0x80;
	*data++ = audio_round(8, *right++) + 0x80;
      }
      break;

    case AUDIO_MODE_DITHER:
      while (len--) {
	*data++ = audio_dither(8, *left++,  &left_err)  + 0x80;
	*data++ = audio_dither(8, *right++, &right_err) + 0x80;
      }
      break;

    default:
      return 0;
    }

    return nsamples * 2;
  }
  else {  /* mono */
    switch (mode) {
    case AUDIO_MODE_ROUND:
      while (len--)
	*data++ = audio_round(8, *left++) + 0x80;
      break;

    case AUDIO_MODE_DITHER:
      while (len--)
	*data++ = audio_dither(8, *left++, &left_err) + 0x80;
      break;

    default:
      return 0;
    }

    return nsamples;
  }
}

/*
 * NAME:	audio_pcm_s16le()
 * DESCRIPTION:	write a block of signed 16-bit little-endian PCM samples
 */
unsigned int audio_pcm_s16le(unsigned char *data, unsigned int nsamples,
			     mad_fixed_t const *left, mad_fixed_t const *right,
			     int mode)
{
  static mad_fixed_t left_err, right_err;
  unsigned int len;
  register signed int sample;

  len = nsamples;

  if (right) {  /* stereo */
    switch (mode) {
    case AUDIO_MODE_ROUND:
      while (len--) {
	sample  = audio_round(16, *left++);
	*data++ = (sample >> 0) & 0xff;
	*data++ = (sample >> 8) & 0xff;

	sample  = audio_round(16, *right++);
	*data++ = (sample >> 0) & 0xff;
	*data++ = (sample >> 8) & 0xff;
      }
      break;

    case AUDIO_MODE_DITHER:
      while (len--) {
	sample  = audio_dither(16, *left++,  &left_err);
	*data++ = (sample >> 0) & 0xff;
	*data++ = (sample >> 8) & 0xff;

	sample  = audio_dither(16, *right++, &right_err);
	*data++ = (sample >> 0) & 0xff;
	*data++ = (sample >> 8) & 0xff;
      }
      break;

    default:
      return 0;
    }

    return nsamples * 2 * 2;
  }
  else {  /* mono */
    switch (mode) {
    case AUDIO_MODE_ROUND:
      while (len--) {
	sample  = audio_round(16, *left++);
	*data++ = (sample >> 0) & 0xff;
	*data++ = (sample >> 8) & 0xff;
      }
      break;

    case AUDIO_MODE_DITHER:
      while (len--) {
	sample  = audio_dither(16, *left++, &left_err);
	*data++ = (sample >> 0) & 0xff;
	*data++ = (sample >> 8) & 0xff;
      }
      break;

    default:
      return 0;
    }

    return nsamples * 2;
  }
}

/*
 * NAME:	audio_pcm_s16be()
 * DESCRIPTION:	write a block of signed 16-bit big-endian PCM samples
 */
unsigned int audio_pcm_s16be(unsigned char *data, unsigned int nsamples,
			     mad_fixed_t const *left, mad_fixed_t const *right,
			     int mode)
{
  static mad_fixed_t left_err, right_err;
  unsigned int len;
  register signed int sample;

  len = nsamples;

  if (right) {  /* stereo */
    switch (mode) {
    case AUDIO_MODE_ROUND:
      while (len--) {
	sample  = audio_round(16, *left++);
	*data++ = (sample >> 8) & 0xff;
	*data++ = (sample >> 0) & 0xff;

	sample  = audio_round(16, *right++);
	*data++ = (sample >> 8) & 0xff;
	*data++ = (sample >> 0) & 0xff;
      }
      break;

    case AUDIO_MODE_DITHER:
      while (len--) {
	sample  = audio_dither(16, *left++,  &left_err);
	*data++ = (sample >> 8) & 0xff;
	*data++ = (sample >> 0) & 0xff;

	sample  = audio_dither(16, *right++, &right_err);
	*data++ = (sample >> 8) & 0xff;
	*data++ = (sample >> 0) & 0xff;
      }
      break;

    default:
      return 0;
    }

    return nsamples * 2 * 2;
  }
  else {  /* mono */
    switch (mode) {
    case AUDIO_MODE_ROUND:
      while (len--) {
	sample  = audio_round(16, *left++);
	*data++ = (sample >> 8) & 0xff;
	*data++ = (sample >> 0) & 0xff;
      }
      break;

    case AUDIO_MODE_DITHER:
      while (len--) {
	sample  = audio_dither(16, *left++, &left_err);
	*data++ = (sample >> 8) & 0xff;
	*data++ = (sample >> 0) & 0xff;
      }
      break;

    default:
      return 0;
    }

    return nsamples * 2;
  }
}
