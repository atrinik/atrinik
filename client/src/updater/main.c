/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <direct.h>
#include <shellapi.h>
#include <stdafx.h>

int main(int argc, char *argv[])
{
	FILE *fp;
	char params[HUGE_BUF], path[HUGE_BUF];
	int i;

	while (!(fp = fopen("atrinik.exe", "w")))
	{
		Sleep(500);
	}

	fclose(fp);
	params[0] = '\0';

	for (i = 1; i < argc; i++)
	{
		strncat(params, argv[i], sizeof(params) - strlen(params) - 1);
	}

	snprintf(path, sizeof(path), "%s/.atrinik/temp", getenv("APPDATA"));

	if (access(path, R_OK) == 0)
	{
		SHELLEXECUTEINFO shExecInfo;

		shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		shExecInfo.fMask = NULL;
		shExecInfo.hwnd = NULL;
		shExecInfo.lpVerb = L"runas";
		shExecInfo.lpFile = L"updater.bat";
		shExecInfo.lpParameters = NULL;
		shExecInfo.lpDirectory = NULL;
		shExecInfo.nShow = SW_SHOWNORMAL;
		shExecInfo.hInstApp = NULL;

		ShellExecuteEx(&shExecInfo);
	}

	while (access(path, R_OK) == 0)
	{
		Sleep(500);
	}

	ShellExecute(NULL, "atrinik.exe", params, NULL, NULL, SW_SHOWDEFAULT);
}
