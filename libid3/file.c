/*
 * libid3 - ID3 tag manipulation library
 * Copyright (C) 2000-2001 Robert Leslie
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
 * $Id: file.c,v 1.2 2001/10/17 19:10:08 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "global.h"

# include <stdio.h>
# include <stdlib.h>

# include "id3.h"
# include "file.h"
# include "tag.h"
# include "field.h"

struct filetag {
  struct id3_tag *tag;
  fpos_t location;
  id3_length_t length;
};

struct id3_file {
  FILE *iofile;
  enum id3_file_mode mode;
  int options;
  struct id3_tag *primary;
  unsigned int ntags;
  struct filetag *tags;
};

/*
 * NAME:	query_tag()
 * DESCRIPTION:	check for a tag at a file's current position
 */
static
signed long query_tag(FILE *iofile)
{
  fpos_t position;
  id3_byte_t query[ID3_TAG_QUERYSIZE];
  signed long size;

  if (fgetpos(iofile, &position) == -1)
    return 0;

  size = id3_tag_query(query, fread(query, 1, sizeof(query), iofile));

  if (fsetpos(iofile, &position) == -1)
    return 0;

  return size;
}

/*
 * NAME:	read_tag()
 * DESCRIPTION:	read and parse a tag at a file's current position
 */
static
struct id3_tag *read_tag(FILE *iofile, id3_length_t size)
{
  id3_byte_t *data;
  struct id3_tag *tag = 0;

  data = malloc(size);
  if (data == 0)
    return 0;

  if (fread(data, size, 1, iofile) == 1)
    tag = id3_tag_parse(data, size);

  free(data);

  return tag;
}

/*
 * NAME:	update_primary()
 * DESCRIPTION:	update the primary tag with data from a new tag
 */
static
int update_primary(struct id3_tag *tag, struct id3_tag const *new)
{
  unsigned int i;

  if (!(new->extendedflags & ID3_TAG_EXTENDEDFLAG_TAGISANUPDATE))
    id3_tag_clearframes(tag);

  for (i = 0; i < new->nframes; ++i) {
    if (id3_tag_attachframe(tag, new->frames[i]) == -1)
      return -1;
  }

  return 0;
}

/*
 * NAME:	add_tag()
 * DESCRIPTION:	read, parse, and add a tag to a file structure
 */
static
int add_tag(struct id3_file *file, id3_length_t length)
{
  fpos_t location;
  struct filetag filetag, *tags;
  struct id3_tag *tag;

  if (fgetpos(file->iofile, &location) == -1)
    return -1;

  tags = realloc(file->tags, (file->ntags + 1) * sizeof(*tags));
  if (tags == 0)
    return -1;

  file->tags = tags;

  tag = read_tag(file->iofile, length);
  if (tag == 0)
    return -1;

  if (update_primary(file->primary, tag) == -1) {
    id3_tag_delete(tag);
    return -1;
  }

  filetag.tag      = tag;
  filetag.location = location;
  filetag.length   = length;

  file->tags[file->ntags++] = filetag;

  id3_tag_addref(tag);

  return 0;
}

/*
 * NAME:	search_tags()
 * DESCRIPTION:	search for tags in a file
 */
static
int search_tags(struct id3_file *file)
{
  signed long size;

  /* look for an ID3v1 tag */

  if (fseek(file->iofile, -128, SEEK_END) == 0) {
    size = query_tag(file->iofile);
    if (size > 0) {
      if (add_tag(file, size) == -1)
	return -1;

      file->options |= ID3_FILE_OPTION_ID3V1;
    }
  }

  /* look for a tag at the beginning of file */

  rewind(file->iofile);

  size = query_tag(file->iofile);
  if (size > 0) {
    struct id3_frame const *frame;

    if (add_tag(file, size) == -1)
      return -1;

    /* locate tags indicated by SEEK frames */

    while ((frame = id3_tag_findframe(file->tags[file->ntags - 1].tag,
				      "SEEK", 0))) {
      long seek;

      seek = id3_field_getint(&frame->fields[0]);
      if (seek < 0 || fseek(file->iofile, seek, SEEK_CUR) == -1)
	break;

      size = query_tag(file->iofile);
      if (size > 0 && add_tag(file, size) == -1)
	return -1;
    }
  }

  /* look for a tag before an ID3v1 tag */

  if (fseek(file->iofile, -128 - 10, SEEK_END) == 0) {
    size = query_tag(file->iofile);
    if (size < 0 && fseek(file->iofile, size, SEEK_CUR) == 0) {
      size = query_tag(file->iofile);
      if (size > 0 && add_tag(file, size) == -1)
	return -1;
    }
  }

  return 0;
}

/*
 * NAME:	file->open()
 * DESCRIPTION:	open a file and read its associated tags
 */
struct id3_file *id3_file_open(char const *path, enum id3_file_mode mode)
{
  struct id3_file *file;

  file = malloc(sizeof(*file));
  if (file == 0)
    goto fail;

  file->iofile = fopen(path, (mode == ID3_FILE_MODE_READWRITE) ? "r+b" : "rb");
  if (file->iofile == 0)
    goto fail;

  file->mode    = mode;
  file->options = 0;

  file->primary = id3_tag_new();
  if (file->primary == 0)
    goto fail;

  id3_tag_addref(file->primary);

  file->ntags = 0;
  file->tags  = 0;

  /* load tags from file */

  if (search_tags(file) == -1) {
    id3_file_close(file);
    file = 0;
  }

  if (0) {
  fail:
    if (file) {
      free(file);
      file = 0;
    }
  }

  return file;
}

/*
 * NAME:	file->close()
 * DESCRIPTION:	close and file and delete its associated tags
 */
void id3_file_close(struct id3_file *file)
{
  unsigned int i;

  fclose(file->iofile);

  id3_tag_delref(file->primary);
  id3_tag_delete(file->primary);

  for (i = 0; i < file->ntags; ++i) {
    id3_tag_delref(file->tags[i].tag);
    id3_tag_delete(file->tags[i].tag);
  }

  if (file->tags)
    free(file->tags);

  free(file);
}

/*
 * NAME:	file->tag()
 * DESCRIPTION:	return the primary tag structure for a file
 */
struct id3_tag *id3_file_tag(struct id3_file *file)
{
  return file->primary;
}

/*
 * NAME:	file->update()
 * DESCRIPTION:	rewrite tag(s) to a file
 */
int id3_file_update(struct id3_file *file)
{
  if (file->mode != ID3_FILE_MODE_READWRITE)
    return -1;

  /* ... */

  return 0;
}
