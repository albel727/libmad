/* C code produced by gperf version 2.7 */
/* Command-line: gperf -tCTonD -K id -N id3v2_hash -s -3 -k * id3v2.gperf  */
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
 * Id: id3v2.gperf,v 1.2 2000/03/19 06:43:38 rob Exp 
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <string.h>

# include "id3.h"
# include "id3v2.h"

#define TOTAL_KEYWORDS 137
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 4
#define MIN_HASH_VALUE 29
#define MAX_HASH_VALUE 237
/* maximum key range = 209, duplicates = 17 */

#ifdef __GNUC__
__inline
#endif
static unsigned int
hash (str, len)
     register const char *str;
     register unsigned int len;
{
  static const unsigned char asso_values[] =
    {
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238,  26,
       28,  16,   0, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238,   6,   1,  35,   3,   5,
       53,  30, 238,  52, 238,  50,   9,  11,  24,  60,
       21,  12,  51,  63,  25,  52,   0,  63,  57,  53,
       15, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
      238, 238, 238, 238, 238, 238
    };
  register int hval = 0;

  switch (len)
    {
      default:
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

#ifdef __GNUC__
__inline
#endif
const struct id3v2_frame *
id3v2_hash (str, len)
     register const char *str;
     register unsigned int len;
{
  static const struct id3v2_frame wordlist[] =
    {
      {"MLL",	"MPEG location lookup table",				0},
      {"TDA",	"Date",							id3_text},
      {"TLE",	"Length",						id3_text},
      {"TAL",	"Album/Movie/Show title",				id3_text},
      {"TLA",	"Language(s)",						id3_text},
      {"TALB",	"Album/Movie/Show title",				id3_text},
      {"TMED",	"Media type",						id3_text},
      {"TP4",	"Interpreted, remixed, or otherwise modified by",	id3_text},
      {"TBP",	"BPM (Beats Per Minute)",				id3_text},
      {"TPB",	"Publisher",						id3_text},
      {"TPE4",	"Interpreted, remixed, or otherwise modified by",	id3_text},
      {"TPA",	"Part of a set",					id3_text},
      {"MLLT",	"MPEG location lookup table",				0},
      {"TEN",	"Encoded by",						id3_text},
      {"REV",	"Reverb",						0},
      {"RVA",	"Relative volume adjustment",				0},
      {"TBPM",	"BPM (Beats Per Minute)",				id3_text},
      {"TDAT",	"Date",							id3_text},
      {"RVAD",	"Relative volume adjustment",				0},
      {"TMT",	"Media type",						id3_text},
      {"TP3",	"Conductor/Performer refinement",			id3_text},
      {"TLEN",	"Length",						id3_text},
      {"TLAN",	"Language(s)",						id3_text},
      {"ETC",	"Event timing codes",					0},
      {"TT3",	"Subtitle/Description refinement",			id3_text},
      {"TPE3",	"Conductor/Performer refinement",			id3_text},
      {"EQU",	"Equalization",						0},
      {"AENC",	"Audio encryption",					0},
      {"TCM",	"Composer",						id3_text},
      {"TP1",	"Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group",	id3_text},
      {"TP2",	"Band/Orchestra/Accompaniment",				id3_text},
      {"EQUA",	"Equalization",						0},
      {"TT1",	"Content group description",				id3_text},
      {"TPE1",	"Lead performer(s)/Soloist(s)",				id3_text},
      {"TT2",	"Title/Songname/Content description",			id3_text},
      {"TPE2",	"Band/Orchestra/Accompaniment",				id3_text},
      {"TRD",	"Recording dates",					id3_text},
      {"TKE",	"Initial key",						id3_text},
      {"TDY",	"Playlist delay",					id3_text},
      {"IPL",	"Involved people list",					0},
      {"TYE",	"Year",							id3_text},
      {"LNK",	"Linked information",					0},
      {"CNT",	"Play counter",						0},
      {"WPB",	"Publishers official webpage",				0},
      {"TRDA",	"Recording dates",					id3_text},
      {"ULT",	"Unsychronized lyric/text transcription",		0},
      {"TIM",	"Time",							id3_text},
      {"TENC",	"Encoded by",						id3_text},
      {"TDLY",	"Playlist delay",					id3_text},
      {"TOA",	"Original artist(s)/performer(s)",			id3_text},
      {"CRA",	"Audio encryption",					0},
      {"TIME",	"Time",							id3_text},
      {"TOL",	"Original lyricist(s)/text writer(s)",			id3_text},
      {"GEO",	"General encapsulated object",				0},
      {"GEOB",	"General encapsulated object",				0},
      {"CRM",	"Encrypted meta frame",					0},
      {"SLT",	"Synchronized lyric/text",				0},
      {"MCI",	"Music CD Identifier",					0},
      {"TPUB",	"Publisher",						id3_text},
      {"TOAL",	"Original album/Movie/Show title",			id3_text},
      {"MCDI",	"Music CD Identifier",					0},
      {"POP",	"Popularimeter",					0},
      {"TFT",	"File type",						id3_text},
      {"RVRB",	"Reverb",						0},
      {"PCNT",	"Play counter",						0},
      {"BUF",	"Recommended buffer size",				0},
      {"COM",	"Comments",						0},
      {"TXT",	"Lyricist/Text writer",					id3_text},
      {"PIC",	"Attached picture",					0},
      {"WCM",	"Commercial information",				0},
      {"TOT",	"Original album/Movie/Show title",			id3_text},
      {"TOPE",	"Original artist(s)/performer(s)",			id3_text},
      {"TCR",	"Copyright message",					id3_text},
      {"TRC",	"ISRC (International Standard Recording Code)",		id3_text},
      {"TEXT",	"Lyricist/Text writer",					id3_text},
      {"TFLT",	"File type",						id3_text},
      {"POPM",	"Popularimeter",					0},
      {"APIC",	"Attached picture",					0},
      {"ENCR",	"Encryption method registration",			0},
      {"COMM",	"Comments",						id3_comment},
      {"TIT3",	"Subtitle/Description refinement",			id3_text},
      {"WCP",	"Copyright/Legal information",				0},
      {"WAR",	"Official artist/performer webpage",			0},
      {"TCO",	"Content type",						id3_text},
      {"WAF",	"Official audio file webpage",				0},
      {"STC",	"Synced tempo codes",					0},
      {"PRIV",	"Private frame",					0},
      {"ETCO",	"Event timing codes",					0},
      {"TRK",	"Track number/Position in set",				id3_text},
      {"TIT1",	"Content group description",				id3_text},
      {"TIT2",	"Title/Songname/Content description",			id3_text},
      {"TCOM",	"Composer",						id3_text},
      {"WAS",	"Official audio source webpage",			0},
      {"TKEY",	"Initial key",						id3_text},
      {"TYER",	"Year",							id3_text},
      {"LINK",	"Linked information",					0},
      {"GRID",	"Group identification registration",			0},
      {"TOR",	"Original release year",				id3_text},
      {"WPUB",	"Publishers official webpage",				0},
      {"TOF",	"Original filename",					id3_text},
      {"TXX",	"User defined text information frame",			0},
      {"TSI",	"Size",							id3_text},
      {"TCOP",	"Copyright message",					id3_text},
      {"WPAY",	"Payment",						0},
      {"TCON",	"Content type",						id3_text},
      {"IPLS",	"Involved people list",					0},
      {"TOLY",	"Original lyricist(s)/text writer(s)",			id3_text},
      {"USLT",	"Unsychronized lyric/text transcription",		0},
      {"SYLT",	"Synchronized lyric/text",				0},
      {"TSS",	"Software/Hardware and settings used for encoding",	id3_text},
      {"OWNE",	"Ownership frame",					0},
      {"TSIZ",	"Size",							id3_text},
      {"TSSE",	"Software/Hardware and settings used for encoding",	id3_text},
      {"COMR",	"Commercial frame",					0},
      {"RBUF",	"Recommended buffer size",				0},
      {"UFI",	"Unique file identifier",				0},
      {"UFID",	"Unique file identifier",				0},
      {"TRCK",	"Track number/Position in set",				id3_text},
      {"TOFN",	"Original filename",					id3_text},
      {"TRSN",	"Internet radio station name",				id3_text},
      {"WCOM",	"Commercial information",				0},
      {"TPOS",	"Part of a set",					id3_text},
      {"USER",	"Terms of use",						0},
      {"TOWN",	"File owner/licensee",					id3_text},
      {"TSRC",	"ISRC (International Standard Recording Code)",		id3_text},
      {"SYTC",	"Synchronized tempo codes",				0},
      {"WXX",	"User defined URL link frame",				0},
      {"WCOP",	"Copyright/Legal information",				0},
      {"WOAR",	"Official artist/performer webpage",			0},
      {"WOAF",	"Official audio file webpage",				0},
      {"TORY",	"Original release year",				id3_text},
      {"WOAS",	"Official audio source webpage",			0},
      {"TXXX",	"User defined text information frame",			0},
      {"TRSO",	"Internet radio station owner",				id3_text},
      {"POSS",	"Position synchronisation frame",			0},
      {"WXXX",	"User defined URL link frame",				0},
      {"WORS",	"Official Internet radio station homepage",		0}
    };

  static const short lookup[] =
    {
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,    0,   -1,   -1,
        -1,   -1,    1,   -1,   -1,   -1,   -1,    2,
      -180,    5, -134,   -2,    6,   -1,    7, -186,
      -129,   -2,   -1,   10,   11,   -1, -342,   -1,
        14,   15,   16,   17,   18,   19,   20,   21,
        22,   23,   24,   25,   -1,   26,   27,   28,
        29,   -1,   30,   31,   32,   33,   34, -340,
        37,   38,   39, -338,   42, -335,   45,   -1,
        46,   47,   48,   49,   50,   51,   52,   53,
        54, -331,   57,   58,   59,   60,   61, -328,
        -1,   64, -325,   67,   68,   69,   70, -323,
      -304,   76,   77,   78,   -1,   79,   80,   81,
      -302,   -1,   84,   85,   86,   87,   88,   -1,
        89,   -1,   90,   91,   92,   93,   94,   95,
      -291,   98,   99,  100,  101,  102,   -1,  103,
       104,  105,   -1,  106,   -1,  107,  108,  109,
       110,  -41,   -2,  111,  112, -296,  -24,   -3,
       116,  117,  118,  119,  -55,   -2,  -63,   -2,
        -1, -321,   -1,  122,  123,   -1,  124,   -1,
       125,  126,   -1,  127,  128,   -1,  129,  -17,
        -2,  -66,   -3,  -72,   -2,  130,  -75,   -2,
       131,  -82,   -2,   -1,  132,  -94,   -2,  133,
       -97,   -2, -102,   -2, -125,   -2,   -1,  134,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  135,   -1,   -1,  136
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int index = lookup[key];

          if (index >= 0)
            {
              register const char *s = wordlist[index].id;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &wordlist[index];
            }
          else if (index < -TOTAL_KEYWORDS)
            {
              register int offset = - 1 - TOTAL_KEYWORDS - index;
              register const struct id3v2_frame *wordptr = &wordlist[TOTAL_KEYWORDS + lookup[offset]];
              register const struct id3v2_frame *wordendptr = wordptr + -lookup[offset + 1];

              while (wordptr < wordendptr)
                {
                  register const char *s = wordptr->id;

                  if (*str == *s && !strcmp (str + 1, s + 1))
                    return wordptr;
                  wordptr++;
                }
            }
        }
    }
  return 0;
}
