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
 * $Id: mad.c,v 1.8 2000/03/05 07:31:55 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "bit.h"
# include "timer.h"
# include "stream.h"
# include "frame.h"
# include "layer12.h"
# include "layer3.h"
# include "mad.h"

# ifdef DEBUG
#  include <stdio.h>
# endif

/*
 * This is the lookup table for computing the CRC-check word.
 * As described in section 2.4.3.1 and depicted in Figure A.9
 * of ISO/IEC 11172-3, the generator polynomial is:
 *
 * G(X) = X^16 + X^15 + X^2 + 1
 */
static
unsigned short const crc_table[256] = {
  0x0000, 0x8005, 0x800f, 0x000a, 0x801b, 0x001e, 0x0014, 0x8011,
  0x8033, 0x0036, 0x003c, 0x8039, 0x0028, 0x802d, 0x8027, 0x0022,
  0x8063, 0x0066, 0x006c, 0x8069, 0x0078, 0x807d, 0x8077, 0x0072,
  0x0050, 0x8055, 0x805f, 0x005a, 0x804b, 0x004e, 0x0044, 0x8041,
  0x80c3, 0x00c6, 0x00cc, 0x80c9, 0x00d8, 0x80dd, 0x80d7, 0x00d2,
  0x00f0, 0x80f5, 0x80ff, 0x00fa, 0x80eb, 0x00ee, 0x00e4, 0x80e1,
  0x00a0, 0x80a5, 0x80af, 0x00aa, 0x80bb, 0x00be, 0x00b4, 0x80b1,
  0x8093, 0x0096, 0x009c, 0x8099, 0x0088, 0x808d, 0x8087, 0x0082,

  0x8183, 0x0186, 0x018c, 0x8189, 0x0198, 0x819d, 0x8197, 0x0192,
  0x01b0, 0x81b5, 0x81bf, 0x01ba, 0x81ab, 0x01ae, 0x01a4, 0x81a1,
  0x01e0, 0x81e5, 0x81ef, 0x01ea, 0x81fb, 0x01fe, 0x01f4, 0x81f1,
  0x81d3, 0x01d6, 0x01dc, 0x81d9, 0x01c8, 0x81cd, 0x81c7, 0x01c2,
  0x0140, 0x8145, 0x814f, 0x014a, 0x815b, 0x015e, 0x0154, 0x8151,
  0x8173, 0x0176, 0x017c, 0x8179, 0x0168, 0x816d, 0x8167, 0x0162,
  0x8123, 0x0126, 0x012c, 0x8129, 0x0138, 0x813d, 0x8137, 0x0132,
  0x0110, 0x8115, 0x811f, 0x011a, 0x810b, 0x010e, 0x0104, 0x8101,

  0x8303, 0x0306, 0x030c, 0x8309, 0x0318, 0x831d, 0x8317, 0x0312,
  0x0330, 0x8335, 0x833f, 0x033a, 0x832b, 0x032e, 0x0324, 0x8321,
  0x0360, 0x8365, 0x836f, 0x036a, 0x837b, 0x037e, 0x0374, 0x8371,
  0x8353, 0x0356, 0x035c, 0x8359, 0x0348, 0x834d, 0x8347, 0x0342,
  0x03c0, 0x83c5, 0x83cf, 0x03ca, 0x83db, 0x03de, 0x03d4, 0x83d1,
  0x83f3, 0x03f6, 0x03fc, 0x83f9, 0x03e8, 0x83ed, 0x83e7, 0x03e2,
  0x83a3, 0x03a6, 0x03ac, 0x83a9, 0x03b8, 0x83bd, 0x83b7, 0x03b2,
  0x0390, 0x8395, 0x839f, 0x039a, 0x838b, 0x038e, 0x0384, 0x8381,

  0x0280, 0x8285, 0x828f, 0x028a, 0x829b, 0x029e, 0x0294, 0x8291,
  0x82b3, 0x02b6, 0x02bc, 0x82b9, 0x02a8, 0x82ad, 0x82a7, 0x02a2,
  0x82e3, 0x02e6, 0x02ec, 0x82e9, 0x02f8, 0x82fd, 0x82f7, 0x02f2,
  0x02d0, 0x82d5, 0x82df, 0x02da, 0x82cb, 0x02ce, 0x02c4, 0x82c1,
  0x8243, 0x0246, 0x024c, 0x8249, 0x0258, 0x825d, 0x8257, 0x0252,
  0x0270, 0x8275, 0x827f, 0x027a, 0x826b, 0x026e, 0x0264, 0x8261,
  0x0220, 0x8225, 0x822f, 0x022a, 0x823b, 0x023e, 0x0234, 0x8231,
  0x8213, 0x0216, 0x021c, 0x8219, 0x0208, 0x820d, 0x8207, 0x0202
};

unsigned short mad_crc(unsigned short init, struct mad_bitptr bits,
		       unsigned int len)
{
  register unsigned short crc = init;
  register unsigned int data;

  while (len >= 8) {
    crc = crc_table[((crc >> 8) ^ mad_bit_read(&bits, 8)) & 0xff] ^ (crc << 8);
    len -= 8;
  }

  while (len--) {
    data = (mad_bit_read(&bits, 1) ^ (crc >> 15)) & 1;
    crc <<= 1;
    if (data)
      crc ^= 0x8005;
  }

  return crc & 0xffff;
}

static
unsigned int const bitrate_table[3][15] = {
  { 0,  32000,  64000,  96000, 128000, 160000, 192000, 224000,
       256000, 288000, 320000, 352000, 384000, 416000, 448000 },
  { 0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,
       128000, 160000, 192000, 224000, 256000, 320000, 384000 },
  { 0,  32000,  40000,  48000,  56000,  64000,  80000,  96000,
       112000, 128000, 160000, 192000, 224000, 256000, 320000 }
};

static
unsigned int const samplefreq_table[4] = { 44100, 48000, 32000 };

static
unsigned int const time_table[3] = { 320, 294, 441 };

static
unsigned char const *sync_word(unsigned char const *start,
			       unsigned char const *end)
{
  register unsigned char const *ptr = start;

  while (ptr < end - 1 &&
	 !(ptr[0] == 0xff && (ptr[1] & 0xf0) == 0xf0))
    ++ptr;

  return (end - ptr < 4) ? 0 : ptr;
}

static
int sync_header(struct mad_stream *stream, struct mad_frame *frame,
		unsigned short *crc)
{
  register unsigned char const *ptr, *end;
  struct mad_bitptr header;
  unsigned int pad_slot, index;

  ptr = stream->next_frame;
  end = stream->bufend;

  if (stream->sync) {
    if (end - ptr < 4) {
      stream->error = MAD_ERR_BUFLEN;
      goto fail;
    }
    else if (!(ptr[0] == 0xff && (ptr[1] & 0xf0) == 0xf0)) {
      stream->next_frame = ptr + 1;

      stream->error = MAD_ERR_LOSTSYNC;
      goto fail;
    }
  }
  else {
    if (ptr == 0) {
      stream->error = MAD_ERR_BUFPTR;
      goto fail;
    }

    ptr = sync_word(ptr, end);

    if (ptr == 0) {
      if (end - stream->next_frame >= 4)
	stream->next_frame = end - 4;

      stream->error = MAD_ERR_BUFLEN;
      goto fail;
    }
  }

  /* default recovery position: possibly bogus syncword */
  stream->next_frame = ptr + 1;

  /* skip syncword */
  mad_bit_init(&header, ptr + 1);
  mad_bit_seek(&header, 4);

  frame->flags = 0;

  /* ID */
  if (mad_bit_read(&header, 1) != 1) {
    stream->error = MAD_ERR_BADID;
    goto fail;
  }

  /* layer */
  frame->layer = (4 - mad_bit_read(&header, 2)) & 3;

  if (frame->layer == 0) {
    stream->error = MAD_ERR_BADLAYER;
    goto fail;
  }

  /* protection_bit */
  if (mad_bit_read(&header, 1) == 0) {
    frame->flags |= MAD_FLAG_PROTECTION;

    *crc = mad_crc(0xffff, header, 16);
  }

  /* bitrate_index */
  index = mad_bit_read(&header, 4);
  if (index == 15) {
    stream->error = MAD_ERR_BADBITRATE;
    goto fail;
  }

  frame->bitrate = bitrate_table[frame->layer - 1][index];

  /* sampling_frequency */
  index = mad_bit_read(&header, 2);
  if (index == 3) {
    stream->error = MAD_ERR_BADSAMPLEFREQ;
    goto fail;
  }

  frame->samplefreq = samplefreq_table[index];

  /* padding_bit */
  pad_slot = mad_bit_read(&header, 1);
  if (pad_slot)
    frame->flags |= MAD_FLAG_PADDING;

  /* private_bit */
  if (mad_bit_read(&header, 1))
    frame->flags |= MAD_FLAG_PRIVATE;

  /* mode */
  frame->mode = 3 - mad_bit_read(&header, 2);

  /* mode_extension */
  frame->mode_ext = mad_bit_read(&header, 2);

  /* copyright */
  if (mad_bit_read(&header, 1))
    frame->flags |= MAD_FLAG_COPYRIGHT;

  /* original/copy */
  if (mad_bit_read(&header, 1))
    frame->flags |= MAD_FLAG_ORIGINAL;

  /* emphasis */
  frame->emphasis = mad_bit_read(&header, 2);

  if (frame->emphasis == 2) {
    stream->error = MAD_ERR_BADEMPHASIS;
    goto fail;
  }

  /* set stream pointers */
  stream->this_frame = ptr;
  stream->ptr        = header;

  /* calculate beginning of next frame */
  if (frame->bitrate == 0 && stream->sync)
    frame->bitrate = stream->freerate;

  if (frame->bitrate) {
    unsigned int N;

    if (frame->layer == 1)
      N = ((12 * frame->bitrate / frame->samplefreq) + pad_slot) * 4;
    else
      N = (144 * frame->bitrate / frame->samplefreq) + pad_slot;

    /* verify there is enough data left in buffer to decode this frame */
    if (N + 4 > end - stream->this_frame) {
      stream->next_frame = stream->this_frame;

      stream->error = MAD_ERR_BUFLEN;
      goto fail;
    }

    stream->next_frame = stream->this_frame + N;
    stream->sync = 1;
  }

  /* calculate frame duration */
  {
    unsigned int time;

    time = time_table[index];

    frame->duration.seconds    = 0;
    frame->duration.parts36750 = (frame->layer == 1) ? time : 3 * time;
  }

  return 0;

 fail:
  stream->sync     = 0;
  stream->freerate = 0;

  return -1;
}

static
int (*const decoders[3])(struct mad_stream *, struct mad_frame *,
			 unsigned short const [2]) = {
  mad_layer_I, mad_layer_II, mad_layer_III
};

int mad_decode(struct mad_stream *stream, struct mad_frame *frame)
{
  unsigned short crc[2];
  int bad_last_frame;

  bad_last_frame = frame->flags & MAD_FLAG_CRCFAILED;

  if (sync_header(stream, frame, &crc[0]) == -1)
    goto fail;

  /* crc_check */
  if (frame->flags & MAD_FLAG_PROTECTION)
    crc[1] = mad_bit_read(&stream->ptr, 16);

  /* decode frame */
  if (decoders[frame->layer - 1](stream, frame, crc) == -1) {
    stream->anc_bitlen = 0;

    if (stream->error == MAD_ERR_BADCRC) {
      frame->flags |= MAD_FLAG_CRCFAILED;

      if (bad_last_frame)
	mad_frame_mute(frame);

      goto done;
    }

    goto fail;
  }

  /* find free bitrate */
  if (frame->bitrate == 0) {
    unsigned char const *ptr;
    unsigned int rate, pad_slot;

    ptr  = mad_bit_byte(&stream->ptr);
    rate = 0;

    pad_slot = (frame->flags & MAD_FLAG_PADDING) ? 1 : 0;

    while (rate == 0 && (ptr = sync_word(ptr, stream->bufend))) {
      unsigned int N, num;

      N = ptr - stream->this_frame;

      if (frame->layer == 1) {
	num = (frame->samplefreq * (N - 4 * pad_slot + 4));
	if (num % (4 * 12 * 1000) < 4800) {
	  rate = num / (4 * 12 * 1000);

	  if (rate > 448)
	    rate = 0;
	}
      }
      else {
	num = (frame->samplefreq * (N - pad_slot + 1));
	if (num % (144 * 1000) < 14400) {
	  rate = num / (144 * 1000);

	  if (frame->layer == 2) {
	    if (rate > 384)
	      rate = 0;
	  }
	  else {  /* frame->layer == 3 */
	    if (rate > 320)
	      rate = 0;
	  }
	}
      }

      if (rate) {
	stream->sync       = 1;
	stream->freerate   = rate * 1000;
	stream->next_frame = ptr;

	frame->bitrate     = stream->freerate;

# ifdef DEBUG
	fprintf(stderr, "free bitrate == %u\n", rate * 1000);
# endif
      }
      else
	++ptr;
    }

    if (rate == 0) {
      stream->error = MAD_ERR_LOSTSYNC;
      goto fail;
    }

    if (frame->layer == 2) {
      /* restart Layer II decoding since it depends on knowing bitrate */
      stream->next_frame = stream->this_frame;

      stream->error = MAD_ERR_LOSTSYNC;
      goto fail;
    }
  }

  /* designate ancillary bits */
  if (frame->layer != 3) {
    struct mad_bitptr next_frame;

    mad_bit_init(&next_frame, stream->next_frame);

    stream->anc_ptr    = stream->ptr;
    stream->anc_bitlen = mad_bit_length(&stream->ptr, &next_frame);
  }

 done:
  return 0;

 fail:
  return -1;
}
