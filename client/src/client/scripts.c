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

/**
 * @file
 * Implements client side scripting support. */

#include <global.h>

#ifndef WIN32
#	include <sys/wait.h>
#endif

static void script_dead(int i);
static void script_process_cmd(int i);
static int script_by_name(const char *name);
static void script_send_item(int i, const char *head, const object *it);

/** List of scripts.*/
static struct script *scripts = NULL;

/** Number of scripts. */
static int num_scripts = 0;

#ifdef WIN32

#define write(x, y, z) emulate_write(x, y, z)
#define read(x, y, z) emulate_read(x, y, z)

static int emulate_read(HANDLE fd, char *buf, size_t len)
{
	DWORD dwBytesRead;
	BOOL rc;

	FlushFileBuffers(fd);
	rc = ReadFile(fd, buf, (DWORD) len, &dwBytesRead, NULL);

	if (rc == 0)
	{
		return -1;
	}

	buf[dwBytesRead] = '\0';

	return dwBytesRead;
}

static int emulate_write(HANDLE fd, const char *buf, size_t len)
{
	DWORD dwBytesWritten;
	BOOL rc;

	rc = WriteFile(fd, buf, (DWORD) len, &dwBytesWritten, NULL);
	FlushFileBuffers(fd);

	if (rc == 0)
	{
		return -1;
	}

	return dwBytesWritten;
}

#endif

/**
 * Load a script.
 * @param cparams Parameters. Includes path to the file and after that
 * parameters to pass to the script. */
void script_load(const char *cparams)
{
#ifndef WIN32
	pid_t pid;
	int fds[2], i = 1;
	char *argv[256];
	struct stat statbuf;
#else
	SECURITY_ATTRIBUTES saAttr;
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartupInfo;
	HANDLE hChildStdinRd, hChildStdinWr, hChildStdinWrDup, hChildStdoutRd;
	HANDLE hChildStdoutWr, hChildStdoutRdDup, hSaveStdin, hSaveStdout;
#endif
	char *name, *args, params[MAX_BUF];

	if (!cparams)
	{
		draw_info(COLOR_RED, "Please specify a script to execute.");
		return;
	}

	/* cparams as passed in is a const value, so need to copy it
	 * to data we can write over. */
	strncpy(params, cparams, MAX_BUF - 1);
	params[MAX_BUF - 1] = '\0';

	/* Get name and args */
	name = params;
	args = name;

	while (*args && *args != ' ')
	{
		args++;
	}

	while (*args && *args == ' ')
	{
		*args++ = '\0';
	}

	if (*args == '\0')
	{
		args = NULL;
	}

#ifndef WIN32
	/* Fill in argv[] */
	argv[0] = name;

	while (args && *args && i < (int) (sizeof(argv) / sizeof(*argv)) - 1)
	{
		argv[i++] = args;

		while (*args && *args != ' ')
		{
			args++;
		}

		while (*args && *args == ' ')
		{
			*args++ = '\0';
		}
	}

	argv[i] = NULL;

	if (stat(argv[0], &statbuf) || !S_ISREG(statbuf.st_mode) || !(statbuf.st_mode & S_IXUSR))
	{
		draw_info(COLOR_RED, "The script does not exist, is not a regular file or is not executable.");
		return;
	}

	/* Create a pair of sockets */
	if (socketpair(PF_LOCAL, SOCK_STREAM, AF_LOCAL, fds))
	{
		draw_info(COLOR_RED, "Unable to start script: socketpair failed.");
		return;
	}

	pid = fork();

	if (pid == -1)
	{
		close(fds[0]);
		close(fds[1]);

		draw_info(COLOR_RED, "Unable to start script: fork failed.");
		return;
	}

	if (pid == 0)
	{
		int r;

		/* Clean up file descriptor space */
		r = dup2(fds[0], 0);

		if (r != 0)
		{
			fprintf(stderr, "Script Child: Failed to set pipe as stdin\n");
		}

		r = dup2(fds[0], 1);

		if (r != 1)
		{
			fprintf(stderr, "Script Child: Failed to set pipe as stdout\n");
		}

		/* Execute */
		r = execvp(argv[0], argv);

		if (r != -1)
		{
			printf("draw %s Script child: no error, but no execvp().\n", COLOR_RED);
		}
		else
		{
			printf("draw %s Script child failed to start: %s\n", COLOR_RED, strerror(errno));
		}

		exit(1);
	}

	close(fds[0]);

	if (fcntl(fds[1], F_SETFL, O_NDELAY) == -1)
	{
		LOG(llevDebug, "Error on fcntl.\n");
	}
#else
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = 1;
	saAttr.lpSecurityDescriptor = NULL;

	hSaveStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
	{
		draw_info(COLOR_RED, "script_load(): stdout CreatePipe() failed.");
		return;
	}

	if (!SetStdHandle(STD_OUTPUT_HANDLE, hChildStdoutWr))
	{
		draw_info(COLOR_RED, "script_load(): Failed to redirect stdout using SetStdHandle().");
		return;
	}

	if (!DuplicateHandle(GetCurrentProcess(), hChildStdoutRd, GetCurrentProcess(), &hChildStdoutRdDup, 0, 0, DUPLICATE_SAME_ACCESS))
	{
		draw_info(COLOR_RED, "script_load(): Failed to duplicate stdout using DuplicateHandle().");
		return;
	}

	CloseHandle(hChildStdoutRd);

	hSaveStdin = GetStdHandle(STD_INPUT_HANDLE);

	if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0))
	{
		draw_info(COLOR_RED, "script_load(): stdin CreatePipe() failed.");
		return;
	}

	if (!SetStdHandle(STD_INPUT_HANDLE, hChildStdinRd))
	{
		draw_info(COLOR_RED, "script_load(): Failed to redirect stdin using SetStdHandle().");
		return;
	}

	if (!DuplicateHandle(GetCurrentProcess(), hChildStdinWr, GetCurrentProcess(), &hChildStdinWrDup, 0, 0, DUPLICATE_SAME_ACCESS))
	{
		draw_info(COLOR_RED, "script_load(): failed to duplicate stdin using DuplicateHandle()");
		return;
	}

	CloseHandle(hChildStdinWr);

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&siStartupInfo, sizeof(STARTUPINFO));
	siStartupInfo.cb = sizeof(STARTUPINFO);

	if (args)
	{
		args[-1] = ' ';
	}

	if (!CreateProcess(NULL, name, NULL, NULL, 1, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &siStartupInfo, &piProcInfo))
	{
		draw_info(COLOR_RED, "script_load(): CreateProcess() failed.");
		return;
	}

	CloseHandle(piProcInfo.hThread);

	if (args)
	{
		args[-1] = '\0';
	}

	if (!SetStdHandle(STD_INPUT_HANDLE, hSaveStdin))
	{
		draw_info(COLOR_RED, "script_load(): Restoring original stdin failed.");
		return;
	}

	if (!SetStdHandle(STD_OUTPUT_HANDLE, hSaveStdout))
	{
		draw_info(COLOR_RED, "script_load(): Restoring original stdout failed.");
		return;
	}
#endif

	/* realloc script array to add new entry; fill in the data */
	scripts = realloc(scripts, sizeof(scripts[0]) * (num_scripts + 1));
	scripts[num_scripts].name = strdup(cparams);
	scripts[num_scripts].params = args ? strdup(args) : NULL;

#ifndef WIN32
	scripts[num_scripts].out_fd = fds[1];
	scripts[num_scripts].in_fd = fds[1];
	scripts[num_scripts].pid = pid;
#else
	scripts[num_scripts].out_fd = hChildStdinWrDup;
	scripts[num_scripts].in_fd = hChildStdoutRdDup;
	scripts[num_scripts].pid = piProcInfo.dwProcessId;
	scripts[num_scripts].process = piProcInfo.hProcess;
#endif
	scripts[num_scripts].cmd_count = 0;
	scripts[num_scripts].events = NULL;
	scripts[num_scripts].events_count = 0;

	num_scripts++;
}

/** Print currently running scripts. */
void script_list(void)
{
	if (num_scripts == 0)
	{
		draw_info(COLOR_WHITE, "No scripts are currently running.");
	}
	else
	{
		int i;

		draw_info_format(COLOR_WHITE, "%d scripts are currently running:", num_scripts);

		for (i = 0; i < num_scripts; i++)
		{
			if (scripts[i].params)
			{
				draw_info_format(COLOR_WHITE, "%d %s  %s", i + 1, scripts[i].name, scripts[i].params);
			}
			else
			{
				draw_info_format(COLOR_WHITE, "%d %s", i + 1, scripts[i].name);
			}
		}
	}
}

/**
 * Process loaded scripts. */
void script_process(void)
{
	int i, r;
#ifdef WIN32
	DWORD nAvailBytes = 0, dwStatus;
	char cTmp;
	BOOL bRC, bStatus;
#else
	fd_set tmp_read;
	int pollret;
	struct timeval timeout;
#endif

	/* Determine which script's fd is set */
	for (i = 0; i < num_scripts; i++)
	{
#ifndef WIN32
		FD_ZERO(&tmp_read);
		FD_SET(scripts[i].in_fd, &tmp_read);

		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		if ((pollret = select(scripts[i].in_fd + 1, &tmp_read, NULL, NULL, &timeout)) == -1)
		{
			LOG(llevDebug, "Got errno %d on select call: %s.\n", errno, strerror(errno));
		}

		if (FD_ISSET(scripts[i].in_fd, &tmp_read))
#else
		bStatus = GetExitCodeProcess(scripts[i].process, &dwStatus);
		bRC = PeekNamedPipe(scripts[i].in_fd, &cTmp, 1, NULL, &nAvailBytes, NULL);

		if (nAvailBytes)
#endif
		{
			/* Read in script[i].cmd */
			r = read(scripts[i].in_fd, scripts[i].cmd + scripts[i].cmd_count, sizeof(scripts[i].cmd) - scripts[i].cmd_count - 1);

			if (r > 0)
			{
				scripts[i].cmd_count += r;
			}
#ifndef WIN32
			else if (r == 0 || errno == EBADF)
#else
			else if (r == 0 || GetLastError() == ERROR_BROKEN_PIPE)
#endif
			{
				/* Script has exited; delete it */
				script_dead(i);
				return;
			}

			scripts[i].cmd[scripts[i].cmd_count] = '\0';

			/* If a newline or full buffer has been reached, process it */
			while (scripts[i].cmd_count == sizeof(scripts[i].cmd) - 1 || strchr(scripts[i].cmd, '\n'))
			{
				script_process_cmd(i);
				scripts[i].cmd[scripts[i].cmd_count] = '\0';
			}

			/* Only process one script at a time */
			return;
		}
#ifdef WIN32
		/* Error: assume dead */
		else if (!bRC || (bStatus && (dwStatus != STILL_ACTIVE)))
		{
			script_dead(i);
		}
#endif
	}
}

/**
 * Process script command.
 * @param i ID of the script. */
static void script_process_cmd(int i)
{
	char cmd[HUGE_BUF], *c;
	int l;

	/* Strip out just this one command */
	for (l = 0; l < scripts[i].cmd_count; l++)
	{
		if (scripts[i].cmd[l] == '\n')
		{
			break;
		}
	}

	l++;
	memcpy(cmd, scripts[i].cmd, l);

#ifndef WIN32
	cmd[l - 1] = '\0';
#else
	cmd[l - 2] = '\0';
#endif

	if (l < scripts[i].cmd_count)
	{
		memmove(scripts[i].cmd, scripts[i].cmd + l, scripts[i].cmd_count - l);
		scripts[i].cmd_count -= l;
	}
	else
	{
		scripts[i].cmd_count = 0;
	}

	/* Now the data in scripts[i] is ready for the next read.
	 * We have a complete command in cmd[].
	 * Process it. */
	if (!strncmp(cmd, "draw ", 5))
	{
		char color[COLOR_BUF];
		const char *cp = cmd;

		cp += 5;

		snprintf(color, sizeof(color), "%s", cp);
		color[sizeof(color) - 1] = '\0';
		cp += 7;

		draw_info(color, cp);
	}
	else if (!strncmp(cmd, "log ", 4))
	{
		int log_level;

		c = cmd + 4;

		while (*c && !isdigit(*c))
		{
			c++;
		}

		/* No log level specified */
		if (!*c)
		{
			LOG(llevBug, "script_process_cmd(): Log command did not have log level set.\n");
			return;
		}

		log_level = atoi(c);

		while (*c && *c != ' ')
		{
			c++;
		}

		/* No log message specified */
		if (!*c)
		{
			LOG(llevBug, "script_process_cmd(): Log command did not have log message set.\n");
			return;
		}

		while (*c == ' ')
		{
			c++;
		}

		LOG(log_level, "%s\n", c);
	}
	else if (!strncmp(cmd, "request ", 8))
	{
		char buf[MAX_BUF];
		int w = 0;

		c = cmd + 8;

		if (!strncmp(c, "player", 6))
		{
			snprintf(buf, sizeof(buf), "request player %d %s:%s\n", cpl.ob->tag, cpl.name, cpl.ext_title);
			w = write(scripts[i].out_fd, buf, strlen(buf));
		}
		else if (!strncmp(c, "weight", 5))
		{
			snprintf(buf, sizeof(buf), "request weight %d %d %d\n", cpl.weight_limit, (int) (cpl.ob->weight * 1000), (int) (cpl.real_weight * 1000));
			w = write(scripts[i].out_fd, buf, strlen(buf));
		}
		else if (!strncmp(c, "stat ", 5))
		{
			c += 5;

			if (!strncmp(c, "stats", 5))
			{
				snprintf(buf, sizeof(buf), "request stat stats %d %d %d %d %d %d %d\n", cpl.stats.Str, cpl.stats.Dex, cpl.stats.Con, cpl.stats.Int, cpl.stats.Wis, cpl.stats.Pow, cpl.stats.Cha);
				w = write(scripts[i].out_fd, buf, strlen(buf));
			}
			else if (!strncmp(c, "combat", 6))
			{
				snprintf(buf, sizeof(buf), "request stat combat %d %d %d %d %d\n", cpl.stats.wc, cpl.stats.ac, cpl.stats.dam, cpl.stats.speed, cpl.stats.weapon_sp);
				w = write(scripts[i].out_fd, buf, strlen(buf));
			}
			else if (!strncmp(c, "hp", 2))
			{
				snprintf(buf, sizeof(buf), "request stat hp %d %d %d %d %d %d %d\n", cpl.stats.hp, cpl.stats.maxhp, cpl.stats.sp, cpl.stats.maxsp, cpl.stats.grace, cpl.stats.maxgrace, cpl.stats.food);
				w = write(scripts[i].out_fd,buf,strlen(buf));
			}
			else if (!strncmp(c, "exp", 3))
			{
				int s;

				snprintf(buf, sizeof(buf), "request stat exp %d %"FMT64, cpl.stats.level, cpl.stats.exp);
				w = write(scripts[i].out_fd, buf, strlen(buf));

				for (s = 0; s < MAX_SKILL; s++)
				{
					snprintf(buf, sizeof(buf), " %d %"FMT64, cpl.stats.skill_level[s], cpl.stats.skill_exp[s]);
					w = write(scripts[i].out_fd, buf, strlen(buf));
				}

				w = write(scripts[i].out_fd, "\n", 1);
			}
			else if (!strncmp(c, "protections", 11))
			{
				int s;

				snprintf(buf, sizeof(buf), "request stat protections");
				w = write(scripts[i].out_fd, buf, strlen(buf));

				for (s = CS_STAT_PROT_START; s <= CS_STAT_PROT_END; s++)
				{
					snprintf(buf, sizeof(buf), " %d", cpl.stats.protection[s - CS_STAT_PROT_START]);
					w = write(scripts[i].out_fd, buf, strlen(buf));
				}

				w = write(scripts[i].out_fd, "\n", 1);
			}
		}
		else if (!strncmp(c, "items ", 6))
		{
			c += 6;

			if (!strncmp(c, "inv", 3))
			{
				object *it = cpl.ob->inv;

				while (it)
				{
					script_send_item(i, "request items inv ", it);
					it = it->next;
				}

				strcpy(buf, "request items inv end\n");
				w = write(scripts[i].out_fd, buf, strlen(buf));
			}
			else if (!strncmp(c, "applied", 7))
			{
				object *it = cpl.ob->inv;

				while (it)
				{
					if (it->flags & F_APPLIED)
					{
						script_send_item(i, "request items applied ", it);
					}

					it = it->next;
				}

				strcpy(buf, "request items applied end\n");
				w = write(scripts[i].out_fd, buf, strlen(buf));
			}
			else if (!strncmp(c, "below", 5))
			{
				object *it = cpl.below->inv;

				while (it)
				{
					script_send_item(i, "request items below ", it);
					it = it->next;
				}

				strcpy(buf, "request items below end\n");
				w = write(scripts[i].out_fd, buf, strlen(buf));
			}
		}
		else
		{
			draw_info_format(COLOR_RED, "Script %d %s malfunction; unimplemented request: %s", i + 1, scripts[i].name, cmd);
		}

		if (w < 0)
		{
			LOG(llevBug, "script_process_cmd(): Write system call failed.\n");
		}
	}
	else if (!strncmp(cmd, "issue ", 6))
	{
		c = cmd + 6;

		if (!strncmp(c, "command ", 8))
		{
			c += 8;

			if (!client_command_check(c))
			{
				send_command(c);
			}
		}
		else if (!strncmp(c, "string ", 7))
		{
			c += 7;

			cs_write_string(c, strlen(c));
		}
	}
	else if (!strncmp(cmd, "event ", 6))
	{
		c = cmd + 6;

		if (!strncmp(c, "register ", 9))
		{
			c += 9;

			c = strdup(c);
			scripts[i].events = realloc(scripts[i].events, (scripts[i].events_count + 1) * sizeof(scripts[i].events[1]));
			scripts[i].events[scripts[i].events_count] = c;
			scripts[i].events_count++;
		}
		else if (!strncmp(c, "unregister ", 11))
		{
			int j;

			c += 11;

			for (j = 0; j < scripts[i].events_count; j++)
			{
				if (strcmp(c, scripts[i].events[j]) == 0 )
				{
					free(scripts[i].events[j]);

					while (j + 1 < scripts[i].events_count)
					{
						scripts[i].events[j] = scripts[i].events[j + 1];
						j++;
					}

					scripts[i].events_count--;

					break;
				}
			}
		}
	}
	else
	{
		draw_info_format(COLOR_RED, "Script %d %s malfunction; invalid command: %s", i + 1, scripts[i].name, cmd);
	}
}

/**
 * Send information about an item to script.
 * @param i ID of the script.
 * @param head What to prefix the information with.
 * @param it The item. */
static void script_send_item(int i, const char *head, const object *it)
{
	char buf[HUGE_BUF];
	int w;

	snprintf(buf, sizeof(buf), "%s%d %d %f %d %d %s\n", head, it->tag, it->nrof, it->weight, it->flags, it->itype, it->s_name);
	w = write(scripts[i].out_fd, buf, strlen(buf));

	if (w < 0)
	{
		LOG(llevBug, "script_send_item(): Write system call failed.\n");
	}
}

/**
 * Trigger a script event.
 * @param event_id ID of the event.
 * @param void_data Data.
 * @param data_len Length of the data.
 * @return Always returns 0. */
int script_trigger_event(const char *cmd, const uint8 *data, const int data_len, const enum CmdFormat format)
{
	int i, e, w, len;

	/* For each script... */
	for (i = 0; i < num_scripts; ++i)
	{
		/* For each registered event */
		for (e = 0; e < scripts[i].events_count; e++)
		{
			char buf[10240];

			if (strcmp(cmd, scripts[i].events[e]))
			{
				continue;
			}

			len = data_len;

			switch (format)
			{
				case ASCII:
					snprintf(buf, sizeof(buf), "event %s %s\n", cmd, data);
					break;

				case SHORT_INT:
					snprintf(buf, sizeof(buf), "event %s %d %d\n", cmd, GetShort_String(data), GetInt_String(data + 2));
					break;

				case SHORT_ARRAY:
				{
					int be, p;

					be = snprintf(buf, sizeof(buf), "event %s", cmd);

					for (p = 0; p * 2 < len && p < 100; ++p)
					{
						be += snprintf(buf + be, sizeof(buf) - be, " %d", GetShort_String(data + p * 2));
					}

					be += snprintf(buf + be, sizeof(buf) - be, "\n");
					break;
				}

				case INT_ARRAY:
				{
					int be, p;

					be = snprintf(buf, sizeof(buf), "event %s", cmd);

					for (p = 0; p * 4 < len; ++p)
					{
						be += snprintf(buf + be, sizeof(buf) - be, " %d", GetInt_String(data + p * 4));
					}

					be += snprintf(buf + be, sizeof(buf) - be, "\n");
					break;
				}

				case STATS:
				{
					int j = 0, c, be = 0;

					while (j < len)
					{
						c = data[j++];

						be += snprintf(buf + be, sizeof(buf) - be, "event %s", cmd);

						if (c >= CS_STAT_PROT_START && c <= CS_STAT_PROT_END)
						{
							be += snprintf(buf + be, sizeof(buf) - be, " protects %d %d\n", c - CS_STAT_PROT_START, (sint16) *(data + j++));
						}
						else
						{
							switch (c)
							{
								case CS_STAT_TARGET_HP:
									be += snprintf(buf + be, sizeof(buf) - be, " target_hp %d\n", (int)*(data + j++));
									cpl.target_hp = (int)*(data + j++);
									break;

								case CS_STAT_REG_HP:
									be += snprintf(buf + be, sizeof(buf) - be, " regen_hp %f\n", GetShort_String(data + j) / 10.0f);
									j += 2;
									break;

								case CS_STAT_REG_MANA:
									be += snprintf(buf + be, sizeof(buf) - be, " regen_mana %f\n", GetShort_String(data + j) / 10.0f);
									j += 2;
									break;

								case CS_STAT_REG_GRACE:
									be += snprintf(buf + be, sizeof(buf) - be, " regen_grace %f\n", GetShort_String(data + j) / 10.0f);
									j += 2;
									break;

								case CS_STAT_HP:
									be += snprintf(buf + be, sizeof(buf) - be, " hp %d\n", GetInt_String(data + j));
									j += 4;
									break;

								case CS_STAT_MAXHP:
									be += snprintf(buf + be, sizeof(buf) - be, " maxhp %d\n", GetInt_String(data + j));
									j += 4;
									break;

								case CS_STAT_SP:
									be += snprintf(buf + be, sizeof(buf) - be, " sp %d\n", GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_MAXSP:
									be += snprintf(buf + be, sizeof(buf) - be, " maxsp %d\n", GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_GRACE:
									be += snprintf(buf + be, sizeof(buf) - be, " grace %d\n", GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_MAXGRACE:
									be += snprintf(buf + be, sizeof(buf) - be, " maxgrace %d\n", GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_STR:
									be += snprintf(buf + be, sizeof(buf) - be, " str %d\n", (int) *(data + j++));
									break;

								case CS_STAT_INT:
									be += snprintf(buf + be, sizeof(buf) - be, " int %d\n", (int) *(data + j++));
									break;

								case CS_STAT_POW:
									be += snprintf(buf + be, sizeof(buf) - be, " pow %d\n", (int) *(data + j++));
									break;

								case CS_STAT_WIS:
									be += snprintf(buf + be, sizeof(buf) - be, " wis %d\n", (int) *(data + j++));
									break;

								case CS_STAT_DEX:
									be += snprintf(buf + be, sizeof(buf) - be, " dex %d\n", (int) *(data + j++));
									break;

								case CS_STAT_CON:
									be += snprintf(buf + be, sizeof(buf) - be, " con %d\n", (int) *(data + j++));
									break;

								case CS_STAT_CHA:
									be += snprintf(buf + be, sizeof(buf) - be, " cha %d\n", (int) *(data + j++));
									break;

								case CS_STAT_EXP:
									be += snprintf(buf + be, sizeof(buf) - be, " exp %d\n", GetInt_String(data + j));
									j += 4;
									break;

								case CS_STAT_LEVEL:
									be += snprintf(buf + be, sizeof(buf) - be, " level %d\n", (char) *(data + j++));
									break;

								case CS_STAT_WC:
									be += snprintf(buf + be, sizeof(buf) - be, " wc %d\n", (char) GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_AC:
									be += snprintf(buf + be, sizeof(buf) - be, " ac %d\n", (char) GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_DAM:
									be += snprintf(buf + be, sizeof(buf) - be, " dam %d\n", GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_SPEED:
									be += snprintf(buf + be, sizeof(buf) - be, " speed %d\n", GetInt_String(data + j));
									j += 4;
									break;

								case CS_STAT_FOOD:
									be += snprintf(buf + be, sizeof(buf) - be, " food %d\n", GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_WEAP_SP:
									be += snprintf(buf + be, sizeof(buf) - be, " weapon_speed %d\n", (int)*(data + j++));
									break;

								case CS_STAT_FLAGS:
									be += snprintf(buf + be, sizeof(buf) - be, " flags %d\n", GetShort_String(data + j));
									j += 2;
									break;

								case CS_STAT_WEIGHT_LIM:
									be += snprintf(buf + be, sizeof(buf) - be, " weight_limit %d\n", GetInt_String(data + j));
									j += 4;
									break;

								case CS_STAT_ACTION_TIME:
									be += snprintf(buf + be, sizeof(buf) - be, " action_time %f\n", abs(GetInt_String(data + j)) / 1000.0f);
									j += 4;
									break;

								case CS_STAT_SKILLEXP_AGILITY:
								case CS_STAT_SKILLEXP_PERSONAL:
								case CS_STAT_SKILLEXP_MENTAL:
								case CS_STAT_SKILLEXP_PHYSIQUE:
								case CS_STAT_SKILLEXP_MAGIC:
								case CS_STAT_SKILLEXP_WISDOM:
									be += snprintf(buf + be, sizeof(buf) - be, " skill_exp %d %"FMT64"\n", (c - CS_STAT_SKILLEXP_START) / 2, GetInt64_String(data + j));
									j += 8;
									break;

								case CS_STAT_SKILLEXP_AGLEVEL:
								case CS_STAT_SKILLEXP_PELEVEL:
								case CS_STAT_SKILLEXP_MELEVEL:
								case CS_STAT_SKILLEXP_PHLEVEL:
								case CS_STAT_SKILLEXP_MALEVEL:
								case CS_STAT_SKILLEXP_WILEVEL:
									be += snprintf(buf + be, sizeof(buf) - be, " skill_level %d %d\n", (c - CS_STAT_SKILLEXP_START - 1) / 2, (sint16)*(data + j++));
									break;

								case CS_STAT_EXT_TITLE:
								{
									int rlen = data[j++];

									be += snprintf(buf + be, sizeof(buf) - be, " ext_title %s\n", cpl.ext_title);
									j += rlen;
									break;
								}

								default:
									j = len;
									break;
							}
						}
					}

					break;
				}

				case MIXED:
				case NODATA:
				default:
				{
					int be, p;

					/* We may receive null data, in which case len has no meaning */
					if (!data)
					{
						len = 0;
					}

					be = snprintf(buf, sizeof(buf), "event %s %d bytes unparsed:", cmd, len);

					for (p = 0; p < len && p < 100; ++p)
					{
						be += snprintf(buf + be, sizeof(buf) - be, " %02x", data[p]);
					}

					be += snprintf(buf + be, sizeof(buf) - be, "\n");
					break;
				}
			}

			w = write(scripts[i].out_fd, buf, strlen(buf));

			if (w < 0)
			{
				LOG(llevBug, "script_trigger_event(): Write system call failed.\n");
			}
		}
	}

	return 0;
}

/**
 * Send a message by player to a running script.
 * @param params The script name to find and send the message to. */
void script_send(const char *params)
{
	int i = 0, w;
	char *p;

	if (!params)
	{
		return;
	}

	p = strchr(params, ' ');

	if (!p)
	{
		draw_info(COLOR_RED, "No message to send specified.");
		return;
	}

	while (*p == ' ')
	{
		*p++ = '\0';
	}

	i = script_by_name(params);

	if (i < 0)
	{
		draw_info(COLOR_RED, "No such running script.");
		return;
	}

	/* Send the message */
	w = write(scripts[i].out_fd, "scriptsend ", 11);
	w = write(scripts[i].out_fd, p, strlen(p));
	w = write(scripts[i].out_fd, "\n", 1);

	if (w < 0)
	{
		LOG(llevBug, "script_send(): Write system call failed.\n");
	}
}

/**
 * Kill all loaded scripts. Used by Win32 build of the client
 * when exiting the client. */
void script_killall(void)
{
#ifdef WIN32
	while (num_scripts > 0)
	{
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, scripts[0].pid);
		script_dead(0);
	}
#endif
}

/**
 * Free resources used by a script, and remove it from the @ref scripts
 * list.
 * @param i ID of the script to remove. */
static void script_dead(int i)
{
	/* Release resources */
#ifndef WIN32
	close(scripts[i].in_fd);
	close(scripts[i].out_fd);
#else
	CloseHandle(scripts[i].in_fd);
	CloseHandle(scripts[i].out_fd);
	CloseHandle(scripts[i].process);
#endif

	free(scripts[i].name);
	free(scripts[i].params);

#ifndef WIN32
	waitpid(-1, NULL, WNOHANG);
#endif

	/* Move scripts with higher index numbers down one slot */
	if (i < (num_scripts - 1))
	{
		memmove(&scripts[i], &scripts[i + 1], sizeof(scripts[i]) * (num_scripts - i - 1));
	}

	/* Update our count */
	num_scripts--;
}

/**
 * Find a script by name.
 * @param name Name of the script. Can be numeric in which case it is
 * compared against the number of loaded scripts.
 * @return ID of the script, -1 if none found. */
static int script_by_name(const char *name)
{
	int i, l = 0;

	if (!name)
	{
		return (num_scripts == 1 ? 0 : -1);
	}

	/* Parse script number */
	if (isdigit(*name))
	{
		i = atoi(name);
		i--;

		if (i >= 0 && i < num_scripts)
		{
			return i;
		}
	}

	while (name[l] && name[l] != ' ')
	{
		l++;
	}

	for (i = 0; i < num_scripts; i++)
	{
		if (strncmp(name, scripts[i].name, l) == 0)
		{
			return i;
		}
	}

	return -1;
}

/**
 * Automatically load selected scripts on startup of the client, read
 * from file. */
void script_autoload(void)
{
	FILE *fp;
	char line[MAX_BUF];

	if (!(fp = fopen_wrapper(SCRIPTS_AUTOLOAD, "r+")))
	{
		LOG(llevInfo, "Can't find file %s. Will not load any scripts.\n", SCRIPTS_AUTOLOAD);
		return;
	}

	while (fgets(line, sizeof(line) - 1, fp))
	{
		if (line[0] == '#' || line[0] == '\n')
		{
			continue;
		}

#ifndef WIN32
		line[strlen(line) - 1] = '\0';
#else
		line[strlen(line) - 2] = '\0';
#endif

		script_load(line);
	}
}

/**
 * Unload a script.
 * @param params Script name or ID. Uses script_by_name() to find the
 * real script ID. */
void script_unload(const char *params)
{
	int i = script_by_name(params);

	if (i < 0 || i >= num_scripts)
	{
		draw_info(COLOR_RED, "No such running script.");
		return;
	}

#ifndef WIN32
	kill(scripts[i].pid, SIGHUP);
#else
	GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, scripts[i].pid);
#endif

	draw_info_format(COLOR_GREEN, "Unloaded script #%d.", i + 1);
	script_dead(i);
}
