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
 * $Id: resample.c,v 1.2 2000/09/10 22:04:18 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "resample.h"
# include "mad.h"

static
mad_fixed_t const resample_table[6][6] = {
  /* 48000 */ { 0x10000000L /* 1.000000000 */,
		0x116a3b36L /* 1.088435374 */,
		0x18000000L /* 1.500000000 */,
		0x20000000L /* 2.000000000 */,
		0x22d4766cL /* 2.176870748 */,
		0x30000000L /* 3.000000000 */ },

  /* 44100 */ { 0x0eb33333L /* 0.918750000 */,
		0x10000000L /* 1.000000000 */,
		0x160ccccdL /* 1.378125000 */,
		0x1d666666L /* 1.837500000 */,
		0x20000000L /* 2.000000000 */,
		0x2c19999aL /* 2.756250000 */ },

  /* 32000 */ { 0x0aaaaaabL /* 0.666666667 */,
		0x0b9c2779L /* 0.725623583 */,
		0x10000000L /* 1.000000000 */,
		0x15555555L /* 1.333333333 */,
		0x17384ef3L /* 1.451247166 */,
		0x20000000L /* 2.000000000 */ },

  /* 24000 */ { 0x08000000L /* 0.500000000 */,
		0x08b51d9bL /* 0.544217687 */,
		0x0c000000L /* 0.750000000 */,
		0x10000000L /* 1.000000000 */,
		0x116a3b36L /* 1.088435374 */,
		0x18000000L /* 1.500000000 */ },

  /* 22050 */ { 0x0759999aL /* 0.459375000 */,
		0x08000000L /* 0.500000000 */,
		0x0b066666L /* 0.689062500 */,
		0x0eb33333L /* 0.918750000 */,
		0x10000000L /* 1.000000000 */,
		0x160ccccdL /* 1.378125000 */ },

  /* 16000 */ { 0x05555555L /* 0.333333333 */,
		0x05ce13bdL /* 0.362811791 */,
		0x08000000L /* 0.500000000 */,
		0x0aaaaaabL /* 0.666666667 */,
		0x0b9c2779L /* 0.725623583 */,
		0x10000000L /* 1.000000000 */ }
};

static
int supported_rate(unsigned int rate)
{
  switch (rate) {
  case 48000:
  case 44100:
  case 32000:
  case 24000:
  case 22050:
  case 16000:
    return 1;
  }

  return 0;
}

/*
 * NAME:	resample_init()
 * DESCRIPTION:	initialize resampling state
 */
int resample_init(struct resample_state *state,
		  unsigned int oldrate, unsigned int newrate)
{
  unsigned int oldi, newi;

  if (!supported_rate(oldrate) ||
      !supported_rate(newrate))
    return -1;

  /* 48000 => 0, 44100 => 1, 32000 => 2,
     24000 => 3, 22050 => 4, 16000 => 5 */
  oldi = ((oldrate >> 7) & 0x000f) + ((oldrate >> 15) & 0x0001) - 8;
  newi = ((newrate >> 7) & 0x000f) + ((newrate >> 15) & 0x0001) - 8;

  state->ratio = resample_table[oldi][newi];

  state->step = 0;
  state->last = 0;

  return 0;
}

/*
 * NAME:	resample_block()
 * DESCRIPTION:	algorithmically change the sampling rate of a PCM sample block
 */
unsigned int resample_block(struct resample_state *state,
			    unsigned int nsamples, mad_fixed_t const *old,
			    mad_fixed_t *new)
{
  mad_fixed_t const *end, *begin;

  /*
   * This resampling algorithm is based on a linear interpolation, which is
   * not at all the best sounding but is relatively fast and efficient.
   *
   * A better algorithm would be one that implements a bandlimited
   * interpolation.
   */

  if (state->ratio == MAD_F_ONE) {
    memcpy(new, old, nsamples * sizeof(mad_fixed_t));
    return nsamples;
  }

  end   = old + nsamples;
  begin = new;

  if (state->step < 0) {
    state->step = mad_f_fracpart(-state->step);

    while (state->step < MAD_F_ONE) {
      *new++ = state->step ?
	state->last + mad_f_mul(*old - state->last, state->step) : state->last;

      state->step += state->ratio;
      if (((state->step + 0x00000080L) & 0x0fffff00L) == 0)
	state->step = (state->step + 0x00000080L) & ~0x0fffffffL;
    }

    state->step -= MAD_F_ONE;
  }

  while (end - old > 1 + mad_f_intpart(state->step)) {
    old        += mad_f_intpart(state->step);
    state->step = mad_f_fracpart(state->step);

    *new++ = state->step ?
      *old + mad_f_mul(old[1] - old[0], state->step) : *old;

    state->step += state->ratio;
    if (((state->step + 0x00000080L) & 0x0fffff00L) == 0)
      state->step = (state->step + 0x00000080L) & ~0x0fffffffL;
  }

  if (end - old == 1 + mad_f_intpart(state->step)) {
    state->last = end[-1];
    state->step = -state->step;
  }
  else
    state->step -= mad_f_fromint(end - old);

  return new - begin;
}
