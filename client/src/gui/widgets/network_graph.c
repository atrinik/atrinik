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
 * Implements network graph type widgets.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <network_graph.h>
#include <toolkit_string.h>

#ifndef __CPROTO__

/**
 * Holds network graph data about a particular data type.
 */
typedef struct network_graph_data {
    size_t *data; ///< The actual data.
    int width; ///< Number of entries in the data array.
    int pos; ///< Position in the data array.
    Uint32 ticks; ///< When the data was last logged.
    size_t max; ///< Peak.
} network_graph_data_t;

/**
 * Network graph widget sub-structure.
 */
typedef struct network_graph_widget {
    /**
     * The data type sources.
     */
    network_graph_data_t data[NETWORK_GRAPH_TYPE_MAX];

    /**
     * Currently displayed type.
     */
    int type;

    /**
     * Which traffic types to display.
     */
    uint32_t filters;
} network_graph_widget_t;

/**
 * Work structure used to enqueue a network graph update.
 */
typedef struct network_graph_work {
    struct network_graph_work *next; ///< Next entry.
    int type; ///< Data type.
    int traffic; ///< Traffic type.
    size_t bytes; ///< Bytes.
} network_graph_work_t;

/**
 * String representations of the network graph types.
 */
static const char *const network_graph_types[NETWORK_GRAPH_TYPE_MAX] = {
    "Game data", "HTTP data"
};

/**
 * String representations of the network traffic types.
 */
static const char *const network_graph_filters[NETWORK_GRAPH_TRAFFIC_MAX] = {
    "Received", "Transmitted"
};

/**
 * Colors of the network graph types.
 */
static const char *const network_graph_colors[NETWORK_GRAPH_TYPE_MAX] = {
    "#ff0000", "#00ff00"
};

/**
 * Mutex used to provide reentrant API for network graph updates.
 */
static SDL_mutex *network_graph_mutex = NULL;

/**
 * The work queue.
 */
static network_graph_work_t *work_queue = NULL;

/* Prototypes */
static void widget_network_graph_update(widgetdata *widget, int type,
        int traffic, size_t bytes);

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    if (!widget->redraw) {
        return;
    }

    network_graph_widget_t *network_graph = widget->subwidget;
    network_graph_data_t *data = &network_graph->data[network_graph->type];

    SDL_FillRect(widget->surface, NULL, 0);

    if (data->data == NULL) {
        return;
    }

    for (int i = 0; i < NETWORK_GRAPH_TRAFFIC_MAX; i++) {
        if (!BIT_QUERY(network_graph->filters, i)) {
            continue;
        }

        SDL_Color color;
        if (!text_color_parse(network_graph_colors[i], &color)) {
            LOG(ERROR, "Could not parse color: %s",
                    network_graph_colors[i]);
            continue;
        }

        int ly = widget->h - 1;
        for (int x = 0; x < data->pos && x < widget->w; x++) {
            size_t bytes = data->data[NETWORK_GRAPH_TRAFFIC_MAX * x + i];
            long double factor;
            if (data->max == 0) {
                factor = 0.0;
            } else {
                factor = bytes / (long double) data->max;
            }
            int y = (widget->h - 1) - (widget->h - 1) * factor;

            lineRGBA(widget->surface, MAX(0, x - 1), ly, x, y, color.r,
                    color.g, color.b, 255);
            ly = y;
        }
    }
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget, int draw)
{
    if (!widget->show) {
        if (network_graph_mutex != NULL) {
            SDL_DestroyMutex(network_graph_mutex);
            network_graph_mutex = NULL;
        }

        return;
    }

    if (network_graph_mutex == NULL) {
        network_graph_mutex = SDL_CreateMutex();
    }

    network_graph_widget_t *network_graph = widget->subwidget;

    SDL_LockMutex(network_graph_mutex);
    while (work_queue != NULL) {
        network_graph_work_t *work = work_queue;

        for (widgetdata *tmp = cur_widget[NETWORK_GRAPH_ID]; tmp != NULL;
                tmp = tmp->type_next) {
            widget_network_graph_update(tmp, work->type, work->traffic,
                    work->bytes);
        }

        LL_DELETE(work_queue, work);
        efree(work);
    }
    SDL_UnlockMutex(network_graph_mutex);

    for (int type = 0; type < NETWORK_GRAPH_TYPE_MAX; type++) {
        network_graph_data_t *data = &network_graph->data[type];
        if (LastTick - data->ticks <= 1000) {
            continue;
        }

        for (int traffic = 0; traffic < NETWORK_GRAPH_TRAFFIC_MAX; traffic++) {
            widget_network_graph_update(widget, type, traffic, 0);
        }
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    network_graph_widget_t *network_graph = widget->subwidget;
    network_graph_data_t *data = &network_graph->data[network_graph->type];

    if (event->type == SDL_MOUSEMOTION) {
        int x = event->motion.x - widget->x;
        if (x < 0 || x >= widget->w) {
            return 0;
        }

        char buf[HUGE_BUF];
        snprintf(VS(buf), "Maximum: %" PRIu64 " Bytes/s (%" PRIu64 " kB/s)",
                (uint64_t) data->max, (uint64_t) data->max / 1000);

        for (int i = 0; i < NETWORK_GRAPH_TRAFFIC_MAX; i++) {
            size_t bytes = data->data[x * NETWORK_GRAPH_TRAFFIC_MAX + i];
            snprintfcat(VS(buf), "\n%s: %" PRIu64 " Bytes/s (%" PRIu64 " kB/s)",
                    network_graph_filters[i], (uint64_t) bytes,
                    (uint64_t) bytes / 1000);
        }

        tooltip_create(event->motion.x, event->motion.y, FONT_ARIAL11, buf);
        tooltip_multiline(200);
        tooltip_enable_delay(100);
    }

    return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    network_graph_widget_t *network_graph = widget->subwidget;

    for (int i = 0; i < NETWORK_GRAPH_TYPE_MAX; i++) {
        if (network_graph->data[i].data != NULL) {
            efree(network_graph->data[i].data);
        }
    }
}

static void menu_network_graph_display_change(widgetdata *widget,
        widgetdata *menuitem, SDL_Event *event)
{
    network_graph_widget_t *network_graph = widget->subwidget;

    for (widgetdata *tmp = menuitem->inv; tmp != NULL; tmp = tmp->next) {
        if (tmp->type == LABEL_ID) {
            _widget_label *label = LABEL(tmp);

            for (int i = 0; i < NETWORK_GRAPH_TYPE_MAX; i++) {
                if (strcmp(network_graph_types[i], label->text) == 0) {
                    network_graph->type = i;
                    widget->redraw = 1;
                    break;
                }
            }

            break;
        }
    }
}

static void menu_network_graph_display(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    network_graph_widget_t *network_graph = widget->subwidget;
    widgetdata *submenu = MENU(menuitem->env)->submenu;

    for (int i = 0; i < NETWORK_GRAPH_TYPE_MAX; i++) {
        add_menuitem(submenu, network_graph_types[i],
                &menu_network_graph_display_change, MENU_RADIO,
                network_graph->type == i);
    }
}

static void menu_network_graph_filters_change(widgetdata *widget,
        widgetdata *menuitem, SDL_Event *event)
{
    network_graph_widget_t *network_graph = widget->subwidget;

    for (widgetdata *tmp = menuitem->inv; tmp != NULL; tmp = tmp->next) {
        if (tmp->type == LABEL_ID) {
            _widget_label *label = LABEL(tmp);

            for (int i = 0; i < NETWORK_GRAPH_TRAFFIC_MAX; i++) {
                if (strcmp(network_graph_filters[i], label->text) == 0) {
                    BIT_FLIP(network_graph->filters, i);
                    widget->redraw = 1;
                    break;
                }
            }

            break;
        }
    }
}

static void menu_network_graph_filters(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    network_graph_widget_t *network_graph = widget->subwidget;
    widgetdata *submenu = MENU(menuitem->env)->submenu;

    for (int i = 0; i < NETWORK_GRAPH_TRAFFIC_MAX; i++) {
        add_menuitem(submenu, network_graph_filters[i],
                &menu_network_graph_filters_change, MENU_CHECKBOX,
                BIT_QUERY(network_graph->filters, i));
    }
}

/** @copydoc widgetdata::menu_handle_func */
static int widget_menu_handle(widgetdata *widget, SDL_Event *event)
{
    widgetdata *menu = create_menu(event->motion.x, event->motion.y, widget);
    widget_menu_standard_items(widget, menu);
    add_menuitem(menu, "Display  >", &menu_network_graph_display, MENU_SUBMENU,
            0);
    add_menuitem(menu, "Filters  >", &menu_network_graph_filters, MENU_SUBMENU,
            0);
    menu_finalize(menu);
    return 1;
}

/**
 * Actually performs updating for the specified network graph widget.
 * @param widget The widget.
 * @param type The network graph type.
 * @param traffic The traffic type (tx/rx).
 * @param bytes Bytes.
 */
static void widget_network_graph_update(widgetdata *widget, int type,
        int traffic, size_t bytes)
{
    HARD_ASSERT(widget != NULL);

    if (!widget->show) {
        return;
    }

    network_graph_widget_t *network_graph = widget->subwidget;
    network_graph_data_t *data = &network_graph->data[type];

    if (data->data == NULL || data->width != widget->w) {
        if (data->data == NULL) {
            data->ticks = LastTick;
        }

        data->data = ereallocz(data->data,
                sizeof(*data->data) * NETWORK_GRAPH_TRAFFIC_MAX * data->width,
                sizeof(*data->data) * NETWORK_GRAPH_TRAFFIC_MAX * widget->w);
        data->width = widget->w;
    }

    if (data->width == 0) {
        return;
    }

    if (LastTick - data->ticks > 1000) {
        data->pos++;
        data->ticks = LastTick;
    }

    if (data->pos == data->width) {
        data->pos--;

        bool recalc_max = false;
        for (int i = 0; i < NETWORK_GRAPH_TRAFFIC_MAX; i++) {
            if (data->data[i] >= data->max) {
                recalc_max = true;
            }
        }

        memmove(data->data, data->data + NETWORK_GRAPH_TRAFFIC_MAX,
                sizeof(*data->data) * (NETWORK_GRAPH_TRAFFIC_MAX *
                (data->width - 1)));

        if (recalc_max) {
            data->max = 0;
        }

        for (int i = 0; i < NETWORK_GRAPH_TRAFFIC_MAX; i++) {
            data->data[data->pos * NETWORK_GRAPH_TRAFFIC_MAX + i] = 0;

            if (recalc_max) {
                for (int x = 0; x < data->pos; x++) {
                    size_t bytes2 = data->data[x * NETWORK_GRAPH_TRAFFIC_MAX +
                            i];
                    if (bytes2 > data->max) {
                        data->max = bytes2;
                    }
                }
            }
        }
    }

    size_t *dst = &data->data[data->pos * NETWORK_GRAPH_TRAFFIC_MAX + traffic];
    *dst += bytes;

    if (*dst > data->max) {
        data->max = *dst;
    }

    widget->redraw = 1;
}

/**
 * Updates all network graph widgets with new data.
 * @param type The network graph type.
 * @param traffic The traffic type (tx/rx).
 * @param bytes Bytes.
 * @note This function is thread-safe.
 */
void network_graph_update(int type, int traffic, size_t bytes)
{
    HARD_ASSERT(type >= 0 && type < NETWORK_GRAPH_TYPE_MAX);
    HARD_ASSERT(traffic >= 0 && traffic < NETWORK_GRAPH_TRAFFIC_MAX);

    if (network_graph_mutex == NULL) {
        return;
    }

    network_graph_work_t *work = ecalloc(1, sizeof(*work));
    work->type = type;
    work->traffic = traffic;
    work->bytes = bytes;

    SDL_LockMutex(network_graph_mutex);
    LL_PREPEND(work_queue, work);
    SDL_UnlockMutex(network_graph_mutex);
}

/**
 * Initializes one network graph widget.
 * @param widget The widget.
 */
void widget_network_graph_init(widgetdata *widget)
{
    network_graph_widget_t *network_graph = ecalloc(1, sizeof(*network_graph));
    network_graph->filters = ~0U;

    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->event_func = widget_event;
    widget->deinit_func = widget_deinit;
    widget->menu_handle_func = widget_menu_handle;
    widget->subwidget = network_graph;
}

#endif
