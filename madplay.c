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
 * $Id: madplay.c,v 1.36 2000/09/12 01:09:59 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <stdio.h>
# include <stdarg.h>
# include <stdlib.h>
# include <string.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <errno.h>

# ifdef HAVE_GETOPT_H
#  include <getopt.h>
# endif

# ifdef HAVE_MMAP
#  include <sys/mman.h>
# endif

# ifndef O_BINARY
#  define O_BINARY  0
# endif

# include "mad.h"
# include "audio.h"
# include "resample.h"
# include "id3.h"

# define MPEG_BUFSZ	32000

struct audio {
  int fd;
# ifdef HAVE_MMAP
  void *fdm;
# endif

  unsigned char *data;
  unsigned long len;

  int verbose;
  int quiet;

  unsigned long bad_frame;

  struct mad_decoder decoder;

  struct stats {
    unsigned long framecount;
    struct mad_timer timer;

    int vbr;
    unsigned int bitrate;
    unsigned long vbr_frames, vbr_rate;
    unsigned long nsecs;
  } stats;

  unsigned int channels;
  unsigned int speed_in, speed_out;

  int select;
  int mode;

  int (*command)(union audio_control *);

  struct resample_state resample[2];
  mad_fixed_t (*resampled)[2][MAX_NSAMPLES];
};

static
int on_same_line;

/*
 * NAME:	message()
 * DESCRIPTION:	show a message, possibly overwriting a previous w/o newline
 */
static
int message(char const *format, ...)
{
  int len, newline, result;
  va_list args;

  len = strlen(format);
  newline = (format[len - 1] == '\n');

  if (on_same_line && newline && len > 1)
    putc('\n', stderr);

  va_start(args, format);
  result = vfprintf(stderr, format, args);
  va_end(args);

  if (on_same_line && !newline && result < on_same_line) {
    unsigned int i;

    i = on_same_line - result;
    while (i--)
      putc(' ', stderr);
  }

  on_same_line = !newline ? result : 0;

  if (on_same_line) {
    putc('\r', stderr);
    fflush(stderr);
  }

  return result;
}

/*
 * NAME:	error()
 * DESCRIPTION:	show an error using proper interaction with message()
 */
static
void error(char const *id, char const *format, ...)
{
  int err;
  va_list args;

  err = errno;

  if (on_same_line)
    message("\n");

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

static
char const *const layer_str[3] = { "I", "II", "III" };

static
char const *const mode_str[4] = {
  "single channel", "dual channel", "joint stereo", "stereo"
};

/*
 * NAME:	gen_stats()
 * DESCRIPTION:	generate and output stream statistics
 */
static
void gen_stats(struct mad_frame const *frame, struct stats *stats)
{
  unsigned int bitrate;
  unsigned long seconds;

  bitrate = frame->bitrate / 1000;

  stats->vbr_rate += bitrate;
  stats->vbr_frames++;

  stats->vbr += (stats->bitrate && stats->bitrate != bitrate) ? 128 : -1;
  if (stats->vbr < 0)
    stats->vbr = 0;
  else if (stats->vbr > 512)
    stats->vbr = 512;

  stats->bitrate = bitrate;

  seconds = mad_timer_count(&stats->timer, MAD_TIMER_SECONDS);
  if (seconds >= stats->nsecs) {
    char time_str[14];
    char const *joint_str = "";

    stats->nsecs = seconds + 1;

    mad_timer_str(&stats->timer, time_str, "%02u:%02u", MAD_TIMER_MINUTES);

    if (frame->mode == MAD_MODE_JOINT_STEREO) {
      switch (frame->flags & (MAD_FLAG_MS_STEREO | MAD_FLAG_I_STEREO)) {
      case MAD_FLAG_MS_STEREO:
	joint_str = " (MS)";
	break;
      case MAD_FLAG_I_STEREO:
	joint_str = " (I)";
	break;
      case (MAD_FLAG_MS_STEREO | MAD_FLAG_I_STEREO):
	joint_str = " (MS+I)";
	break;
      }
    }

    message(" %s Layer %s, %s%u kbps%s, %u Hz, %s%s, %sCRC",
	    time_str, layer_str[frame->layer - 1],
	    stats->vbr ? "VBR (avg " : "",
	    stats->vbr ? ((stats->vbr_rate * 2) / stats->vbr_frames + 1) / 2 :
	    stats->bitrate,
	    stats->vbr ? ")" : "",
	    frame->sfreq, mode_str[frame->mode], joint_str,
	    (frame->flags & MAD_FLAG_PROTECTION) ? "" : "no ");
  }
}

/*
 * NAME:	error_str()
 * DESCRIPTION:	return a string describing a MAD error
 */
static
char const *error_str(int error)
{
  static char str[13];

  switch (error) {
  case MAD_ERR_NOMEM:		return "not enough memory";
  case MAD_ERR_LOSTSYNC:	return "lost synchronization";
  case MAD_ERR_BADLAYER:	return "reserved header layer value";
  case MAD_ERR_BADBITRATE:	return "forbidden bitrate value";
  case MAD_ERR_BADSAMPLEFREQ:	return "reserved sample frequency value";
  case MAD_ERR_BADEMPHASIS:	return "reserved emphasis value";
  case MAD_ERR_BADCRC:		return "CRC check failed";
  case MAD_ERR_BADBITALLOC:	return "forbidden bit allocation value";
  case MAD_ERR_BADSCALEFACTOR:	return "bad scalefactor index";
  case MAD_ERR_BADFRAMELEN:	return "bad frame length";
  case MAD_ERR_BADBIGVALUES:	return "bad big_values count";
  case MAD_ERR_BADBLOCKTYPE:	return "reserved block_type";
  case MAD_ERR_BADDATAPTR:	return "bad main_data_begin pointer";
  case MAD_ERR_BADDATALEN:	return "bad main data length";
  case MAD_ERR_BADPART3LEN:	return "bad audio data length";
  case MAD_ERR_BADHUFFTABLE:	return "bad Huffman table select";
  case MAD_ERR_BADSTEREO:	return "incompatible block_type for MS";

  default:
    sprintf(str, "error 0x%04x", error);
    return str;
  }
}

/*
 * NAME:	decode_input()
 * DESCRIPTION:	load input stream buffer with data
 */
static
int decode_input(void *data, struct mad_stream *stream)
{
  struct audio *audio = data;
  int len;

# ifdef HAVE_MMAP
  if (audio->fdm) {
    unsigned long skip = 0;

    if (stream->next_frame) {
      struct stat stat;

      if (fstat(audio->fd, &stat) == -1)
	return MAD_DECODER_BREAK;
      else if (stat.st_size <= audio->len)
	return MAD_DECODER_STOP;

      /* file size changed; update mmap */

      skip = stream->next_frame - audio->data;

      if (munmap(audio->fdm, audio->len) == -1) {
	audio->fdm  = 0;
	audio->data = 0;
	return MAD_DECODER_BREAK;
      }

      audio->fdm = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, audio->fd, 0);
      if (audio->fdm == MAP_FAILED) {
	audio->fdm  = 0;
	audio->data = 0;
	return MAD_DECODER_BREAK;
      }

      audio->data = audio->fdm;
      audio->len  = stat.st_size;
    }

    mad_stream_buffer(stream, audio->data + skip, audio->len - skip);

    return MAD_DECODER_CONTINUE;
  }
# endif

  if (stream->next_frame) {
    memmove(audio->data, stream->next_frame,
	    audio->len = &audio->data[audio->len] - stream->next_frame);
  }

  do {
    len = read(audio->fd, audio->data + audio->len, MPEG_BUFSZ - audio->len);
  }
  while (len == -1 && errno == EINTR);

  if (len == -1) {
    error("input", ":read");
    return MAD_DECODER_BREAK;
  }
  else if (len == 0)
    return MAD_DECODER_STOP;

  mad_stream_buffer(stream, audio->data, audio->len += len);

  return MAD_DECODER_CONTINUE;
}

/*
 * NAME:	decode_output()
 * DESCRIPTION:	configure audio module and output decoded samples
 */
static
int decode_output(void *data,
		  struct mad_frame const *frame, struct mad_synth const *synth)
{
  struct audio *audio = data;
  mad_fixed_t const *ch1, *ch2;
  unsigned int nchannels;
  union audio_control control;

  if (frame->mode == MAD_MODE_SINGLE_CHANNEL) {
    nchannels = 1;

    ch1 = synth->pcmout[0];
    ch2 = 0;
  }
  else if (frame->mode == MAD_MODE_DUAL_CHANNEL || audio->select) {
    nchannels = 1;

    if (audio->select == 0) {
      if (audio->quiet < 2)
	error("output", "no channel selected for dual channel; using first");

      audio->select = 1;
    }

    ch1 = synth->pcmout[audio->select - 1];
    ch2 = 0;
  }
  else {
    nchannels = 2;

    ch1 = synth->pcmout[0];
    ch2 = synth->pcmout[1];
  }

  if (audio->channels != nchannels ||
      audio->speed_in != frame->sfreq) {
    control.command = audio_cmd_config;

    control.config.channels = nchannels;
    control.config.speed    = frame->sfreq;

    if (audio->command(&control) == -1) {
      error("output", audio_error);
      return MAD_DECODER_BREAK;
    }

    audio->channels  = nchannels;
    audio->speed_in  = frame->sfreq;
    audio->speed_out = control.config.speed;

    /* check whether resampling is necessary */
    if (audio->speed_out == audio->speed_in) {
      if (audio->resampled) {
	resample_finish(&audio->resample[0]);
	resample_finish(&audio->resample[1]);

	free(audio->resampled);
	audio->resampled = 0;
      }
    }
    else {
      if (audio->quiet < 1) {
	error("output", "sample frequency %u Hz not available; closest %u Hz",
	      frame->sfreq, control.config.speed);
      }

      if (audio->resampled) {
	resample_finish(&audio->resample[0]);
	resample_finish(&audio->resample[1]);
      }
      else {
	audio->resampled = malloc(sizeof(*audio->resampled));
	if (audio->resampled == 0) {
	  error("output", "not enough memory to allocate resampling buffer");
	  return MAD_DECODER_BREAK;
	}
      }

      if (resample_init(&audio->resample[0],
			audio->speed_in, audio->speed_out) == -1 ||
	  resample_init(&audio->resample[1],
			audio->speed_in, audio->speed_out) == -1) {
	error("output", "cannot resample %u Hz to %u Hz",
	      audio->speed_in, audio->speed_out);

	free(audio->resampled);
	audio->resampled = 0;

	return MAD_DECODER_BREAK;
      }
      else if (audio->quiet < 2) {
	error("output", "resampling %u Hz to %u Hz",
	      audio->speed_in, audio->speed_out);
      }
    }
  }

  control.command = audio_cmd_play;

  if (audio->resampled) {
    control.play.nsamples = resample_block(&audio->resample[0], synth->pcmlen,
					   ch1, (*audio->resampled)[0]);
    control.play.samples[0] = (*audio->resampled)[0];

    if (ch2) {
      resample_block(&audio->resample[1], synth->pcmlen,
		     ch2, (*audio->resampled)[1]);
      control.play.samples[1] = (*audio->resampled)[1];
    }
    else
      control.play.samples[1] = 0;
  }
  else {
    control.play.nsamples   = synth->pcmlen;
    control.play.samples[0] = ch1;
    control.play.samples[1] = ch2;
  }

  control.play.mode = audio->mode;

  if (audio->command(&control) == -1) {
    error("output", audio_error);
    return MAD_DECODER_BREAK;
  }

  mad_timer_add(&audio->stats.timer, &frame->duration);
  audio->stats.framecount++;

  if (audio->verbose)
    gen_stats(frame, &audio->stats);

  return MAD_DECODER_CONTINUE;
}

/*
 * NAME:	decode_error()
 * DESCRIPTION:	handle a decoding error
 */
static
int decode_error(void *data, struct mad_stream *stream,
		 struct mad_frame *frame)
{
  struct audio *audio = data;

  if (stream->error == MAD_ERR_LOSTSYNC &&
      strncmp(stream->this_frame, "ID3", 3) == 0) {
    unsigned long tagsize;

    /* ID3v2 tag */

# ifdef HAVE_MMAP
    if (audio->fdm &&
	lseek(audio->fd,
	      stream->this_frame - stream->buffer, SEEK_SET) == -1) {
      error("error", ":lseek");
      return MAD_DECODER_BREAK;
    }
# endif

    if (id3_v2_read(message, stream->this_frame,
		    stream->bufend - stream->this_frame,
		    audio->fd, audio->quiet, &tagsize) == -1) {
      error("ID3", id3_error);
      return MAD_DECODER_BREAK;
    }
    else if (tagsize > stream->bufend - stream->this_frame)
      mad_stream_skip(stream, stream->bufend - stream->this_frame);
    else
      mad_stream_skip(stream, tagsize);
  }
  else if (stream->error == MAD_ERR_LOSTSYNC &&
	   strncmp(stream->this_frame, "TAG", 3) == 0) {
    /* ID3v1 tag */
    mad_stream_skip(stream, 128);
  }
  else if (audio->quiet < 2 &&
	   (stream->error == MAD_ERR_LOSTSYNC || stream->sync)) {
    error("error", "frame %lu: %s",
	  audio->stats.framecount, error_str(stream->error));
  }

  if (stream->error == MAD_ERR_BADCRC) {
    if (audio->bad_frame == audio->stats.framecount)
      mad_frame_mute(frame);

    audio->bad_frame = audio->stats.framecount + 1;

    return MAD_DECODER_IGNORE;
  }

  return MAD_DECODER_CONTINUE;
}

/*
 * NAME:	initialize()
 * DESCRIPTION:	prepare audio struct for decoding and output
 */
static
void initialize(struct audio *audio, int fd, int verbose, int quiet,
		int select, int mode,
		int (*filter)(void *, struct mad_frame *),
		int (*command)(union audio_control *))
{
  audio->fd = fd;
# ifdef HAVE_MMAP
  audio->fdm = 0;
# endif

  audio->data = 0;
  audio->len  = 0;

  audio->verbose = verbose;
  audio->quiet   = quiet;

  audio->bad_frame = 0;

  mad_decoder_init(&audio->decoder, audio,
		   decode_input, filter, decode_output, decode_error);

  audio->stats.framecount = 0;
  mad_timer_init(&audio->stats.timer);

  audio->stats.vbr        = 0;
  audio->stats.bitrate    = 0;
  audio->stats.vbr_frames = 0;
  audio->stats.vbr_rate   = 0;
  audio->stats.nsecs      = 0;

  audio->channels  = 0;
  audio->speed_in  = 0;
  audio->speed_out = 0;

  audio->select = select;
  audio->mode   = mode;

  audio->command = command;

  /* audio->resample */
  audio->resampled = 0;
}

/*
 * NAME:	finish()
 * DESCRIPTION:	terminate decoding
 */
static
void finish(struct audio *audio)
{
  mad_timer_finish(&audio->stats.timer);
  mad_decoder_finish(&audio->decoder);

  if (audio->resampled) {
    resample_finish(&audio->resample[0]);
    resample_finish(&audio->resample[1]);

    free(audio->resampled);
    audio->resampled = 0;
  }
}

/*
 * NAME:	filter->mono()
 * DESCRIPTION:	transform stereo frame to mono
 */
static
int filter_mono(void *data, struct mad_frame *frame)
{
  unsigned int ns, s, sb;
  mad_fixed_t left, right;

  if (frame->mode == MAD_MODE_SINGLE_CHANNEL)
    return MAD_DECODER_CONTINUE;

  ns = MAD_NSBSAMPLES(frame);

  for (s = 0; s < ns; ++s) {
    for (sb = 0; sb < 32; ++sb) {
      left  = frame->sbsample[0][s][sb];
      right = frame->sbsample[1][s][sb];

      frame->sbsample[0][s][sb] = (left + right) / 2;
      /* frame->sbsample[1][s][sb] = 0; */
    }
  }

  frame->mode = MAD_MODE_SINGLE_CHANNEL;

  return MAD_DECODER_CONTINUE;
}

# ifdef EXPERIMENTAL
/*
 * NAME:	filter->mixer()
 * DESCRIPTION:	pre-empt decoding by dumping frame to independent mixer
 */
static
int filter_mixer(void *data, struct mad_frame *frame)
{
  struct audio *audio = data;

  if (fwrite(frame, sizeof(*frame), 1, stdout) != 1)
    return MAD_DECODER_BREAK;

  mad_timer_add(&audio->stats.timer, &frame->duration);
  audio->stats.framecount++;

  if (audio->verbose)
    gen_stats(frame, &audio->stats);

  return MAD_DECODER_IGNORE;
}

/*
 * NAME:	filter->experiment()
 * DESCRIPTION:	experimental filter
 */
static
int filter_experiment(void *data, struct mad_frame *frame)
{
  struct audio *audio = data;
  unsigned int ns, s;
  static mad_fixed_t this, last;
  static signed long trend;

  ns = MAD_NSBSAMPLES(frame);

  for (s = 0; s < ns; ++s) {
    this = frame->sbsample[0][s][0];

    if (this == last || abs(abs(this) - abs(last)) < abs(this))
      ++trend;
    else
      --trend;

    message("%12.9f\t%12.9f\t%ld\n",
	    mad_f_todouble(this),
	    mad_f_todouble(abs(this) - abs(last)),
	    trend);

    last = this;
  }

  mad_timer_add(&audio->stats.timer, &frame->duration);
  audio->stats.framecount++;

  if (audio->verbose)
    gen_stats(frame, &audio->stats);

  return MAD_DECODER_IGNORE;
}
# endif

/*
 * NAME:	decode()
 * DESCRIPTION:	decode and output an entire bitstream
 */
static
int decode(struct audio *audio)
{
  unsigned char *ptr;
  int len, count, result = 0;
# ifdef HAVE_MMAP
  struct stat stat;
# endif

  /* check for ID3v1 tag */

  if (!audio->quiet &&
      lseek(audio->fd, -128, SEEK_END) != -1) {
    unsigned char id3v1[128];

    for (ptr = id3v1, count = 128; count; count -= len, ptr += len) {
      do {
	len = read(audio->fd, ptr, count);
      }
      while (len == -1 && errno == EINTR);

      if (len == -1) {
	error("decode", ":read");
	goto fail;
      }
      else if (len == 0)
	break;
    }

    if (count == 0 && strncmp(id3v1, "TAG", 3) == 0)
      id3_v1_show(message, id3v1);

    if (lseek(audio->fd, 0, SEEK_SET) == -1) {
      error("decode", ":lseek");
      goto fail;
    }
  }

  /* prepare input buffers */

# ifdef HAVE_MMAP
  if (fstat(audio->fd, &stat) == -1) {
    error("decode", ":fstat");
    goto fail;
  }

  if (S_ISREG(stat.st_mode) && stat.st_size > 0) {
    audio->fdm = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, audio->fd, 0);
    if (audio->fdm == MAP_FAILED) {
      error("decode", ":mmap");
      goto fail;
    }

    audio->data = audio->fdm;
    audio->len  = stat.st_size;
  }
# endif

  if (audio->data == 0) {
    audio->data = malloc(MPEG_BUFSZ);
    if (audio->data == 0) {
      error("decode", "not enough memory to allocate input buffer");
      goto fail;
    }

    audio->len = 0;
  }

  /* decode */

  if (mad_decoder_run(&audio->decoder, MAD_DECODER_SYNC) == -1) {
    error("decode", "error while decoding");
    goto fail;
  }

  goto done;

 fail:
  result = -1;

 done:
# ifdef HAVE_MMAP
  if (audio->fdm) {
    if (munmap(audio->fdm, audio->len) == -1) {
      error("decode", ":munmap");
      result = -1;
    }

    audio->fdm  = 0;
    audio->data = 0;
  }
# endif

  if (audio->data) {
    free(audio->data);
    audio->data = 0;
  }

  return result;
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
  fprintf(stderr, "Usage: "
	  "%s [options] [-o [type:]file.out] "
	  "file.mpg [...]\n", argv0);

  fprintf(stderr,
	  "\t-v\tshow status while decoding\n"
	  "\t-q\tbe quiet but show warnings\n"
	  "\t-Q\tbe quiet and do not show warnings\n"
	  "\n"
	  "\t-m\tmix channels to produce monaural output\n"
	  "\t-1\tselect first (left) channel output only\n"
	  "\t-2\tselect second (right) channel output only\n"
	  "\n"
# ifdef EXPERIMENTAL
	  "\t-x\toutput pre-synthesis samples for external mixer\n"
	  "\t-e\tenable experimental filter\n"
	  "\n"
# endif
	  "\t-d\tdo not dither output samples\n"
	  "\n"
	  "\t-o\tselect output format and destination\n");

  fprintf(stderr, "\nSupported output format types:\n"
	  "\tRAW\tbinary signed 16-bit little-endian linear PCM\n"
	  "\tWAVE\tMicrosoft RIFF/WAVE, 16-bit PCM format\n"
# ifdef DEBUG
	  "\tHEX\thexadecimal signed 24-bit linear PCM\n"
# endif
	  "\tNULL\tno output (decode only)\n");

  exit(1);
}

/*
 * NAME:	main()
 * DESCRIPTION:	program entry point
 */
int main(int argc, char *argv[])
{
  int opt, i, fd, verbose = 0, quiet = 0, result = 0, select, mode;
  int (*filter)(void *, struct mad_frame *) = 0;
  int (*output)(union audio_control *) = 0;
  char const *fname, *opath = 0;
  struct audio audio;

  if (argc > 1) {
    if (strcmp(argv[1], "--version") == 0 ||
	strcmp(argv[1], "--copyright") == 0) {
      printf("%s - %s\n", mad_version, mad_copyright);
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
  }

  /* select all channels by default */
  select = 0;

  /* dither output by default */
  mode = AUDIO_MODE_DITHER;

  while ((opt = getopt(argc, argv, "vqQ"
		       "m12d"
# ifdef EXPERIMENTAL
		       "xe"
# endif
		       "o:")) != -1) {
    switch (opt) {
    case 'v':
      verbose = 1;
      quiet   = 0;
      break;

    case 'q':
      quiet   = 1;
      verbose = 0;
      break;

    case 'Q':
      quiet   = 2;
      verbose = 0;
      break;

    case 'm':
      filter = filter_mono;
      select = 0;
      break;

    case '1':
    case '2':
      select = opt - '1' + 1;
      if (filter == filter_mono)
	filter = 0;
      break;

    case 'd':
      mode = AUDIO_MODE_ROUND;
      break;

# ifdef EXPERIMENTAL
    case 'x':
      filter = filter_mixer;
      break;

    case 'e':
      filter = filter_experiment;
      break;
# endif

    case 'o':
      opath = optarg;

      output = audio_output(&opath);
      if (output == 0)
	error(0, "%s: undecided output module; using default", opath);
      break;

    default:
      usage(argv[0]);
    }
  }

  if (optind == argc)
    usage(argv[0]);

# ifdef EXPERIMENTAL
  if (filter == filter_mixer)
    output = 0;
  else
# endif
    if (output == 0)
      output = audio_output(0);

  if (!quiet)
    message("%s - %s\n", mad_version, mad_copyright);

  if (output && audio_init(output, opath) == -1)
    return 2;

  for (i = optind; i < argc; ++i) {
    if (strcmp(argv[i], "-") == 0) {
      fname = "stdin";
      fd = STDIN_FILENO;
    }
    else {
      fname = argv[i];
      fd = open(fname, O_RDONLY | O_BINARY);
      if (fd == -1) {
	error(0, ":", fname);
	result = 3;
	continue;
      }
    }

    initialize(&audio, fd, verbose, quiet, select, mode, filter, output);

    if (decode(&audio) == -1)
      result = 4;
    else if (!quiet) {
      char time_str[19];

      mad_timer_str(&audio.stats.timer, time_str,
		    "%u:%02u:%02u.%1u", MAD_TIMER_HOURS);

      message("%s: %lu frames decoded (%s)\n", fname,
	      audio.stats.framecount, time_str);
    }

    finish(&audio);

    if (fd != STDIN_FILENO && close(fd) == -1) {
      error(0, ":", fname);
      result = 5;
    }
  }

  if (output && audio_finish(output) == -1)
    result = 6;

  return result;
}
