/* Automatically generated by po2tbl.sed from mad.pot.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "libgettext.h"

const struct _msg_ent _msg_tbl[] = {
  {"", 1},
  {"Copyright (C) %s %s", 2},
  {"MPEG Audio Decoder %s", 3},
  {"\
This program is free software; you can redistribute it and/or modify it\n\
under the terms of the GNU General Public License as published by the\n\
Free Software Foundation; either version 2 of the License, or (at your\n\
option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful, but\n\
WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n\
General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License along\n\
with this program; if not, write to the Free Software Foundation, Inc.,\n\
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA\n\
\n\
If you would like to negotiate alternate licensing terms, you may do so\n\
by contacting the author: %s <%s>\n", 4},
  {"%s: option `%s' is ambiguous\n", 5},
  {"%s: option `--%s' doesn't allow an argument\n", 6},
  {"%s: option `%c%s' doesn't allow an argument\n", 7},
  {"%s: option `%s' requires an argument\n", 8},
  {"%s: unrecognized option `--%s'\n", 9},
  {"%s: unrecognized option `%c%s'\n", 10},
  {"%s: illegal option -- %c\n", 11},
  {"%s: invalid option -- %c\n", 12},
  {"%s: option requires an argument -- %c\n", 13},
  {"%s: option `-W %s' is ambiguous\n", 14},
  {"%s: option `-W %s' doesn't allow an argument\n", 15},
  {"Usage: %s [OPTIONS] FILE [...]\n", 16},
  {"Try `%s --help' for more information.\n", 17},
  {"Decode and play MPEG audio FILE(s).\n", 18},
  {"\
\n\
Verbosity:\n", 19},
  {"  -v, --verbose              show status while decoding\n", 20},
  {"  -q, --quiet                be quiet but show warnings\n", 21},
  {"  -Q, --very-quiet           be quiet and do not show warnings\n", 22},
  {"\
\n\
Audio output:\n", 23},
  {"\
  -o, --output=[TYPE:]PATH   send output to PATH with format TYPE (see \
below)\n", 24},
  {"  -d, --no-dither            do not dither output PCM samples\n", 25},
  {"      --fade-in[=DURATION]   fade-in songs over DURATION (default %s)\n", 26},
  {"\
      --fade-out[=DURATION]  fade-out songs over DURATION (default %s)\n", 27},
  {"  -g, --gap=DURATION         set inter-song gap to DURATION\n", 28},
  {"  -x, --cross-fade           cross-fade songs (use with negative gap)\n", 29},
  {"  -a, --attenuate=DECIBELS   attenuate signal by DECIBELS (-)\n", 30},
  {"  -a, --amplify=DECIBELS     amplify signal by DECIBELS (+)\n", 31},
  {"\
\n\
Channel selection:\n", 32},
  {"  -1, --left                 output first (left) channel only\n", 33},
  {"  -2, --right                output second (right) channel only\n", 34},
  {"\
  -m, --mono                 mix left and right channels for monaural \
output\n", 35},
  {"  -S, --stereo               force stereo output\n", 36},
  {"\
\n\
Experimental:\n", 37},
  {"\
      --external-mix         output pre-synthesis samples for external \
mixer\n", 38},
  {"      --experimental         enable experimental filter\n", 39},
  {"\
\n\
Playback:\n", 40},
  {"  -s, --start=TIME           skip to begin at TIME (HH:MM:SS.DDD)\n", 41},
  {"  -t, --time=DURATION        play only for DURATION (HH:MM:SS.DDD)\n", 42},
  {"  -z, --shuffle              randomize file list\n", 43},
  {"  -r, --repeat[=MAX]         play files MAX times, or indefinitely\n", 44},
  {"\
\n\
Miscellaneous:\n", 45},
  {"  -V, --version              display version number and exit\n", 46},
  {"      --license              show copyright/license message and exit\n", 47},
  {"  -h, --help                 display this help and exit\n", 48},
  {"\
\n\
Supported output format types:\n", 49},
  {"  raw     binary signed 16-bit host-endian linear PCM\n", 50},
  {"  wave    Microsoft RIFF/WAVE, 16-bit PCM format (*.wav)\n", 51},
  {"  snd     Sun/NeXT audio, 8-bit ISDN mu-law (*.au, *.snd)\n", 52},
  {"  hex     hexadecimal signed 24-bit linear PCM\n", 53},
  {"  null    no output (decode only)\n", 54},
  {"invalid %s specification \"%s\"", 55},
  {"%s must be positive", 56},
  {"invalid decibel specification \"%s\"", 57},
  {"decibel value must be in the range %+d to %+d", 58},
  {"gap time", 59},
  {"fade-in time", 60},
  {"fade-out time", 61},
  {"multiple output destinations not supported", 62},
  {"unknown output format type for \"%s\"", 63},
  {"invalid repeat count \"%s\"", 64},
  {"start time", 65},
  {"playing time", 66},
  {"`%s --license' for licensing information.\n", 67},
  {"Build options: %s\n", 68},
  {"cross-fade ignored without gap", 69},
  {"cross-fade ignored without negative gap", 70},
  {"%s: Not a regular file\n", 71},
  {"%8.1f MB  %c%3u kbps  %s  %s\n", 72},
  {"Usage: %s [-s] FILE [...]\n", 73},
  {"TOTAL", 74},
  {"Usage: %s input1 [input2 ...]", 75},
  {"%s: unknown output format type", 76},
  {"not enough memory to allocate mixing buffers", 77},
  {"mixing %d streams\n", 78},
  {"I", 79},
  {"II", 80},
  {"III", 81},
  {"single channel", 82},
  {"dual channel", 83},
  {"joint stereo", 84},
  {"stereo", 85},
  {" (LR)", 86},
  {" (MS)", 87},
  {" (I)", 88},
  {" (MS+I)", 89},
  {"%s Layer %s, %s%u kbps%s, %u Hz, %s%s, %s", 90},
  {"VBR (avg ", 91},
  {")", 92},
  {"CRC", 93},
  {"no CRC", 94},
  {"no channel selected for dual channel; using first", 95},
  {"mono output not available; forcing stereo", 96},
  {"stereo output not available; using first channel (use -m to mix)", 97},
  {"sample frequency %u Hz not available; closest %u Hz", 98},
  {"not enough memory to allocate resampling buffer", 99},
  {"cannot resample %u Hz to %u Hz", 100},
  {"resampling %u Hz to %u Hz", 101},
  {"not enough memory", 102},
  {"lost synchronization", 103},
  {"reserved header layer value", 104},
  {"forbidden bitrate value", 105},
  {"reserved sample frequency value", 106},
  {"reserved emphasis value", 107},
  {"CRC check failed", 108},
  {"forbidden bit allocation value", 109},
  {"bad scalefactor index", 110},
  {"bad frame length", 111},
  {"bad big_values count", 112},
  {"reserved block_type", 113},
  {"bad main_data_begin pointer", 114},
  {"bad main data length", 115},
  {"bad audio data length", 116},
  {"bad Huffman table select", 117},
  {"Huffman data overrun", 118},
  {"incompatible block_type for MS", 119},
  {"frame %lu: %s", 120},
  {"not enough memory to allocate input buffer", 121},
  {"stdin", 122},
  {"%lu frames decoded (%s), %+.1f dB peak amplitude, %lu clipped samples\n", 123},
  {"not enough memory to allocate sample buffer", 124},
  {"not enough memory to allocate filters", 125},
  {"lead-in", 126},
  {"no supported audio format available", 127},
  {"error getting error text", 128},
  {"failed to create synchronization object", 129},
  {"wait abandoned", 130},
  {"wait timeout", 131},
  {"wait failed", 132},
  {"failed to close synchronization object", 133},
  {"\
 Title: %-30s  Artist: %s\n\
 Album: %-30s   Genre: %s\n", 134},
  {"  Year: %-4s  Track: %-3u               Comment: %s\n", 135},
  {"  Year: %-4s                           Comment: %s\n", 136},
  {"EOF while reading tag", 137},
  {"invalid header", 138},
  {"ID3: version 2.%u.%u not supported\n", 139},
  {"ID3: version 2.%u.%u, flags 0x%02x, size %lu bytes\n", 140},
  {"ID3: not enough memory to allocate tag buffer\n", 141},
  {"ID3: unknown flags 0x%02x\n", 142},
  {"ID3: extended header flags 0x%04x, %lu bytes padding\n", 143},
  {"ID3: total frame CRC 0x%04lx\n", 144},
  {"ID3: experimental tag\n", 145},
  {"ID3: unhandled %s (%s): flags 0x%04x, %lu bytes\n", 146},
  {"ID3: unknown frame \"%s\" (flags 0x%04x; %lu bytes)\n", 147},
  {"Remix", 148},
  {"Cover", 149},
  {"ID3: %s: no data\n", 150},
  {"ID3: %s: Unicode\n", 151},
  {"ID3: %s: not enough memory\n", 152},
  {"ID3: %s: bad format\n", 153},
  {"Recommended buffer size", 154},
  {"Play counter", 155},
  {"Comments", 156},
  {"Audio encryption", 157},
  {"Encrypted meta frame", 158},
  {"Equalization", 159},
  {"Event timing codes", 160},
  {"General encapsulated object", 161},
  {"Involved people list", 162},
  {"Linked information", 163},
  {"Music CD Identifier", 164},
  {"MPEG location lookup table", 165},
  {"Attached picture", 166},
  {"Popularimeter", 167},
  {"Reverb", 168},
  {"Relative volume adjustment", 169},
  {"Synchronized lyric/text", 170},
  {"Synced tempo codes", 171},
  {"Album/Movie/Show title", 172},
  {"BPM (Beats Per Minute)", 173},
  {"Composer", 174},
  {"Content type", 175},
  {"Copyright message", 176},
  {"Date", 177},
  {"Playlist delay", 178},
  {"Encoded by", 179},
  {"File type", 180},
  {"Time", 181},
  {"Initial key", 182},
  {"Language(s)", 183},
  {"Length", 184},
  {"Media type", 185},
  {"Original artist(s)/performer(s)", 186},
  {"Original filename", 187},
  {"Original lyricist(s)/text writer(s)", 188},
  {"Original release year", 189},
  {"Original album/Movie/Show title", 190},
  {"Lead artist(s)/Lead performer(s)/Soloist(s)/Performing group", 191},
  {"Band/Orchestra/Accompaniment", 192},
  {"Conductor/Performer refinement", 193},
  {"Interpreted, remixed, or otherwise modified by", 194},
  {"Part of a set", 195},
  {"Publisher", 196},
  {"ISRC (International Standard Recording Code)", 197},
  {"Recording dates", 198},
  {"Track number/Position in set", 199},
  {"Size", 200},
  {"Software/Hardware and settings used for encoding", 201},
  {"Content group description", 202},
  {"Title/Songname/Content description", 203},
  {"Subtitle/Description refinement", 204},
  {"Lyricist/Text writer", 205},
  {"User defined text information frame", 206},
  {"Year", 207},
  {"Unique file identifier", 208},
  {"Unsychronized lyric/text transcription", 209},
  {"Official audio file webpage", 210},
  {"Official artist/performer webpage", 211},
  {"Official audio source webpage", 212},
  {"Commercial information", 213},
  {"Copyright/Legal information", 214},
  {"Publishers official webpage", 215},
  {"User defined URL link frame", 216},
  {"Commercial frame", 217},
  {"Encryption method registration", 218},
  {"Group identification registration", 219},
  {"Ownership frame", 220},
  {"Position synchronisation frame", 221},
  {"Private frame", 222},
  {"Synchronized tempo codes", 223},
  {"File owner/licensee", 224},
  {"Lead performer(s)/Soloist(s)", 225},
  {"Internet radio station name", 226},
  {"Internet radio station owner", 227},
  {"Terms of use", 228},
  {"Official Internet radio station homepage", 229},
  {"Payment", 230},
  {"Blues", 231},
  {"Classic Rock", 232},
  {"Country", 233},
  {"Dance", 234},
  {"Disco", 235},
  {"Funk", 236},
  {"Grunge", 237},
  {"Hip-Hop", 238},
  {"Jazz", 239},
  {"Metal", 240},
  {"New Age", 241},
  {"Oldies", 242},
  {"Other", 243},
  {"Pop", 244},
  {"R&B", 245},
  {"Rap", 246},
  {"Reggae", 247},
  {"Rock", 248},
  {"Techno", 249},
  {"Industrial", 250},
  {"Alternative", 251},
  {"Ska", 252},
  {"Death Metal", 253},
  {"Pranks", 254},
  {"Soundtrack", 255},
  {"Euro-Techno", 256},
  {"Ambient", 257},
  {"Trip-Hop", 258},
  {"Vocal", 259},
  {"Jazz+Funk", 260},
  {"Fusion", 261},
  {"Trance", 262},
  {"Classical", 263},
  {"Instrumental", 264},
  {"Acid", 265},
  {"House", 266},
  {"Game", 267},
  {"Sound Clip", 268},
  {"Gospel", 269},
  {"Noise", 270},
  {"AlternRock", 271},
  {"Bass", 272},
  {"Soul", 273},
  {"Punk", 274},
  {"Space", 275},
  {"Meditative", 276},
  {"Instrumental Pop", 277},
  {"Instrumental Rock", 278},
  {"Ethnic", 279},
  {"Gothic", 280},
  {"Darkwave", 281},
  {"Techno-Industrial", 282},
  {"Electronic", 283},
  {"Pop-Folk", 284},
  {"Eurodance", 285},
  {"Dream", 286},
  {"Southern Rock", 287},
  {"Comedy", 288},
  {"Cult", 289},
  {"Gangsta", 290},
  {"Top 40", 291},
  {"Christian Rap", 292},
  {"Pop/Funk", 293},
  {"Jungle", 294},
  {"Native American", 295},
  {"Cabaret", 296},
  {"New Wave", 297},
  {"Psychedelic", 298},
  {"Rave", 299},
  {"Showtunes", 300},
  {"Trailer", 301},
  {"Lo-Fi", 302},
  {"Tribal", 303},
  {"Acid Punk", 304},
  {"Acid Jazz", 305},
  {"Polka", 306},
  {"Retro", 307},
  {"Musical", 308},
  {"Rock & Roll", 309},
  {"Hard Rock", 310},
  {"Folk", 311},
  {"Folk/Rock", 312},
  {"National Folk", 313},
  {"Swing", 314},
  {"Fast-Fusion", 315},
  {"Bebob", 316},
  {"Latin", 317},
  {"Revival", 318},
  {"Celtic", 319},
  {"Bluegrass", 320},
  {"Avantgarde", 321},
  {"Gothic Rock", 322},
  {"Progressive Rock", 323},
  {"Psychedelic Rock", 324},
  {"Symphonic Rock", 325},
  {"Slow Rock", 326},
  {"Big Band", 327},
  {"Chorus", 328},
  {"Easy Listening", 329},
  {"Acoustic", 330},
  {"Humour", 331},
  {"Speech", 332},
  {"Chanson", 333},
  {"Opera", 334},
  {"Chamber Music", 335},
  {"Sonata", 336},
  {"Symphony", 337},
  {"Booty Bass", 338},
  {"Primus", 339},
  {"Porn Groove", 340},
  {"Satire", 341},
  {"Slow Jam", 342},
  {"Club", 343},
  {"Tango", 344},
  {"Samba", 345},
  {"Folklore", 346},
  {"Ballad", 347},
  {"Power Ballad", 348},
  {"Rhythmic Soul", 349},
  {"Freestyle", 350},
  {"Duet", 351},
  {"Punk Rock", 352},
  {"Drum Solo", 353},
  {"A Cappella", 354},
  {"Euro-House", 355},
  {"Dance Hall", 356},
  {"Goa", 357},
  {"Drum & Bass", 358},
  {"Club-House", 359},
  {"Hardcore", 360},
  {"Terror", 361},
  {"Indie", 362},
  {"BritPop", 363},
  {"Negerpunk", 364},
  {"Polsk Punk", 365},
  {"Beat", 366},
  {"Christian Gangsta Rap", 367},
  {"Heavy Metal", 368},
  {"Black Metal", 369},
  {"Crossover", 370},
  {"Contemporary Christian", 371},
  {"Christian Rock", 372},
  {"Merengue", 373},
  {"Salsa", 374},
  {"Thrash Metal", 375},
  {"Anime", 376},
  {"JPop", 377},
  {"Synthpop", 378},
};

int _msg_tbl_length = 378;
