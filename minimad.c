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
 * $Id: minimad.c,v 1.9 2000/09/14 19:40:37 rob Exp $
 */

# include <stdio.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/mman.h>

# include "mad.h"

static void decode(unsigned char const *, unsigned long);

int main(int argc, char *argv[])
{
  struct stat stat;
  void *fdm;

  if (argc != 1)
    return 1;

  if (fstat(STDIN_FILENO, &stat) == -1 ||
      stat.st_size == 0)
    return 2;

  /* 1. map input stream into memory */

  fdm = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, STDIN_FILENO, 0);
  if (fdm == MAP_FAILED)
    return 3;

  decode(fdm, stat.st_size);

  if (munmap(fdm, stat.st_size) == -1)
    return 4;

  return 0;
}

/* private message buffer */

struct buffer {
  unsigned char const *start;
  unsigned long length;
};

/* 2. called when more input is needed; refill stream buffer */

static
int input(void *data, struct mad_stream *stream)
{
  struct buffer *buffer = data;

  if (!buffer->length)
    return MAD_DECODER_STOP;

  mad_stream_buffer(stream, buffer->start, buffer->length);

  buffer->length = 0;

  return MAD_DECODER_CONTINUE;
}

/* utility to scale and round samples to 16 bits */

static inline
signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/* 3. called to process output */

static
int output(void *data, struct mad_frame const *frame,
	               struct mad_synth const *synth)
{
  unsigned int nchannels, nsamples;
  mad_fixed_t const *left_ch, *right_ch;

  /* frame->sfreq contains the sampling frequency */

  nchannels = MAD_NCHANNELS(frame);
  nsamples  = synth->pcmlen;
  left_ch   = synth->pcmout[0];
  right_ch  = synth->pcmout[1];

  while (nsamples--) {
    signed int sample;

    /* output sample(s) in 16-bit signed little-endian PCM */

    sample = scale(*left_ch++);
    putchar((sample >> 0) & 0xff);
    putchar((sample >> 8) & 0xff);

    if (nchannels == 2) {
      sample = scale(*right_ch++);
      putchar((sample >> 0) & 0xff);
      putchar((sample >> 8) & 0xff);
    }
  }

  return MAD_DECODER_CONTINUE;
}

/* 4. called to handle a decoding error */

static
int error(void *data, struct mad_stream *stream,
	              struct mad_frame *frame)
{
  struct buffer *buffer = data;

  fprintf(stderr, "decoding error 0x%04x at byte offset %u\n",
	  stream->error, stream->this_frame - buffer->start);

  return MAD_DECODER_STOP;
}

/* 5. put it all together */

static
void decode(unsigned char const *start, unsigned long length)
{
  struct buffer buffer;
  struct mad_decoder decoder;

  buffer.start  = start;
  buffer.length = length;

  /* configure input, output, and error functions */

  mad_decoder_init(&decoder, &buffer, input, 0 /* filter */, output, error);

  /* start the decoder */

  mad_decoder_run(&decoder, MAD_DECODER_SYNC);

  mad_decoder_finish(&decoder);
}
