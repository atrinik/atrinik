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
 * Map effects handling. */

#include <include.h>

/** Linked list of possible effects. */
static effect_struct *effects = NULL;
/** Current effect. */
static effect_struct *current_effect = NULL;

/**
 * Initialize effects from file. */
void effects_init()
{
	FILE *fp;
	char buf[MAX_BUF], *cp;
	effect_struct *effect = NULL;
	effect_sprite_def *sprite_def = NULL;

	/* Try to deinitialize all effects first. */
	effects_deinit();

	fp = server_file_open(SERVER_FILE_EFFECTS);

	if (!fp)
	{
		return;
	}

	/* Read the file... */
	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		/* Ignore comments and blank lines. */
		if (buf[0] == '#' || buf[0] == '\n')
		{
			continue;
		}

		/* End a previous 'effect xxx' or 'sprite xxx' block. */
		if (!strcmp(buf, "end\n"))
		{
			/* Inside effect block. */
			if (effect)
			{
				/* Inside sprite block. */
				if (sprite_def)
				{
					/* Add this sprite to the linked list. */
					sprite_def->next = effect->sprite_defs;
					effect->sprite_defs = sprite_def;
					/* Update total chance value. */
					effect->chance_total += sprite_def->chance;
					sprite_def = NULL;
				}
				/* Inside effect block. */
				else
				{
					/* Add this effect to the linked list of effects. */
					effect->next = effects;
					effects = effect;
					effect = NULL;
				}
			}

			continue;
		}

		cp = strrchr(buf, '\n');

		/* Eliminate newline. */
		if (cp)
		{
			*cp = '\0';
		}

		/* Parse definitions inside sprite block. */
		if (sprite_def)
		{
			if (!strncmp(buf, "chance ", 7))
			{
				sprite_def->chance = atoi(buf + 7);
			}
			else if (!strncmp(buf, "weight ", 7))
			{
				sprite_def->weight = atof(buf + 7);
			}
			else if (!strncmp(buf, "weight_mod ", 11))
			{
				sprite_def->weight_mod = atof(buf + 11);
			}
			else if (!strncmp(buf, "delay ", 6))
			{
				sprite_def->delay = atoi(buf + 6);
			}
			else if (!strncmp(buf, "wind ", 5))
			{
				sprite_def->wind = atoi(buf + 5);
			}
			else if (!strncmp(buf, "wiggle ", 7))
			{
				sprite_def->wiggle = atof(buf + 7);
			}
			else if (!strncmp(buf, "wind_mod ", 9))
			{
				sprite_def->wind_mod = atof(buf + 9);
			}
			else if (!strncmp(buf, "x ", 2))
			{
				sprite_def->x = atoi(buf + 2);
			}
			else if (!strncmp(buf, "y ", 2))
			{
				sprite_def->y = atoi(buf + 2);
			}
			else if (!strncmp(buf, "xpos ", 5))
			{
				sprite_def->xpos = atoi(buf + 5);
			}
			else if (!strncmp(buf, "ypos ", 5))
			{
				sprite_def->ypos = atoi(buf + 5);
			}
			else if (!strncmp(buf, "reverse ", 8))
			{
				sprite_def->reverse = atoi(buf + 8);
			}
		}
		/* Parse definitions inside effect block. */
		else if (effect)
		{
			if (!strncmp(buf, "wind_chance ", 12))
			{
				effect->wind_chance = atof(buf + 12);
			}
			else if (!strncmp(buf, "sprite_chance ", 14))
			{
				effect->sprite_chance = atof(buf + 14);
			}
			else if (!strncmp(buf, "delay ", 6))
			{
				effect->delay = atoi(buf + 6);
			}
			else if (!strncmp(buf, "wind_blow_dir ", 14))
			{
				effect->wind_blow_dir = atoi(buf + 14);
			}
			else if (!strncmp(buf, "max_sprites ", 12))
			{
				effect->max_sprites = atoi(buf + 12);
			}
			else if (!strncmp(buf, "wind_mod ", 9))
			{
				effect->wind_mod = atof(buf + 9);
			}
			else if (!strncmp(buf, "sprites_per_move ", 17))
			{
				effect->sprites_per_move = atoi(buf + 17);
			}
			/* Start of sprite block. */
			else if (!strncmp(buf, "sprite ", 7))
			{
				sprite_def = calloc(1, sizeof(*sprite_def));
				/* Store the sprite ID. */
				sprite_def->id = get_bmap_id(buf + 7);
				/* Initialize default values. */
				sprite_def->chance = 1;
				sprite_def->weight = 1.0;
				sprite_def->weight_mod = 2.0;
				sprite_def->wind = 1;
				sprite_def->wiggle = 1.0;
				sprite_def->wind_mod = 1.0;
				sprite_def->x = -1;
				sprite_def->y = -1;
				sprite_def->reverse = 0;
			}
		}
		/* Start of effect block. */
		else if (!strncmp(buf, "effect ", 7))
		{
			effect = calloc(1, sizeof(effect_struct));
			/* Store the effect unique name. */
			strncpy(effect->name, buf + 7, sizeof(effect->name) - 1);
			effect->name[sizeof(effect->name) - 1] = '\0';
			/* Initialize default values. */
			effect->wind_chance = 0.98;
			effect->sprite_chance = 60.0;
			effect->wind_blow_dir = WIND_BLOW_RANDOM;
			effect->wind_mod = 1.0;
			effect->max_sprites = -1;
			effect->sprites_per_move = 1;
		}
	}

	/* Close the file. */
	fclose(fp);
}

/**
 * Deinitialize ::effects linked list. */
void effects_deinit()
{
	effect_struct *effect, *effect_next;
	effect_sprite_def *sprite_def, *sprite_def_next;

	/* Deinitialize all effects. */
	for (effect = effects; effect; effect = effect_next)
	{
		effect_next = effect->next;

		/* Deinitialize the effect's sprite definitions. */
		for (sprite_def = effect->sprite_defs; sprite_def; sprite_def = sprite_def_next)
		{
			sprite_def_next = sprite_def->next;
			effect_sprite_def_free(sprite_def);
		}

		/* Deinitialize the shown sprites and the actual effect. */
		effect_sprites_free(effect);
		effect_free(effect);
	}

	effects = current_effect = NULL;
}

/**
 * Deinitialize shown sprites of a single effect.
 * @param effect The effect to have shown sprites deinitialized. */
void effect_sprites_free(effect_struct *effect)
{
	effect_sprite *tmp, *next;

	for (tmp = effect->sprites; tmp; tmp = next)
	{
		next = tmp->next;
		effect_sprite_free(tmp);
	}

	effect->sprites = effect->sprites_end = NULL;
}

/**
 * Deinitialize a single effect.
 * @param effect Effect that will be freed. */
void effect_free(effect_struct *effect)
{
	free(effect);
}

/**
 * Deinitialize a single sprite definition.
 * @param sprite_def Sprite definition that will be freed. */
void effect_sprite_def_free(effect_sprite_def *sprite_def)
{
	free(sprite_def);
}

/**
 * Deinitialize a single shown sprite.
 * @param sprite Sprite that will be freed. */
void effect_sprite_free(effect_sprite *sprite)
{
	free(sprite);
}

/**
 * Remove a single shown sprite from the linked list and free it.
 * @param sprite Sprite to remove and free. */
void effect_sprite_remove(effect_sprite *sprite)
{
	if (!sprite || !current_effect)
	{
		return;
	}

	if (sprite->prev)
	{
		sprite->prev->next = sprite->next;
	}
	else
	{
		current_effect->sprites = sprite->next;
	}

	if (sprite->next)
	{
		sprite->next->prev = sprite->prev;
	}
	else
	{
		current_effect->sprites_end = sprite->prev;
	}

	effect_sprite_free(sprite);
}

/**
 * Allocate a new sprite object and add it to the link of currently shown sprites.
 *
 * A random sprite definition object will be chosen.
 * @param effect Effect this is being done for.
 * @param x Initial X position.
 * @param y Initial Y position.
 * @return The created sprite. */
effect_sprite *effect_sprite_add(effect_struct *effect, int x, int y)
{
	int roll;
	effect_sprite_def *tmp;
	effect_sprite *sprite;

	/* Choose which sprite to use. */
	roll = rndm(1, effect->chance_total) - 1;

	for (tmp = effect->sprite_defs; tmp; tmp = tmp->next)
	{
		roll -= tmp->chance;

		if (roll < 0)
		{
			break;
		}
	}

	if (!tmp)
	{
		return NULL;
	}

	/* Allocate a new sprite. */
	sprite = calloc(1, sizeof(*sprite));
	sprite->def = tmp;
	sprite->x = sprite->def->x == -1 ? x : options.mapstart_x + sprite->def->x;
	sprite->y = sprite->def->y == -1 ? y : options.mapstart_y + sprite->def->y;

	/* Add it to the linked list. */
	if (!effect->sprites)
	{
		effect->sprites = effect->sprites_end = sprite;
	}
	else
	{
		effect->sprites_end->next = sprite;
		sprite->prev = effect->sprites_end;
		effect->sprites_end = sprite;
	}

	return sprite;
}

/**
 * Try to play effect sprites. */
void effect_sprites_play()
{
	effect_sprite *tmp, *next;
	int num_sprites = 0;

	/* No current effect or not playing, quit. */
	if (!current_effect || GameStatus != GAME_STATUS_PLAY)
	{
		return;
	}

	for (tmp = current_effect->sprites; tmp; tmp = next)
	{
		next = tmp->next;

		/* Off-screen? */
		if (tmp->x < options.mapstart_x || options.mapstart_x + tmp->x + FaceList[tmp->def->id].sprite->bitmap->w > options.mapstart_x + MAP_TILE_POS_XOFF * options.map_size_x || tmp->y < options.mapstart_y || options.mapstart_y + tmp->y + FaceList[tmp->def->id].sprite->bitmap->h > options.mapstart_y + MAP_START_YOFF + MAP_TILE_POS_YOFF * options.map_size_y)
		{
			effect_sprite_remove(tmp);
			continue;
		}

		/* Show the sprite. */
		sprite_blt(FaceList[tmp->def->id].sprite, options.mapstart_x + tmp->x, options.mapstart_y + tmp->y, NULL, NULL);
		num_sprites++;

		/* Move it if there is no delay configured or if enough time has passed. */
		if (!tmp->def->delay || !tmp->delay_ticks || SDL_GetTicks() - tmp->delay_ticks > tmp->def->delay)
		{
			int ypos = tmp->def->weight * tmp->def->weight_mod;

			if (tmp->def->reverse)
			{
				ypos = -ypos;
			}

			tmp->y += ypos;
			tmp->x += (-1.0 + 3.0 * RANDOM() / (RAND_MAX + 1.0)) * tmp->def->wiggle;

			/* Apply wind. */
			if (tmp->def->wind && current_effect->wind_blow_dir != WIND_BLOW_NONE)
			{
				tmp->x += ((double) current_effect->wind / tmp->def->weight + tmp->def->weight * tmp->def->weight_mod * ((-1.0 + 2.0 * RANDOM() / (RAND_MAX + 1.0)) * tmp->def->wind_mod));
			}

			tmp->delay_ticks = SDL_GetTicks();
		}
	}

	/* Change wind direction... */
	if (current_effect->wind_blow_dir == WIND_BLOW_RANDOM && current_effect->wind_chance != 1.0 && (current_effect->wind_chance == 0.0 || RANDOM() / (RAND_MAX + 1.0) >= current_effect->wind_chance))
	{
		current_effect->wind += (-2.0 + 4.0 * RANDOM() / (RAND_MAX + 1.0)) * current_effect->wind_mod;
	}

	if (current_effect->wind_blow_dir == WIND_BLOW_LEFT)
	{
		current_effect->wind = -1.0 * current_effect->wind_mod;
	}
	else if (current_effect->wind_blow_dir == WIND_BLOW_RIGHT)
	{
		current_effect->wind = 1.0 * current_effect->wind_mod;
	}

	if ((current_effect->max_sprites == -1 || num_sprites < current_effect->max_sprites) && (!current_effect->delay || !current_effect->delay_ticks || SDL_GetTicks() - current_effect->delay_ticks > current_effect->delay) && RANDOM() / (RAND_MAX + 1.0) >= (100.0 - current_effect->sprite_chance) / 100.0)
	{
		int i, x, y;
		effect_sprite *sprite;

		for (i = 0; i < current_effect->sprites_per_move; i++)
		{
			/* Calculate where to put the sprite. */
			x = (double) ScreenSurfaceMap->w * RANDOM() / (RAND_MAX + 1.0);
			y = options.mapstart_y + (double) 60 * RANDOM() / (RAND_MAX + 1.0);

			/* Add the sprite. */
			sprite = effect_sprite_add(current_effect, x, y);

			if (!sprite)
			{
				return;
			}

			/* Invalid sprite. */
			if (sprite->def->id == -1 || !FaceList[sprite->def->id].sprite)
			{
				effect_sprite_remove(sprite);
				return;
			}

			if (sprite->def->reverse)
			{
				sprite->y = MAP_START_YOFF + MAP_TILE_POS_YOFF * options.map_size_y - FaceList[sprite->def->id].sprite->bitmap->h;
			}

			sprite->x += sprite->def->xpos;
			sprite->y += sprite->def->ypos;
		}

		current_effect->delay_ticks = SDL_GetTicks();
	}
}

/**
 * Start an effect identified by its name.
 * @param name Name of the effect to start.
 * @return 1 if the effect was started, 0 otherwise. */
int effect_start(const char *name)
{
	effect_struct *tmp;

	for (tmp = effects; tmp; tmp = tmp->next)
	{
		if (!strcmp(tmp->name, name))
		{
			tmp->wind = 0;
			current_effect = tmp;
			return 1;
		}
	}

	return 0;
}

/**
 * Stop currently playing effect. */
void effect_stop()
{
	if (!current_effect)
	{
		return;
	}

	effect_sprites_free(current_effect);
	current_effect = NULL;
}
