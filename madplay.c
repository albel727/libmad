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
 * $Id: madplay.c,v 1.13 2000/03/05 05:19:16 rob Exp $
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

# define MPEG_BUFSZ	16384

struct audio {
  struct mad_stream stream;
  struct mad_frame frame;
  struct mad_synth synth;
  struct mad_timer timer;

  void (*filter)(struct mad_frame *);

  unsigned long framecount;

  short stereo;
  unsigned int speed;

  union audio_control control;
  int (*command)(union audio_control *);
};

static
int on_same_line;

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

static
int error(char const *format, ...)
{
  int err, result;
  va_list args;

  err = errno;

  newline();

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
void initialize(struct audio *audio, void (*filter)(struct mad_frame *),
		int (*command)(union audio_control *))
{
  mad_stream_init(&audio->stream);
  mad_frame_init(&audio->frame);
  mad_synth_init(&audio->synth);
  mad_timer_init(&audio->timer);

  audio->filter = filter;

  audio->framecount = 0;

  audio->stereo = -1;
  audio->speed  = 0;

  audio->command = command;
}

static
int play(struct audio *audio)
{
  if (audio->stereo != (audio->frame.mode ? 1 : 0) ||
      audio->speed  != audio->frame.samplefreq) {
    audio->control.command = audio_cmd_config;

    audio->control.config.stereo = audio->frame.mode ? 1 : 0;
    audio->control.config.speed  = audio->frame.samplefreq;

    if (audio->command(&audio->control) == -1) {
      error(audio_error);
      return -1;
    }

    audio->stereo = audio->control.config.stereo;
    audio->speed  = audio->control.config.speed;
  }

  if (audio->filter)
    audio->filter(&audio->frame);

  mad_synthesis(&audio->frame, &audio->synth);

  audio->control.command = audio_cmd_play;

  audio->control.play.nsamples   = audio->synth.pcmlen;
  audio->control.play.samples[0] = audio->synth.pcmout[0];
  audio->control.play.samples[1] = audio->synth.pcmout[1];

  if (audio->command(&audio->control) == -1) {
    error(audio_error);
    return -1;
  }

  mad_timer_add(&audio->timer, &audio->frame.duration);
  audio->framecount++;

  return 0;
}

static
void filter_mono(struct mad_frame *frame)
{
  unsigned int ns, s, sb;
  fixed_t left, right;

  if (frame->mode == MAD_MODE_SINGLE_CHANNEL)
    return;

  ns = MAD_NUMSAMPLES(frame);

  for (s = 0; s < ns; ++s) {
    for (sb = 0; sb < 32; ++sb) {
      left  = frame->sbsample[0][s][sb];
      right = frame->sbsample[1][s][sb];

      frame->sbsample[0][s][sb] = (left + right) >> 1;
      /* frame->sbsample[1][s][sb] = 0; */
    }
  }

  frame->mode = MAD_MODE_SINGLE_CHANNEL;
}

static
char const *error_str(int error)
{
  static char str[13];

  switch (error) {
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

static
char const *const genre_str[256] = {
  "Blues",		"Classic Rock",		"Country",
  "Dance",		"Disco",		"Funk",
  "Grunge",		"Hip-Hop",		"Jazz",
  "Metal",		"New Age",		"Oldies",
  "Other",		"Pop",			"R&B",
  "Rap",		"Reggae",		"Rock",
  "Techno",		"Industrial",		"Alternative",
  "Ska",		"Death Metal",		"Pranks",
  "Soundtrack",		"Euro-Techno",		"Ambient",
  "Trip-Hop",		"Vocal",		"Jazz+Funk",
  "Fusion",		"Trance",		"Classical",
  "Instrumental",	"Acid",			"House",
  "Game",		"Sound Clip",		"Gospel",
  "Noise",		"AlternRock",		"Bass",
  "Soul",		"Punk",			"Space",
  "Meditative",		"Instrumental Pop",	"Instrumental Rock",
  "Ethnic",		"Gothic",		"Darkwave",
  "Techno-Industrial",	"Electronic",		"Pop-Folk",
  "Eurodance",		"Dream",		"Southern Rock",
  "Comedy",		"Cult",			"Gangsta",
  "Top 40",		"Christian Rap",	"Pop/Funk",
  "Jungle",		"Native American",	"Cabaret",
  "New Wave",		"Psychadelic",		"Rave",
  "Showtunes",		"Trailer",		"Lo-Fi",
  "Tribal",		"Acid Punk",		"Acid Jazz",
  "Polka",		"Retro",		"Musical",
  "Rock & Roll",	"Hard Rock",

  /* Winamp extensions */

  "Folk",		"Folk-Rock",		"National Folk",
  "Swing",		"Fast Fusion",		"Bebob",
  "Latin",		"Revival",		"Celtic",
  "Bluegrass",		"Avantgarde",		"Gothic Rock",
  "Progressive Rock",	"Psychedelic Rock",	"Symphonic Rock",
  "Slow Rock",		"Big Band",		"Chorus",
  "Easy Listening",	"Acoustic",		"Humour",
  "Speech",		"Chanson",		"Opera",
  "Chamber Music",	"Sonata",		"Symphony",
  "Booty Bass",		"Primus",		"Porn Groove",
  "Satire",		"Slow Jam",		"Club",
  "Tango",		"Samba",		"Folklore",
  "Ballad",		"Power Ballad",		"Rhythmic Soul",
  "Freestyle",		"Duet",			"Punk Rock",
  "Drum Solo",		"A capella",		"Euro-House",
  "Dance Hall"
};

static
void show_id3v1(unsigned char const id3v1[128])
{
  char title[31], artist[31], album[31], year[5], comment[31];
  char const *genre;

  /* ID3v1 */

  title[30] = artist[30] = album[30] = year[4] = comment[30] = 0;

  memcpy(title,   &id3v1[3],  30);
  memcpy(artist,  &id3v1[33], 30);
  memcpy(album,   &id3v1[63], 30);
  memcpy(year,    &id3v1[93],  4);
  memcpy(comment, &id3v1[97], 30);

  genre = genre_str[id3v1[127]];
  if (genre == 0)
    genre = "";

  message(" Title: %-30s  Artist: %s\n"
	  " Album: %-30s   Genre: %s\n",
	  title, artist, album, genre);

  if (comment[28] == 0 && comment[29] != 0) {
    unsigned int track;

    /* ID3v1.1 */

    track = comment[29];

    message("  Year: %-4s  Track: %-3u               Comment: %s\n",
	    year, track, comment);
  }
  else {
    message("  Year: %-4s                           Comment: %s\n",
	    year, comment);
  }
}

static
char const *const layer_str[3] = { "I", "II", "III" };

static
char const *const mode_str[4] = {
  "single channel", "dual channel", "joint stereo", "stereo"
};

static
int decode(int fd, struct audio *audio, int verbose, int quiet)
{
  static unsigned char *data;
  unsigned int bitrate = 0, vbr_frames = 0, vbr_rate = 0, dlen = 0;
  int layer = 0, vbr = 0;
  void *fdm = 0;
  char time_str[8];
# ifdef HAVE_MMAP
  struct stat stat;
# endif

  /* check for ID3v1 tag */

  if (lseek(fd, -128, SEEK_END) != -1) {
    unsigned char id3v1[128];

    if (read(fd, id3v1, 128) == 128 &&
	id3v1[0] == 'T' && id3v1[1] == 'A' && id3v1[2] == 'G') {
      if (!quiet)
	show_id3v1(id3v1);
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
      error(":lseek");
      goto fail;
    }
  }

  /* regular decoding */

# ifdef HAVE_MMAP
  if (fstat(fd, &stat) == -1) {
    error(":fstat");
    goto fail;
  }

  if (S_ISREG(stat.st_mode) && stat.st_size > 0) {
    fdm = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fdm == MAP_FAILED) {
      error(":mmap");
      goto fail;
    }

    mad_stream_buffer(&audio->stream, fdm, stat.st_size);
  }
  else
# endif
    if (!data) {
      data = malloc(MPEG_BUFSZ);
      if (data == 0) {
	error("not enough memory to allocate input buffer");
	goto fail;
      }
    }

  while (1) {
    if (!fdm) {
      int len;

      len = read(fd, data + dlen, MPEG_BUFSZ - dlen);
      if (len == -1) {
	error(":read");
	goto fail;
      }
      else if (len == 0)
	break;

      mad_stream_buffer(&audio->stream, data, dlen += len);
    }

    while (1) {
      if (mad_decode(&audio->stream, &audio->frame) == -1) {
	if (MAD_RECOVERABLE(audio->stream.error)) {
	  if (!quiet) {
	    error("frame %lu: %s", audio->framecount,
		  error_str(audio->stream.error));
	  }
	  continue;
	}
	else
	  break;
      }

# if 0
      if ((audio->frame.flags & (MAD_FLAG_PROTECTION | MAD_FLAG_CRCFAILED)) ==
	  (MAD_FLAG_PROTECTION | MAD_FLAG_CRCFAILED))
	continue;
# endif

      if (layer == 0)
	layer = audio->frame.layer;
      else if (audio->frame.layer != layer ||
	       audio->frame.samplefreq != audio->speed) {
	/* not usually a good thing; avoid playing garbage */
	layer = 0;
	continue;
      }

      if (play(audio) == -1) {
# ifdef HAVE_MMAP
	if (fdm)
	  munmap(fdm, stat.st_size);
# endif

	goto fail;
      }

      if (verbose) {
	if (bitrate && audio->frame.bitrate != bitrate)
	  vbr = 1;

	bitrate   = audio->frame.bitrate;
	vbr_rate += bitrate;

	if (++vbr_frames == 0)
	  vbr = 0;  /* overflow; prevent division by 0 */

	while (vbr_rate   && vbr_rate   % 2 == 0 &&
	       vbr_frames && vbr_frames % 2 == 0) {
	  vbr_rate   /= 2;
	  vbr_frames /= 2;
	}

	mad_timer_str(&audio->timer, time_str,
		      "%02u:%02u.%u", timer_minutes);

	message(" %s Layer %s, %s%u kbps%s, %u Hz, %s; %sCRC%s",
		time_str, layer_str[audio->frame.layer - 1],
		vbr ? "VBR (avg " : "",
		vbr ? vbr_rate / vbr_frames / 1000 : bitrate / 1000,
		vbr ? ")" : "",
		audio->frame.samplefreq, mode_str[audio->frame.mode],
		(audio->frame.flags & MAD_FLAG_PROTECTION) ? "" : "no ",
		(audio->frame.flags & MAD_FLAG_CRCFAILED) ? " failed" : "");
      }
    }

    if (fdm)
      break;
    else {
      memmove(data, audio->stream.next_frame,
	      dlen = &data[dlen] - audio->stream.next_frame);
    }
  }

  newline();

# ifdef HAVE_MMAP
  if (fdm && munmap(fdm, stat.st_size) == -1) {
    error(":munmap");
    goto fail;
  }
# endif

  return 0;

 fail:
  return -1;
}

int main(int argc, char *argv[])
{
  int opt, read_stdin = 0, verbose = 0, quiet = 0;
  int i, result, fd;
  void (*filter)(struct mad_frame *) = 0;
  int (*output)(union audio_control *) = 0;
  char const *fname, *opath = 0;
  static struct audio audio;

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

  while ((opt = getopt(argc, argv, "mvqo:")) != -1) {
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

    case 'o':
      output = audio_output(opath = optarg);
      if (output == 0)
	error("%s: no output module available; using default", opath);
      break;

    default:
      error("Usage: %s [-m] [-v|-q] [-o file.out] [file.mpg ...]", argv[0]);
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

  audio.control.command   = audio_cmd_init;
  audio.control.init.path = opath;
  if (output(&audio.control) == -1) {
    error(audio_error, audio.control.init.path);
    return 2;
  }

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
	error(":", fname);
	result = 3;
	continue;
      }
    }

    initialize(&audio, filter, output);

    if (decode(fd, &audio, verbose, quiet) == -1)
      result = 4;
    else if (!quiet)
      message("%s: %lu frames decoded\n", fname, audio.framecount);

    if (read_stdin)
      read_stdin = 0;
    else if (close(fd) == -1) {
      error(":close");
      result = 5;
    }
  }

  audio.control.command = audio_cmd_finish;
  if (output(&audio.control) == -1) {
    error(audio_error);
    result = 6;
  }

  return result;
}
