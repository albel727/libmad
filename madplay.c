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
 * $Id: madplay.c,v 1.17 2000/03/19 06:43:38 rob Exp $
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
# include <sys/mman.h>
# include <errno.h>

# include "libmad.h"
# include "audio.h"
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

  struct mad_decoder decoder;

  struct stats {
    unsigned long framecount;
    struct mad_timer timer;

    int vbr;
    unsigned int bitrate, vbr_frames, vbr_rate;
    unsigned long nsecs;
  } stats;

  unsigned int channels;
  unsigned int speed;

  int (*command)(union audio_control *);
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

  if (on_same_line) {
    if (newline) {
      if (len > 1)
	fprintf(stderr, "\n");
    }
  }

  va_start(args, format);
  result = vfprintf(stderr, format, args);
  va_end(args);

  if (on_same_line && !newline && result < on_same_line) {
    unsigned int i;

    i = on_same_line - result;
    while (i--)
      fprintf(stderr, " ");
  }

  on_same_line = !newline ? result : 0;

  if (on_same_line) {
    fprintf(stderr, "\r");
    fflush(stderr);
  }

  return result;
}

# define newline()  (on_same_line ? message("\n") : 0)

/*
 * NAME:	error()
 * DESCRIPTION:	show an error using proper interaction with message()
 */
static
int error(char const *id, char const *format, ...)
{
  int err, result;
  va_list args;

  err = errno;

  newline();

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

    result = 0;
  }
  else {
    result  = vfprintf(stderr, format, args);
    result += fprintf(stderr, "\n");
  }

  va_end(args);

  return result;
}

static
char const *const layer_str[3] = { "I", "II", "III" };

static
char const *const mode_str[4] = {
  "single channel", "dual channel", "joint stereo", "stereo"
};

/*
 * NAME:	show_stats()
 * DESCRIPTION:	output stream statistics
 */
static
void show_stats(struct mad_frame const *frame, struct stats *stats)
{
  char time_str[6];

  if (stats->bitrate && frame->bitrate != stats->bitrate)
    stats->vbr = 1;

  stats->vbr_rate += (stats->bitrate = frame->bitrate);

  if (++stats->vbr_frames == 0)
    stats->vbr = 0;  /* overflow; prevent division by 0 */

  while (stats->vbr_rate   && stats->vbr_rate   % 2 == 0 &&
	 stats->vbr_frames && stats->vbr_frames % 2 == 0) {
    stats->vbr_rate   /= 2;
    stats->vbr_frames /= 2;
  }

  if (mad_timer_seconds(&stats->timer) >= stats->nsecs) {
    stats->nsecs = mad_timer_seconds(&stats->timer) + 1;

    mad_timer_str(&stats->timer, time_str, "%02u:%02u", MAD_TIMER_MINUTES);

    message(" %s Layer %s, %s%u kbps%s, %u Hz, %s, %sCRC",
	    time_str, layer_str[frame->layer - 1],
	    stats->vbr ? "VBR (avg " : "",
	    stats->vbr ? (stats->vbr_rate / stats->vbr_frames + 500) / 1000 :
	    stats->bitrate / 1000,
	    stats->vbr ? ")" : "",
	    frame->sfreq, mode_str[frame->mode],
	    (frame->flags & MAD_FLAG_PROTECTION) ? "" : "no ");
  }
}

/*
 * NAME:	error_str()
 * DESCRIPTION:	return a string describing MAD error
 */
static
char const *error_str(int error)
{
  static char str[13];

  switch (error) {
  case MAD_ERR_NOMEM:		return "not enough memory";
  case MAD_ERR_LOSTSYNC:	return "lost synchronization";
  case MAD_ERR_BADID:		return "bad header ID field";
  case MAD_ERR_BADLAYER:	return "reserved header layer value";
  case MAD_ERR_BADBITRATE:	return "forbidden bitrate value";
  case MAD_ERR_BADSAMPLEFREQ:	return "reserved sample frequency value";
  case MAD_ERR_BADEMPHASIS:	return "reserved emphasis value";
  case MAD_ERR_BADCRC:		return "CRC check failed";
  case MAD_ERR_BADBITALLOC:	return "forbidden bit allocation value";
  case MAD_ERR_BADSCALEFACTOR:	return "bad scalefactor index";
  case MAD_ERR_BADBIGVALUES:	return "bad big_values count";
  case MAD_ERR_BADBLOCKTYPE:	return "reserved block_type";
  case MAD_ERR_BADDATAPTR:	return "bad main_data_begin pointer";
  case MAD_ERR_BADDATALENGTH:	return "bad main data length";
  case MAD_ERR_BADPART3LEN:	return "bad audio data length";
  case MAD_ERR_BADHUFFTABLE:	return "bad Huffman table select";
  case MAD_ERR_BADHUFFDATA:	return "Huffman data value out of range";
  case MAD_ERR_BADSTEREO:	return "incompatible block_type for M/S";

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
  union audio_control control;

  if (audio->channels != MAD_NUMCHANNELS(frame) ||
      audio->speed    != frame->sfreq) {
    control.command = audio_cmd_config;

    control.config.channels = MAD_NUMCHANNELS(frame);
    control.config.speed    = frame->sfreq;

    if (audio->command(&control) == -1) {
      error("output", audio_error);
      return MAD_DECODER_BREAK;
    }

    if (audio->quiet < 2 && control.config.speed != frame->sfreq) {
      error("output", "sample frequency %u not available (closest %u)",
	    frame->sfreq, control.config.speed);
    }

    audio->channels = MAD_NUMCHANNELS(frame);
    audio->speed    = frame->sfreq;
  }

  control.command = audio_cmd_play;

  control.play.nsamples   = synth->pcmlen;
  control.play.samples[0] = synth->pcmout[0];
  control.play.samples[1] = synth->pcmout[1];

  if (audio->command(&control) == -1) {
    error("output", audio_error);
    return MAD_DECODER_BREAK;
  }

  mad_timer_add(&audio->stats.timer, &frame->duration);
  audio->stats.framecount++;

  if (audio->verbose)
    show_stats(frame, &audio->stats);

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
    error("error", "frame %lu: %s", audio->stats.framecount,
	  error_str(stream->error));
  }

  return MAD_DECODER_CONTINUE;
}

/*
 * NAME:	initialize()
 * DESCRIPTION:	prepare audio struct for decoding and output
 */
static
void initialize(struct audio *audio, int fd, int verbose, int quiet,
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

  mad_decoder_init(&audio->decoder);
  mad_decoder_input(&audio->decoder, decode_input, audio);
  mad_decoder_filter(&audio->decoder, filter, audio);
  mad_decoder_output(&audio->decoder, decode_output, audio);
  mad_decoder_error(&audio->decoder, decode_error, audio);

  audio->stats.framecount = 0;
  mad_timer_init(&audio->stats.timer);

  audio->stats.vbr        = 0;
  audio->stats.bitrate    = 0;
  audio->stats.vbr_frames = 0;
  audio->stats.vbr_rate   = 0;
  audio->stats.nsecs      = 0;

  audio->channels = 0;
  audio->speed    = 0;

  audio->command = command;
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
}

/*
 * NAME:	filter->mono()
 * DESCRIPTION:	transform stereo frame to mono
 */
static
int filter_mono(void *data, struct mad_frame *frame)
{
  unsigned int ns, s, sb;
  fixed_t left, right;

  if (frame->mode == MAD_MODE_SINGLE_CHANNEL)
    return MAD_DECODER_CONTINUE;

  ns = MAD_NUMSBSAMPLES(frame);

  for (s = 0; s < ns; ++s) {
    for (sb = 0; sb < 32; ++sb) {
      left  = frame->sbsample[0][s][sb];
      right = frame->sbsample[1][s][sb];

      frame->sbsample[0][s][sb] = (left + right) >> 1;
      /* frame->sbsample[1][s][sb] = 0; */
    }
  }

  frame->mode = MAD_MODE_SINGLE_CHANNEL;

  return MAD_DECODER_CONTINUE;
}

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
      error("decode", ":unmap");
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
 * NAME:	main()
 * DESCRIPTION:	program entry point
 */
int main(int argc, char *argv[])
{
  int opt, read_stdin = 0, verbose = 0, quiet = 0;
  int i, result, fd;
  int (*filter)(void *, struct mad_frame *) = 0;
  int (*output)(union audio_control *) = 0;
  char const *fname, *opath = 0;
  struct audio audio;

  if (argc > 1) {
    if (strcmp(argv[1], "--version") == 0) {
      printf("%s - %s\n", mad_version, mad_copyright);
      printf("`%s --license' for licensing information.\n", argv[0]);
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

  while ((opt = getopt(argc, argv, "mvqQo:")) != -1) {
    switch (opt) {
    case 'm':
      filter = filter_mono;
      break;

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

    case 'o':
      opath = optarg;

      output = audio_output(&opath);
      if (output == 0)
	error(0, "%s: no output module available; using default", opath);
      break;

    default:
      error(0, "Usage: %s [-m] [-v|-q|-Q] [-o file.out] [file.mpg ...]",
	    argv[0]);
      return 1;
    }
  }

  if (output == 0)
    output = audio_output(0);

  if (optind == argc) {
    read_stdin = 1;
    ++argc;
  }

  if (!quiet)
    message("%s - %s\n", mad_version, mad_copyright);

  if (audio_init(output, opath) == -1)
    return 2;

  result = 0;

  for (i = optind; i < argc; ++i) {
    if (!read_stdin && strcmp(argv[i], "-") == 0)
      read_stdin = 1;

    if (read_stdin) {
      fname = "stdin";
      fd = STDIN_FILENO;
    }
    else {
      fname = argv[i];
      fd = open(fname, O_RDONLY);
      if (fd == -1) {
	error(0, ":", fname);
	result = 3;
	continue;
      }
    }

    initialize(&audio, fd, verbose, quiet, filter, output);

    if (decode(&audio) == -1)
      result = 4;
    else if (!quiet) {
      char time_str[14];

      mad_timer_str(&audio.stats.timer, time_str,
		    "%u:%02u.%1u", MAD_TIMER_MINUTES);

      message("%s: %lu frames decoded (%s)\n", fname,
	      audio.stats.framecount, time_str);
    }

    finish(&audio);

    if (read_stdin)
      read_stdin = 0;
    else if (close(fd) == -1) {
      error(0, ":", fname);
      result = 5;
    }
  }

  if (audio_finish(output) == -1)
    result = 6;

  return result;
}
