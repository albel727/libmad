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
 * $Id: audio_sun.c,v 1.3 2000/03/05 18:11:34 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <unistd.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/audioio.h>
# include <errno.h>

# include "libmad.h"
# include "audio.h"

# define AUDIO_DEVICE	"/dev/audio"

static int sfd;
static int stereo;

static
int init(struct audio_init *init)
{
  audio_info_t info;

  if (init->path == 0)
    init->path = AUDIO_DEVICE;

  sfd = open(init->path, O_WRONLY);
  if (sfd == -1) {
    audio_error = ":";
    return -1;
  }

  AUDIO_INITINFO(&info);

  info.play.precision = 16;
  info.play.encoding  = AUDIO_ENCODING_LINEAR;

  if (ioctl(sfd, AUDIO_SETINFO, &info) == -1) {
    audio_error = ":ioctl(AUDIO_SETINFO)";
    return -1;
  }

  return 0;
}

static
int config(struct audio_config *config)
{
  audio_info_t info;

  if (ioctl(sfd, AUDIO_DRAIN, 0) == -1) {
    audio_error = ":ioctl(AUDIO_DRAIN)";
    return -1;
  }

  AUDIO_INITINFO(&info);

  info.play.sample_rate = config->speed;
  info.play.channels    = config->channels;

  if (ioctl(sfd, AUDIO_SETINFO, &info) == -1) {
    audio_error = ":ioctl(AUDIO_SETINFO)";
    return -1;
  }

  stereo = (config->channels == 2);

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

    /* big-endian */

    sample = scale(*left++);
    *ptr++ = (sample >> 8) & 0xff;
    *ptr++ = (sample >> 0) & 0xff;

    if (stereo) {
      sample = scale(*right++);
      *ptr++ = (sample >> 8) & 0xff;
      *ptr++ = (sample >> 0) & 0xff;
    }
  }

  len = play->nsamples * 2;
  if (stereo)
    len *= 2;

  return output(data, len);
}

static
int finish(struct audio_finish *finish)
{
  int result = 0;

  if (close(sfd) == -1 && result == 0) {
    audio_error = ":close";
    result = -1;
  }

  return result;
}

int audio_sun(union audio_control *control)
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
