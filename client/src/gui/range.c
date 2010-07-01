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

/** Default icon length */
#define ICONDEFLEN 32

/**
 * Locate and return the name of range item.
 * @param tag Item tag
 * @return "Nothing" if no range item, the range item name otherwise */
static char *get_range_item_name(int tag)
{
	item *tmp;

	if (tag != FIRE_ITEM_NO)
	{
		tmp = locate_item(tag);

		if (tmp)
			return tmp->s_name;
	}

	return "Nothing";
}

/**
 * Mouse event on range widget.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param event SDL event type
 * @param MEvent Mouse event type (MOUSE_DOWN, MOUSE_UP) */
void widget_range_event(widgetdata *widget, int x, int y, SDL_Event event, int MEvent)
{
	if (x > widget->x1 + 5 && x < widget->x1 + 38 && y >= widget->y1 + 3 && y <= widget->y1 + 33)
	{
		if (MEvent == MOUSE_DOWN)
		{
			if (event.button.button == SDL_BUTTON_LEFT)
				process_macro_keys(KEYFUNC_RANGE, 0);
			/* Mousewheel up */
			else if (event.button.button == 4)
				process_macro_keys(KEYFUNC_RANGE, 0);
		}
		else if (MEvent == MOUSE_UP)
		{
			if (draggingInvItem(DRAG_GET_STATUS) > DRAG_IWIN_BELOW)
			{
				/* KEYFUNC_APPLY and KEYFUNC_DROP works only if cpl.inventory_win = IWIN_INV. The tag must
				 * be placed in cpl.win_inv_tag. So we do this and after DnD we restore the old values. */
				int old_inv_win = cpl.inventory_win;
				int old_inv_tag = cpl.win_inv_tag;
				cpl.inventory_win = IWIN_INV;

				/* Range field */
				if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV && x >= widget->x1 && x <= widget->x1 + 78 && y >= widget->y1 && y <= widget->y1 + 35)
				{
					RangeFireMode = 4;

					/* Drop to player doll */
					process_macro_keys(KEYFUNC_APPLY, 0);
				}

				cpl.inventory_win = old_inv_win;
				cpl.win_inv_tag = old_inv_tag;
			}
		}
	}
}

/**
 * Show range widget.
 * @param x X position of the range
 * @param y Y position of the range */
void widget_show_range(widgetdata *widget)
{
	char buf[256];
	SDL_Rect rec_range;
	SDL_Rect rec_item;
	item *op;
	item *tmp;

	rec_range.w = 160;
	rec_item.w = 185;
	examine_range_inv();

	sprite_blt(Bitmaps[BITMAP_RANGE], widget->x1 - 2, widget->y1, NULL, NULL);

	switch (RangeFireMode)
	{
		case FIRE_MODE_BOW:
			if (fire_mode_tab[FIRE_MODE_BOW].item != FIRE_ITEM_NO)
			{
				snprintf(buf, sizeof(buf), "using %s", get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].item));
				blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_BOW].item, widget->x1 + 3, widget->y1 + 2);

				StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, &rec_range, NULL);

				if (fire_mode_tab[FIRE_MODE_BOW].amun != FIRE_ITEM_NO)
				{
					tmp = locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_BOW].amun);

					if (tmp)
					{
						if (tmp->itype == TYPE_ARROW)
							snprintf(buf, sizeof(buf), "ammo %s (%d)", get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].amun), tmp->nrof);
						else
							snprintf(buf, sizeof(buf), "ammo %s", get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].amun));
					}
					else
						snprintf(buf, sizeof(buf), "ammo not selected");

					blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_BOW].amun, widget->x1 + 43, widget->y1 + 2);
				}
				else
				{
					snprintf(buf, sizeof(buf), "ammo not selected");
				}

				StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
			}
			else
			{
				StringBlt(ScreenSurface, &SystemFont, "no range weapon applied", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, &rec_range, NULL);
			}

			sprite_blt(Bitmaps[BITMAP_RANGE_MARKER], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
			break;

			/* Wands, staves, rods and horns */
		case FIRE_MODE_WAND:
			if (!locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_WAND].item))
				fire_mode_tab[FIRE_MODE_WAND].item = FIRE_ITEM_NO;

			if (fire_mode_tab[FIRE_MODE_WAND].item != FIRE_ITEM_NO)
			{
				snprintf(buf, sizeof(buf), "%s", get_range_item_name(fire_mode_tab[FIRE_MODE_WAND].item));
				StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
				sprite_blt(Bitmaps[BITMAP_RANGE_TOOL], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_WAND].item, widget->x1 + 43, widget->y1 + 2);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_TOOL_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "nothing applied", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
			}

			StringBlt(ScreenSurface, &SystemFont, "use range tool", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, &rec_range, NULL);
			break;

			/* The summon range ctrl will come from server only after the player cast a summon spell */
		case FIRE_MODE_SUMMON:
			if (fire_mode_tab[FIRE_MODE_SUMMON].item != FIRE_ITEM_NO)
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_CTRL], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, fire_mode_tab[FIRE_MODE_SUMMON].name, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, NULL, NULL);
				sprite_blt(FaceList[fire_mode_tab[FIRE_MODE_SUMMON].item].sprite, widget->x1 + 43, widget->y1 + 2, NULL, NULL);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_CTRL_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "no golem summoned", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
			}

			StringBlt(ScreenSurface, &SystemFont, "mind control", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, &rec_item, NULL);
			break;

			/* These are client only, no server signal needed */
		case FIRE_MODE_SKILL:
			if (fire_mode_tab[FIRE_MODE_SKILL].skill)
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_SKILL], widget->x1 + 3, widget->y1 + 2, NULL, NULL);

				if (fire_mode_tab[FIRE_MODE_SKILL].skill->flag != -1)
				{
					sprite_blt(fire_mode_tab[FIRE_MODE_SKILL].skill->icon, widget->x1 + 43, widget->y1 + 2, NULL, NULL);
					StringBlt(ScreenSurface, &SystemFont, fire_mode_tab[FIRE_MODE_SKILL].skill->name, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
				}
				else
					fire_mode_tab[FIRE_MODE_SKILL].skill = NULL;
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_SKILL_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "no skill selected", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
			}

			StringBlt(ScreenSurface, &SystemFont, "use skill", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, &rec_range, NULL);

			break;

		case FIRE_MODE_SPELL:
			if (fire_mode_tab[FIRE_MODE_SPELL].spell)
			{
				/* We use wizard spells as default */
				sprite_blt(Bitmaps[BITMAP_RANGE_WIZARD], widget->x1 + 3, widget->y1 + 2, NULL, NULL);

				if (fire_mode_tab[FIRE_MODE_SPELL].spell->flag != -1)
				{
					sprite_blt(fire_mode_tab[FIRE_MODE_SPELL].spell->icon, widget->x1 + 43, widget->y1 + 2, NULL, NULL);
					StringBlt(ScreenSurface, &SystemFont, fire_mode_tab[FIRE_MODE_SPELL].spell->name, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
				}
				else
					fire_mode_tab[FIRE_MODE_SPELL].spell = NULL;
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_WIZARD_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "no spell selected", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
			}

			StringBlt(ScreenSurface, &SystemFont, "cast spell", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, &rec_range, NULL);

			break;

		case FIRE_MODE_THROW:
			if (!(op = locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_THROW].item)))
				fire_mode_tab[FIRE_MODE_THROW].item = FIRE_ITEM_NO;

			if (fire_mode_tab[FIRE_MODE_THROW].item != FIRE_ITEM_NO)
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_THROW], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_THROW].item, widget->x1 + 43, widget->y1 + 2);

				if (op->nrof > 1)
				{
					if (op->nrof > 9999)
						snprintf(buf, sizeof(buf), "many");
					else
						snprintf(buf, sizeof(buf), "%d",op->nrof);

					StringBlt(ScreenSurface, &Font6x3Out, buf, widget->x1 + 43 + (ICONDEFLEN / 2) - (get_string_pixel_length(buf, &Font6x3Out) / 2), widget->y1 + 22, COLOR_WHITE, NULL, NULL);
				}

				snprintf(buf, sizeof(buf), "%s", get_range_item_name(fire_mode_tab[FIRE_MODE_THROW].item));
				StringBlt(ScreenSurface, &SystemFont,buf, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_THROW_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "no item ready", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, &rec_item, NULL);
			}

			snprintf(buf, sizeof(buf), "throw item");
			StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, &rec_range, NULL);

			break;
	}
}
