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
 * $Id: audio_oss.c,v 1.16 2000/10/25 21:51:39 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "global.h"

# include <unistd.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/soundcard.h>
# include <errno.h>

# include "mad.h"
# include "audio.h"

# if !defined(AFMT_S32_NE)
#  if defined(WORDS_BIGENDIAN)
#   define AFMT_S32_NE  AFMT_S32_BE
#  else
#   define AFMT_S32_NE  AFMT_S32_LE
#  endif
# endif

# if !defined(AFMT_S16_NE)
#  if defined(WORDS_BIGENDIAN)
#   define AFMT_S16_NE  AFMT_S16_BE
#  else
#   define AFMT_S16_NE  AFMT_S16_LE
#  endif
# endif

# define AUDIO_DEVICE	"/dev/dsp"

static int sfd;
static unsigned int (*audio_pcm)(unsigned char *, unsigned int,
				 mad_fixed_t const *, mad_fixed_t const *,
				 enum audio_mode);

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
  int format, channels, speed;

  if (ioctl(sfd, SNDCTL_DSP_SYNC, 0) == -1) {
    audio_error = ":ioctl(SNDCTL_DSP_SYNC)";
    return -1;
  }

# if (defined(AFMT_S32_LE) && AFMT_S32_NE == AFMT_S32_LE) ||  \
     (defined(AFMT_S32_BE) && AFMT_S32_NE == AFMT_S32_BE)
  format = AFMT_S32_NE;
# else
  format = AFMT_S16_NE;
# endif

  if (ioctl(sfd, SNDCTL_DSP_SETFMT, &format) == -1) {
    audio_error = ":ioctl(SNDCTL_DSP_SETFMT)";
    return -1;
  }

  switch (format) {
# if defined(AFMT_S32_LE)
  case AFMT_S32_LE:
    audio_pcm = audio_pcm_s32le;
    break;
# endif

# if defined(AFMT_S32_BE)
  case AFMT_S32_BE:
    audio_pcm = audio_pcm_s32be;
    break;
# endif

  case AFMT_S16_LE:
    audio_pcm = audio_pcm_s16le;
    break;

  case AFMT_S16_BE:
    audio_pcm = audio_pcm_s16be;
    break;

  case AFMT_U8:
    audio_pcm = audio_pcm_u8;
    break;

  case AFMT_MU_LAW:
    audio_pcm = audio_pcm_mulaw;
    break;

  default:
    audio_error = _("no supported audio format available");
    return -1;
  }

  channels = config->channels;
  if (ioctl(sfd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
    audio_error = ":ioctl(SNDCTL_DSP_CHANNELS)";
    return -1;
  }

  if (channels != config->channels) {
    audio_error = _("required number of channels not available");
    return -1;
  }

  speed = config->speed;
  if (ioctl(sfd, SNDCTL_DSP_SPEED, &speed) == -1) {
    audio_error = ":ioctl(SNDCTL_DSP_SPEED)";
    return -1;
  }

  if (speed != config->speed) {
    audio_error = _("sample speed not available");
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

static
int play(struct audio_play *play)
{
  unsigned char data[MAX_NSAMPLES * 4 * 2];
  unsigned int len;

  len = audio_pcm(data, play->nsamples,
		  play->samples[0], play->samples[1], play->mode);

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

int audio_oss(union audio_control *control)
{
  audio_error = 0;

  switch (control->command) {
  case AUDIO_COMMAND_INIT:
    return init(&control->init);

  case AUDIO_COMMAND_CONFIG:
    return config(&control->config);

  case AUDIO_COMMAND_PLAY:
    return play(&control->play);

  case AUDIO_COMMAND_FINISH:
    return finish(&control->finish);
  }

  return 0;
}
