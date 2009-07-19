/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

#include "include.h"

char *skill_level_name[] = {
	"",
	"Ag",
	"Pe",
	"Me",
	"Ph",
	"Ma",
	"Wi"
};

/* This function returns number of items and adjusst the inventory window data */
int get_inventory_data(item *op, int *ctag, int *slot, int *start, int *count, int wxlen, int wylen)
{
	register item *tmp, *tmpc;
	register int i = 0;
	int ret = -1;

	cpl.window_weight = 0.0f;
	*ctag = -1;
	*count = 0;

	if (!op)
	{
		*slot = *start = 0;
		return -1;
	}

	if (!op->inv)
	{
		*slot = *start = 0;
		return -1;
	}

	if (*slot < 0)
		*slot = 0;

	/* Pre count items, and adjust slot cursor */
	for (tmp = op->inv; tmp; tmp = tmp->next)
	{
		(*count)++;
		cpl.window_weight += tmp->weight * (float)tmp->nrof;

		if (tmp->tag == cpl.container_tag)
			cpl.container = tmp;

		if (cpl.container && cpl.container->tag == tmp->tag)
		{
			tmpc = cpl.sack->inv;

			for (; tmpc; tmpc = tmpc->next)
				(*count)++;
		}
	}

	if (!*count)
		*slot = 0;
	else if (*slot >= *count)
		*slot = *count - 1;

	/* Now find tag */
	for (tmp = op->inv; tmp; tmp = tmp->next)
	{
		if (*slot == i)
			ret = tmp->tag;

		i++;

		if (cpl.container && cpl.container->tag == tmp->tag)
		{
			tmpc = cpl.sack->inv;

			for (; tmpc; tmpc = tmpc->next)
			{
				if (*slot == i)
				{
					*ctag = cpl.container->tag;
					ret = tmpc->tag;
				}

				i++;
			}
		}
	}

	/* And adjust the slot/start position of the window */
	if (*slot < *start)
		*start = *slot - (*slot % wxlen);
	else if (*slot > *start + (wxlen * wylen) - 1)
		*start = ((int)(*slot / wxlen)) * wxlen - (wxlen * (wylen - 1));

	return ret;
}

static void show_inventory_item_stats(item *tmp, int x, int y)
{
	char buf[256];
    SDL_Rect tmp_rect;
    tmp_rect.w = 222;

    if (tmp->nrof > 1)
        snprintf(buf, sizeof(buf), "%d %s", tmp->nrof, tmp->s_name);
    else
        snprintf(buf, sizeof(buf), "%s", tmp->s_name);

    StringBlt(ScreenSurface, &SystemFont, buf, x + 23, y - 26, COLOR_HGOLD, &tmp_rect, NULL);

    snprintf(buf, sizeof(buf), "weight: %4.3f", tmp->weight * (float)tmp->nrof);
    StringBlt(ScreenSurface, &SystemFont, buf, x + 170, y - 14, COLOR_HGOLD, NULL, NULL);

	/* This comes from server when not identified */
    if (tmp->item_qua == 255)
        StringBlt(ScreenSurface, &SystemFont, "not identified", x + 23, y - 14, COLOR_RED, NULL, NULL);
    else
    {
        StringBlt(ScreenSurface, &SystemFont, "con: ", x + 23, y - 14, COLOR_HGOLD, NULL, NULL);

        snprintf(buf, sizeof(buf), "%d", tmp->item_qua);
        StringBlt(ScreenSurface, &SystemFont, buf, x + 57 - get_string_pixel_length(buf, &SystemFont), y - 14, COLOR_HGOLD, NULL, NULL);

        snprintf(buf, sizeof(buf), "(%d)", tmp->item_con);
        StringBlt(ScreenSurface, &SystemFont, buf, x + 60, y - 14, COLOR_HGOLD, NULL, NULL);

        if (tmp->item_level)
        {
            snprintf(buf, sizeof(buf), "allowed: lvl %d %s", tmp->item_level, skill_level_name[tmp->item_skill]);

            if ((!tmp->item_skill && tmp->item_level <= cpl.stats.level) || (tmp->item_skill && tmp->item_level <= cpl.stats.skill_level[tmp->item_skill - 1]))
                StringBlt(ScreenSurface, &SystemFont, buf, x + 100 , y - 14, COLOR_HGOLD, NULL, NULL);
            else
                StringBlt(ScreenSurface, &SystemFont, buf, x + 100, y - 14, COLOR_RED, NULL, NULL);
        }
        else
            StringBlt(ScreenSurface, &SystemFont, "allowed: all", x + 100, y - 14, COLOR_HGOLD, NULL, NULL);
    }
}

void widget_inventory_event(int x, int y, SDL_Event event)
{
    int mx = 0, my = 0;
    mx = x - cur_widget[MAIN_INV_ID].x1;
    my = y - cur_widget[MAIN_INV_ID].y1;

    switch (event.type)
    {
    	case SDL_MOUSEBUTTONUP:
			if (draggingInvItem(DRAG_GET_STATUS) > DRAG_IWIN_BELOW)
			{
				/* KEYFUNC_APPLY and KEYFUNC_DROP works only if cpl.inventory_win = IWIN_INV. The tag must
				 * be placed in cpl.win_inv_tag. So we do this and after DnD we restore the old values. */
				int old_inv_win = cpl.inventory_win;
				int old_inv_tag = cpl.win_inv_tag;
				cpl.inventory_win = IWIN_INV;

				if (draggingInvItem(DRAG_GET_STATUS) == DRAG_PDOLL)
				{
					cpl.win_inv_tag = cpl.win_pdoll_tag;
					/* We dont have to check for the coordinates, if we are here we are in the widget */

					/* Drop to inventory */
					process_macro_keys(KEYFUNC_APPLY, 0);
				}

				cpl.inventory_win = old_inv_win;
				cpl.win_inv_tag = old_inv_tag;
			}
			else if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_BELOW)
			{
				sound_play_effect(SOUND_GET, 0, 100);
				process_macro_keys(KEYFUNC_GET, 0);
			}

			draggingInvItem(DRAG_NONE);
			/* ready for next item */
			itemExamined = 0;
			break;

		case SDL_MOUSEBUTTONDOWN:
			/* Inventory (open / close) */
			if (mx >= 4 && mx <= 22 && my >= 4 && my <= 26)
			{
				if (cpl.inventory_win == IWIN_INV)
					cpl.inventory_win = IWIN_BELOW;
				else
					cpl.inventory_win = IWIN_INV;

				break;
			}

			/* scrollbar */
			if (mx > 226 && mx < 236)
			{
				if (my <= 39 && my >= 30 && cpl.win_inv_slot >= INVITEMXLEN)
					cpl.win_inv_slot -= INVITEMXLEN;
				else if (my >= 116 && my <= 125)
				{
					cpl.win_inv_slot += INVITEMXLEN;

					if (cpl.win_inv_slot > cpl.win_inv_count)
						cpl.win_inv_slot = cpl.win_inv_count;
				}
			}
			else if (mx > 3)
			{
				/* Stuff */

				/* Mouseweel */
				if (event.button.button == 4 && cpl.win_inv_slot >= INVITEMXLEN)
					cpl.win_inv_slot -= INVITEMXLEN;
				/* Mouseweel */
				else if (event.button.button == 5)
				{
					cpl.win_inv_slot += INVITEMXLEN;

					if (cpl.win_inv_slot > cpl.win_inv_count)
						cpl.win_inv_slot = cpl.win_inv_count;
				}
				else if((event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE) && my > 29 && my < 125)
				{
					cpl.win_inv_slot = (my - 30) / 32 * INVITEMXLEN + (mx - 3) / 32 + cpl.win_inv_start;
					cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);

					if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE)
						process_macro_keys(KEYFUNC_MARK, 0);
					else
					{
						if (cpl.inventory_win == IWIN_INV)
							draggingInvItem(DRAG_IWIN_INV);
					}
				}
			}

			break;

		case SDL_MOUSEMOTION:
			/* Scrollbar sliders */
			if (event.button.button == SDL_BUTTON_LEFT && !draggingInvItem(DRAG_GET_STATUS))
			{
				/* IWIN_INV Slider */
				if (cpl.inventory_win == IWIN_INV && my + 38 && my + 116 && mx + 227 && mx + 236)
				{
					if (old_mouse_y - y > 0)
						cpl.win_inv_slot -= INVITEMXLEN;
					else if (old_mouse_y - y < 0)
						cpl.win_inv_slot += INVITEMXLEN;

					if (cpl.win_inv_slot > cpl.win_inv_count)
						cpl.win_inv_slot = cpl.win_inv_count;

					break;
				}
			}
    }
}

void widget_show_inventory_window(int x, int y)
{
    register int i;
    int invxlen, invylen;
    item  *op, *tmp, *tmpx = NULL;
    item  *tmpc;
    char buf[256];

    if (cpl.inventory_win != IWIN_INV)
    {
        if (!options.playerdoll)
            cur_widget[PDOLL_ID].show = 0;

        cur_widget[MAIN_INV_ID].ht = 32;
        sprite_blt(Bitmaps[BITMAP_INV_BG], x, y, NULL, NULL);

        StringBlt(ScreenSurface, &SystemFont, "Carry", x + 140, y + 4, COLOR_HGOLD, NULL, NULL);

        snprintf(buf, sizeof(buf), "%4.3f kg", cpl.real_weight);
        StringBlt(ScreenSurface, &SystemFont, buf, x + 140 + 35, y + 4, COLOR_DEFAULT, NULL, NULL);

        StringBlt(ScreenSurface, &SystemFont, "Limit", x + 140, y + 15, COLOR_HGOLD, NULL, NULL);

        snprintf(buf, sizeof(buf), "%4.3f kg", (float) cpl.weight_limit / 1000.0);
        StringBlt(ScreenSurface, &SystemFont, buf, x + 140 + 35, y + 15, COLOR_DEFAULT, NULL, NULL);

        StringBlt(ScreenSurface, &Font6x3Out, "(SHIFT for inventory)", x + 32, y + 9, COLOR_DEFAULT, NULL, NULL);

		return;
    }

    if (!options.playerdoll)
       cur_widget[PDOLL_ID].show = 1;

    cur_widget[MAIN_INV_ID].ht = 129;

    invxlen = INVITEMXLEN;
    invylen = INVITEMYLEN;

    sprite_blt(Bitmaps[BITMAP_INVENTORY], x, y, NULL, NULL);

    y += 26 + 3;
    x += 3;

    blt_window_slider(Bitmaps[BITMAP_INV_SCROLL], ((cpl.win_inv_count - 1) / invxlen) + 1, invylen, cpl.win_inv_start / invxlen, -1, x + 226, y + 11);

    if (!cpl.ob)
        return;

    op = cpl.ob;

    for (tmpc = NULL, i = 0, tmp = op->inv; tmp && i < cpl.win_inv_start; tmp = tmp->next)
    {
        i++;

        if (cpl.container && cpl.container->tag == tmp->tag)
        {
            tmpx = tmp;
            tmpc = cpl.sack->inv;

            for (; tmpc && i < cpl.win_inv_start; tmpc = tmpc->next,i++);

            if (tmpc)
                break;
        }
    }

    i = 0;

    if (tmpc)
    {
		tmp = tmpx;
		goto jump_in_container1;
    }

    for (; tmp && i < invxlen * invylen; tmp = tmp->next)
    {
		/* TODO: Implement marked items */
#if 0
        if (tmp->tag == cpl.mark_count)
            sprite_blt(Bitmaps[BITMAP_INVSLOT_MARKED], x + (i % invxlen) * 32, y + (i / invxlen) * 32, NULL, NULL);
#endif

        blt_inv_item(tmp, x + (i % invxlen) * 32 + 1, y + (i / invxlen) * 32 + 1);

        if (cpl.inventory_win != IWIN_BELOW && i + cpl.win_inv_start == cpl.win_inv_slot)
        {
            sprite_blt(Bitmaps[BITMAP_INVSLOT], x + (i % invxlen) * 32, y + (i / invxlen) * 32, NULL, NULL);
            show_inventory_item_stats(tmp, x, y);
        }

        i++;

        /* We have an open container - 'insert' the items inside in the panel */
        if (cpl.container && cpl.container->tag == tmp->tag)
        {
            sprite_blt(Bitmaps[BITMAP_CMARK_START], x + ((i - 1) % invxlen) * 32 + 1, y + ((i - 1) / invxlen) * 32 + 1, NULL, NULL);
            tmpc = cpl.sack->inv;

			jump_in_container1:
            for (; tmpc && i < invxlen * invylen; tmpc = tmpc->next)
            {
				/* TODO: Implement marked items */
#if 0
                if (tmpc->tag == cpl.mark_count)
                   sprite_blt(Bitmaps[BITMAP_INVSLOT_MARKED], x + (i % invxlen) * 32, y + (i / invxlen) * 32, NULL, NULL);
#endif

                blt_inv_item(tmpc, x + (i % invxlen) * 32 + 1, y + (i / invxlen) * 32 + 1);

                if (cpl.inventory_win != IWIN_BELOW && i + cpl.win_inv_start == cpl.win_inv_slot)
                {
                    sprite_blt(Bitmaps[BITMAP_INVSLOT], x + (i % invxlen) * 32, y + (i / invxlen) * 32, NULL, NULL);

                    show_inventory_item_stats(tmpc, x, y);
                }

                sprite_blt(Bitmaps[BITMAP_CMARK_MIDDLE], x + (i % invxlen) * 32 + 1, y + (i / invxlen) * 32 + 1, NULL, NULL);
                i++;
            }

            if (!tmpc)
                sprite_blt(Bitmaps[BITMAP_CMARK_END], x + ((i - 1) % invxlen) * 32 + 1, y + ((i - 1) / invxlen) * 32 + 1, NULL, NULL);
        }
    }
}

void widget_below_window_event(int x, int y, int MEvent)
{
    /* ground ( IWIN_BELOW )  */
    if (y >= cur_widget[BELOW_INV_ID].y1 + 19 && y <= cur_widget[BELOW_INV_ID].y1 + cur_widget[BELOW_INV_ID].ht - 4 && x > cur_widget[BELOW_INV_ID].x1 + 4 && x < cur_widget[BELOW_INV_ID].x1 + cur_widget[BELOW_INV_ID].wd - 12)
    {
        item *Item;

        if (cpl.inventory_win == IWIN_INV)
            cpl.inventory_win = IWIN_BELOW;

        cpl.win_below_slot = (x - cur_widget[BELOW_INV_ID].x1 - 5) / 32;

        cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);

        Item = locate_item(cpl.win_below_tag);

        if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
            draggingInvItem(DRAG_IWIN_BELOW);
        else
            process_macro_keys(KEYFUNC_APPLY, 0);
    }
    else if (y >= cur_widget[BELOW_INV_ID].y1 + 20 && y <= cur_widget[BELOW_INV_ID].y1 + 29 && x > cur_widget[BELOW_INV_ID].x1 + 262 && x < cur_widget[BELOW_INV_ID].x1 + 269 && MEvent == MOUSE_DOWN)
    {
        if (cpl.inventory_win == IWIN_INV)
            cpl.inventory_win = IWIN_BELOW;

        cpl.win_below_slot = cpl.win_below_slot - INVITEMBELOWXLEN;

        if (cpl.win_below_slot < 0)
            cpl.win_below_slot = 0;

        cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
    }
    else if (y >= cur_widget[BELOW_INV_ID].y1 + 42 && y <= cur_widget[BELOW_INV_ID].y1 + 51 && x > cur_widget[BELOW_INV_ID].x1 + 262 && x < cur_widget[BELOW_INV_ID].x1 + 269 && MEvent == MOUSE_DOWN)
    {
        if (cpl.inventory_win == IWIN_INV)
            cpl.inventory_win = IWIN_BELOW;

        cpl.win_below_slot = cpl.win_below_slot + INVITEMBELOWXLEN;

        if (cpl.win_below_slot > cpl.win_below_count -1)
            cpl.win_below_slot = cpl.win_below_count -1;

        cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
    }
}

void widget_show_below_window(item *op, int x, int y)
{
    register int i, slot,at;
    item *tmp, *tmpc, *tmpx = NULL;
    char buf[256];
    SDL_Rect tmp_rect;
    tmp_rect.w = 265;

    sprite_blt(Bitmaps[BITMAP_BELOW], x, y, NULL, NULL);

    blt_window_slider(Bitmaps[BITMAP_BELOW_SCROLL], ((cpl.win_below_count - 1) / INVITEMBELOWXLEN) + 1, INVITEMBELOWYLEN, cpl.win_below_start / INVITEMBELOWXLEN, -1, x + 263, y + 30);

    if (!op)
        return;

    for (i = 0, tmpc = NULL, tmp = op->inv; tmp && i < cpl.win_below_start; tmp = tmp->next)
    {
        i++;
        tmpx = tmp;

        if (cpl.container && cpl.container->tag == tmp->tag)
        {
            tmpc = cpl.sack->inv;

            for (; tmpc && i < cpl.win_below_start; tmpc = tmpc->next, i++);

            if (tmpc)
                break;
        }
    }

    i = 0;

    if (tmpc)
    {
        tmp = tmpx;
        goto jump_in_container2;
    }

    for (; tmp && i < INVITEMBELOWXLEN * INVITEMBELOWYLEN; tmp = tmp->next)
    {
        at = tmp->applied;

        if (tmp->tag != cpl.container_tag)
            tmp->applied = 0;

        blt_inv_item(tmp, x + (i % INVITEMBELOWXLEN) * 32 + 5, y + (i / INVITEMBELOWXLEN) * 32 + 19);
        tmp->applied = at;

        if (i + cpl.win_below_start == cpl.win_below_slot)
        {
            if (cpl.inventory_win == IWIN_BELOW)
                slot = BITMAP_INVSLOT;
            else
                slot = BITMAP_INVSLOT_U;

            sprite_blt(Bitmaps[slot], x + (i % INVITEMBELOWXLEN) * 32 + 5, y + (i / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);

            if (tmp->nrof > 1)
                snprintf(buf, sizeof(buf), "%d %s", tmp->nrof, tmp->s_name);
            else
                snprintf(buf, sizeof(buf), "%s", tmp->s_name);

            StringBlt(ScreenSurface, &SystemFont, buf, x + 6, y + 5, COLOR_HGOLD, &tmp_rect, NULL);
        }

        i++;

        /* We have an open container - 'insert' the items inside in the panel */
        if (cpl.container && cpl.container->tag == tmp->tag)
        {
            sprite_blt(Bitmaps[BITMAP_CMARK_START], x + ((i - 1) % INVITEMBELOWXLEN) * 32 + 5, y + ((i - 1) / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);
            tmpc = cpl.sack->inv;

			jump_in_container2:
            for (; tmpc && i < INVITEMBELOWXLEN * INVITEMBELOWYLEN; tmpc = tmpc->next)
            {
                blt_inv_item(tmpc, x + (i % INVITEMBELOWXLEN) * 32 + 5, y + (i / INVITEMBELOWXLEN) * 32 + 19);

                if (i + cpl.win_below_start == cpl.win_below_slot)
                {
                    if (cpl.inventory_win == IWIN_BELOW)
                        slot = BITMAP_INVSLOT;
                    else
                        slot = BITMAP_INVSLOT_U;

                    sprite_blt(Bitmaps[slot], x + (i % INVITEMBELOWXLEN) * 32 + 5, y + (i / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);

                    if (tmpc->nrof > 1)
                        snprintf(buf, sizeof(buf), "%d %s", tmpc->nrof, tmpc->s_name);
                    else
                        snprintf(buf, sizeof(buf), "%s", tmpc->s_name);

                    StringBlt(ScreenSurface, &SystemFont, buf, x + 6, y + 4, COLOR_HGOLD, &tmp_rect, NULL);
                }

                sprite_blt(Bitmaps[BITMAP_CMARK_MIDDLE], x + (i % INVITEMBELOWXLEN) * 32 + 5, y + (i / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);
                i++;
            }

            if (!tmpc)
                sprite_blt(Bitmaps[BITMAP_CMARK_END], x + ((i - 1) % INVITEMBELOWXLEN) * 32 + 5, y + ((i - 1) / INVITEMBELOWXLEN) * 32 + 19, NULL, NULL);
        }
    }
}

#define ICONDEFLEN 32
int blt_inv_item_centered(item *tmp, int x, int y)
{
    register int temp;
	int xstart, xlen, ystart, ylen;
	sint16 anim1;
    SDL_Rect box;
    _BLTFX bltfx;
	bltfx.flags = 0;
	bltfx.dark_level = 0;
    bltfx.surface = NULL;
    bltfx.alpha = 128;

#if 0
    bltfx.flags = 1;
    bltfx.dark_level = 0;
#endif

    if (!FaceList[tmp->face].sprite)
		return 0;

	if (FaceList[tmp->face].sprite->status != SPRITE_STATUS_LOADED)
		return 0;

	anim1 = tmp->face;

	/* This is part of animation... Because ISO items have different offsets and sizes,
	 * we must use ONE sprite base offset to center an animation over an animation.
	 * we use the first frame of an animation for it.*/
	if (tmp->animation_id > 0)
	{
		check_animation_status(tmp->animation_id);

		/* First bitmap of this animation */
		if (animations[tmp->animation_id].num_animations && animations[tmp->animation_id].facings <= 1)
			anim1 = animations[tmp->animation_id].faces[0];
	}

	/* Fallback: first animation bitmap not loaded */
	if (!FaceList[anim1].sprite)
		anim1 = tmp->face;

	/* Also fallback here */
	if (FaceList[anim1].sprite->status != SPRITE_STATUS_LOADED)
		anim1 = tmp->face;

	xstart = FaceList[anim1].sprite->border_left;
	xlen = FaceList[anim1].sprite->bitmap->w - xstart-FaceList[anim1].sprite->border_right;
	ystart = FaceList[anim1].sprite->border_up;
	ylen = FaceList[anim1].sprite->bitmap->h - ystart-FaceList[anim1].sprite->border_down;

	if (xlen > 32)
	{
        box.w = 32;
		temp = (xlen - 32) / 2;
		box.x = xstart + temp;
		xstart = 0;
	}
	else
	{
		box.w = xlen;
		box.x = xstart;
		xstart = (32 - xlen) / 2;
	}

	if (ylen > 32)
	{
        box.h = 32;
		temp = (ylen - 32) / 2;
		box.y = ystart + temp;
		ystart = 0;
	}
	else
	{
		box.h = ylen;
		box.y = ystart;
		ystart = (32 - ylen) / 2;
	}

	/* Now we have a perfect centered sprite.
	 * But: If this is the start pos of our
	 * first animation and not of our sprite,
	 * we must shift it a bit to insert our
	 * face exactly. */
	if (anim1 != tmp->face)
	{
		temp = xstart-box.x;

		box.x = 0;
		box.w = FaceList[tmp->face].sprite->bitmap->w;
		xstart = temp;

		temp = ystart - box.y + (FaceList[anim1].sprite->bitmap->h - FaceList[tmp->face].sprite->bitmap->h);
		box.y = 0;
		box.h = FaceList[tmp->face].sprite->bitmap->h;
		ystart = temp;

		if (xstart < 0)
		{
			box.x = -xstart;
			box.w = FaceList[tmp->face].sprite->bitmap->w + xstart;

			if (box.w > 32)
				box.w = 32;

			xstart = 0;
		}
		else
		{
			if (box.w + xstart > 32)
				box.w -= ((box.w + xstart) - 32);
		}

		if (ystart < 0)
		{
			box.y = -ystart;
			box.h = FaceList[tmp->face].sprite->bitmap->h + ystart;

			if (box.h > 32)
				box.h = 32;

			ystart = 0;
		}
		else
		{
			if (box.h + ystart > 32)
				box.h -= ((box.h + ystart) - 32);
		}
	}

	if (tmp->flagsval & F_INVISIBLE)
		bltfx.flags = BLTFX_FLAG_SRCALPHA | BLTFX_FLAG_GREY;

	if (tmp->flagsval & F_ETHEREAL)
		bltfx.flags = BLTFX_FLAG_SRCALPHA;

    sprite_blt(FaceList[tmp->face].sprite, x + xstart, y + ystart, &box, &bltfx);

    return 1;
}

void blt_inv_item(item *tmp, int x, int y)
{
    blt_inv_item_centered(tmp, x, y);

	if (tmp->nrof > 1)
	{
		char buf[64];

        if (tmp->nrof > 9999)
			snprintf(buf, sizeof(buf), "many");
		else
			snprintf(buf, sizeof(buf), "%d", tmp->nrof);

		StringBlt(ScreenSurface, &Font6x3Out, buf, x + (ICONDEFLEN / 2) - (get_string_pixel_length(buf, &Font6x3Out) / 2), y + 18, COLOR_WHITE, NULL, NULL);
	}

	if (tmp->locked)
		sprite_blt(Bitmaps[BITMAP_LOCK], x, y + ICONDEFLEN - Bitmaps[BITMAP_LOCK]->bitmap->w - 2, NULL, NULL);

	/* Applied and unpaid same spot - can't apply unpaid items */
    if (tmp->applied)
		sprite_blt(Bitmaps[BITMAP_APPLY], x, y, NULL, NULL);

    if (tmp->unpaid)
		sprite_blt(Bitmaps[BITMAP_UNPAID], x, y, NULL, NULL);

    if (tmp->magical)
		sprite_blt(Bitmaps[BITMAP_MAGIC], x + ICONDEFLEN - Bitmaps[BITMAP_MAGIC]->bitmap->w - 2, y + ICONDEFLEN-Bitmaps[BITMAP_MAGIC]->bitmap->h - 2, NULL, NULL);

	if (tmp->cursed)
		sprite_blt(Bitmaps[BITMAP_CURSED], x + ICONDEFLEN-Bitmaps[BITMAP_CURSED]->bitmap->w - 2, y, NULL, NULL);

	if (tmp->damned)
		sprite_blt(Bitmaps[BITMAP_DAMNED], x + ICONDEFLEN - Bitmaps[BITMAP_DAMNED]->bitmap->w - 2, y, NULL, NULL);

	if (tmp->traped)
		sprite_blt(Bitmaps[BITMAP_TRAPED], x + 8, y + 7, NULL, NULL);
}

void examine_range_inv()
{
	register item *op, *tmp;

	op = cpl.ob;

	if (!op->inv)
		return;

    fire_mode_tab[FIRE_MODE_BOW].item = FIRE_ITEM_NO;
    fire_mode_tab[FIRE_MODE_WAND].item = FIRE_ITEM_NO;

	for (tmp = op->inv; tmp; tmp = tmp->next)
	{
		if (tmp->applied && tmp->itype == TYPE_BOW)
		{
			fire_mode_tab[FIRE_MODE_BOW].item = tmp->tag;
		}
		else if (tmp->applied && (tmp->itype == TYPE_WAND || tmp->itype == TYPE_ROD || tmp->itype == TYPE_HORN))
		{
			fire_mode_tab[FIRE_MODE_WAND].item = tmp->tag;
		}
	}
}

/* For throwing and amunnition, we need a "ready/apply/use" mechanism.
 * I used the mark cmd, because we can include this selection mechanism
 * inside the old cmd structure. Also, the mark cmd is extended with it.
 * Notice, that the range part of the mark cmd is client side only...
 * The server has no knowledge about this double use. It marks the item too
 * but we don't care about it. That's because we have to mark always in only very
 * special cases (like ignite something or mark an item for enchanting).
 * Here are the only double effects, that we want server side mark something
 * but we set the item then too as throw/amunnition on client side.
 * Here the player must take care and remark the right range item
 * because range selection is a fight action and marking items server side
 * is not, we will have no bad game play effects. MT. */
void examine_range_marks(int tag)
{
    register item *op, *tmp;
    char buf[256];

    op = cpl.ob;

    if (!op->inv)
		return;

	/* lets check the inventory for the marked item */
	for (tmp = op->inv; tmp; tmp = tmp->next)
	{
		/* if item is in inventory, check the stats and adjust the range table */
		if (tmp->tag == tag)
		{
#if 0
			snprintf(buf, sizeof(buf), "GO ready %s (%d %d).", tmp->s_name, tmp->stype, tmp->stype & 128);
			draw_info(buf, COLOR_WHITE);
#endif

			if ((tmp->itype == TYPE_ARROW && !(tmp->stype & 128)) || tmp->itype == TYPE_CONTAINER)
            {
#if 0
				snprintf(buf, sizeof(buf), "GO1 ready %s.", tmp->s_name);
				draw_info(buf, COLOR_WHITE);
#endif
				if (fire_mode_tab[FIRE_MODE_BOW].amun == tmp->tag)
                {
                    snprintf(buf, sizeof(buf), "Unready %s.", tmp->s_name);
                    draw_info(buf, COLOR_WHITE);

                    fire_mode_tab[FIRE_MODE_BOW].amun = FIRE_ITEM_NO;
                }
                else if (fire_mode_tab[FIRE_MODE_BOW].item != FIRE_ITEM_NO)
                {
                    snprintf(buf, sizeof(buf), "Ready %s as ammunition.", tmp->s_name);
                    draw_info(buf, COLOR_WHITE);

                    fire_mode_tab[FIRE_MODE_BOW].amun = tmp->tag;
                }

                return;
            }
            else if ((tmp->itype == TYPE_POTION && tmp->stype & 128) || (tmp->stype & 128 && tmp->itype != TYPE_WEAPON) || (tmp->itype == TYPE_WEAPON && tmp->applied))
            {
#if 0
				snprintf(buf, sizeof(buf), "GO2 ready %s. (%d)", tmp->s_name, tmp->stype & 128);
  				draw_info(buf, COLOR_WHITE);
#endif
                if (fire_mode_tab[FIRE_MODE_THROW].item == tmp->tag)
                {
                    snprintf(buf, sizeof(buf), "Unready %s.", tmp->s_name);
                    draw_info(buf, COLOR_WHITE);

                    fire_mode_tab[FIRE_MODE_THROW].item = FIRE_ITEM_NO;
                }
                else
                {
                    snprintf(buf, sizeof(buf), "Ready %s for throwing.", tmp->s_name);
                    draw_info(buf, COLOR_WHITE);

                    fire_mode_tab[FIRE_MODE_THROW].item = tmp->tag;
                }

                return;
            }

            if (tmp->itype == TYPE_WEAPON && !(tmp->applied))
                snprintf(buf, sizeof(buf), "Can't ready unapplied weapon %s", tmp->s_name);
            else
                snprintf(buf, sizeof(buf), "Can't throw %s.", tmp->s_name);

			draw_info(buf, COLOR_WHITE);
		}
	}
}
