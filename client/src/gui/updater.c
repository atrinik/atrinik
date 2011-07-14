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

#include <global.h>

#define UPDATER_CHECK_URL "http://www.atrinik.org/page/client_update"
#define UPDATER_PATH_URL "http://www.atrinik.org/cms/uploads"

static curl_data *dl_data = NULL;
static char **download_packages_file;
static char **download_packages_sha1;
static size_t download_packages_num = 0;
static size_t download_package_next = 0;
static size_t download_packages_downloaded = 0;
static uint8 download_package_process = 0;
static progress_dots progress;

static char *updater_get_dir(char *buf, size_t len)
{
	snprintf(buf, len, "%s/.atrinik/temp", get_config_dir());
	return buf;
}

static void cleanup_patch_files()
{
	char dir_path[HUGE_BUF];

	rmrf(updater_get_dir(dir_path, sizeof(dir_path)));
}

static void popup_draw_func_post(popup_struct *popup)
{
	SDL_Rect box;

	box.x = popup->x;
	box.y = popup->y + 10;
	box.w = popup->surface->w;
	box.h = popup->surface->h;

	string_blt(ScreenSurface, FONT_SERIF20, "<u>Settings</u>", box.x, box.y + 10, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_MARKUP, &box);
	box.y += 50;

	progress_dots_show(&progress, ScreenSurface, box.x + box.w / 2 - progress_dots_width(&progress) / 2, box.y);
	box.y += 30;

	if (!progress.done && dl_data)
	{
		if (!strncmp(dl_data->url, UPDATER_CHECK_URL, strlen(UPDATER_CHECK_URL)))
		{
			string_blt_shadow(ScreenSurface, FONT_ARIAL11, "Downloading list of updates...", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
		}
		else
		{
			string_blt_shadow_format(ScreenSurface, FONT_ARIAL11, box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box, "Downloading update #%"FMT64" out of %"FMT64"...", download_package_next, download_packages_num);
		}
	}

	if (dl_data)
	{
		sint8 ret = curl_download_finished(dl_data);

		progress.done = 0;

		if (ret == -1)
		{
			cleanup_patch_files();
			progress.done = 1;

			string_blt_shadow(ScreenSurface, FONT_ARIAL11, "Connection timed out.", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

			box.y += 20;

			if (button_show(BITMAP_BUTTON, -1, BITMAP_BUTTON_DOWN, box.x + box.w / 2 - Bitmaps[BITMAP_BUTTON]->bitmap->w / 2, box.y, "Retry", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
			{
				popup_destroy_visible();
				updater_open();
				return;
			}
		}
		else if (ret == 1)
		{
			if (!strncmp(dl_data->url, UPDATER_CHECK_URL, strlen(UPDATER_CHECK_URL)))
			{
				if (dl_data->memory)
				{
					char *cp, *line, *tmp[2];

					cp = strdup(dl_data->memory);

					line = strtok(cp, "\n");

					while (line)
					{
						if (split_string(line, tmp, arraysize(tmp), '\t') == 2)
						{
							download_packages_file = realloc(download_packages_file, sizeof(*download_packages_file) * (download_packages_num + 1));
							download_packages_sha1 = realloc(download_packages_sha1, sizeof(*download_packages_sha1) * (download_packages_num + 1));
							download_packages_file[download_packages_num] = strdup(tmp[0]);
							download_packages_sha1[download_packages_num] = strdup(tmp[1]);
							download_packages_num++;
						}

						line = strtok(NULL, "\n");
					}

					free(cp);
				}

#ifdef WIN32
				if (download_packages_num)
				{
					download_package_process = 1;
					download_package_next = 0;
				}
#endif

				curl_data_free(dl_data);
				dl_data = NULL;
			}

			if (download_package_process)
			{
				if (download_package_next != 0 && dl_data)
				{
					char sha1_output_ascii[41];
					unsigned char sha1_output[20];
					size_t i;

					sha1((unsigned char *) dl_data->memory, dl_data->size, sha1_output);

					for (i = 0; i < 20; i++)
					{
						sprintf(sha1_output_ascii + i * 2, "%02x", sha1_output[i]);
					}

					if (!strcmp(download_packages_sha1[download_package_next - 1], sha1_output_ascii))
					{
						char dir_path[HUGE_BUF], filename[HUGE_BUF];
						FILE *fp;

						updater_get_dir(dir_path, sizeof(dir_path));

						if (access(dir_path, R_OK) != 0)
						{
							mkdir(dir_path, 0755);
						}

						snprintf(filename, sizeof(filename), "%s/client_patch_%09"FMT64".tar.gz", dir_path, (sint64) download_package_next - 1);
						fp = fopen(filename, "wb");

						if (fp)
						{
							fwrite(dl_data->memory, 1, dl_data->size, fp);
							fclose(fp);
							download_packages_downloaded++;
						}
					}
					else
					{
						download_package_next = download_packages_num;
					}

					curl_data_free(dl_data);
					dl_data = NULL;
				}

				if (download_package_next < download_packages_num)
				{
					char url[HUGE_BUF];

					snprintf(url, sizeof(url), UPDATER_PATH_URL"/%s", download_packages_file[download_package_next]);
					dl_data = curl_download_start(url);
					download_package_next++;
				}
			}
		}
	}
	else
	{
		progress.done = 1;

		if (!download_packages_num)
		{
			string_blt_shadow(ScreenSurface, FONT_ARIAL11, "Your client is up-to-date.", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			box.y += 60;

			if (button_show(BITMAP_BUTTON, -1, BITMAP_BUTTON_DOWN, box.x + box.w / 2 - Bitmaps[BITMAP_BUTTON]->bitmap->w / 2, box.y, "Close", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
			{
				popup_destroy_visible();
				return;
			}
		}
		else
		{
#ifdef WIN32
			string_blt_shadow_format(ScreenSurface, FONT_ARIAL11, box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box, "%"FMT64" updates downloaded successfully.", download_packages_downloaded);
			box.y += 20;
			string_blt_shadow(ScreenSurface, FONT_ARIAL11, "Restart the client to complete the update.", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

			if (download_packages_downloaded < download_packages_num)
			{
				string_blt_shadow_format(ScreenSurface, FONT_ARIAL11, box.x, box.y + 20, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box, "%"FMT64" updates failed to download (possibly due to a connection failure).", download_packages_num - download_packages_downloaded);
				string_blt_shadow(ScreenSurface, FONT_ARIAL11, "You may need to retry updating after restarting the client.", box.x, box.y + 40, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			}

			box.y += 60;

			if (button_show(BITMAP_BUTTON, -1, BITMAP_BUTTON_DOWN, box.x + box.w / 2 - Bitmaps[BITMAP_BUTTON]->bitmap->w / 2, box.y, "Restart", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
			{
				char path[HUGE_BUF], wdir[HUGE_BUF];

				snprintf(path, sizeof(path), "%s\\updater.bat", getcwd(wdir, sizeof(wdir) - 1));
				ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
				system_end();
				exit(0);
			}
#else
			string_blt_shadow(ScreenSurface, FONT_ARIAL11, "Updates are available; please use your distribution's package/update", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			box.y += 20;
			string_blt_shadow(ScreenSurface, FONT_ARIAL11, "manager to update, or visit <a=url:http://www.atrinik.org/>www.atrinik.org</a> for help.", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_MARKUP, &box);
#endif
		}
	}
}

static int popup_destroy_callback(popup_struct *popup)
{
	size_t i;

	(void) popup;

	if (dl_data)
	{
		curl_data_free(dl_data);
		dl_data = NULL;
	}

	for (i = 0; i < download_packages_num; i++)
	{
		free(download_packages_file[i]);
		free(download_packages_sha1[i]);
	}

	if (download_packages_file)
	{
		free(download_packages_file);
		download_packages_file = NULL;
		free(download_packages_sha1);
		download_packages_sha1 = NULL;
	}

	download_packages_num = 0;
	download_package_next = 0;
	download_package_process = 0;
	download_packages_downloaded = 0;

	return 1;
}

void updater_open()
{
	popup_struct *popup;
	CURL *curl;
	char url[HUGE_BUF], version[MAX_BUF], *version_escaped;

	popup = popup_create(BITMAP_POPUP);
	popup->destroy_callback_func = popup_destroy_callback;
	popup->draw_func_post = popup_draw_func_post;

	curl = curl_easy_init();
	package_get_version_full(version, sizeof(version));
	version_escaped = curl_easy_escape(curl, version, 0);
	snprintf(url, sizeof(url), UPDATER_CHECK_URL"&version=%s", version_escaped);
	curl_free(version_escaped);
	curl_easy_cleanup(curl);

	dl_data = curl_download_start(url);

	progress_dots_create(&progress);
}
