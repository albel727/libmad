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
 * $Id: audio_oss.c,v 1.9 2000/05/24 05:06:24 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <unistd.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/soundcard.h>
# include <errno.h>

# include "libmad.h"
# include "audio.h"

# define AUDIO_DEVICE	"/dev/audio"

# ifdef EMPEG
#  include <string.h>
#  define AUDIO_FILLSZ	4608
# endif

static int sfd;
static int stereo;

static
int init(struct audio_init *init)
{
  if (init->path == 0)
    init->path = AUDIO_DEVICE;

  sfd = open(init->path, O_WRONLY);
  if (sfd == -1) {
    audio_error = ":";
    return -1;
  }

  return 0;
}

static
int config(struct audio_config *config)
{
  int format, speed;

  if (ioctl(sfd, SNDCTL_DSP_SYNC, 0) == -1) {
    audio_error = ":ioctl(SNDCTL_DSP_SYNC)";
    return -1;
  }

  format = AFMT_S16_LE;
  if (ioctl(sfd, SNDCTL_DSP_SETFMT, &format) == -1) {
    audio_error = ":ioctl(SNDCTL_DSP_SETFMT)";
    return -1;
  }

  if (format != AFMT_S16_LE) {
    audio_error = "AFMT_S16_LE not available";
    return -1;
  }

  stereo = (config->channels == 2);
  if (ioctl(sfd, SNDCTL_DSP_STEREO, &stereo) == -1) {
    audio_error = ":ioctl(SNDCTL_DSP_STEREO)";
    return -1;
  }

  if ((stereo  && config->channels != 2) ||
      (!stereo && config->channels == 2)) {
    audio_error = "stereo/mono configuration failed";
    return -1;
  }

  speed = config->speed;
  if (ioctl(sfd, SNDCTL_DSP_SPEED, &speed) == -1) {
    audio_error = ":ioctl(SNDCTL_DSP_SPEED)";
    return -1;
  }

  if (speed != config->speed) {
    audio_error = "sample speed not available";
    config->speed = speed;
  }

  return 0;
}

static
int output(unsigned char const *ptr, unsigned int len)
{
  while (len) {
    int wrote;

    wrote = write(sfd, ptr, len);
    if (wrote == -1) {
      if (errno == EINTR)
	continue;
      else {
	audio_error = ":write";
	return -1;
      }
    }

    ptr += wrote;
    len -= wrote;
  }

  return 0;
}

# ifdef EMPEG

static
int buffer(unsigned char const *ptr, unsigned int len)
{
  static unsigned char hold[AUDIO_FILLSZ];
  static unsigned int held;
  unsigned int left, grab;

  if (len == 0) {
    if (held) {
      memset(&hold[held], 0, AUDIO_FILLSZ - held);
      held = 0;

      return output(hold, AUDIO_FILLSZ);
    }

    return 0;
  }

  if (held == 0 && len == AUDIO_FILLSZ)
    return output(ptr, len);

  left = AUDIO_FILLSZ - held;

  while (len) {
    grab = len < left ? len : left;

    memcpy(&hold[held], ptr, grab);
    held += grab;
    left -= grab;

    ptr  += grab;
    len  -= grab;

    if (left == 0) {
      if (output(hold, AUDIO_FILLSZ) == -1)
	return -1;

      held = 0;
      left = AUDIO_FILLSZ;
    }
  }

  return 0;
}

# define flush()  buffer(0, 0)

# endif

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
  unsigned char data[MAX_NSAMPLES * 2 * 2];
  unsigned char *ptr;
  mad_fixed_t const *left, *right;
  unsigned int len;

  ptr   = data;
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

# ifdef EMPEG
  return buffer(data, len);
# else
  return output(data, len);
# endif
}

static
int finish(struct audio_finish *finish)
{
  int result = 0;

# ifdef EMPEG
  if (flush() == -1)
    result = -1;
# endif

  if (close(sfd) == -1 && result == 0) {
    audio_error = ":close";
    result = -1;
  }

  return result;
}

int audio_oss(union audio_control *control)
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
