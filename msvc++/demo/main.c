/*
 * mad - MPEG audio decoder
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
 * $Id: main.c,v 1.1 2001/09/25 09:03:50 rob Exp $
 */

# include <windows.h>
# include <stdio.h>

# include "mad.h"
# include "version.h"
# include "player.h"

static
int decode(char const *path)
{
	struct player player;
	char const *playlist[1];
	int result;

	player_init(&player);

	player.verbosity = 1;
	player.output.command = audio_output(0);

	playlist[0] = path;

	result = player_run(&player, 1, playlist);

	player_finish(&player);

	return result;
}

int main(int argc, char *argv[])
{
    OPENFILENAME ofn;
    char filename[MAX_PATH];
    char key;

    ver_banner(stderr);

    filename[0] = 0;

    ofn.lStructSize       = sizeof(ofn);
    ofn.hwndOwner         = NULL;
    ofn.hInstance         = NULL;
    ofn.lpstrFilter       = "MPEG Audio Files\0" "*.MP1;*.MP2;*.MP3\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter    = 0;
    ofn.nFilterIndex      = 1;
    ofn.lpstrFile         = filename;
    ofn.nMaxFile          = sizeof(filename);
    ofn.lpstrFileTitle    = NULL;
    ofn.nMaxFileTitle     = 0;
    ofn.lpstrInitialDir   = NULL;
    ofn.lpstrTitle        = "MAD Demo";
    ofn.Flags             = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset       = 0;
    ofn.nFileExtension    = 0;
    ofn.lpstrDefExt       = "MP3";
    ofn.lCustData         = 0;
    ofn.lpfnHook          = NULL;
    ofn.lpTemplateName    = NULL;

	if (!GetOpenFileName(&ofn))
		return 0;

	if (decode(filename) == -1)
		fprintf(stderr, "decode(\"%s\") failed\n", filename);

    fprintf(stderr, "\nPress ENTER: ");
    fflush(stderr);
    scanf("%c", &key);

    return 0;
}

/*
 * The following makes editing nicer under Emacs!
 *
 * Local Variables:
 * c-indentation-style: "msvc++"
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 */
