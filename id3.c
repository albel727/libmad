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
 * $Id: id3.c,v 1.2 2000/03/19 06:43:38 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <sys/types.h>
# include <unistd.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>

# include "id3v2.h"
# include "id3.h"

char const *id3_error;

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

/*
 * NAME:	v1->show()
 * DESCRIPTION:	decode and display ID3v1 (or ID3v1.1) tag
 */
void id3_v1_show(int (*message)(char const *, ...),
		 unsigned char const tag[128])
{
  char title[31], artist[31], album[31], year[5], comment[31];
  char const *genre;

  /* ID3v1 */

  title[30] = artist[30] = album[30] = year[4] = comment[30] = 0;

  memcpy(title,   &tag[3],  30);
  memcpy(artist,  &tag[33], 30);
  memcpy(album,   &tag[63], 30);
  memcpy(year,    &tag[93],  4);
  memcpy(comment, &tag[97], 30);

  genre = genre_str[tag[127]];
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

/*
 * NAME:	int2()
 * DESCRIPTION:	decode 16-bit big-endian value
 */
static
unsigned int int2(unsigned char const *ptr)
{
  return (ptr[0] << 8) | (ptr[1] << 0);
}

/*
 * NAME:	int3()
 * DESCRIPTION:	decode 24-bit big-endian value
 */
static
unsigned long int3(unsigned char const *ptr)
{
  return (ptr[0] << 16) | (ptr[1] << 8) | (ptr[2] << 0);
}

/*
 * NAME:	int4()
 * DESCRIPTION: decode 32-bit big-endian value
 */
static
unsigned long int4(unsigned char const *ptr)
{
  return (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | (ptr[3] << 0);
}

/*
 * NAME:	skip()
 * DESCRIPTION:	ignore data from input
 */
static
int skip(int fd, unsigned long count)
{
  if (lseek(fd, count, SEEK_CUR) == -1) {
    unsigned char buffer[256];
    unsigned long len;

    while (count) {
      do {
	len = read(fd, buffer, count < sizeof(buffer) ?
		   count : sizeof(buffer));
      }
      while (len == -1 && errno == EINTR);

      if (len == -1)
	return -1;
      else if (len == 0)
	return 0;

      count -= len;
    }
  }

  return 0;
}

/*
 * NAME:	v2->read()
 * DESCRIPTION: decode and optionally display ID3v2 tag
 */
int id3_v2_read(int (*message)(char const *, ...),
		unsigned char const *buffer, unsigned long buflen,
		int fd, int quiet, unsigned long *tagsize)
{
  unsigned char header[10];
  struct {
    unsigned int major;
    unsigned int revision;
  } version;
  unsigned int flags;
  unsigned long size, count, len;
  unsigned char *ptr, *end, *tag = 0;
  int result = 0;

  enum {
    flag_unsync       = 0x80,
    flag_extended     = 0x40,
    flag_experimental = 0x20,

    flag_unknown      = 0x1f
  };

  if (buflen < 10) {
    memcpy(header, buffer, buflen);

    for (ptr = header + buflen, count = 10 - buflen;
	 count; count -= len, ptr += len) {
      do {
	len = read(fd, ptr, count);
      }
      while (len == -1 && errno == EINTR);

      if (len == -1) {
	id3_error = ":read";
	goto fail;
      }
      else if (len == 0) {
	id3_error = "EOF while reading tag";
	goto fail;
      }
    }

    buffer += buflen;
    buflen  = 0;
  }
  else {
    memcpy(header, buffer, 10);
    buffer += 10;
    buflen -= 10;
  }

  if (header[3] == 0xff || header[4] == 0xff ||
      (header[6] & 0x80) || (header[7] & 0x80) ||
      (header[8] & 0x80) || (header[9] & 0x80)) {
    id3_error = "invalid header";
    goto fail;
  }

  version.major    = header[3];
  version.revision = header[4];

  flags = header[5];

  /* high bit is not used */
  size = (header[6] << 21) | (header[7] << 14) |
         (header[8] <<  7) | (header[9] <<  0);

  *tagsize = 10 + size;

  if (quiet)
    goto abort;

  if (version.major < 2 || version.major > 3) {
    message("ID3: version 2.%u.%u not supported\n",
	    version.major, version.revision);
    goto abort;
  }

  message("ID3: version 2.%u.%u, flags 0x%02x, size %lu bytes\n",
	  version.major, version.revision, flags, size);

  if (size == 0)
    goto done;

  tag = malloc(size);
  if (tag == 0) {
    message("ID3: not enough memory to allocate tag buffer\n");
    goto abort;
  }

  if (buflen < size) {
    memcpy(tag, buffer, buflen);

    for (ptr = tag + buflen, count = size - buflen;
	 count; count -= len, ptr += len) {
      do {
	len = read(fd, ptr, count);
      }
      while (len == -1 && errno == EINTR);

      if (len == -1) {
	id3_error = ":read";
	goto fail;
      }
      else if (len == 0) {
	id3_error = "EOF while reading tag";
	goto fail;
      }
    }

    buffer += buflen;
    buflen  = 0;
  }
  else {
    memcpy(tag, buffer, size);
    buffer += size;
    buflen -= size;
  }

  if (flags & flag_unknown)
    message("ID3: unknown flags 0x%02x\n", flags & flag_unknown);

  /* undo unsynchronisation */

  if (flags & flag_unsync) {
    unsigned char *new = tag;

    count = size;

    for (ptr = tag; ptr < tag + size - 1; ++ptr) {
      *new++ = *ptr;

      if (ptr[0] == 0xff && ptr[1] == 0x00)
	--count, ++ptr;
    }

    size = count;
  }

  ptr = tag;
  end = tag + size;

  /* extended header */

  if ((flags & flag_extended) && end - ptr >= 4) {
    unsigned int ext_flags;
    unsigned long padding;

    count = int4(ptr);

    ptr += 4;

    if (count >= 6 && end - ptr >= count) {
      enum {
	ext_flag_crc = 0x8000
      };

      ext_flags = int2(&ptr[0]);
      padding   = int4(&ptr[2]);

      ptr   += 6;
      count -= 6;

      message("ID3: extended header flags 0x%04x, %lu bytes padding\n",
	      ext_flags, padding);

      end -= padding;

      if ((ext_flags & ext_flag_crc) &&
	  count >= 4 && end - ptr >= count) {
	unsigned long crc;

	crc = int4(ptr);

	ptr   += 4;
	count -= 4;

	/* check CRC... */
	message("ID3: total frame CRC 0x%04lx\n", crc);
      }
    }

    /* skip remainder of extended header (if any) */
    ptr += count;
  }

  /* experimental notice */

  if (flags & flag_experimental)
    message("ID3: experimental tag\n");

  /* read frames */

  while (ptr < end) {
    char id[5];
    unsigned int frame_flags;
    struct id3v2_frame const *frame;

    enum {
      frame_tag_alter_preserve  = 0x8000,
      frame_file_alter_preserve = 0x4000,
      frame_read_only           = 0x2000,

      frame_compression         = 0x0080,
      frame_encryption          = 0x0040,
      frame_grouping_identity   = 0x0020
    };

    if (version.major == 2) {
      if (end - ptr < 6)
	break;

      memcpy(id, ptr, len = 3);
      id[3] = 0;

      size        = int3(&ptr[3]);
      frame_flags = 0;

      ptr += 6;
    }
    else {  /* version.major == 3 */
      if (end - ptr < 10)
	break;

      memcpy(id, ptr, len = 4);
      id[4] = 0;

      size        = int4(&ptr[4]);
      frame_flags = int2(&ptr[8]);

      ptr += 10;
    }

    if (id[0] == 0 ||
	end - ptr < size)
      break;

    frame = id3v2_hash(id, len);
    if (frame) {
      if (frame->handler) {
	frame->handler(message, frame->id, frame->description,
		       frame_flags, ptr, size);
      }
      else {
	message("ID3: unhandled %s (%s): flags 0x%04x, %lu bytes\n",
		frame->id, frame->description, frame_flags, size);
      }
    }
    else {
      message("ID3: unknown frame \"%s\" (flags 0x%04x; %lu bytes)\n",
	      id, frame_flags, size);
    }

    ptr += size;
  }

  goto done;

 fail:
  result = -1;

 done:
  if (tag)
    free(tag);

  return result;

 abort:
  /* skip the unread part of the tag */
  return (size > buflen) ? skip(fd, size - buflen) : 0;
}

/*
 * NAME:	content_type()
 * DESCRIPTION:	decode content type or calculate length
 */
static
unsigned long content_type(unsigned char const *ptr, unsigned long size,
			   char *text)
{
  unsigned char const *end = &ptr[size];
  unsigned long count = 0;

  if (text)
    *text = 0;

  while (ptr < end && *ptr == '(') {
    unsigned char const *start;
    unsigned int genre;

    start = ptr++;

    if (ptr == end || *ptr == ')') {
      ptr = start;
      break;
    }

    if (*ptr == '(')
      break;

    if (end - ptr >= 3 && ptr[2] == ')' &&
	(strncmp(ptr, "RX", 2) == 0 ||
	 strncmp(ptr, "CR", 2) == 0)) {
      if (count) {
	if (text)
	  strcat(text, "/");
	++count;
      }

      if (text)
	strcat(text, *ptr == 'R' ? "Remix" : "Cover");
      count += 5;

      ptr += 3;
      continue;
    }

    genre = 0;
    while (ptr < end) {
      if (*ptr < '0' || *ptr > '9')
	break;

      genre = 10 * genre + (*ptr - '0');
      if (genre > 255)
	break;

      ++ptr;
    }

    if (ptr == end || *ptr != ')' || genre > 255 || genre_str[genre] == 0) {
      ptr = start;
      break;
    }

    ++ptr;

    if (count) {
      if (text)
	strcat(text, "/");
      ++count;
    }

    if (text)
      strcat(text, genre_str[genre]);
    count += strlen(genre_str[genre]);
  }

  if (text) {
    strncat(text, ptr, end - ptr);
    text[count] = 0;
  }
  count += end - ptr;

  return count;
}

/*
 * NAME:	id3->text()
 * DESCRIPTION:	handle text frame
 */
void id3_text(int (*message)(char const *, ...),
	      char const *id, char const *description, unsigned int flags,
	      unsigned char const *data, unsigned long size)
{
  char *text;
  int content;
  unsigned long count;

  if (size == 0) {
    message("ID3: %s: no data\n", id);
    return;
  }

  if (*data) {
    message("ID3: %s: Unicode\n", description);
    return;
  }

  content = (size > 1 && data[1] == '(' &&
	     (strcmp(id, "TCON") == 0 || strcmp(id, "TCO") == 0));

  if (content)
    count = content_type(data + 1, size - 1, 0);
  else
    count = size - 1;

  text = malloc(count + 1);
  if (text == 0) {
    message("ID3: %s: not enough memory\n", description);
    return;
  }

  if (content)
    content_type(data + 1, size - 1, text);
  else {
    memcpy(text, data + 1, size - 1);
    text[size - 1] = 0;
  }

  message(" %s: %s\n", description, text);

  free(text);
}

/*
 * NAME:	id3->comment()
 * DESCRIPTION:	handle comment frame
 */
void id3_comment(int (*message)(char const *, ...),
		 char const *id, char const *description, unsigned int flags,
		 unsigned char const *data, unsigned long size)
{
  char language[4];
  char *text;
  unsigned char const *descrip, *ptr;
  unsigned long len;

  if (size == 0) {
    message("ID3: %s: no data\n", id);
    return;
  }

  if (*data) {
    message("ID3: %s: Unicode\n", description);
    return;
  }

  if (size < 5)
    goto fail;

  memcpy(language, data + 1, 3);
  language[3] = 0;

  descrip = &data[4];

  ptr = memchr(descrip, 0, size - 4);
  if (ptr == 0)
    goto fail;

  message(" %s: %s\n", description, descrip);

  ++ptr;
  len = &data[size] - ptr;
  if (len == 0)
    return;

  text = malloc(len + 1);
  if (text == 0) {
    message("ID3: %s: not enough memory\n", description);
    return;
  }

  memcpy(text, ptr, len);
  if (text[len - 1] == '\n')
    text[len - 1] = 0;
  else
    text[len] = 0;

  message("%s\n", text);

  free(text);
  return;

 fail:
  message("ID3: %s: bad format\n", id);
}
