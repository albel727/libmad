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
 * $Id: audio_empeg.c,v 1.4 2000/09/24 02:30:02 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <unistd.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/soundcard.h>
# include <errno.h>
# include <string.h>

# include "mad.h"
# include "audio.h"

# define AUDIO_DEVICE	"/dev/dsp"
# define AUDIO_FILLSZ	4608

static int sfd;

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
  /*
   * The empeg-car's audio device is locked at 44100 Hz stereo 16-bit
   * signed little-endian; no configuration is necessary or possible,
   * but we may need to resample the output and/or convert to stereo.
   */

  config->speed = 44100;

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

static
int play(struct audio_play *play)
{
  unsigned char data[MAX_NSAMPLES * 2 * 2];
  mad_fixed_t const *left, *right;
  unsigned int len;

  left  = play->samples[0];
  right = play->samples[1];

  if (!right)
    right = left;  /* always stereo */

  len = audio_pcm_s16le(data, play->nsamples, left, right, play->mode);

  return buffer(data, len);
}

static
int finish(struct audio_finish *finish)
{
  int result = 0;

  if (flush() == -1)
    result = -1;

  if (close(sfd) == -1 && result == 0) {
    audio_error = ":close";
    result = -1;
  }

  return result;
}

int audio_empeg(union audio_control *control)
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
