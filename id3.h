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
 * $Id: id3.h,v 1.2 2000/03/19 06:43:38 rob Exp $
 */

extern char const *id3_error;

void id3_v1_show(int (*)(char const *, ...), unsigned char const [128]);
int id3_v2_read(int (*)(char const *, ...), unsigned char const *,
		 unsigned long, int, int, unsigned long *);

void id3_text(int (*)(char const *, ...), char const *, char const *,
	      unsigned int, unsigned char const *, unsigned long);
void id3_comment(int (*)(char const *, ...), char const *, char const *,
		 unsigned int, unsigned char const *, unsigned long);

