/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 *  */

#include <include.h>

/* We get TEXT from keyboard. This is for console input */
void key_string_event(SDL_KeyboardEvent *key)
{
	char c;
	int i;

	if (key->type == SDL_KEYDOWN)
	{
		switch (key->keysym.sym)
		{
			case SDLK_ESCAPE:
				SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
				InputStringEscFlag = 1;
				return;

			case SDLK_KP_ENTER:
			case SDLK_RETURN:
			case SDLK_TAB:
				if (key->keysym.sym != SDLK_TAB || GameStatus < GAME_STATUS_WAITFORPLAY)
				{
					SDL_EnableKeyRepeat(0 , SDL_DEFAULT_REPEAT_INTERVAL);
					InputStringFlag = 0;
					/* Mark that we've got something here */
					InputStringEndFlag = 1;

					/* record this line in input history only if we are in console mode */
					if (cpl.input_mode == INPUT_MODE_CONSOLE)
						textwin_addhistory(InputString);
				}
				else if (key->keysym.sym == SDLK_TAB)
				{
					help_files_struct *help_files_tmp;
					int possibilities = 0;
					char cmd_buf[MAX_BUF];

					if (InputString[0] != '/' || strrchr(InputString, ' '))
					{
						break;
					}

					for (help_files_tmp = help_files; help_files_tmp; help_files_tmp = help_files_tmp->next)
					{
						if (strcmp(help_files_tmp->title + strlen(help_files_tmp->title) - 8, " Command"))
						{
							continue;
						}

						if (!strncmp(help_files_tmp->helpname, InputString + 1, InputCount - 1))
						{
							if ((help_files_tmp->dm_only && !cpl.dm) || !help_files_tmp->autocomplete)
							{
								continue;
							}

							if (possibilities == 0)
							{
								strncpy(cmd_buf, help_files_tmp->helpname, sizeof(cmd_buf));
							}
							else
							{
								if (possibilities == 1)
								{
									draw_info_format(COLOR_WHITE, "\nMatching commands:\n%s", cmd_buf);
								}

								draw_info(help_files_tmp->helpname, COLOR_WHITE);
							}

							possibilities++;
						}
					}

					if (possibilities == 1)
					{
						snprintf(InputString, sizeof(InputString), "/%s ", cmd_buf);
						InputCount = CurrentCursorPos = (int) strlen(InputString);
					}
				}

				break;

				/* Erases the previous character or word if CTRL is pressed */
			case SDLK_BACKSPACE:
				if (InputCount && CurrentCursorPos)
				{
					int ii;

					/* Actual position of the cursor */
					ii = CurrentCursorPos;
					/* Where we will end up, by default one character back */
					i = ii - 1;

					if (key->keysym.mod & KMOD_CTRL)
					{
						/* Jumps eventual whitespaces */
						while (InputString[i] == ' ' && i >= 0)
							i--;

						/* Jumps a word */
						while (InputString[i] != ' ' && i >= 0)
							i--;

						/* we end up at the beginning of the current word */
						i++;
					}

					/* This loop copies even the terminating \0 of the buffer */
					while (ii <= InputCount)
						InputString[i++] = InputString[ii++];

					CurrentCursorPos -= (ii - i);
					InputCount -= (ii - i);
				}
				break;

				/* Shifts a character or a word if CTRL is pressed */
			case SDLK_LEFT:
				if (key->keysym.mod & KMOD_CTRL)
				{
					i = CurrentCursorPos - 1;

					/* Jumps eventual whitespaces */
					while (InputString[i] == ' ' && i >= 0)
						i--;

					/* Jumps a word */
					while (InputString[i] != ' ' && i >= 0)
						i--;

					/* Places the cursor on the first letter of this word */
					CurrentCursorPos = i + 1;
				}
				else if (CurrentCursorPos > 0)
					CurrentCursorPos--;

				break;

				/* Shifts a character or a word if CTRL is pressed */
			case SDLK_RIGHT:
				if (key->keysym.mod & KMOD_CTRL)
				{
					i = CurrentCursorPos;

					/* Jumps eventual whitespaces */
					while (InputString[i] == ' ' && i < InputCount)
						i++;

					/* Jumps a word */
					while (InputString[i] != ' ' && i < InputCount)
						i++;

					/* Places the cursor right after the jumped word */
					CurrentCursorPos = i;
				}
				else if (CurrentCursorPos < InputCount)
					CurrentCursorPos++;

				break;

				/* If we are in CONSOLE mode, let player scroll back the lines in history */
			case SDLK_UP:
				if (cpl.input_mode == INPUT_MODE_CONSOLE && HistoryPos < MAX_HISTORY_LINES && InputHistory[HistoryPos + 1][0])
				{
					/* First history line is special, it records what we were writing before
					 * scrolling back the history; so, by returning back to zero, we can continue
					 * our editing where we left it. */
					if (HistoryPos == 0)
						strncpy(InputHistory[0], InputString, InputCount);

					HistoryPos++;
					textwin_putstring(InputHistory[HistoryPos]);
				}

				break;

				/* If we are in CONSOLE mode, let player scroll forward the lines in history */
			case SDLK_DOWN:
				if (cpl.input_mode == INPUT_MODE_CONSOLE && HistoryPos > 0)
				{
					HistoryPos--;
					textwin_putstring(InputHistory[HistoryPos]);
				}

				break;

			case SDLK_DELETE:
			{
				int ii;

				/* Actual position of the cursor */
				ii = CurrentCursorPos;

				/* Where we will end up, by default one character ahead */
				i = ii + 1;

				if (ii == InputCount)
					break;

				if (key->keysym.mod & KMOD_CTRL)
				{
					/* Jumps eventual whitespaces */
					while (InputString[i] == ' ' && i < InputCount)
						i++;

					/* Jumps a word */
					while (InputString[i] != ' ' && i < InputCount)
						i++;
				}

				/* This loop copies even the terminating \0 of the buffer */
				while (i <= InputCount)
					InputString[ii++] = InputString[i++];

				InputCount -= (i - ii);

				break;
			}

			case SDLK_HOME:
				CurrentCursorPos = 0;
				break;

			case SDLK_END:
				CurrentCursorPos = InputCount;
				break;

			default:
				/* If we are in number console mode, use GET as quick enter
				 * mode - this is a very handy shortcut */
				if (cpl.input_mode == INPUT_MODE_NUMBER && ((int) key->keysym.sym == get_action_keycode || (int) key->keysym.sym == drop_action_keycode))
				{
					SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
					InputStringFlag = 0;
					/* Mark that we got some text here */
					InputStringEndFlag = 1;
				}

				/* Now keyboard magic - transform a sym (kind of scancode)
				 * to a layout code */
				if (InputCount < InputMax)
				{
					c = 0;

					/* We want only numbers in number mode - even when shift is held */
					if (cpl.input_mode == INPUT_MODE_NUMBER)
					{
						switch (key->keysym.sym)
						{
							case SDLK_0:
							case SDLK_KP0:
								c = '0';
								break;

							case SDLK_KP1:
							case SDLK_1:
								c = '1';
								break;

							case SDLK_KP2:
							case SDLK_2:
								c = '2';
								break;

							case SDLK_KP3:
							case SDLK_3:
								c = '3';
								break;

							case SDLK_KP4:
							case SDLK_4:
								c = '4';
								break;

							case SDLK_KP5:
							case SDLK_5:
								c = '5';
								break;

							case SDLK_KP6:
							case SDLK_6:
								c = '6';
								break;

							case SDLK_KP7:
							case SDLK_7:
								c = '7';
								break;

							case SDLK_KP8:
							case SDLK_8:
								c = '8';
								break;

							case SDLK_KP9:
							case SDLK_9:
								c = '9';
								break;

							default:
								c = 0;
								break;
						}

						if (c)
						{
							InputString[CurrentCursorPos++] = c;
							InputCount++;
							InputString[InputCount] = 0;
						}
					}
					else
					{
						if ((key->keysym.unicode & 0xFF80) == 0)
							c = key->keysym.unicode & 0x7F;

						c = key->keysym.unicode & 0xff;

						if (c >= 32)
						{
							if (key->keysym.mod & KMOD_SHIFT)
								c = toupper(c);

							i = InputCount;

							while (i >= CurrentCursorPos)
							{
								InputString[i + 1] = InputString[i];
								i--;
							}

							InputString[CurrentCursorPos] = c;
							CurrentCursorPos++;
							InputCount++;
							InputString[InputCount] = 0;
						}
					}
				}
				break;
		}
	}
}

/**
 * Handle mouse hold events in number input widget. */
void mouse_InputNumber()
{
	static int timeVal = 1;
	int x, y;

	if (!(SDL_GetMouseState(&x, &y) & SDL_BUTTON(SDL_BUTTON_LEFT)))
	{
		timeVal = 1;
		return;
	}

	x = x - cur_widget[IN_NUMBER_ID]->x1;
	y = y - cur_widget[IN_NUMBER_ID]->y1;

	if (x < 230 || x > 237 || y < 5)
	{
		return;
	}

	/* Plus */
	if (y > 13)
	{
		x = atoi(InputString) + timeVal;

		if (x > cpl.nrof)
		{
			x = cpl.nrof;
		}
	}
	/* Minus */
	else
	{
		x = atoi(InputString) - timeVal;

		if (x < 1)
		{
			x = 1;
		}
	}

	snprintf(InputString, sizeof(InputString), "%d", x);
	InputCount = (int) strlen(InputString);
	timeVal += (timeVal / 8) + 1;
}
