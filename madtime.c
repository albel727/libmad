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
 * $Id: madtime.c,v 1.8 2000/09/08 00:47:25 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <stdio.h>
# include <stdlib.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <string.h>
# include <sys/mman.h>

# ifdef HAVE_GETOPT_H
#  include <getopt.h>
# endif

# ifndef O_BINARY
#  define O_BINARY  0
# endif

# include "mad.h"

static
signed int scan(unsigned char const *ptr, unsigned long len,
		struct mad_timer *duration)
{
  struct mad_stream stream;
  struct mad_frame frame;
  unsigned long bitrate, kbps, count;
  int vbr;

  mad_stream_init(&stream);
  mad_frame_init(&frame);

  mad_stream_buffer(&stream, ptr, len);

  bitrate = kbps = count = vbr = 0;

  while (1) {
    if (mad_frame_header(&frame, &stream, 0) == -1) {
      if (MAD_RECOVERABLE(stream.error))
	continue;
      else
	break;
    }

    if (bitrate && frame.bitrate != bitrate)
      vbr = 1;

    bitrate = frame.bitrate;

    kbps += bitrate / 1000;
    ++count;

    mad_timer_add(duration, &frame.duration);
  }

  mad_frame_finish(&frame);
  mad_stream_finish(&stream);

  if (count == 0)
    count = 1;

  return ((kbps * 2) / count + 1) / 2 * (vbr ? -1 : 1);
}

static
int calc(char const *path, struct mad_timer *duration,
	 signed int *kbps, unsigned long *kbytes)
{
  int fd;
  struct stat stat;
  void *fdm;

  fd = open(path, O_RDONLY | O_BINARY);
  if (fd == -1) {
    perror(path);
    return -1;
  }

  if (fstat(fd, &stat) == -1) {
    perror("fstat");
    close(fd);
    return -1;
  }

  if (!S_ISREG(stat.st_mode)) {
    fprintf(stderr, "%s: Not a regular file\n", path);
    close(fd);
    return -1;
  }

  *kbytes = (stat.st_size + 512) / 1024;

  fdm = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (fdm == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return -1;
  }

  if (fdm) {
    *kbps = scan(fdm, stat.st_size, duration);

    if (munmap(fdm, stat.st_size) == -1) {
      perror("munmap");
      close(fd);
      return -1;
    }
  }
  else
    *kbps = 0;

  if (close(fd) == -1) {
    perror("close");
    return -1;
  }

  return 0;
}

static
void show(struct mad_timer const *duration, signed int kbps,
	  unsigned long kbytes, char const *label)
{
  char duration_str[13];

  mad_timer_str(duration, duration_str, "%4u:%02u:%02u.%1u", MAD_TIMER_HOURS);

  printf("%8.1f MB  %c%3u kbps  %s  %s\n", kbytes / 1024.0,
	 kbps < 0 ? '~' : ' ', abs(kbps), duration_str, label);
}

static
void usage(char const *argv0)
{
  fprintf(stderr, "Usage: %s [-s] file.mpg [...]\n", argv0);
}

/*
 * NAME:	main()
 * DESCRIPTION:	program entry point
 */
int main(int argc, char *argv[])
{
  struct mad_timer total;
  unsigned long total_kbps, total_kbytes, count;
  signed int bitrate;
  int vbr, opt, i, sum_only = 0;

  while ((opt = getopt(argc, argv, "s")) != -1) {
    switch (opt) {
    case 's':
      sum_only = 1;
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

  mad_timer_init(&total);

  total_kbps = total_kbytes = count = bitrate = vbr = 0;

  for (i = optind; i < argc; ++i) {
    struct mad_timer duration;
    signed int kbps;
    unsigned long kbytes;

    mad_timer_init(&duration);

    if (calc(argv[i], &duration, &kbps, &kbytes) == -1)
      continue;

    if (!sum_only)
      show(&duration, kbps, kbytes, argv[i]);

    mad_timer_add(&total, &duration);

    total_kbytes += kbytes;

    if (kbps) {
      total_kbps += abs(kbps);
      ++count;
    }

    if (kbps < 0 || (bitrate && kbps != bitrate))
      vbr = 1;

    bitrate = kbps;

    mad_timer_finish(&duration);
  }

  if (count == 0)
    count = 1;

  if (argc > 2 || sum_only) {
    show(&total, ((total_kbps * 2) / count + 1) / 2 * (vbr ? -1 : 1),
	 total_kbytes, "TOTAL");
  }

  mad_timer_finish(&total);

  return 0;
}
