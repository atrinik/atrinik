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
#define UPDATER_PATCH_SUFFIX ".tar.gz"

static curl_data *dl_data = NULL;
static char **download_packages_file;
static char **download_packages_sha1;
static size_t download_packages_num = 0;
static size_t download_package_next = 0;
static uint8 download_package_process = 0;

static void cleanup_patch_files()
{
	DIR *dir;
	struct dirent *currentfile;

	dir = opendir(".");

	if (!dir)
	{
		return;
	}

	while ((currentfile = readdir(dir)))
	{
		if (!strcmp(currentfile->d_name + strlen(currentfile->d_name) - strlen(UPDATER_PATCH_SUFFIX), UPDATER_PATCH_SUFFIX))
		{
			unlink(currentfile->d_name);
		}
	}

	closedir(dir);
}

static void popup_draw_func_post(popup_struct *popup)
{
	(void) popup;

	if (dl_data)
	{
		sint8 ret = curl_download_finished(dl_data);

		if (ret == 0)
		{
			// downloading
		}
		else if (ret == -1)
		{
			cleanup_patch_files();

			// timed out, retry button
		}
		else if (ret == 1)
		{
			if (!strncmp(dl_data->url, UPDATER_CHECK_URL, strlen(UPDATER_CHECK_URL)))
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

				if (download_packages_num)
				{
					download_package_process = 1;
					download_package_next = 0;
				}

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
						char filename[MAX_BUF];
						FILE *fp;

						snprintf(filename, sizeof(filename), "client_patch_%09"FMT64".tar.gz", (sint64) download_package_next - 1);
						fp = fopen(filename, "wb");

						if (fp)
						{
							fwrite(dl_data->memory, 1, dl_data->size, fp);
							fclose(fp);
						}
					}
					else
					{
						download_package_process = 0;
					}

					curl_data_free(dl_data);
					dl_data = NULL;
				}

				if (download_package_process && download_package_next < download_packages_num)
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
		// up-to-date
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
}
