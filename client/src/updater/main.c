/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * Handles the Windows client wrapper. Basically this updater is executed
 * by shortcuts instead of atrinik.exe in order to handle extracting
 * update patches.
 *
 * @author Alex Tokar
 */

#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <direct.h>
#include <shellapi.h>

#define HUGE_BUF 4096 * 12

int main(int argc, char *argv[])
{
    char params[HUGE_BUF], path[HUGE_BUF], wdir[HUGE_BUF];
    int i;

    params[0] = '\0';

    /* Construct the command parameters from arguments. */
    for (i = 1; i < argc; i++) {
        strncat(params, argv[i], sizeof(params) - strlen(params) - 1);
    }

    /* Check if we have upgrades to apply. */
    snprintf(path, sizeof(path), "%s/.atrinik/temp", getenv("APPDATA"));

    if (access(path, R_OK) == 0) {
        SHELLEXECUTEINFO shExecInfo;

        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = 0;
        shExecInfo.hwnd = NULL;
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = "atrinik.bat";
        shExecInfo.lpParameters = params;
        shExecInfo.lpDirectory = NULL;
        shExecInfo.nShow = SW_SHOW;
        shExecInfo.hInstApp = NULL;

        ShellExecuteEx(&shExecInfo);
    } else {
        /* No updates, execute the client. */
        snprintf(path, sizeof(path), "%s\\atrinik.exe", getcwd(wdir, sizeof(wdir) - 1));
        ShellExecute(NULL, "open", path, params, NULL, SW_SHOWNORMAL);
    }

    return 0;
}
