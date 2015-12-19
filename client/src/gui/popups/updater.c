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
 * Handles the update popup, which is triggered by clicking the 'Update'
 * button on the main client screen.
 *
 * As soon as the update popup is opened, cURL will attempt to check with
 * the update server whether there are any new versions available, by
 * sending it the current client's version, which the update server
 * checks. The update server will send a response, which may be empty,
 * but may contain filenames and SHA-1 sums of updates that the client
 * has to download in order to update to the latest version.
 *
 * Now, the behavior is different on the platform the client is running
 * on.
 *
 * For Windows:
 *
 * When the response is received, it is parsed, and the updates (if any)
 * are stored in ::download_packages The client will then attempt to
 * download the updates by their file names one-by-one by using cURL, and
 * saving the result in "client_patch_NUM.tar.gz" where NUM is the ID of
 * the update (starting with zero and increasing each time a new file is
 * downloaded) and is padded with zeroes. The directory the file is saved
 * in is determined by updater_get_dir(). On Windows, for example, it
 * would be stored in %AppData%/.atrinik/temp. It is not saved in the
 * same directory as the client executable due to Windows UAC, which
 * marks various directories, including Program Files as protected.
 *
 * Note that if the downloaded file content does not match the SHA-1, the
 * update will be stopped, and it will only install the updates
 * downloaded prior to the one that failed the SHA-1 check (if any).
 *
 * After the updater has finished downloading all updates (if none, it
 * just informs the user that they are running an up-to-date client), it
 * tells the user to restart their client to apply the updates, and shows
 * a handy 'Restart' button, which closes the client and opens it again.
 * This is done by calling up_dater.exe - which is also the executable
 * called by shortcuts - which runs atrinik_updater.bat as administrator
 * (popping up UAC prompt), which extracts the downloaded updates,
 * removes the temporary directory, and starts up the client. If there
 * are no updates, up_dater.exe simply starts up the Atrinik client
 * normally.
 *
 * The reason there is up_dater.exe and atrinik_updater.bat is that
 * Windows UAC is unable to only prompt for administrator password when
 * requested from a running program - it can only do so when starting up
 * a program, and administrator rights are necessary to write to
 * protected directories, such as Program Files, where the client may be
 * installed. up_dater.exe has an underscore in its filename because
 * otherwise Windows would (due to backwards compatibility, or some other
 * reason) popup UAC prompt each time it's started, even when there are
 * no updates available and it's not running atrinik_updater.bat as
 * administrator.
 *
 * For GNU/Linux:
 *
 * If there are any updates available, the user is simply instructed to
 * use their update manager to update.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>

/** Holds data that is currently being downloaded. */
static curl_data *dl_data = NULL;
/** Information about the packages that have to be downloaded. */
static update_file_struct *download_packages;
/** Number of packages to download (len of ::download_packages). */
static size_t download_packages_num = 0;
/** Next entry in ::download_packages that should be downloaded. */
static size_t download_package_next = 0;
/** Number of packages downloaded so far. */
static size_t download_packages_downloaded = 0;
/** Whether we are downloading packages. */
static uint8_t download_package_process = 0;
/** Progress dots in the popup. */
static progress_dots progress;
/**
 * Button buffer.
 */
static button_struct button_close, button_retry, button_restart;

/**
 * Get temporary directory where updates will be stored.
 * @param buf
 * Where to store the result.
 * @param len
 * Size of 'buf'.
 * @return
 * 'buf'.
 */
static char *updater_get_dir(char *buf, size_t len)
{
    snprintf(buf, len, "%s/.atrinik/temp", get_config_dir());
    return buf;
}

/**
 * Cleans up updater files - basically recursively removes the temporary
 * directory.
 */
static void cleanup_patch_files(void)
{
    char dir_path[HUGE_BUF];

    rmrf(updater_get_dir(dir_path, sizeof(dir_path)));
}

/**
 * Start updater download.
 */
static void updater_download_start(void)
{
    CURL *curl;
    char url[HUGE_BUF], version[MAX_BUF], *version_escaped;

    /* Construct URL. */
    curl = curl_easy_init();
    package_get_version_full(version, sizeof(version));
    version_escaped = curl_easy_escape(curl, version, 0);
    snprintf(url, sizeof(url), UPDATER_CHECK_URL "&version=%s", version_escaped);
    curl_free(version_escaped);
    curl_easy_cleanup(curl);

    /* Start downloading the list of available updates. */
    dl_data = curl_download_start(url, NULL);

    progress_dots_create(&progress);
}

/**
 * Cleanup after downloading.
 */
static void updater_download_clean(void)
{
    size_t i;

    /* Free data that is being downloaded, if the user quits mid-download.
     * Also remove the temp directory, as the update has clearly not
     * finished downloading its data */
    if (dl_data) {
        cleanup_patch_files();
        curl_data_free(dl_data);
        dl_data = NULL;
    }

    /* Free the allocated filenames and SHA-1 sums. */
    for (i = 0; i < download_packages_num; i++) {
        efree(download_packages[i].filename);
        efree(download_packages[i].sha1);
    }

    if (download_packages) {
        efree(download_packages);
        download_packages = NULL;
    }

    download_packages_num = 0;
    download_package_next = 0;
    download_package_process = 0;
    download_packages_downloaded = 0;
}

/** @copydoc popup_struct::draw_post_func */
static int popup_draw_post(popup_struct *popup)
{
    SDL_Rect box;

    box.x = popup->x;
    box.y = popup->y;
    box.w = popup->surface->w;
    box.h = 38;

    text_show(ScreenSurface, FONT_SERIF20, "Updater", box.x, box.y, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
    box.y += 60;

    /* Show the progress dots. */
    progress_dots_show(&progress, ScreenSurface, box.x + box.w / 2 - progress_dots_width(&progress) / 2, box.y);
    box.y += 30;

    /* Not done yet and downloading something, inform the user. */
    if (!progress.done && dl_data) {
        /* Downloading list of updates? */
        if (!strncmp(dl_data->url, UPDATER_CHECK_URL, strlen(UPDATER_CHECK_URL))) {
            text_show_shadow(ScreenSurface, FONT_ARIAL11, "Downloading list of updates...", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
        } else {
            text_show_shadow_format(ScreenSurface, FONT_ARIAL11, box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box, "Downloading update #%"PRId64 " out of %"PRId64 "...", (uint64_t) download_package_next, (uint64_t) download_packages_num);
        }
    }

    /* There is something being downloaded. */
    if (dl_data) {
        curl_state_t state = curl_download_get_state(dl_data);

        /* We are not done yet... */
        progress.done = 0;

        /* Failed? */
        if (state == CURL_STATE_ERROR) {
            /* Remove the temporary directory. */
            cleanup_patch_files();
            progress.done = 1;

            text_show_shadow(ScreenSurface, FONT_ARIAL11, "Connection timed out.", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

            box.y += 20;

            button_retry.x = box.x + box.w / 2 - texture_surface(button_retry.texture)->w / 2;
            button_retry.y = box.y;
            button_show(&button_retry, "Retry");
        } else if (state == CURL_STATE_OK) {
            /* Finished downloading. */

            /* Is it the list of updates? */
            if (!strncmp(dl_data->url, UPDATER_CHECK_URL, strlen(UPDATER_CHECK_URL))) {
                if (dl_data->memory) {
                    char *cp, *line, *tmp[2];

                    cp = estrdup(dl_data->memory);

                    line = strtok(cp, "\n");

                    while (line) {
                        if (string_split(line, tmp, arraysize(tmp), '\t') == 2) {
                            download_packages = erealloc(download_packages, sizeof(*download_packages) * (download_packages_num + 1));
                            download_packages[download_packages_num].filename = estrdup(tmp[0]);
                            download_packages[download_packages_num].sha1 = estrdup(tmp[1]);
                            download_packages_num++;
                        }

                        line = strtok(NULL, "\n");
                    }

                    efree(cp);
                }

#ifdef WIN32

                if (download_packages_num) {
                    download_package_process = 1;
                    download_package_next = 0;
                }
#endif

                curl_data_free(dl_data);
                dl_data = NULL;
            }

            /* Are we downloading packages? */
            if (download_package_process) {
                /* Have we got anything to store yet, or are we just starting
                 * the download? */
                if (download_package_next != 0 && dl_data) {
                    char sha1_output_ascii[41];
                    unsigned char sha1_output[20];
                    size_t i;

                    /* Calculate the SHA-1 sum of the downloaded data. */
                    sha1((unsigned char *) dl_data->memory, dl_data->size, sha1_output);

                    /* Create the ASCII SHA-1 sum. snprintf() is not
                     * needed, because no overflow can happen in this
                     * case. */
                    for (i = 0; i < 20; i++) {
                        sprintf(sha1_output_ascii + i * 2, "%02x", sha1_output[i]);
                    }

                    /* Compare the SHA-1 sum. */
                    if (!strcmp(download_packages[download_package_next - 1].sha1, sha1_output_ascii)) {
                        char dir_path[HUGE_BUF], filename[HUGE_BUF];
                        FILE *fp;

                        /* Get the temporary directory. */
                        updater_get_dir(dir_path, sizeof(dir_path));

                        /* Create the temporary directory if it doesn't exist
                         * yet. */
                        if (access(dir_path, R_OK) != 0) {
                            mkdir(dir_path, 0755);
                        }

                        /* Construct the path. */
                        snprintf(filename, sizeof(filename), "%s/client_patch_%09"PRId64 ".tar.gz", dir_path, (uint64_t) download_package_next - 1);
                        fp = fopen(filename, "wb");

                        if (fp) {
                            fwrite(dl_data->memory, 1, dl_data->size, fp);
                            fclose(fp);
                            download_packages_downloaded++;
                        }
                    } else {
                        /* Did not match, stop downloading, even if there are
                         * more. */
                        download_package_next = download_packages_num;
                    }

                    curl_data_free(dl_data);
                    dl_data = NULL;
                }

                /* Starting the download (possibly next package). */
                if (download_package_next < download_packages_num) {
                    char url[HUGE_BUF];

                    /* Construct the URL. */
                    snprintf(url, sizeof(url), UPDATER_PATH_URL "/%s", download_packages[download_package_next].filename);
                    dl_data = curl_download_start(url, NULL);
                    download_package_next++;
                }
            }
        }
    } else {
        /* Finished all downloads. */

        progress.done = 1;

        /* No packages, so the client is up-to-date. */
        if (!download_packages_num) {
            text_show_shadow(ScreenSurface, FONT_ARIAL11, "Your client is up-to-date.", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
            box.y += 60;

            button_close.x = box.x + box.w / 2 - texture_surface(button_close.texture)->w / 2;
            button_close.y = box.y;
            button_show(&button_close, "Close");
        } else {
#ifdef WIN32
            text_show_shadow_format(ScreenSurface, FONT_ARIAL11, box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box, "%"PRId64 " update(s) downloaded successfully.", (uint64_t) download_packages_downloaded);
            box.y += 20;
            text_show_shadow(ScreenSurface, FONT_ARIAL11, "Restart the client to complete the update.", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

            if (download_packages_downloaded < download_packages_num) {
                text_show_shadow_format(ScreenSurface, FONT_ARIAL11, box.x, box.y + 20, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box, "%"PRId64 " update(s) failed to download (possibly due to a connection failure).", (uint64_t) (download_packages_num - download_packages_downloaded));
                text_show_shadow(ScreenSurface, FONT_ARIAL11, "You may need to retry updating after restarting the client.", box.x, box.y + 40, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
            }

            box.y += 60;

            /* Show a restart button, which will call up_dater.exe to
             * apply the updates (using atrinik_updater.bat) and restart
             * the client. */
            button_restart.x = box.x + box.w / 2 - texture_surface(button_restart.texture)->w / 2;
            button_restart.y = box.y;
            button_show(&button_restart, "Restart");
#else
            text_show_shadow(ScreenSurface, FONT_ARIAL11, "Updates are available; please use your distribution's package/update", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
            box.y += 20;
            text_show_shadow(ScreenSurface, FONT_ARIAL11, "manager to update, or visit [a=url:http://www.atrinik.org/]www.atrinik.org[/a] for help.", box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_MARKUP, &box);
#endif
        }
    }

    return 1;
}

/** @copydoc popup_struct::popup_event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
    if (button_event(&button_close, event)) {
        popup_destroy(popup);
        return 1;
    } else if (button_event(&button_retry, event)) {
        updater_download_clean();
        updater_download_start();
        return 1;
    }
#ifdef WIN32
    else if (button_event(&button_restart, event)) {
        char path[HUGE_BUF], wdir[HUGE_BUF];

        snprintf(path, sizeof(path), "%s\\atrinik2.exe", getcwd(wdir, sizeof(wdir) - 1));
        ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
        exit(0);
        return 1;
    }
#endif

    return -1;
}

/**
 * Called when the updater popup is destroyed; frees the data used (if
 * any), etc.
 * @param popup
 * Updater popup.
 */
static int popup_destroy_callback(popup_struct *popup)
{
    updater_download_clean();

    button_destroy(&button_close);
    button_destroy(&button_retry);
    button_destroy(&button_restart);

    return 1;
}

/**
 * Open the updater popup.
 */
void updater_open(void)
{
    popup_struct *popup;

    /* Create the popup. */
    popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    popup->destroy_callback_func = popup_destroy_callback;
    popup->draw_post_func = popup_draw_post;
    popup->event_func = popup_event;

    button_create(&button_close);
    button_create(&button_retry);
    button_create(&button_restart);

    updater_download_start();
}
