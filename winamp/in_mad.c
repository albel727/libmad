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
 * $Id: in_mad.c,v 1.5 2000/11/20 04:58:13 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "global.h"

# include <windows.h>
# include <commctrl.h>
# include <prsht.h>
# include <wininet.h>
# include <stdio.h>
# include <stdarg.h>
# include <string.h>
# include <locale.h>

# include "resource.h"
# include "messages.h"

# include "in2.h"
# include "mad.h"

# define PLUGIN_VERSION		VERSION ""

# define WM_WA_MPEG_EOF		(WM_USER + 2)
# define WM_MAD_SCAN_FINISHED	(WM_USER + 3)

# define PCM_CHUNK		576

static In_Module module;

struct input {
  enum input_type { INPUT_FILE, INPUT_STREAM } type;
  union {
    HANDLE file;
    HINTERNET stream;
  } handle;
};

static struct state {
  char path[MAX_PATH];	/* currently playing path/URL */
  struct input input;	/* input source */
  DWORD size;		/* file size in bytes, or bytes until next metadata */
  int length;		/* total playing time in ms */
  int bitrate;		/* average bitrate in kbps */
  int position;		/* current playing position in ms */
  int paused;		/* are we paused? */
  int seek;		/* seek target in ms, or -1 */
  int stop;		/* stop flag */
} state = { "", { INPUT_FILE, { INVALID_HANDLE_VALUE } },
	    0, 0, 0, 0, 0, -1, 0 };

# define REGISTRY_KEY		"Software\\Winamp\\MAD Plug-in"

static DWORD conf_enabled;	/* plug-in enabled? */
static DWORD conf_priority;	/* decoder thread priority -2..+2 */
static DWORD conf_resolution;	/* bits per output sample */
static char  conf_titlefmt[64];	/* title format */
static DWORD conf_lengthcalc;	/* full (slow) length calculation? */
static DWORD conf_avgbitrate;	/* display average bitrate? */

# define DEFAULT_TITLEFMT	"%1 - %2"
# define ALTERNATE_TITLEFMT	"%7"

static HKEY registry = INVALID_HANDLE_VALUE;

static HANDLE decode_thread = INVALID_HANDLE_VALUE;
static HANDLE length_thread = INVALID_HANDLE_VALUE;

static HINTERNET internet = INVALID_HANDLE_VALUE;

# define DEBUG(str)	MessageBox(module.hMainWindow, (str), "Debug",  \
				   MB_ICONEXCLAMATION | MB_OK);

static
void show_error(HWND owner, char *title, DWORD error, ...)
{
  UINT style;
  char str[256], *message;

  if (owner == 0)
    owner = module.hMainWindow;

  switch ((error >> 30) & 0x3) {
  case STATUS_SEVERITY_INFORMATIONAL:
    style = MB_ICONINFORMATION;
    if (title == 0)
      title = "Information";
    break;

  case STATUS_SEVERITY_WARNING:
    style = MB_ICONWARNING;
    if (title == 0)
      title = "Warning";
    break;

  case STATUS_SEVERITY_SUCCESS:
  default:
    style = 0;
    if (error & 0x40000000L)
      break;
    /* else fall through */

  case STATUS_SEVERITY_ERROR:
    style = MB_ICONERROR;
    if (title == 0)
      title = "Error";
    break;
  }

  if (error == ERROR_INTERNET_EXTENDED_ERROR) {
    DWORD size;

    size = sizeof(str);
    if (InternetGetLastResponseInfo(&error, str, &size) == TRUE)
      message = str;
    else {
      if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
	message = LocalAlloc(0, size);
	if (message &&
	    InternetGetLastResponseInfo(&error, message, &size) == FALSE)
	  message = LocalFree(message);
      }
      else
	message = 0;
    }
  }
  else {
    va_list args;

    va_start(args, error);

    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
		      FORMAT_MESSAGE_FROM_HMODULE |
		      FORMAT_MESSAGE_ALLOCATE_BUFFER,
		      module.hDllInstance, error, 0,
		      (LPTSTR) &message, 0, &args) == 0)
      message = 0;

    va_end(args);
  }

  MessageBox(owner, message ? message : "An unknown error occurred.",
	     title, style | MB_OK);

  if (message != str)
    LocalFree(message);
}

static
void apply_config(void)
{
  module.FileExtensions = conf_enabled ?
    "MP3\0" "MPEG Audio Layer III files (*.MP3)\0"
    "MP2\0" "MPEG Audio Layer II files (*.MP2)\0"
    "MP1\0" "MPEG Audio Layer I files (*.MP1)\0" : "";
}

static
void do_init(void)
{
  InitCommonControls();

  if (RegCreateKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY,
		     0, "", REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, 0,
		     &registry, 0) != ERROR_SUCCESS) {
    registry = INVALID_HANDLE_VALUE;

    conf_enabled    = 1;
    conf_priority   = 0;
    conf_resolution = 16;
    strcpy(conf_titlefmt, DEFAULT_TITLEFMT);
    conf_lengthcalc = 0;
    conf_avgbitrate = 1;
  }
  else {
    DWORD type, size;

    size = sizeof(conf_enabled);
    if (RegQueryValueEx(registry, "enabled", 0,
			&type, (BYTE *) &conf_enabled,
			&size) != ERROR_SUCCESS || type != REG_DWORD)
      conf_enabled = 1;

    size = sizeof(conf_priority);
    if (RegQueryValueEx(registry, "priority", 0,
			&type, (BYTE *) &conf_priority,
			&size) != ERROR_SUCCESS || type != REG_DWORD)
      conf_priority = 0;

    size = sizeof(conf_resolution);
    if (RegQueryValueEx(registry, "resolution", 0,
			&type, (BYTE *) &conf_resolution,
			&size) != ERROR_SUCCESS || type != REG_DWORD ||
	(conf_resolution !=  8 && conf_resolution != 16 &&
	 conf_resolution != 24 && conf_resolution != 32))
      conf_resolution = 16;

    size = sizeof(conf_titlefmt);
    if (RegQueryValueEx(registry, "titlefmt", 0,
			&type, (BYTE *) &conf_titlefmt,
			&size) != ERROR_SUCCESS || type != REG_SZ)
      strcpy(conf_titlefmt, DEFAULT_TITLEFMT);

    size = sizeof(conf_lengthcalc);
    if (RegQueryValueEx(registry, "lengthcalc", 0,
			&type, (BYTE *) &conf_lengthcalc,
			&size) != ERROR_SUCCESS || type != REG_DWORD)
      conf_lengthcalc = 0;

    size = sizeof(conf_avgbitrate);
    if (RegQueryValueEx(registry, "avgbitrate", 0,
			&type, (BYTE *) &conf_avgbitrate,
			&size) != ERROR_SUCCESS || type != REG_DWORD)
      conf_avgbitrate = 1;
  }

  apply_config();
}

static
void do_quit(void)
{
  if (registry != INVALID_HANDLE_VALUE) {
    RegCloseKey(registry);
    registry = INVALID_HANDLE_VALUE;
  }

  if (internet != INVALID_HANDLE_VALUE) {
    InternetCloseHandle(internet);
    internet = INVALID_HANDLE_VALUE;
  }
}

static CALLBACK
BOOL config_dialog(HWND dialog, UINT message,
		   WPARAM wparam, LPARAM lparam)
{
  int which;

  switch (message) {
  case WM_INITDIALOG:
    if (conf_enabled)
      CheckDlgButton(dialog, IDC_CONF_ENABLED, BST_CHECKED);

    SendDlgItemMessage(dialog, IDC_TITLE_FORMAT, EM_SETLIMITTEXT,
		       sizeof(conf_titlefmt) - 1, 0);
    SetDlgItemText(dialog, IDC_TITLE_FORMAT, conf_titlefmt);

    SendDlgItemMessage(dialog, IDC_PRIO_SLIDER,
		       TBM_SETRANGE, FALSE, MAKELONG(2 - 2, 2 + 2));
    SendDlgItemMessage(dialog, IDC_PRIO_SLIDER,
		       TBM_SETPOS, TRUE, 2 - conf_priority);

    switch (conf_resolution) {
    case  8: which = IDC_RES_8BITS;  break;
    case 16: which = IDC_RES_16BITS; break;
    case 24: which = IDC_RES_24BITS; break;
    case 32: which = IDC_RES_32BITS; break;
    default:
      which = 0;
    }
    if (which)
      CheckRadioButton(dialog, IDC_RES_8BITS, IDC_RES_32BITS, which);

    if (!conf_lengthcalc)
      CheckDlgButton(dialog, IDC_MISC_FASTTIMECALC, BST_CHECKED);
    if (conf_avgbitrate)
      CheckDlgButton(dialog, IDC_MISC_AVGBITRATE, BST_CHECKED);

    return TRUE;

  case WM_COMMAND:
    switch (LOWORD(wparam)) {
    case IDOK:
      conf_enabled =
	(IsDlgButtonChecked(dialog, IDC_CONF_ENABLED) == BST_CHECKED);

      conf_priority = 2 -
	SendDlgItemMessage(dialog, IDC_PRIO_SLIDER, TBM_GETPOS, 0, 0);

      if (IsDlgButtonChecked(dialog, IDC_RES_8BITS) == BST_CHECKED)
	conf_resolution =  8;
      else if (IsDlgButtonChecked(dialog, IDC_RES_16BITS) == BST_CHECKED)
	conf_resolution = 16;
      else if (IsDlgButtonChecked(dialog, IDC_RES_24BITS) == BST_CHECKED)
	conf_resolution = 24;
      else if (IsDlgButtonChecked(dialog, IDC_RES_32BITS) == BST_CHECKED)
	conf_resolution = 32;

      if (SendDlgItemMessage(dialog, IDC_TITLE_FORMAT,
			     WM_GETTEXTLENGTH, 0, 0) == 0)
	strcpy(conf_titlefmt, DEFAULT_TITLEFMT);
      else {
	GetDlgItemText(dialog, IDC_TITLE_FORMAT,
		       conf_titlefmt, sizeof(conf_titlefmt));
      }

      conf_lengthcalc =
	(IsDlgButtonChecked(dialog, IDC_MISC_FASTTIMECALC) != BST_CHECKED);
      conf_avgbitrate =
	(IsDlgButtonChecked(dialog, IDC_MISC_AVGBITRATE) == BST_CHECKED);

      /* fall through */

    case IDCANCEL:
      EndDialog(dialog, wparam);
      return TRUE;
    }
    break;
  }

  return FALSE;
}

static
void show_config(HWND parent)
{
  if (DialogBox(module.hDllInstance, MAKEINTRESOURCE(IDD_CONFIG),
		parent, config_dialog) == IDOK) {
    if (registry != INVALID_HANDLE_VALUE) {
      RegSetValueEx(registry, "enabled", 0, REG_DWORD,
		    (BYTE *) &conf_enabled, sizeof(conf_enabled));
      RegSetValueEx(registry, "priority", 0, REG_DWORD,
		    (BYTE *) &conf_priority, sizeof(conf_priority));
      RegSetValueEx(registry, "resolution", 0, REG_DWORD,
		    (BYTE *) &conf_resolution, sizeof(conf_resolution));
      RegSetValueEx(registry, "titlefmt", 0, REG_SZ,
		    (BYTE *) &conf_titlefmt, sizeof(conf_titlefmt));
      RegSetValueEx(registry, "lengthcalc", 0, REG_DWORD,
		    (BYTE *) &conf_lengthcalc, sizeof(conf_lengthcalc));
      RegSetValueEx(registry, "avgbitrate", 0, REG_DWORD,
		    (BYTE *) &conf_avgbitrate, sizeof(conf_avgbitrate));
    }

    apply_config();
  }
}

static
void show_about(HWND parent)
{
  MessageBox(parent,
	     "MPEG Audio Decoder version " MAD_VERSION "\n"
	     "Copyright \xA9 " MAD_PUBLISHYEAR " " MAD_AUTHOR "\n\n"
	     "Winamp plug-in version " PLUGIN_VERSION,
	     "About MAD Plug-in", MB_ICONINFORMATION | MB_OK);
}

static
int is_stream(char *path)
{
  return
    strncmp(path, "http://", 7) == 0 ||
    strncmp(path, "ftp://",  6) == 0;
}

static
int is_ourfile(char *path)
{
  return conf_enabled && is_stream(path);
}

static
void input_init(struct input *input, enum input_type type, HANDLE handle)
{
  input->type = type;

  switch (type) {
  case INPUT_FILE:
    input->handle.file = handle;
    break;

  case INPUT_STREAM:
    input->handle.stream = handle;
    break;
  }
}

static
DWORD input_read(struct input *input, unsigned char *buffer, DWORD bytes)
{
  switch (input->type) {
  case INPUT_FILE:
    if (ReadFile(input->handle.file, buffer, bytes, &bytes, 0) == FALSE)
      bytes = -1;
    break;

  case INPUT_STREAM:
    if (InternetReadFile(input->handle.stream, buffer, bytes, &bytes) == FALSE)
      bytes = -1;
    break;

  default:
    bytes = -1;
  }

  return bytes;
}

static
DWORD input_seek(struct input *input, DWORD position, DWORD method)
{
  switch (input->type) {
  case INPUT_FILE:
    return SetFilePointer(input->handle.file, position, 0, method);

  case INPUT_STREAM:
  default:
    return -1;
  }
}

static
void input_close(struct input *input)
{
  switch (input->type) {
  case INPUT_FILE:
    if (input->handle.file != INVALID_HANDLE_VALUE) {
      CloseHandle(input->handle.file);
      input->handle.file = INVALID_HANDLE_VALUE;
    }
    break;

  case INPUT_STREAM:
    if (input->handle.stream != INVALID_HANDLE_VALUE) {
      InternetCloseHandle(input->handle.stream);
      input->handle.stream = INVALID_HANDLE_VALUE;
    }
    break;
  }
}

static
int input_skip(struct input *input, unsigned long count)
{
  if (input_seek(input, count, FILE_CURRENT) == -1) {
    unsigned char buffer[256];
    unsigned long len;

    while (count) {
      len = input_read(input, buffer,
		       count < sizeof(buffer) ? count : sizeof(buffer));

      if (len == -1)
	return -1;
      else if (len == 0)
	return 0;

      count -= len;
    }
  }

  return 0;
}

static
int id3v2_tag(unsigned char const *buffer, unsigned long buflen,
	      struct input *input, unsigned long *tagsize)
{
  unsigned char header_data[10];
  unsigned char const *header;
  struct {
    unsigned int major;
    unsigned int revision;
  } version;
  unsigned int flags;
  unsigned long size, count, len;
  unsigned char *ptr, *end, *tag = 0;
  int result = 0;

  enum {
    FLAG_UNSYNC       = 0x0080,
    FLAG_EXTENDED     = 0x0040,
    FLAG_EXPERIMENTAL = 0x0020,

    FLAG_UNKNOWN      = 0x001f
  };

  if (buflen < 10) {
    header = header_data;

    memcpy(header_data, buffer, buflen);

    for (ptr = header_data + buflen, count = 10 - buflen;
	 count; count -= len, ptr += len) {
      len = input_read(input, ptr, count);

      if (len == 0 || len == -1)
	goto fail;
    }

    buffer += buflen;
    buflen  = 0;
  }
  else {
    header = buffer;

    buffer += 10;
    buflen -= 10;
  }

  if (header[3] == 0xff || header[4] == 0xff ||
      (header[6] & 0x80) || (header[7] & 0x80) ||
      (header[8] & 0x80) || (header[9] & 0x80))
    goto fail;

  version.major    = header[3];
  version.revision = header[4];

  flags = header[5];

  /* high bit is not used */
  size = (header[6] << 21) | (header[7] << 14) |
         (header[8] <<  7) | (header[9] <<  0);

  *tagsize = 10 + size;

  if (version.major < 2 || version.major > 3)
    goto abort;

  if (size == 0)
    goto done;

  tag = LocalAlloc(0, size);
  if (tag == 0)
    goto abort;

  if (buflen < size) {
    memcpy(tag, buffer, buflen);

    for (ptr = tag + buflen, count = size - buflen;
	 count; count -= len, ptr += len) {
      len = input_read(input, ptr, count);

      if (len == 0 || len == -1)
	goto fail;
    }

    buffer += buflen;
    buflen  = 0;
  }
  else {
    memcpy(tag, buffer, size);

    buffer += size;
    buflen -= size;
  }

  /* undo unsynchronisation */

  if (flags & FLAG_UNSYNC) {
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

  goto done;

 fail:
  result = -1;

 done:
  if (tag)
    LocalFree(tag);

  return result;

 abort:
  /* skip the unread part of the tag */
  return (size > buflen) ? input_skip(input, size - buflen) : 0;
}

static inline
signed long linear_dither(unsigned int bits, mad_fixed_t sample,
			  mad_fixed_t *error)
{
  mad_fixed_t quantized;

  /* dither */
  sample += *error;

  /* clip */
  quantized = sample;
  if (sample >= MAD_F_ONE)
    quantized = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    quantized = -MAD_F_ONE;

  /* quantize */
  quantized &= ~((1L << (MAD_F_FRACBITS + 1 - bits)) - 1);

  /* error */
  *error = sample - quantized;

  return quantized >> (MAD_F_FRACBITS + 1 - bits);
}

# if 1
static
unsigned int pack_pcm(unsigned char *data, unsigned int nsamples,
		      mad_fixed_t const *left, mad_fixed_t const *right,
		      int resolution)
{
  static mad_fixed_t left_err, right_err;
  unsigned char const *start;
  register signed long sample0, sample1;
  int effective, bytes;

  start     = data;
  effective = (resolution > 24) ? 24 : resolution;
  bytes     = resolution / 8;

  if (right) {  /* stereo */
    while (nsamples--) {
      sample0 = linear_dither(effective, *left++,  &left_err);
      sample1 = linear_dither(effective, *right++, &right_err);

      switch (resolution) {
      case 8:
	data[0] = sample0 + 0x80;
	data[1] = sample1 + 0x80;
	break;

      case 32:
	sample0 <<= 8;
	sample1 <<= 8;
	data[        3] = sample0 >> 24;
	data[bytes + 3] = sample1 >> 24;
      case 24:
	data[        2] = sample0 >> 16;
	data[bytes + 2] = sample1 >> 16;
      case 16:
	data[        1] = sample0 >>  8;
	data[bytes + 1] = sample1 >>  8;
	data[        0] = sample0 >>  0;
	data[bytes + 0] = sample1 >>  0;
      }

      data += bytes * 2;
    }
  }
  else {  /* mono */
    while (nsamples--) {
      sample0 = linear_dither(effective, *left++, &left_err);

      switch (resolution) {
      case 8:
	data[0] = sample0 + 0x80;
	break;

      case 32:
	sample0 <<= 8;
	data[3] = sample0 >> 24;
      case 24:
	data[2] = sample0 >> 16;
      case 16:
	data[1] = sample0 >>  8;
	data[0] = sample0 >>  0;
      }
      data += bytes;
    }
  }

  return data - start;
}
# else
static
unsigned int pack_pcm(void *data, unsigned int nsamples,
		      mad_fixed_t const *left, mad_fixed_t const *right,
		      int resolution)
{
  static mad_fixed_t left_err, right_err;
  unsigned char const *start;
  register signed long sample0, sample1;

  start = data;

  if (right) {  /* stereo */
    switch (resolution) {
    case 8:
      {
	unsigned short *ptr = data;

	while (nsamples--) {
	  sample0 = linear_dither(8, *left++,  &left_err);
	  sample1 = linear_dither(8, *right++, &right_err);

	  *ptr++ = ((0x80 + sample0) & 0xff) | ((0x80 + sample1) << 8);
	}

	data = ptr;
      }
      break;

    case 16:
      {
	unsigned long *ptr = data;

	while (nsamples--) {
	  sample0 = linear_dither(16, *left++,  &left_err);
	  sample1 = linear_dither(16, *right++, &right_err);

	  *ptr++ = (sample0 & 0xffff) | (sample1 << 16);
	}

	data = ptr;
      }
      break;

    case 24:
      {
	unsigned short *ptr = data;

	while (nsamples--) {
	  sample0 = linear_dither(24, *left++,  &left_err);
	  sample1 = linear_dither(24, *right++, &right_err);

	  ptr[0] = sample0;
	  ptr[1] = ((sample0 >> 16) & 0xff) | ((sample1 << 8) & 0xff00);
	  ptr[2] = sample1 >> 8;

	  ptr += 3;
	}

	data = ptr;
      }
      break;

    case 32:
      {
	unsigned long *ptr = data;

	while (nsamples--) {
	  sample0 = linear_dither(24, *left++,  &left_err);
	  sample1 = linear_dither(24, *right++, &right_err);

	  ptr[0] = sample0 << 8;
	  ptr[1] = sample1 << 8;

	  ptr += 2;
	}

	data = ptr;
      }
      break;
    }
  }
  else {  /* mono */
    unsigned int effective, bytes;

    effective = resolution;
    if (resolution > 24)
      effective = 24;

    bytes = resolution / 8;

    while (nsamples--) {
      sample0 = linear_dither(effective, *left++, &left_err);

      switch (resolution) {
      case 8:
	data[0] = sample0 + 0x80;
	break;

      case 32:
	sample0 <<= 8;
	data[3] = sample0 >> 24;
      case 24:
	data[2] = sample0 >> 16;
      case 16:
	data[1] = sample0 >>  8;
	data[0] = sample0 >>  0;
      }

      data += bytes;
    }
  }

  return (unsigned char const *) data - start;
}
# endif

struct stats {
  int vbr;
  unsigned int bitrate;
  unsigned long vbr_frames;
  unsigned long vbr_rate;
};

static
void stats_init(struct stats *stats)
{
  stats->vbr        = 0;
  stats->bitrate    = 0;
  stats->vbr_frames = 0;
  stats->vbr_rate   = 0;
}

static
int stats_update(struct stats *stats, unsigned long bitrate)
{
  bitrate /= 1000;

  stats->vbr_rate += bitrate;
  stats->vbr_frames++;

  stats->vbr += (stats->bitrate && stats->bitrate != bitrate) ? 128 : -1;
  if (stats->vbr < 0)
    stats->vbr = 0;
  else if (stats->vbr > 512)
    stats->vbr = 512;

  stats->bitrate = bitrate;

  return stats->vbr ?
    ((stats->vbr_rate * 2) / stats->vbr_frames + 1) / 2 : stats->bitrate;
}

static
int do_error(struct mad_stream *stream, struct mad_frame *frame,
	     struct input *input, int *last_error)
{
  switch (stream->error) {
  case MAD_ERROR_BADCRC:
    if (last_error) {
      if (*last_error) {
	if (frame)
	  mad_frame_mute(frame);
      }
      else
	*last_error = 1;
    }
    break;

  case MAD_ERROR_LOSTSYNC:
    if (strncmp(stream->this_frame, "ID3", 3) == 0) {
      unsigned long tagsize;

      /* ID3v2 tag */
      if (id3v2_tag(stream->this_frame, stream->bufend - stream->this_frame,
		    input, &tagsize) == -1)
	return 1;

      if (tagsize > stream->bufend - stream->this_frame)
	mad_stream_skip(stream, stream->bufend - stream->this_frame);
      else
	mad_stream_skip(stream, tagsize);
    }
    else if (strncmp(stream->this_frame, "TAG", 3) == 0) {
      /* ID3v1 tag */
      mad_stream_skip(stream, 128);
    }

    /* fall through */

  default:
    return 1;
  }

  return 0;
}

static
DWORD WINAPI run_decode_thread(void *param)
{
  struct state *state = param;
  struct mad_stream stream;
  struct mad_frame frame;
  struct mad_synth synth;

  int resolution = conf_resolution;

  unsigned char *input_buffer;
  static unsigned char output_buffer[(575 + 1152) * 4 * 2];
  unsigned int input_size, input_length = 0, output_length = 0;

  mad_timer_t timer, duration;
  struct stats stats;
  int avgbitrate, bitrate, last_bitrate = 0, seek_skip = 0, last_error = 0;

  input_size = 40000 /* 1 s at 320 kbps */ * 5;
  input_buffer = LocalAlloc(0, input_size);
  if (input_buffer == 0) {
    show_error(0, "Out of Memory", IDS_ERR_NOMEM);
    return 1;
  }

  mad_stream_init(&stream);
  mad_frame_init(&frame);
  mad_synth_init(&synth);

  timer = duration = mad_timer_zero;

  stats_init(&stats);

  while (1) {
    if (input_length < input_size / 2) {
      DWORD bytes;

      bytes = input_read(&state->input, input_buffer + input_length,
			 input_size - input_length);
      if (bytes <= 0) {
	if (bytes == -1)
	  show_error(0, "Error Reading Data", GetLastError());
	break;
      }

      input_length += bytes;
    }

    mad_stream_buffer(&stream, input_buffer, input_length);

    if (seek_skip) {
      int skip;

      skip = 2;
      while (skip) {
	if (mad_frame_decode(&frame, &stream) == 0) {
	  mad_timer_add(&timer, frame.header.duration);
	  if (--skip == 0)
	    mad_synth_frame(&synth, &frame);
	}
	else if (!MAD_RECOVERABLE(stream.error))
	  break;
      }

      module.outMod->Flush(seek_skip);

      seek_skip = 0;
    }

    while (!state->stop) {
      char *output_ptr;
      int nch, bytes;

      if (state->seek != -1 && state->length >= 0) {
	int new_position;

	if (state->seek < 0)
	  new_position = (double) state->length * -state->seek / 1000;
	else
	  new_position = state->seek;

	state->position = new_position;
	state->seek     = -1;

	if (input_seek(&state->input, (double) new_position *
		       state->size / state->length, FILE_BEGIN) != -1) {
	  mad_timer_set(&timer, new_position / 1000,
			new_position % 1000, 1000);

	  mad_frame_mute(&frame);
	  mad_synth_mute(&synth);

	  input_length = stream.next_frame - &input_buffer[0];
	  stream.error = MAD_ERROR_BUFLEN;
	  stream.sync  = 0;

	  if (new_position > 0)
	    seek_skip = new_position;
	  else
	    module.outMod->Flush(0);

	  break;
	}
      }

      if (mad_frame_decode(&frame, &stream) == -1) {
	if (!MAD_RECOVERABLE(stream.error))
	  break;

	module.SetInfo(-1, -1, -1, 0);
	last_bitrate = 0;

	if (do_error(&stream, &frame, &state->input, &last_error))
	  continue;
      }
      else
	last_error = 0;

      avgbitrate = state->bitrate ?
	state->bitrate : stats_update(&stats, frame.header.bitrate);

      bitrate = conf_avgbitrate ? avgbitrate : frame.header.bitrate / 1000;

      /*
       * Winamp exhibits odd behavior if SetInfo() is called again with the
       * same bitrate as the last call. A symptom is a failure of the song
       * title to scroll in the task bar (if that option has been enabled.)
       */

      if (bitrate != last_bitrate) {
	module.SetInfo(bitrate, -1, -1, 1);
	last_bitrate = bitrate;
      }

      mad_synth_frame(&synth, &frame);

      nch = MAD_NCHANNELS(&frame.header);

      output_length +=
	pack_pcm(output_buffer + output_length, synth.pcm.length,
		 synth.pcm.samples[0], nch == 1 ? 0 : synth.pcm.samples[1],
		 resolution);

      output_ptr = output_buffer;

      mad_timer_set(&duration, 0, PCM_CHUNK, frame.header.sfreq);

      bytes = PCM_CHUNK * (resolution / 8) * nch;

      while (output_length >= bytes) {
	int dsp;

	dsp = module.dsp_isactive();

	while (!(state->stop || (state->seek != -1 && state->length >= 0)) &&
	       module.outMod->CanWrite() < (dsp ? bytes * 2 : bytes))
	  Sleep(20);

	if (state->stop || (state->seek != -1 && state->length >= 0))
	  break;

	module.SAAddPCMData(output_ptr,  nch, resolution, state->position);
	module.VSAAddPCMData(output_ptr, nch, resolution, state->position);

	mad_timer_add(&timer, duration);
	state->position = mad_timer_count(timer, MAD_UNITS_MILLISECONDS);

	if (dsp) {
	  static unsigned char dsp_buffer[sizeof(output_buffer) * 2];
	  int nsamples;

	  memcpy(dsp_buffer, output_ptr, bytes);

	  nsamples = module.dsp_dosamples((short *) dsp_buffer, PCM_CHUNK,
					  resolution, nch, frame.header.sfreq);
	  module.outMod->Write(dsp_buffer, nsamples * (resolution / 8) * nch);
	}
	else
	  module.outMod->Write(output_ptr, bytes);

	output_ptr    += bytes;
	output_length -= bytes;
      }

      if (state->seek != -1 && state->length >= 0)
	output_length = 0;
      else if (output_length)
	memmove(output_buffer, output_ptr, output_length);
    }

    if (state->stop || stream.error != MAD_ERROR_BUFLEN)
      break;

    memmove(input_buffer, stream.next_frame,
	    input_length = &input_buffer[input_length] - stream.next_frame);
  }

  mad_synth_finish(&synth);
  mad_frame_finish(&frame);
  mad_stream_finish(&stream);

  while (!state->stop && module.outMod->IsPlaying()) {
    Sleep(10);

    module.outMod->CanWrite();  /* ?? */
  }

  if (!state->stop)
    PostMessage(module.hMainWindow, WM_WA_MPEG_EOF, 0, 0);

  LocalFree(input_buffer);

  return 0;
}

static
int scan_header(struct input *input, struct mad_header *dest)
{
  struct mad_stream stream;
  struct mad_header header;
  unsigned char buffer[8192];
  unsigned int buflen = 0;
  int count = 0, result = 0;

  mad_stream_init(&stream);
  mad_header_init(&header);

  while (1) {
    if (buflen < sizeof(buffer)) {
      DWORD bytes;

      bytes = input_read(input, buffer + buflen, sizeof(buffer) - buflen);
      if (bytes <= 0) {
	if (bytes == -1)
	  result = -1;
	break;
      }

      buflen += bytes;
    }

    mad_stream_buffer(&stream, buffer, buflen);

    while (1) {
      if (mad_header_decode(&header, &stream) == -1) {
	if (!MAD_RECOVERABLE(stream.error))
	  break;

	if (do_error(&stream, 0, input, 0))
	  continue;
      }

      if (count++)
	break;
    }

    if (count || stream.error != MAD_ERROR_BUFLEN)
      break;

    memmove(buffer, stream.next_frame,
	    buflen = &buffer[buflen] - stream.next_frame);
  }

  if (count)
    *dest = header;
  else
    result = -1;

  mad_header_finish(&header);
  mad_stream_finish(&stream);

  return result;
}

static
void scan_file(struct input *input, int *stop_flag,
	       int *length, int *bitrate)
{
  struct mad_stream stream;
  struct mad_header header;
  mad_timer_t timer;
  unsigned char buffer[8192];
  unsigned int buflen = 0;
  struct stats stats;
  int avgbitrate = 0;

  mad_stream_init(&stream);
  mad_header_init(&header);

  timer = mad_timer_zero;

  stats_init(&stats);

  while (1) {
    if (buflen < sizeof(buffer)) {
      DWORD bytes;

      bytes = input_read(input, buffer + buflen, sizeof(buffer) - buflen);
      if (bytes <= 0)
	break;

      buflen += bytes;
    }

    mad_stream_buffer(&stream, buffer, buflen);

    while (!stop_flag || !*stop_flag) {
      if (mad_header_decode(&header, &stream) == -1) {
	if (!MAD_RECOVERABLE(stream.error))
	  break;

	if (do_error(&stream, 0, input, 0))
	  continue;
      }

      if (length)
	mad_timer_add(&timer, header.duration);
      if (bitrate)
	avgbitrate = stats_update(&stats, header.bitrate);
    }

    if ((stop_flag && *stop_flag) || stream.error != MAD_ERROR_BUFLEN)
      break;

    memmove(buffer, stream.next_frame, &buffer[buflen] - stream.next_frame);
    buflen -= stream.next_frame - &buffer[0];
  }

  mad_header_finish(&header);
  mad_stream_finish(&stream);

  if (length)
    *length = mad_timer_count(timer, MAD_UNITS_MILLISECONDS);
  if (bitrate)
    *bitrate = avgbitrate;
}

static
DWORD WINAPI run_length_thread(void *param)
{
  struct state *state = param;
  HANDLE file;
  struct input input;

  file = CreateFile(state->path, GENERIC_READ,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
		    OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
  if (file == INVALID_HANDLE_VALUE)
    return -1;

  input_init(&input, INPUT_FILE, file);

  scan_file(&input, &state->stop, &state->length, &state->bitrate);

  input_close(&input);

  return 0;
}

static
int decode_start(struct state *state, struct mad_header *header)
{
  int max_latency, nch, priority;
  DWORD thread_id;

  state->bitrate  = 0;
  state->position = 0;
  state->paused   = 0;
  state->seek     = -1;
  state->stop     = 0;

  nch = MAD_NCHANNELS(header);

  max_latency =
    module.outMod->Open(header->sfreq, nch, conf_resolution, -1, -1);

  if (max_latency < 0) {  /* error opening output device */
    input_close(&state->input);
    return 3;
  }

  module.SetInfo(header->bitrate / 1000, header->sfreq / 1000, nch, 0);

  if (state->input.type == INPUT_FILE) {
    /* start file length calculation thread */
    length_thread = CreateThread(0, 0, run_length_thread, state,
				 0, &thread_id);
    SetThreadPriority(length_thread, THREAD_PRIORITY_BELOW_NORMAL);
  }

  /* initialize visuals */
  module.SAVSAInit(max_latency, header->sfreq);
  module.VSASetInfo(header->sfreq, nch);

  /* set the output module's default volume */
  module.outMod->SetVolume(-666);  /* ?? */

  /* start decoder thread */
  decode_thread = CreateThread(0, 0, run_decode_thread, state,
			       0, &thread_id);

  switch (conf_priority) {
  case -2:
    priority = THREAD_PRIORITY_LOWEST;
    break;
  case -1:
    priority = THREAD_PRIORITY_BELOW_NORMAL;
    break;
  case 0:
  default:
    priority = THREAD_PRIORITY_NORMAL;
    break;
  case +1:
    priority = THREAD_PRIORITY_ABOVE_NORMAL;
    break;
  case +2:
    priority = THREAD_PRIORITY_HIGHEST;
    break;
  }
  SetThreadPriority(decode_thread, priority);

  return 0;
}

static
int play_stream(struct state *state)
{
  HINTERNET stream;
  struct mad_header header;

  if (internet == INVALID_HANDLE_VALUE) {
    internet = InternetOpen("MAD/" PLUGIN_VERSION " (Winamp Plug-in)",
			    INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
  }

  stream = InternetOpenUrl(internet, state->path, "Icy-MetaData: 0", -1,
			   INTERNET_FLAG_NO_CACHE_WRITE, 0);
  if (stream == 0) {
    show_error(0, "Error Opening Stream", GetLastError());
    return -1;
  }

  input_init(&state->input, INPUT_STREAM, stream);

  if (scan_header(&state->input, &header) == -1) {
    input_close(&state->input);

    show_error(0, "Error Reading Stream", IDS_WARN_NOHEADER);
    return 2;
  }

  state->size     = 0;
  state->length   = 0;

  module.is_seekable = 0;

  return decode_start(state, &header);
}

static
int play_file(struct state *state)
{
  HANDLE file;
  struct mad_header header;

  file = CreateFile(state->path, GENERIC_READ,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
		    OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
  if (file == INVALID_HANDLE_VALUE) {
    DWORD error;

    show_error(0, "Error Opening File", error = GetLastError());
    return (error == ERROR_FILE_NOT_FOUND) ? -1 : 1;
  }

  input_init(&state->input, INPUT_FILE, file);

  if (scan_header(&state->input, &header) == -1) {
    input_close(&state->input);

    show_error(0, "Error Reading File", IDS_WARN_NOHEADER);
    return 2;
  }

  input_seek(&state->input, 0, FILE_BEGIN);

  state->size   = GetFileSize(file, 0);
  state->length = -(state->size / (header.bitrate / 8 / 1000));  /* est. */

  module.is_seekable = 1;

  return decode_start(state, &header);
}

static
int do_play(char *path)
{
  strcpy(state.path, path);

  return is_stream(path) ? play_stream(&state) : play_file(&state);
}

static
void do_pause(void)
{
  state.paused = 1;
  module.outMod->Pause(1);
}

static
void do_unpause(void)
{
  state.paused = 0;
  module.outMod->Pause(0);
}

static
int is_paused(void)
{
  return state.paused;
}

static
void do_stop(void)
{
  if (decode_thread != INVALID_HANDLE_VALUE) {
    state.stop = 1;

    if (WaitForSingleObject(decode_thread, INFINITE) == WAIT_TIMEOUT)
      TerminateThread(decode_thread, 0);

    CloseHandle(decode_thread);
    decode_thread = INVALID_HANDLE_VALUE;
  }

  if (length_thread != INVALID_HANDLE_VALUE) {
    if (WaitForSingleObject(length_thread, INFINITE) == WAIT_TIMEOUT)
      TerminateThread(length_thread, 0);

    CloseHandle(length_thread);
    length_thread = INVALID_HANDLE_VALUE;
  }

  input_close(&state.input);

  module.outMod->Close();
  module.SAVSADeInit();
}

static
int get_length(void)
{
  return abs(state.length);
}

static
int get_outputtime(void)
{
  if (state.seek >= 0)
    return state.seek;

  if (state.seek < -1)
    return (double) state.length * state.seek / 1000;

  return state.position +
    (module.outMod->GetOutputTime() - module.outMod->GetWrittenTime());
}

static
void set_outputtime(int position)
{
  int seek;

  seek = position;

  if (state.length < 0) {
    seek = (double) position * 1000 / state.length;
    if (seek >= 0)
      seek = position;
    else if (seek == -1)
      seek = -2;
  }

  state.seek = seek;
}

static
void set_volume(int volume)
{
  module.outMod->SetVolume(volume);
}

static
void set_pan(int pan)
{
  module.outMod->SetPan(pan);
}

struct id3v1 {
  unsigned char data[128];
  char title[30 + 1];
  char artist[30 + 1];
  char album[30 + 1];
  char year[4 + 1];
  char comment[30 + 1];
  int track;
  int genre;
};

static
void strip(char *str)
{
  char *ptr;

  ptr = str + strlen(str);
  while (ptr > str && ptr[-1] == ' ')
    --ptr;

  if (ptr > str || *ptr == ' ')
    *ptr = 0;
}

static
int id3v1_fromdata(struct id3v1 *tag)
{
  if (memcmp(tag->data, "TAG", 3) != 0) {
    tag->title[0]   = 0;
    tag->artist[0]  = 0;
    tag->album[0]   = 0;
    tag->year[0]    = 0;
    tag->comment[0] = 0;

    tag->track = 0;
    tag->genre = -1;

    return -1;
  }

  tag->title[30]   = 0;
  tag->artist[30]  = 0;
  tag->album[30]   = 0;
  tag->year[4]     = 0;
  tag->comment[30] = 0;

  memcpy(tag->title,   &tag->data[3],  30);
  memcpy(tag->artist,  &tag->data[33], 30);
  memcpy(tag->album,   &tag->data[63], 30);
  memcpy(tag->year,    &tag->data[93],  4);
  memcpy(tag->comment, &tag->data[97], 30);

  strip(tag->title);
  strip(tag->artist);
  strip(tag->album);
  strip(tag->year);
  strip(tag->comment);

  tag->track = 0;
  if (tag->data[125] == 0)
    tag->track = tag->data[126];

  tag->genre = tag->data[127];

  return 0;
}

static
int id3v1_todata(struct id3v1 *tag)
{
  int i;

  memset(tag->data, ' ', sizeof(tag->data));

  memcpy(&tag->data[0],  "TAG",        3);
  memcpy(&tag->data[3],  tag->title,   strlen(tag->title));
  memcpy(&tag->data[33], tag->artist,  strlen(tag->artist));
  memcpy(&tag->data[63], tag->album,   strlen(tag->album));
  memcpy(&tag->data[93], tag->year,    strlen(tag->year));
  memcpy(&tag->data[97], tag->comment, strlen(tag->comment));

  if (tag->track) {
    tag->data[125] = 0;
    tag->data[126] = tag->track;
  }

  tag->data[127] = tag->genre;

  /* check whether tag is empty */

  for (i = 3; i < 127; ++i) {
    if (tag->data[i] != ' ')
      return 1;
  }

  return tag->data[127] != (unsigned char) ~0;
}

struct fileinfo {
  HANDLE file;
  DWORD attributes;
  DWORD size;

  char dirname[MAX_PATH];
  char *basename;

  struct {
    HWND dialog;
    HANDLE scan_thread;
    int stop;
    int length;
    int bitrate;
    struct mad_header header;
  } mpeg;

  struct {
    int has;
    struct id3v1 tag;
  } id3v1;

  struct {
    int has;
  } id3v2;
};

static
DWORD WINAPI run_scan_thread(void *param)
{
  HANDLE file;
  struct fileinfo *info = param;
  struct input input;

  if (DuplicateHandle(GetCurrentProcess(), info->file,
		      GetCurrentProcess(), &file, GENERIC_READ, FALSE, 0) == 0)
    return 1;

  input_init(&input, INPUT_FILE, file);

  input_seek(&input, 0, FILE_BEGIN);
  scan_header(&input, &info->mpeg.header);

  if (!info->mpeg.stop)
    PostMessage(info->mpeg.dialog, WM_MAD_SCAN_FINISHED, 1, 0);

  input_seek(&input, 0, FILE_BEGIN);
  scan_file(&input, &info->mpeg.stop, &info->mpeg.length, &info->mpeg.bitrate);

  if (!info->mpeg.stop)
    PostMessage(info->mpeg.dialog, WM_MAD_SCAN_FINISHED, 0, 0);

  input_close(&input);

  return 0;
}

static
void groupnumber(char *str, int num)
{
  struct lconv *lconv;
  char *ptr, sep;
  int len, grouping, count, m;

  sprintf(str, "%d", num);
  len = strlen(str);

  lconv = localeconv();

  sep      = *lconv->thousands_sep;
  grouping = *lconv->grouping;

  if (sep == 0)
    sep = ',';
  if (grouping == 0)
    grouping = 3;

  count = (len - 1) / grouping;

  ptr  = str + len + count;
  *ptr = 0;

  m = grouping;
  while (count) {
    ptr[-1] = ptr[-1 - count];
    --ptr;

    if (--m == 0) {
      *--ptr = sep;
      --count;
      m = grouping;
    }
  }
}

static CALLBACK
UINT mpeg_callback(HWND hwnd, UINT message, PROPSHEETPAGE *psp)
{
  struct fileinfo *info = (struct fileinfo *) psp->lParam;
  HANDLE scan_thread = info->mpeg.scan_thread;

  switch (message) {
  case PSPCB_CREATE:
    return TRUE;

  case PSPCB_RELEASE:
    if (scan_thread != INVALID_HANDLE_VALUE) {
      info->mpeg.stop = 1;

      if (WaitForSingleObject(scan_thread, INFINITE) == WAIT_TIMEOUT)
	TerminateThread(scan_thread, 0);

      CloseHandle(scan_thread);
      info->mpeg.scan_thread = INVALID_HANDLE_VALUE;
    }
    break;
  }

  return 0;
}

static CALLBACK
BOOL mpeg_dialog(HWND dialog, UINT message,
		 WPARAM wparam, LPARAM lparam)
{
  static PROPSHEETPAGE *psp;
  static struct fileinfo *info;

  switch (message) {
  case WM_INITDIALOG:
    {
      DWORD thread_id;
      char str[40];
      int size;
      char *unit;

      psp  = (PROPSHEETPAGE *) lparam;
      info = (struct fileinfo *) psp->lParam;

      info->mpeg.dialog      = dialog;
      info->mpeg.stop        = 0;
      info->mpeg.scan_thread =
	CreateThread(0, 0, run_scan_thread, info, 0, &thread_id);

      SetDlgItemText(dialog, IDC_MPEG_LOCATION, info->dirname);

      size = info->size;
      unit = " bytes";

      if (size >= 1024) {
	size /= 1024;
	unit  = "KB";

	if (size >= 1024) {
	  size /= 1024;
	  unit  = "MB";

	  if (size >= 1024) {
	    size /= 1024;
	    unit  = "GB";

	    if (size >= 1024) {
	      size /= 1024;
	      unit  = "TB";
	    }
	  }
	}
      }

      sprintf(str, "%d%s", size, unit);

      if (*unit != ' ') {
	strcat(str, " (");
	groupnumber(str + strlen(str), info->size);
	strcat(str, " bytes)");
      }

      SetDlgItemText(dialog, IDC_MPEG_SIZE, str);
    }
    return FALSE;

  case WM_MAD_SCAN_FINISHED:
    {
      struct mad_header *header = &info->mpeg.header;
      char *ptr;
      char str[19];

      if (wparam) {
	switch (header->sfreq) {
	case 48000:
	case 44100:
	case 32000:
	  ptr = "MPEG-1";
	  break;

	case 24000:
	case 22050:
	case 16000:
	  ptr = "MPEG-2";
	  break;

	case 12000:
	case 11025:
	case  8000:
	  ptr = "MPEG 2.5";
	  break;

	default:
	  ptr = "unknown";
	}
	SetDlgItemText(dialog, IDC_MPEG_TYPE, ptr);

	switch (header->layer) {
	case MAD_LAYER_I:
	  ptr = "I";
	  break;

	case MAD_LAYER_II:
	  ptr = "II";
	  break;

	case MAD_LAYER_III:
	  ptr = "III";
	  break;

	default:
	  ptr = "unknown";
	}
	SetDlgItemText(dialog, IDC_MPEG_LAYER, ptr);

	sprintf(str, "%lu kbps", header->bitrate / 1000);
	SetDlgItemText(dialog, IDC_MPEG_BITRATE, str);

	sprintf(str, "%u Hz", header->sfreq);
	SetDlgItemText(dialog, IDC_MPEG_SAMPLERATE, str);

	switch (header->emphasis) {
	case MAD_EMPHASIS_NONE:
	  ptr = "none";
	  break;

	case MAD_EMPHASIS_50_15_MS:
	  ptr = "50/15 ms";
	  break;

	case MAD_EMPHASIS_CCITT_J_17:
	  ptr = "CCITT J.17";
	  break;

	default:
	  ptr = "unknown";
	}
	SetDlgItemText(dialog, IDC_MPEG_EMPHASIS, ptr);

	switch (header->mode) {
	case MAD_MODE_SINGLE_CHANNEL:
	  CheckDlgButton(dialog, IDC_MPEG_SINGLECH, BST_CHECKED);
	  break;

	case MAD_MODE_DUAL_CHANNEL:
	  CheckDlgButton(dialog, IDC_MPEG_DUALCH, BST_CHECKED);
	  break;

	case MAD_MODE_JOINT_STEREO:
	  CheckDlgButton(dialog, IDC_MPEG_JOINTST, BST_CHECKED);
	  break;

	case MAD_MODE_STEREO:
	  CheckDlgButton(dialog, IDC_MPEG_STEREO, BST_CHECKED);
	  break;
	}

	if (header->flags & MAD_FLAG_COPYRIGHT)
	  CheckDlgButton(dialog, IDC_MPEG_COPYRIGHT, BST_CHECKED);
	if (header->flags & MAD_FLAG_ORIGINAL)
	  CheckDlgButton(dialog, IDC_MPEG_ORIGINAL, BST_CHECKED);
	if (header->flags & MAD_FLAG_PROTECTION)
	  CheckDlgButton(dialog, IDC_MPEG_CRC, BST_CHECKED);
      }
      else {
	mad_timer_set(&header->duration, info->mpeg.length / 1000,
		      info->mpeg.length % 1000, 1000);
	mad_timer_string(header->duration, str,
			 "%lu:%02u:%02u.%1u", MAD_UNITS_HOURS,
			 MAD_UNITS_DECISECONDS, 0);
	ptr = strchr(str, '.');
	if (ptr)
	  *ptr = *localeconv()->decimal_point;
	SetDlgItemText(dialog, IDC_MPEG_LENGTH, str);

	if (info->mpeg.bitrate != header->bitrate / 1000) {
	  sprintf(str, "%d kbps", info->mpeg.bitrate);
	  SetDlgItemText(dialog, IDC_MPEG_BITRATELABEL, "Avg. Bitrate:");
	  SetDlgItemText(dialog, IDC_MPEG_BITRATE, str);
	}
      }
    }
    return TRUE;

  case WM_NOTIFY:
    switch (((NMHDR *) lparam)->code) {
    case PSN_SETACTIVE:
      return TRUE;

    case PSN_KILLACTIVE:
      SetWindowLong(dialog, DWL_MSGRESULT, FALSE);
      return TRUE;

    case PSN_APPLY:
      return TRUE;
    }
    break;
  }

  return FALSE;
}

static
char const *const genre_str[] = {
# include "id3genre.dat"
};

# define NGENRES	(sizeof(genre_str) / sizeof(genre_str[0]))

static CALLBACK
BOOL id3v1_dialog(HWND dialog, UINT message,
		  WPARAM wparam, LPARAM lparam)
{
  static PROPSHEETPAGE *psp;
  static struct fileinfo *info;
  struct id3v1 *tag;
  int index;

  switch (message) {
  case WM_INITDIALOG:
    psp  = (PROPSHEETPAGE *) lparam;
    info = (struct fileinfo *) psp->lParam;

    /* initialize genre combobox */

    for (index = 0; index < NGENRES; ++index) {
      SendDlgItemMessage(dialog, IDC_ID3V1_GENRE,
			 CB_ADDSTRING, 0, (LPARAM) genre_str[index]);
    }

    index = SendDlgItemMessage(dialog, IDC_ID3V1_GENRE,
			       CB_ADDSTRING, 0, (LPARAM) " ");

    /* disable fields for read-only files, or set text limits */

    if (info->attributes & FILE_ATTRIBUTE_READONLY) {
      EnableWindow(GetDlgItem(dialog, IDC_ID3V1_TITLE),   FALSE);
      EnableWindow(GetDlgItem(dialog, IDC_ID3V1_ARTIST),  FALSE);
      EnableWindow(GetDlgItem(dialog, IDC_ID3V1_ALBUM),   FALSE);
      EnableWindow(GetDlgItem(dialog, IDC_ID3V1_YEAR),    FALSE);
      EnableWindow(GetDlgItem(dialog, IDC_ID3V1_TRACK),   FALSE);
      EnableWindow(GetDlgItem(dialog, IDC_ID3V1_GENRE),   FALSE);
      EnableWindow(GetDlgItem(dialog, IDC_ID3V1_COMMENT), FALSE);
    }
    else {
      SendDlgItemMessage(dialog, IDC_ID3V1_TITLE,   EM_SETLIMITTEXT, 30, 0);
      SendDlgItemMessage(dialog, IDC_ID3V1_ARTIST,  EM_SETLIMITTEXT, 30, 0);
      SendDlgItemMessage(dialog, IDC_ID3V1_ALBUM,   EM_SETLIMITTEXT, 30, 0);
      SendDlgItemMessage(dialog, IDC_ID3V1_YEAR,    EM_SETLIMITTEXT,  4, 0);
      SendDlgItemMessage(dialog, IDC_ID3V1_TRACK,   EM_SETLIMITTEXT,  3, 0);
      SendDlgItemMessage(dialog, IDC_ID3V1_COMMENT, EM_SETLIMITTEXT, 30, 0);
    }

    /* load existing tag data */

    tag = &info->id3v1.tag;

    if (info->id3v1.has)
      SetDlgItemText(dialog, IDC_ID3V1_TITLE, tag->title);
    else {
      char filename[31], *ptr;

      strncpy(filename, info->basename, 30);
      filename[30] = 0;

      ptr = strrchr(filename, '.');
      if (ptr)
	*ptr = 0;

      for (ptr = filename; *ptr; ++ptr) {
	if (*ptr == '_')
	  *ptr = ' ';
      }

      SetDlgItemText(dialog, IDC_ID3V1_TITLE, filename);
    }

    SetDlgItemText(dialog, IDC_ID3V1_ARTIST,  tag->artist);
    SetDlgItemText(dialog, IDC_ID3V1_ALBUM,   tag->album);
    SetDlgItemText(dialog, IDC_ID3V1_YEAR,    tag->year);
    SetDlgItemText(dialog, IDC_ID3V1_COMMENT, tag->comment);

    if (tag->track)
      SetDlgItemInt(dialog, IDC_ID3V1_TRACK, tag->track, FALSE);

    if (tag->genre >= 0) {
      if (tag->genre < NGENRES) {
	index = SendDlgItemMessage(dialog, IDC_ID3V1_GENRE, CB_FINDSTRINGEXACT,
				   -1, (LPARAM) genre_str[tag->genre]);
      }
      else if (tag->genre < 255) {
	char str[14];

	sprintf(str, "(%d)", tag->genre);

	index = SendDlgItemMessage(dialog, IDC_ID3V1_GENRE,
				   CB_ADDSTRING, 0, (LPARAM) str);
      }
    }

    SendDlgItemMessage(dialog, IDC_ID3V1_GENRE, CB_SETCURSEL, index, 0);

    return FALSE;

  case WM_NOTIFY:
    switch (((NMHDR *) lparam)->code) {
    case PSN_SETACTIVE:
      return TRUE;

    case PSN_KILLACTIVE:
      {
	int track;
	BOOL success;

	track = GetDlgItemInt(dialog, IDC_ID3V1_TRACK, &success, FALSE);
	if (!success || track < 1 || track > 255) {
	  if (SendDlgItemMessage(dialog, IDC_ID3V1_TRACK,
				 WM_GETTEXTLENGTH, 0, 0) > 0) {
	    show_error(dialog, "Invalid Track", IDS_WARN_BADTRACK);
	    SetFocus(GetDlgItem(dialog, IDC_ID3V1_TRACK));
	    SetWindowLong(dialog, DWL_MSGRESULT, TRUE);
	    return TRUE;
	  }
	}

	if (success && SendDlgItemMessage(dialog, IDC_ID3V1_COMMENT,
					  WM_GETTEXTLENGTH, 0, 0) > 28) {
	  show_error(dialog, "ID3v1.1 Comment Too Long",
		     IDS_WARN_COMMENTTOOLONG);
	  SetFocus(GetDlgItem(dialog, IDC_ID3V1_COMMENT));
	  SetWindowLong(dialog, DWL_MSGRESULT, TRUE);
	  return TRUE;
	}
      }

      SetWindowLong(dialog, DWL_MSGRESULT, FALSE);
      return TRUE;

    case PSN_APPLY:
      if (!(info->attributes & FILE_ATTRIBUTE_READONLY)) {
	char selected[25];
	DWORD bytes;

	tag = &info->id3v1.tag;

	/* title, artist, album, year, comment, track */

	GetDlgItemText(dialog, IDC_ID3V1_TITLE,
		       tag->title,   sizeof(tag->title));
	GetDlgItemText(dialog, IDC_ID3V1_ARTIST,
		       tag->artist,  sizeof(tag->artist));
	GetDlgItemText(dialog, IDC_ID3V1_ALBUM,
		       tag->album,   sizeof(tag->album));
	GetDlgItemText(dialog, IDC_ID3V1_YEAR,
		       tag->year,    sizeof(tag->year));
	GetDlgItemText(dialog, IDC_ID3V1_COMMENT,
		       tag->comment, sizeof(tag->comment));

	tag->track = GetDlgItemInt(dialog, IDC_ID3V1_TRACK, 0, FALSE);

	/* genre */

	tag->genre = -1;

	GetDlgItemText(dialog, IDC_ID3V1_GENRE, selected, sizeof(selected));

	if (selected[0] == '(')
	  sscanf(selected + 1, "%d", &tag->genre);
	else {
	  for (index = 0; index < NGENRES; ++index) {
	    if (strcmp(selected, genre_str[index]) == 0) {
	      tag->genre = index;
	      break;
	    }
	  }
	}

	/* write or delete tag */

	SetFilePointer(info->file, info->id3v1.has ?
		       -sizeof(info->id3v1.tag.data) : 0, 0, FILE_END);

	if (id3v1_todata(tag)) {
	  if (WriteFile(info->file, info->id3v1.tag.data,
			sizeof(info->id3v1.tag.data), &bytes, 0) == 0)
	    show_error(dialog, "Error Writing ID3v1 Tag", GetLastError());
	  else if (bytes != sizeof(info->id3v1.tag.data))
	    show_error(dialog, 0, IDS_ERR_ID3V1WRITE);
	}
	else
	  SetEndOfFile(info->file);
      }
      return TRUE;
    }
    break;
  }

  return FALSE;
}

static CALLBACK
BOOL id3v2_dialog(HWND dialog, UINT message,
		  WPARAM wparam, LPARAM lparam)
{
  static PROPSHEETPAGE *psp;
  static struct fileinfo *info;

  switch (message) {
  case WM_INITDIALOG:
    psp  = (PROPSHEETPAGE *) lparam;
    info = (struct fileinfo *) psp->lParam;
    return FALSE;

  case WM_NOTIFY:
    switch (((NMHDR *) lparam)->code) {
    case PSN_SETACTIVE:
      return TRUE;

    case PSN_KILLACTIVE:
      SetWindowLong(dialog, DWL_MSGRESULT, FALSE);
      return TRUE;

    case PSN_APPLY:
      return TRUE;
    }
    break;
  }

  return FALSE;
}

static CALLBACK
int propsheet_init(HWND dialog, UINT message, LPARAM lparam)
{
  switch (message) {
  case PSCB_INITIALIZED:
    SetDlgItemText(dialog, IDOK, "Save");
    SetFocus(PropSheet_GetTabControl(dialog));
    break;
  }

  return 0;
}

static CALLBACK
int propsheet_ro_init(HWND dialog, UINT message, LPARAM lparam)
{
  switch (message) {
  case PSCB_INITIALIZED:
    SetDlgItemText(dialog, IDCANCEL, "Close");
    EnableWindow(GetDlgItem(dialog, IDOK), FALSE);
    SendDlgItemMessage(dialog, IDOK, BM_SETSTYLE, BS_PUSHBUTTON, FALSE);
    break;
  }

  return propsheet_init(dialog, message, lparam);
}

static
void proppage_init(PROPSHEETPAGE *page, int proppage_id,
		   DLGPROC dlgproc, LPARAM lparam, LPFNPSPCALLBACK callback)
{
  page->dwSize      = sizeof(*page);
  page->dwFlags     = PSP_DEFAULT | (callback ? PSP_USECALLBACK : 0);
  page->hInstance   = module.hDllInstance;
  page->pszTemplate = MAKEINTRESOURCE(proppage_id);
  page->pszIcon     = 0;
  page->pszTitle    = 0;
  page->pfnDlgProc  = dlgproc;
  page->lParam      = lparam;
  page->pfnCallback = callback;
  page->pcRefParent = 0;
}

static
int show_infobox(char *path, HWND parent)
{
  PROPSHEETPAGE psp[3];
  PROPSHEETHEADER psh;
  struct fileinfo info;
  DWORD bytes;

  if (is_stream(path)) {
    MessageBox(parent, "Stream info not yet implemented.",
	       "Not Implemented", MB_ICONWARNING | MB_OK);
    return 0;
  }

  info.file = CreateFile(path, GENERIC_READ | GENERIC_WRITE,
			 FILE_SHARE_READ, 0,
			 OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
  if (info.file == INVALID_HANDLE_VALUE) {
    info.file = CreateFile(path, GENERIC_READ,
			   FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
			   OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, 0);
    if (info.file == INVALID_HANDLE_VALUE) {
      show_error(parent, "Error Opening File", GetLastError());
      return 1;
    }

    info.attributes = FILE_ATTRIBUTE_READONLY;
  }
  else
    info.attributes = 0;

  info.size = GetFileSize(info.file, 0);

  strcpy(info.dirname, path);

  info.basename = strrchr(info.dirname, '\\');
  if (info.basename)
    *info.basename++ = 0;
  else
    info.basename = info.dirname;

  info.mpeg.dialog      = 0;
  info.mpeg.scan_thread = INVALID_HANDLE_VALUE;
  info.mpeg.stop        = 0;
  info.mpeg.length      = 0;
  info.mpeg.bitrate     = 0;
  mad_header_init(&info.mpeg.header);

  SetFilePointer(info.file, -sizeof(info.id3v1.tag.data), 0, FILE_END);
  if (ReadFile(info.file, info.id3v1.tag.data,
	       sizeof(info.id3v1.tag.data), &bytes, 0) == 0)
    bytes = 0;

  if (bytes != sizeof(info.id3v1.tag.data))
    memset(info.id3v1.tag.data, 0, sizeof(info.id3v1.tag.data));

  info.id3v1.has = (id3v1_fromdata(&info.id3v1.tag) == 0);
  info.id3v2.has = 0;

  proppage_init(&psp[0], IDD_PROPPAGE_MPEG,  mpeg_dialog,  (LPARAM) &info,
		mpeg_callback);
  proppage_init(&psp[1], IDD_PROPPAGE_ID3V1, id3v1_dialog, (LPARAM) &info, 0);
  proppage_init(&psp[2], IDD_PROPPAGE_ID3V2, id3v2_dialog, (LPARAM) &info, 0);

  psh.dwSize      = sizeof(psh);
  psh.dwFlags     = PSH_PROPSHEETPAGE | PSH_PROPTITLE | PSH_NOAPPLYNOW |
                    PSH_USECALLBACK;
  psh.hwndParent  = parent;
  psh.hInstance   = module.hDllInstance;
  psh.pszIcon     = 0;
  psh.pszCaption  = info.basename;
  psh.nPages      = 2;  /* 3; */
  psh.nStartPage  = info.id3v2.has ? 2 : (info.id3v1.has ? 1 : 0);
  psh.ppsp        = psp;
  psh.pfnCallback = (info.attributes & FILE_ATTRIBUTE_READONLY) ?
                    propsheet_ro_init : propsheet_init;

  PropertySheet(&psh);

  CloseHandle(info.file);

  return 0;
}

static
void get_fileinfo(char *path, char *title, int *length)
{
  char *source;
  HANDLE file = INVALID_HANDLE_VALUE;

  if (path == 0 || *path == 0) {  /* currently playing file */
    source = state.path;

    if (length && !is_stream(source) && state.length >= 0)
      *length = state.length;
  }
  else {  /* some other file */
    source = path;

    if (length && !is_stream(source)) {
      file = CreateFile(source, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
			OPEN_EXISTING, 0, 0);
      if (file != INVALID_HANDLE_VALUE) {
	struct mad_header header;
	struct input input;

	input_init(&input, INPUT_FILE, file);

	if (conf_lengthcalc)
	  scan_file(&input, 0, length, 0);
	else if (scan_header(&input, &header) != -1)
	  *length = GetFileSize(file, 0) / (header.bitrate / 8 / 1000);
      }
    }
  }

  if (title && is_stream(source)) {
    strcpy(title, source);
    return;
  }

  if (title) {
    struct id3v1 tag;
    DWORD bytes;
    char const *in;
    char *out, *bound;

    if (file == INVALID_HANDLE_VALUE) {
      file = CreateFile(source, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
			OPEN_EXISTING, 0, 0);
    }

    if (file == INVALID_HANDLE_VALUE)
      bytes = 0;
    else {
      SetFilePointer(file, -sizeof(tag.data), 0, FILE_END);
      if (ReadFile(file, tag.data, sizeof(tag.data), &bytes, 0) == 0)
	bytes = 0;
    }

    if (bytes != sizeof(tag.data))
      memset(tag.data, 0, sizeof(tag.data));

    if (id3v1_fromdata(&tag) == -1)
      in = ALTERNATE_TITLEFMT;
    else
      in = conf_titlefmt;

    out   = title;
    bound = out + (MAX_PATH - 10 - 1);  /* ?? */

    while (*in && out < bound) {
      char buffer[MAX_PATH], *ptr;
      char const *copy;

      if (*in != '%') {
	*out++ = *in++;
	continue;
      }

      ++in;
      switch (*in++) {
      case '%':
	*out++ = '%';
	continue;

      case '0':  /* track */
	if (tag.track == 0)
	  *out++ = '?';
	else {
	  if (tag.track >= 100)
	    *out++ = '0' + tag.track / 100;
	  if (/* tag.track >= 10 && */ out < bound)
	    *out++ = '0' + (tag.track % 100) / 10;
	  if (out < bound)
	    *out++ = '0' + tag.track % 10;
	}
	continue;

      case '1':  /* artist */
	copy = tag.artist;
	break;

      case '2':  /* title */
	copy = tag.title;
	break;

      case '3':  /* album */
	copy = tag.album;
	break;

      case '4':  /* year */
	copy = tag.year;
	break;

      case '5':  /* comment */
	copy = tag.comment;
	break;

      case '6':  /* genre */
	copy = (tag.genre >= 0 && tag.genre < NGENRES) ?
	  genre_str[tag.genre] : "?";
	break;

      case '7':  /* file name */
	ptr = strrchr(source, '\\');
	if (ptr)
	  ++ptr;
	else
	  ptr = source;

	strcpy(buffer, ptr);

	ptr = strrchr(buffer, '.');
	if (ptr)
	  *ptr = 0;

	copy = buffer;
	break;

      case '8':  /* file path */
	strcpy(buffer, source);

	ptr = strrchr(buffer, '\\');
	if (ptr)
	  *ptr = 0;

	copy = buffer;
	break;

      case '9':  /* file extension */
	copy = strrchr(source, '.');
	if (copy)
	  ++copy;
	else
	  copy = "?";
	break;

      default:
	*out++ = '%';
	if (out < bound)
	  *out++ = in[-1];
	continue;
      }

      if (*copy == 0)
	copy = "?";

      while (*copy && out < bound)
	*out++ = *copy++;
    }

    *out = 0;
  }

  if (file != INVALID_HANDLE_VALUE)
    CloseHandle(file);
}

static
void set_eq(int on, char data[10], int preamp)
{
  /* this is apparently never called */
}

static
In_Module module = {
  /* version */		IN_VER,

  /* description */	"MAD plug-in " PLUGIN_VERSION,

  /* < hMainWindow */	0,
  /* < hDllInstance */	0,

  /* FileExtensions */	"",

  /* is_seekable */	0,
  /* UsesOutputPlug */	1,

  /* Config */		show_config,
  /* About */		show_about,
  /* Init */		do_init,
  /* Quit */		do_quit,

  /* GetFileInfo */	get_fileinfo,
  /* InfoBox */		show_infobox,
  /* IsOurFile */	is_ourfile,

  /* Play */		do_play,
  /* Pause */		do_pause,
  /* UnPause */		do_unpause,
  /* IsPaused */	is_paused,
  /* Stop */		do_stop,

  /* GetLength */	get_length,
  /* GetOutputTime */	get_outputtime,

  /* SetOutputTime */	set_outputtime,
  /* SetVolume */	set_volume,
  /* SetPan */		set_pan,

  /* < SAVSAInit */	0,
  /* < SAVSADeInit */	0,
  /* < SAAddPCMData */	0,
  /* < SAGetMode */	0,
  /* < SAAdd */		0,

  /* < VSAAddPCMData */	0,
  /* < VSAGetMode */	0,
  /* < VSAAdd */	0,
  /* < VSASetInfo */	0,

  /* < dsp_isactive */	0,
  /* < dsp_dosamples */	0,

  /* EQSet */		set_eq,

  /* < SetInfo */	0,

  /* < outMod */	0
};

__declspec(dllexport)
In_Module *winampGetInModule2(void)
{
  return &module;
}
