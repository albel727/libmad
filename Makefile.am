##
## mad - MPEG audio decoder
## Copyright (C) 2000-2001 Robert Leslie
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
## If you would like to negotiate alternate licensing terms, you may do
## so by contacting the author: Robert Leslie <rob@mars.org>
##
## $Id: Makefile.am,v 1.14 2001/10/17 22:07:13 rob Exp $
##

## Process this file with automake to produce Makefile.in

if BUILD_WINAMP
winamp = winamp
else
winamp =
endif

if BUILD_LIBZ
libz = libz
ldadd_libz = -Llibz
else
libz =
ldadd_libz =
endif

SUBDIRS =		intl po $(libz) libid3 libmad $(winamp)
DIST_SUBDIRS =		intl po   libz  libid3 libmad   winamp  msvc++

bin_PROGRAMS =		madplay
bin_SCRIPTS =		abxtest
EXTRA_PROGRAMS =	madtime minimad madmix mad123

man_MANS =		madplay.1 abxtest.1

EXTRA_DIST =  \
	$(man_MANS) $(bin_SCRIPTS)  \
	CHANGES COPYRIGHT CREDITS README TODO VERSION  \
	id3genre.dat
# id3genre.dat temporary until Winamp plug-in is revised

default_audio =  \
	audio.c audio_raw.c audio_wave.c audio_snd.c audio_null.c audio.h

extra_audio =  \
	audio_empeg.c audio_hex.c audio_oss.c audio_sun.c audio_win32.c

common_sources =  \
	version.c version.h global.h

madplay_SOURCES =  \
	$(common_sources) $(default_audio) madplay.c  \
	resample.c filter.c equalizer.c xing.c player.c  \
	resample.h filter.h equalizer.h xing.h player.h  \
	getopt.c getopt1.c getopt.h

EXTRA_madplay_SOURCES =  \
	$(extra_audio)

madtime_SOURCES =	$(common_sources) madtime.c
minimad_SOURCES =	minimad.c
mad123_SOURCES =	mad123.c

madmix_SOURCES =	$(common_sources) madmix.c $(default_audio)
EXTRA_madmix_SOURCES =	$(extra_audio)

AUDIO =			@AUDIO@

localedir =		$(datadir)/locale
INCLUDES =		-Ilibmad -I$(srcdir)/libid3 -Iintl  \
			-DLOCALEDIR=\"$(localedir)\"

LDADD =			@LIBOBJS@ libmad/libmad.la @INTLLIBS@

madplay_DEPENDENCIES =	$(AUDIO) $(LDADD) libid3/libid3.la
madplay_LDADD =		$(AUDIO) $(LDADD) libid3/libid3.la $(ldadd_libz) -lz

madmix_DEPENDENCIES =	$(AUDIO) $(LDADD)
madmix_LDADD =		$(AUDIO) $(LDADD)

CLEANFILES =		$(EXTRA_PROGRAMS) *.exe gmon.out

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

profile: madplay gmon.out
	gprof madplay | less

gmon.out: madplay test/profile.mpg
	./madplay -vo pcm:/dev/null test/profile.mpg || rm -f gmon.out

.c.s:
	$(COMPILE) -S $<

again: clean all

.PHONY: profile again
