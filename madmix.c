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
 * $Id: madmix.c,v 1.6 2000/09/17 18:49:32 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <stdio.h>
# include <stdarg.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>

# ifdef HAVE_GETOPT_H
#  include <getopt.h>
# endif

# include "mad.h"
# include "audio.h"

struct audio {
  char const *fname;
  FILE *file;
  int active;
  mad_fixed_t scale;

  struct mad_frame frame;
};

/*
 * NAME:	error()
 * DESCRIPTION:	show a labeled error message
 */
static
void error(char const *id, char const *format, ...)
{
  int err;
  va_list args;

  err = errno;

  if (id)
    fprintf(stderr, "%s: ", id);

  va_start(args, format);

  if (*format == ':') {
    if (format[1] == 0) {
      format = va_arg(args, char const *);
      errno = err;
      perror(format);
    }
    else {
      errno = err;
      perror(format + 1);
    }
  }
  else {
    vfprintf(stderr, format, args);
    putc('\n', stderr);
  }

  va_end(args);
}

/*
 * NAME:	do_output()
 * DESCRIPTION:	play mixed output
 */
static
int do_output(int (*audio)(union audio_control *),
	      struct mad_frame *frame, struct mad_synth *synth)
{
  union audio_control control;
  static unsigned int channels;
  static unsigned long speed;

  if (channels != MAD_NCHANNELS(frame) ||
      speed    != frame->sfreq) {
    control.command = audio_cmd_config;

    control.config.channels = MAD_NCHANNELS(frame);
    control.config.speed    = frame->sfreq;

    if (audio(&control) == -1) {
      error("output", audio_error);
      return -1;
    }

    channels = MAD_NCHANNELS(frame);
    speed    = frame->sfreq;
  }

  control.command = audio_cmd_play;

  control.play.nsamples   = synth->pcmlen;
  control.play.samples[0] = synth->pcmout[0];
  control.play.samples[1] = synth->pcmout[1];

  if (audio(&control) == -1) {
    error("output", audio_error);
    return -1;
  }

  return 0;
}

/*
 * NAME:	do_mix()
 * DESCRIPTION:	perform mixing and audio output
 */
static
int do_mix(struct audio *mix, int ninputs, int (*audio)(union audio_control *))
{
  struct mad_frame frame;
  struct mad_synth synth;
  int i, count;

  mad_frame_init(&frame);
  mad_synth_init(&synth);

  count = ninputs;

  while (1) {
    int ch, s, sb;

    mad_frame_mute(&frame);

    for (i = 0; i < ninputs; ++i) {
      if (!mix[i].active)
	continue;

      if (fread(&mix[i].frame, sizeof(mix[i].frame), 1, mix[i].file) != 1) {
	if (ferror(mix[i].file))
	  error("fread", ":", mix[i].fname);
	mix[i].active = 0;
	--count;
	continue;
      }

      mix[i].frame.overlap = 0;

      if (frame.layer == 0) {
	frame.layer    = mix[i].frame.layer;
	frame.mode     = mix[i].frame.mode;
	frame.mode_ext = mix[i].frame.mode_ext;
	frame.emphasis = mix[i].frame.emphasis;

	frame.bitrate  = mix[i].frame.bitrate;
	frame.sfreq    = mix[i].frame.sfreq;

	frame.flags    = mix[i].frame.flags;
	frame.private  = mix[i].frame.private;

	frame.duration = mix[i].frame.duration;
      }

      for (ch = 0; ch < 2; ++ch) {
	for (s = 0; s < 36; ++s) {
	  for (sb = 0; sb < 32; ++sb) {
	    frame.sbsample[ch][s][sb] +=
	      mad_f_mul(mix[i].frame.sbsample[ch][s][sb], mix[i].scale);
	  }
	}
      }
    }

    if (count == 0)
      break;

    mad_synth_frame(&synth, &frame);
    do_output(audio, &frame, &synth);
  }

  mad_synth_finish(&syth);
  mad_frame_finish(&frame);

  return 0;
}

/*
 * NAME:	audio->init()
 * DESCRIPTION:	initialize the audio output module
 */
static
int audio_init(int (*audio)(union audio_control *), char const *path)
{
  union audio_control control;

  control.command   = audio_cmd_init;
  control.init.path = path;

  if (audio(&control) == -1) {
    error("audio", audio_error, control.init.path);
    return -1;
  }

  return 0;
}

/*
 * NAME:	audio->finish()
 * DESCRIPTION:	terminate the audio output module
 */
static
int audio_finish(int (*audio)(union audio_control *))
{
  union audio_control control;

  control.command = audio_cmd_finish;

  if (audio(&control) == -1) {
    error("audio", audio_error);
    return -1;
  }

  return 0;
}

/*
 * NAME:	usage()
 * DESCRIPTION:	display usage message and exit
 */
static
void usage(char const *argv0)
{
  error("Usage", "%s input1 [input2 ...]", argv0);
}

/*
 * NAME:	main()
 * DESCRIPTION:	program entry point
 */
int main(int argc, char *argv[])
{
  int opt, ninputs, i, result = 0;
  int (*output)(union audio_control *) = 0;
  char const *fname, *opath = 0;
  FILE *file;
  struct audio *mix;

  if (argc > 1) {
    if (strcmp(argv[1], "--version") == 0) {
      printf("%s - %s\n", mad_version, mad_copyright);
      printf("Build options: %s\n", mad_build);
      fprintf(stderr, "`%s --license' for licensing information.\n", argv[0]);
      return 0;
    }
    if (strcmp(argv[1], "--license") == 0) {
      printf("\n%s\n\n%s\n", mad_copyright, mad_license);
      return 0;
    }
    if (strcmp(argv[1], "--author") == 0) {
      printf("%s\n", mad_author);
      return 0;
    }
    if (strcmp(argv[1], "--help") == 0) {
      usage(argv[0]);
      return 0;
    }
  }

  while ((opt = getopt(argc, argv, "o:")) != -1) {
    switch (opt) {
    case 'o':
      opath = optarg;

      output = audio_output(&opath);
      if (output == 0) {
	error(0, "%s: unknown output format type", opath);
	return 2;
      }
      break;

    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (optind == argc) {
    usage(argv[0]);
    return 1;
  }

  if (output == 0)
    output = audio_output(0);

  if (audio_init(output, opath) == -1)
    return 2;

  ninputs = argc - optind;

  mix = malloc(ninputs * sizeof(*mix));
  if (mix == 0) {
    error(0, "not enough memory to allocate mixing buffers");
    return 3;
  }

  printf("mixing %d stream%s\n", ninputs, ninputs == 1 ? "" : "s");

  for (i = 0; i < ninputs; ++i) {
    if (strcmp(argv[optind + i], "-") == 0) {
      fname = "stdin";
      file  = stdin;
    }
    else {
      fname = argv[optind + i];
      file = fopen(fname, "rb");
      if (file == 0) {
	error(0, ":", fname);
	return 4;
      }
    }

    mix[i].fname  = fname;
    mix[i].file   = file;
    mix[i].active = 1;
    mix[i].scale  = mad_f_tofixed(1.0);  /* / ninputs); */
  }

  if (do_mix(mix, ninputs, output) == -1)
    result = 5;

  for (i = 0; i < ninputs; ++i) {
    file = mix[i].file;

    if (file != stdin) {
      if (fclose(file) == EOF) {
	error(0, ":", mix[i].fname);
	result = 6;
      }
    }
  }

  free(mix);

  if (audio_finish(output) == -1)
    result = 7;

  return result;
}
