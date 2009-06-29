%{

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


#include <global.h>
#include <loader.h>
#include <newserver.h>
#include <sproto.h>

/*  ****** List of commands used in arch, maps & artifavt file ******

	"Object",		"name",			"race",			"slaying",		"msg",
	"endmsg",		"arch",			"other_arch",	"More",			"animation",
	"end",			"last_heal",	"last_sp",		"last_grace",	"last_eat",
	"speed",		"speed_left",	"slow_move",	"face",			"Str",
	"Dex",			"Con",			"Wis",			"Cha",			"Int",
	"Pow",			"hp",			"maxhp",		"sp",			"maxsp",
	"grace",		"maxgrace",		"exp",			"food",			"dam",
	"wc",			"ac",			"x",			"y",			"nrof",
	"level",		"direction",	"type",			"material",		"value",
	"weight",		"carrying",		"attacktype",	"path_attuned",	"path_repelled",
	"path_denied",	"sys_object",	"magic",		"state",		"alive",
	"applied",		"unpaid",		"no_pick",
	"no_pass",		"walk_on",		"walk_off",		"fly_on",		"fly_off",
	"is_animated",	"flying",		"monster",		"no_attack",	"invulnerable",
	"friendly",		"generator",	"is_thrown",	"auto_apply",	"treasure",
	"see_invisible","can_roll",		"is_turnable",	"is_used_up",	"identified",
	"reflecting",	"changing",		"splitting",	"hitback",		"startequip",
	"blocksview",	"undead",		"scared",		"unaggressive",	"reflect_missile",
	"reflect_spell","no_magic",		"wiz",			"was_wiz",		"no_fix_player",
	"tear_down",	"luck",			"run_away",		"pass_thru",	"can_pass_thru",
	"pick_up",		"anim_speed",	"container",	"no_drop",		"will_apply",
	"can_cast_spell","can_use_scroll","can_use_range","can_use_bow","can_use_armour",
	"can_use_weapon","can_use_ring","has_ready_range","has_ready_bow","xrays",
	"is_floor",		"lifesave",		"sleep",		"stand_still",	"random_move",
	"only_attack",	"attack_movement","move_state",	"confused",		"stealth",
	"connected",	"cursed",		"damned",		"see_anywhere",	"known_magical",
	"known_cursed",	"can_use_skill","been_applied",	"title"			"unique",
	"inv_locked",	"is_wooded",	"is_hilly",		"has_ready_skill",
	"has_ready_weapon","no_skill_ident","glow_radius","is_blind",	"can_see_in_dark",
	"is_cauldron",	"randomitems",	"is_dust",		"no_steal",		"one_hit",
	"berserk",		"sub_type",		"terrain_flag",	"terrain_type", "item_quality",
	"item_condition","item_race",	"item_skill",	"item_level",	"material_real",
	 "mpart_id",	"mpart_nr",		"weapon_speed",	"editor_folder","is_magical",
	"is_vul_elemental","is_proof_elemental","is_vul_magic","is_proof_magic","is_vul_physical",
	"is_proof_physical","is_vul_sphere","is_proof_sphere","is_indestructible","is_male",
	"is_female","is_evil", "is_good","is_neutral","is_invisible",
	"layer","is_aged","make_invisible", "make_ethereal","is_player",
	"use_fix_pos", "inv_face", "inv_animation", "is_ethereal", "is_named",
	"object_int1" <-- thats enemy_count
	"object_int2" <-- thats attacked_by_count
	"object_int3" <-- thats ownercount
	"no_teleport", "corpse", "corpse_forced"
    "player_only" <-- only player can enter flag
	"no_cleric"
	"one_drop"
	"cursed_perm"
	"damned_perm"
	"wc_range"
	"door_closed"
	"item_level_art" <- special token from artifact lists"
	"was_reflected"
	"is_assassin"
*/

#define YY_DECL int lex_load(object *op, int map_flags)

static char *yval();

static void *cur_buffer; /* thats needed to track the used buffers for recursive calling */
static int object_load_flag;
static int lex_error, msg_flag;
static char msgbuf[HUGE_BUF];

#define SET_OR_CLEAR_FLAG(op, flag, val) \
	{ if (val) SET_FLAG(op, flag); else CLEAR_FLAG(op, flag); }

/* SET_RESIST is really only really needed for transition code.  We normally
 * don't care about multiple values overwriting each other, but this is
 * to catch items that have multiple protection/immune/vulnerable.
 * This can be simplified later on to just do the set after all the archs
 * and maps have been updated.
 * We always keep the last value because otherwise the value from the
 * arch may take precedence.
 * Unfortunately, we will report warnings here simply because an object has
 * been modified from the arch.
 */
#define SET_RESIST(op, type, val)  op->resist[type] = val;
#define SET_ATTACK(op, type, val)  op->attack[type] = val;
#define SET_PROTECTION(op, type, val)  op->protection[type] = val;

#define IVAL	atoi(yval())
#define FVAL	(float) atof(yval())

%}

LF	\x0A
CR	\x0D
NL	{CR}?{LF}

S	[ \t]+.+
WS	[ \t]*{NL}

%x MESSAGE

/* Don't have to link with -lfl with this */
%option noyywrap

/* need yy_push_state, yy_pop_state */
%option stack

%%

%{
/* Declare some local variables */
    int ismore=0;

    lex_error=0;

%}

^arch{S}         {	/* If op->arch has been set, then this new object
			 * must be part of the inventory.  So process
			 * appropriately.
			 */
			 /* i changed here copy_object() to copy_object() - we have no idea what with the
			  * next commands to speed & speed_left - so its redundant and very likely a bug
			  * to adjust here speed.
			  */
			if (op->arch) {
			    object *tmp;
				float old_sleft,old_speed;

			    tmp=get_object();
			    tmp->arch = find_archetype(yval());
			    if (tmp->arch!=NULL)
				copy_object_data(&tmp->arch->clone,tmp);
			    strcpy(msgbuf, "");
				old_sleft = tmp->speed_left;
				old_speed = tmp->speed;
			    lex_load(tmp, map_flags);
				  /* we set here AND in load_object the material real values.
				   * we do: If item_quality == 0 AND material defines a base
				   * material, then use material_real to set quality & condition.
                   * So, we don't set it for floor or walls or some else objects
                   * were we don't need it.
                   */
				   if(tmp->material && !tmp->item_quality)
				   {
				       tmp->item_quality = material_real[tmp->material_real].quality;
	                   tmp->item_condition = tmp->item_quality;
				   }

				if(QUERY_FLAG(tmp,FLAG_IDENTIFIED))
				{
					SET_FLAG(tmp,FLAG_KNOWN_MAGICAL);
					SET_FLAG(tmp,FLAG_KNOWN_CURSED);
				}
			    insert_ob_in_ob(tmp,op);

				if (!(map_flags & MAP_STYLE))
				{
					/* first adjust speed */
					/* ok - ONLY change something when we
					 * a.) don't have fix set speed_left in the loading
					* b.) we have negative speed
					*/
					if(old_sleft==tmp->speed_left && tmp->speed<0.0f)
					{
						/* if we have now a negative speed - use default arch
						* speed left for new random set
						*/
						if(tmp->speed<0.0f)
							tmp->speed_left=tmp->arch->clone.speed_left+(float)(RANDOM()%90)/100.0f;
					}
					if(tmp->speed)
						update_ob_speed(tmp); /* kick it now on active list*/

				/* we must do this after the insert_ob_in_ob().
				 * now we look we have a treasure or something else.
				 * If so, we transform it now to real items.
				 * also we adjust here auto_apply.
				 * This was moved from fix_auto_apply.
				 * This here only handles inventory items, the "base"
				 * items are handled in load_objects() .
				 */
					if(QUERY_FLAG(tmp,FLAG_AUTO_APPLY)) /* remember: auto_apply will remove FLAG_AUTO_APPLY */
						auto_apply(tmp);
					/* here we can have a interesting problem when we have kind of
					 * creators (like spawn points) which use the inv object as a clone
					 * kind object. Then we don't want substitute the treasure here or the
					 * generator will clone objects with same treasures over and over.
					 */
					 /* i excluded here only objects inside spawn_points. Add here more
					  * what you need to exclude at object/map loading time.
					  */
					else if(tmp->randomitems && (map_flags & MAP_ORIGINAL) &&(op->type != SPAWN_POINT ))
						create_treasure(tmp->randomitems, tmp, 0, get_enviroment_level(tmp),T_STYLE_UNSET,ART_CHANCE_UNSET,0,NULL);

				}
			}
			/* This is the actual archetype definition then */
			else {
			    op->arch=find_archetype(yval());
			    if (op->arch!=NULL)
					copy_object_data(&op->arch->clone,op);
					/* note: speed will handled in load_object() for this */
			}
		    }

^object{S}	    {	char *yv=yval();

			if (*yv=='\0') {
			    LOG(llevError,"Object lacks name.\n");
			    return LL_IGNORED;
			}
			if (op->arch!=NULL)
				FREE_AND_COPY_HASH(op->arch->name, yv);
			FREE_AND_COPY_HASH(op->name, yv);
			object_load_flag = 1; /* ok - now we know the pre speed values are valid */
		    }

^msg{WS}	    {	BEGIN( MESSAGE ); msgbuf[0]='\0'; msg_flag=0;}
<MESSAGE>^endmsg{WS} {	BEGIN( INITIAL );
			FREE_AND_COPY_HASH(op->msg, msgbuf);
			/* Just print a warning so we can be reasonably safe
			 * about not overflowing the buffer.
			 */
			if (strlen(op->msg) > (HUGE_BUF/2))
			    LOG(llevDebug, "\n\tWarning message length > %d (max allowed=%d): %d\n>%.80s<\n",
				HUGE_BUF/2, HUGE_BUF, strlen(op->msg),op->msg);
		    }
<MESSAGE>.*	    {if(msg_flag)strcat(msgbuf,"\n");msg_flag=1;strcat(msgbuf, yytext); }
^name{S}	    {	char *yv=yval();

			if (*yv=='\0') LOG(llevError,"Name without val\n");
			else FREE_AND_COPY_HASH(op->name, yv);
		    }
^race{S}	    FREE_AND_COPY_HASH(op->race,yval());
^slaying{S}	    FREE_AND_COPY_HASH(op->slaying, yval());

^quickslot{S}		op->quickslot = IVAL;

^other_arch{S}        op->other_arch=find_archetype(yval());
^animation{S}	    {
			if (strcmp (yval(), "NONE") == 0)
			    op->animation_id = 0;
			else
			    op->animation_id = find_animation (yval());
		    }
^inv_animation{S}	    {
			if (strcmp (yval(), "NONE") == 0)
			    op->inv_animation_id = 0;
			else
			    op->inv_animation_id = find_animation (yval());
		    }

^more{WS}	    { /* We need to record that this is a multipart object,
		       * so the calling function can glue things back together
		       */
			ismore=1;
		    }

^end{WS}	    { if (ismore) return LL_MORE; else return LL_NORMAL; }
^object_int1{S}		op->enemy_count = IVAL;
^object_int2{S}		op->attacked_by_count = IVAL;
^object_int3{S}		op->ownercount = IVAL;
^last_heal{S}	    op->last_heal = IVAL;
^last_sp{S}			op->last_sp = IVAL;
^last_grace{S}		op->last_grace = IVAL;
^last_eat{S}	    op->last_eat = IVAL;
^speed{S}			op->speed = FVAL;

^speed_left{S}		op->speed_left = FVAL;
^slow_move{S}	    {	SET_SLOW_PENALTY(op,FVAL);
			SET_FLAG(op, FLAG_SLOW_MOVE);
		    }
^face{S}		{
					register int _face_ = FindFace(yval(), 0);
					op->face = &new_faces[_face_ ];

					if(!_face_)
					    LOG(llevBug,"BUG: Object %s - can't find face %s.\n",
							op->arch?(op->arch->name?op->arch->name:"<no arch name>"):"<no ->arch>",yval());
				}
^inv_face{S}	{
					register int _face_ = FindFace(yval(), 0);
					op->inv_face = &new_faces[_face_ ];

					if(!_face_)
					    LOG(llevBug,"BUG: Object %s - can't find inv_face %s.\n",
							op->arch?(op->arch->name?op->arch->name:"<no arch name>"):"<no ->arch>",yval());
				}

^str{S}			op->stats.Str = IVAL;
^dex{S}			op->stats.Dex = IVAL;
^con{S}			op->stats.Con = IVAL;
^wis{S}			op->stats.Wis = IVAL;
^cha{S}			op->stats.Cha = IVAL;
^int{S}			op->stats.Int = IVAL;
^pow{S}			op->stats.Pow = IVAL;
^hp{S}			op->stats.hp = IVAL;
^maxhp{S}		op->stats.maxhp = IVAL;
^sp{S}			op->stats.sp = IVAL;
^maxsp{S}		op->stats.maxsp = IVAL;
^grace{S}		op->stats.grace = IVAL;
^maxgrace{S}	op->stats.maxgrace = IVAL;
^exp{S}			op->stats.exp = atol(yval());
^food{S}		op->stats.food = IVAL;
^dam{S}			op->stats.dam = IVAL;
^dam_add{S}		op->stats.dam += IVAL;
^wc{S}			op->stats.wc = IVAL;
^wc_add{S}		op->stats.wc += IVAL;
^wc_range{S}		op->stats.wc_range = IVAL;
^wc_range_add{S}	op->stats.wc_range += IVAL;
^ac{S}			op->stats.ac = IVAL;
^ac_add{S}		op->stats.ac += IVAL;
^x{S}		{op->x = IVAL;
#ifdef POSITION_DEBUG
op->ox= op->x;
#endif
}
^y{S}		{op->y = IVAL;
#ifdef POSITION_DEBUG
op->oy= op->y;
#endif
}
^nrof{S}					op->nrof= atol(yval());
^level{S}					op->level = IVAL;
^direction{S}				op->facing=op->anim_last_facing =op->direction = (IVAL%9);
^type{S}					op->type = IVAL;
^material{S}				op->material = IVAL;
^value{S}					op->value = IVAL;
^weight{S}					op->weight = atol(yval());
^carrying{S}				op->carrying = atol(yval());
^attacktype{S}			{
							op->attacktype = IVAL;
							/* LOG(llevDebug, "Warning: Object %s has attacktype %d set\n", op->name, op->attacktype); */
						}
^path_attuned{S}			op->path_attuned = IVAL;
^path_repelled{S}			op->path_repelled = IVAL;
^path_denied{S}				op->path_denied = IVAL;
^magic{S}					op->magic = IVAL;
^state{S}					op->state = IVAL;
^hidden{S}					op->hide = IVAL;
^layer{S}					op->layer = IVAL;
^door_closed{S}				SET_OR_CLEAR_FLAG(op, FLAG_DOOR_CLOSED, IVAL);
^cursed_perm{S}				SET_OR_CLEAR_FLAG(op, FLAG_PERM_CURSED, IVAL);
^damned_perm{S}				SET_OR_CLEAR_FLAG(op, FLAG_PERM_DAMNED, IVAL);
^one_drop{S}				SET_OR_CLEAR_FLAG(op, FLAG_ONE_DROP, IVAL);
^is_traped{S}				SET_OR_CLEAR_FLAG(op, FLAG_IS_TRAPED, IVAL);
^quest_item{S}				SET_OR_CLEAR_FLAG(op, FLAG_QUEST_ITEM, IVAL);
^player_only{S}				SET_OR_CLEAR_FLAG(op, FLAG_PLAYER_ONLY, IVAL);
^is_named{S}				SET_OR_CLEAR_FLAG(op, FLAG_IS_NAMED, IVAL);
^is_player{S}				SET_OR_CLEAR_FLAG(op, FLAG_IS_PLAYER, IVAL);
^is_aged{S}					SET_OR_CLEAR_FLAG(op, FLAG_IS_AGED, IVAL);
^sys_object{S}				SET_OR_CLEAR_FLAG(op, FLAG_SYS_OBJECT, IVAL);
^generator{S}				SET_OR_CLEAR_FLAG(op, FLAG_GENERATOR, IVAL);
^is_thrown{S}				SET_OR_CLEAR_FLAG(op, FLAG_IS_THROWN, IVAL);
^auto_apply{S}				SET_OR_CLEAR_FLAG(op, FLAG_AUTO_APPLY, IVAL);
^treasure{S}				SET_OR_CLEAR_FLAG(op, FLAG_TREASURE, IVAL);
^was_reflected{S}			SET_OR_CLEAR_FLAG(op, FLAG_WAS_REFLECTED, IVAL);
^is_assassin{S}				SET_OR_CLEAR_FLAG(op, FLAG_IS_ASSASSINATION, IVAL);
^is_missile{S}				SET_OR_CLEAR_FLAG(op, FLAG_IS_MISSILE, IVAL);
^see_invisible{S}			SET_OR_CLEAR_FLAG(op, FLAG_SEE_INVISIBLE, IVAL);
^make_invisible{S}			SET_OR_CLEAR_FLAG(op, FLAG_MAKE_INVISIBLE, IVAL);
^make_ethereal{S}			SET_OR_CLEAR_FLAG(op, FLAG_MAKE_ETHEREAL, IVAL);
^can_roll{S} 				SET_OR_CLEAR_FLAG(op, FLAG_CAN_ROLL, IVAL);
^is_turnable{S}				SET_OR_CLEAR_FLAG(op, FLAG_IS_TURNABLE, IVAL);
^is_used_up{S}				SET_OR_CLEAR_FLAG(op, FLAG_IS_USED_UP, IVAL);
^is_invisible{S}			SET_OR_CLEAR_FLAG(op, FLAG_IS_INVISIBLE, IVAL);
^alive{S}					SET_OR_CLEAR_FLAG(op, FLAG_ALIVE, IVAL);
^applied{S}					SET_OR_CLEAR_FLAG(op, FLAG_APPLIED, IVAL);
^unpaid{S}					SET_OR_CLEAR_FLAG(op, FLAG_UNPAID, IVAL);
^no_pick{S}					SET_OR_CLEAR_FLAG(op, FLAG_NO_PICK, IVAL);
^no_pass{S}					SET_OR_CLEAR_FLAG(op, FLAG_NO_PASS, IVAL);
^no_teleport{S}				SET_OR_CLEAR_FLAG(op, FLAG_NO_TELEPORT, IVAL);
^corpse{S}					SET_OR_CLEAR_FLAG(op, FLAG_CORPSE, IVAL);
^corpse_forced{S}			SET_OR_CLEAR_FLAG(op, FLAG_CORPSE_FORCED, IVAL);

^walk_on{S}					SET_OR_CLEAR_FLAG(op, FLAG_WALK_ON, IVAL);
^walk_off{S}				SET_OR_CLEAR_FLAG(op, FLAG_WALK_OFF, IVAL);
^fly_on{S}					SET_OR_CLEAR_FLAG(op, FLAG_FLY_ON, IVAL);
^fly_off{S}					SET_OR_CLEAR_FLAG(op, FLAG_FLY_OFF, IVAL);
^is_animated{S}				{
								SET_OR_CLEAR_FLAG(op, FLAG_ANIMATE, IVAL);
								SET_FLAG(op,FLAG_CLIENT_SENT); /* we using this flag for debugging - ignore */
							}
^flying{S}					SET_OR_CLEAR_FLAG(op, FLAG_FLYING, IVAL);
^monster{S}					SET_OR_CLEAR_FLAG(op, FLAG_MONSTER, IVAL);
^no_attack{S}				SET_OR_CLEAR_FLAG(op, FLAG_NO_ATTACK, IVAL);
^invulnerable{S}			SET_OR_CLEAR_FLAG(op, FLAG_INVULNERABLE, IVAL);
^friendly{S}			{
							if (IVAL)
							{
								SET_FLAG(op, FLAG_FRIENDLY);
								add_friendly_object(op);
							}
							else
								CLEAR_FLAG(op, FLAG_FRIENDLY);
						}
^identified{S}	    {	if (IVAL) {
			    SET_FLAG(op, FLAG_IDENTIFIED);
			    CLEAR_FLAG(op, FLAG_KNOWN_MAGICAL);
			}
			else CLEAR_FLAG(op, FLAG_IDENTIFIED);
		    }
^reflecting{S}	    SET_OR_CLEAR_FLAG(op, FLAG_REFLECTING, IVAL);
^changing{S} 	    SET_OR_CLEAR_FLAG(op, FLAG_CHANGING, IVAL);
^splitting{S}	    SET_OR_CLEAR_FLAG(op, FLAG_SPLITTING, IVAL);
^hitback{S}  	    SET_OR_CLEAR_FLAG(op, FLAG_HITBACK, IVAL);
^startequip{S}	    SET_OR_CLEAR_FLAG(op, FLAG_STARTEQUIP, IVAL);
^blocksview{S}	    SET_OR_CLEAR_FLAG(op, FLAG_BLOCKSVIEW, IVAL);
^undead{S}  	    SET_OR_CLEAR_FLAG(op, FLAG_UNDEAD, IVAL);
^scared{S}  	    SET_OR_CLEAR_FLAG(op, FLAG_SCARED, IVAL);
^unaggressive{S}    SET_OR_CLEAR_FLAG(op, FLAG_UNAGGRESSIVE, IVAL);
^reflect_missile{S} SET_OR_CLEAR_FLAG(op, FLAG_REFL_MISSILE, IVAL);
^can_reflect_missile{S} SET_OR_CLEAR_FLAG(op, FLAG_CAN_REFL_MISSILE, IVAL);
^reflect_spell{S}   SET_OR_CLEAR_FLAG(op, FLAG_REFL_SPELL, IVAL);
^can_reflect_spell{S}   SET_OR_CLEAR_FLAG(op, FLAG_CAN_REFL_SPELL, IVAL);
^no_magic{S} 	    SET_OR_CLEAR_FLAG(op, FLAG_NO_MAGIC, IVAL);
^no_cleric{S} 	    SET_OR_CLEAR_FLAG(op, FLAG_NO_CLERIC, IVAL);
^wiz{S}  	    {	if (IVAL) {
			    SET_FLAG(op, FLAG_WIZ);
			    SET_FLAG(op, FLAG_WAS_WIZ);
			    SET_FLAG(op, FLAG_WIZPASS);
			}
			else {
			    CLEAR_FLAG(op, FLAG_WIZ);
			    CLEAR_FLAG(op, FLAG_WIZPASS);
			}
		    }
^was_wiz{S}  	    SET_OR_CLEAR_FLAG(op, FLAG_WAS_WIZ, IVAL);
^no_fix_player{S}   SET_OR_CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER, IVAL);
^tear_down{S}	    SET_OR_CLEAR_FLAG(op, FLAG_TEAR_DOWN, IVAL);
^luck{S}  	    op->stats.luck = IVAL;
^run_away{S}	    op->run_away = IVAL;
^pass_thru{S}	    SET_OR_CLEAR_FLAG(op, FLAG_PASS_THRU, IVAL);
^can_pass_thru{S}   SET_OR_CLEAR_FLAG(op, FLAG_CAN_PASS_THRU, IVAL);
^pick_up{S}	    op->pick_up = IVAL;
^anim_speed{S}	    op->anim_speed = IVAL;
^container{S}	    op->weight_limit = IVAL;
^no_drop{S}	    SET_OR_CLEAR_FLAG(op, FLAG_NO_DROP, IVAL);
^will_apply{S}	    op->will_apply = IVAL;
^use_fix_pos{S}	    SET_OR_CLEAR_FLAG(op, FLAG_USE_FIX_POS, IVAL);
^is_ethereal{S}	    SET_OR_CLEAR_FLAG(op, FLAG_IS_ETHEREAL, IVAL);
^can_cast_spell{S}	    SET_OR_CLEAR_FLAG(op, FLAG_CAST_SPELL, IVAL);
^can_use_scroll{S}	    SET_OR_CLEAR_FLAG(op, FLAG_USE_SCROLL, IVAL);
^can_use_range{S}	    SET_OR_CLEAR_FLAG(op, FLAG_USE_RANGE, IVAL);
^can_use_bow{S}		    SET_OR_CLEAR_FLAG(op, FLAG_USE_BOW, IVAL);
^can_use_armour{S}	    SET_OR_CLEAR_FLAG(op, FLAG_USE_ARMOUR, IVAL);
^can_use_weapon{S}	    SET_OR_CLEAR_FLAG(op, FLAG_USE_WEAPON, IVAL);
^can_use_ring{S}	    SET_OR_CLEAR_FLAG(op, FLAG_USE_RING, IVAL);
^has_ready_range{S}	    SET_OR_CLEAR_FLAG(op, FLAG_READY_RANGE, IVAL);
^has_ready_bow{S}   SET_OR_CLEAR_FLAG(op, FLAG_READY_BOW, IVAL);
^xrays{S}	    SET_OR_CLEAR_FLAG(op, FLAG_XRAYS, IVAL);
^no_save{S}			SET_OR_CLEAR_FLAG(op, FLAG_NO_SAVE, IVAL);
^is_floor{S}	    SET_OR_CLEAR_FLAG(op, FLAG_IS_FLOOR, IVAL);
^is_male{S}		   SET_OR_CLEAR_FLAG(op, FLAG_IS_MALE, IVAL);
^is_female{S}	   SET_OR_CLEAR_FLAG(op, FLAG_IS_FEMALE, IVAL);
^is_evil{S}		   SET_OR_CLEAR_FLAG(op, FLAG_IS_EVIL, IVAL);
^is_good{S}		   SET_OR_CLEAR_FLAG(op, FLAG_IS_GOOD, IVAL);
^is_neutral{S}	   SET_OR_CLEAR_FLAG(op, FLAG_IS_NEUTRAL, IVAL);
^lifesave{S}	    SET_OR_CLEAR_FLAG(op, FLAG_LIFESAVE, IVAL);
^sleep{S}	    {
			SET_OR_CLEAR_FLAG(op, FLAG_SLEEP, IVAL);
			/*(LOG(llevDebug," Warning: Object %s has sleep set in arch.\n",op->name);*/
		    }
^stand_still{S}	    SET_OR_CLEAR_FLAG(op, FLAG_STAND_STILL, IVAL);
^random_move{S}	    SET_OR_CLEAR_FLAG(op, FLAG_RANDOM_MOVE, IVAL);
^only_attack{S}	    SET_OR_CLEAR_FLAG(op, FLAG_ONLY_ATTACK, IVAL);
^berserk{S}	    SET_OR_CLEAR_FLAG(op, FLAG_BERSERK, IVAL);
^is_magical{S}	    SET_OR_CLEAR_FLAG(op, FLAG_IS_MAGICAL, IVAL);

    /* Start of various attacktypes */
^attack_impact{S}	SET_ATTACK(op, ATNR_PHYSICAL, IVAL);
^attack_magic{S}	SET_ATTACK(op, ATNR_MAGIC, IVAL);
^attack_fire{S}		SET_ATTACK(op, ATNR_FIRE, IVAL);
^attack_electricity{S}	SET_ATTACK(op, ATNR_ELECTRICITY, IVAL);
^attack_cold{S}		SET_ATTACK(op, ATNR_COLD, IVAL);
^attack_confusion{S}	SET_ATTACK(op, ATNR_CONFUSION, IVAL);
^attack_acid{S}		SET_ATTACK(op, ATNR_ACID, IVAL);
^attack_drain{S}	SET_ATTACK(op, ATNR_DRAIN, IVAL);
^attack_weaponmagic{S}	SET_ATTACK(op, ATNR_WEAPONMAGIC, IVAL);
^attack_ghosthit{S}	SET_ATTACK(op, ATNR_GHOSTHIT, IVAL);
^attack_poison{S}	SET_ATTACK(op, ATNR_POISON, IVAL);
^attack_slow{S}		SET_ATTACK(op, ATNR_SLOW, IVAL);
^attack_paralyze{S}	SET_ATTACK(op, ATNR_PARALYZE, IVAL);
^attack_time{S}		SET_ATTACK(op, ATNR_TIME, IVAL);
^attack_fear{S}		SET_ATTACK(op, ATNR_FEAR, IVAL);
^attack_cancellation{S}	SET_ATTACK(op, ATNR_CANCELLATION, IVAL);
^attack_depletion{S}	SET_ATTACK(op, ATNR_DEPLETE, IVAL);
^attack_death{S}	SET_ATTACK(op, ATNR_DEATH, IVAL);
^attack_chaos{S}	SET_ATTACK(op, ATNR_CHAOS, IVAL);
^attack_counterspell{S}	SET_ATTACK(op, ATNR_COUNTERSPELL, IVAL);
^attack_godpower{S}	SET_ATTACK(op, ATNR_GODPOWER, IVAL);
^attack_purity{S}	SET_ATTACK(op, ATNR_HOLYWORD, IVAL);
^attack_blind{S}	SET_ATTACK(op, ATNR_BLIND, IVAL);
^attack_internal{S}	SET_ATTACK(op, ATNR_INTERNAL, IVAL);
^attack_life_steal{S}	SET_ATTACK(op, ATNR_LIFE_STEALING, IVAL);
^attack_slash{S}	SET_ATTACK(op, ATNR_SLASH, IVAL);
^attack_cleave{S}	SET_ATTACK(op, ATNR_CLEAVE, IVAL);
^attack_pierce{S}	SET_ATTACK(op, ATNR_PIERCE, IVAL);
^attack_nether{S}	SET_ATTACK(op, ATNR_NETHER, IVAL);
^attack_sonic{S}	SET_ATTACK(op, ATNR_SONIC, IVAL);
^attack_demonic{S}	SET_ATTACK(op, ATNR_DEMONIC, IVAL);
^attack_psionic{S}	SET_ATTACK(op, ATNR_PSIONIC, IVAL);

    /* Start of various attacktypes */
^resist_impact{S}	SET_RESIST(op, ATNR_PHYSICAL, IVAL);
^resist_magic{S}	SET_RESIST(op, ATNR_MAGIC, IVAL);
^resist_fire{S}		SET_RESIST(op, ATNR_FIRE, IVAL);
^resist_electricity{S}	SET_RESIST(op, ATNR_ELECTRICITY, IVAL);
^resist_cold{S}		SET_RESIST(op, ATNR_COLD, IVAL);
^resist_confusion{S}	SET_RESIST(op, ATNR_CONFUSION, IVAL);
^resist_acid{S}		SET_RESIST(op, ATNR_ACID, IVAL);
^resist_drain{S}	SET_RESIST(op, ATNR_DRAIN, IVAL);
^resist_weaponmagic{S}	SET_RESIST(op, ATNR_WEAPONMAGIC, IVAL);
^resist_ghosthit{S}	SET_RESIST(op, ATNR_GHOSTHIT, IVAL);
^resist_poison{S}	SET_RESIST(op, ATNR_POISON, IVAL);
^resist_slow{S}		SET_RESIST(op, ATNR_SLOW, IVAL);
^resist_paralyze{S}	SET_RESIST(op, ATNR_PARALYZE, IVAL);
^resist_time{S}		SET_RESIST(op, ATNR_TIME, IVAL);
^resist_fear{S}		SET_RESIST(op, ATNR_FEAR, IVAL);
^resist_cancellation{S}	SET_RESIST(op, ATNR_CANCELLATION, IVAL);
^resist_depletion{S}	SET_RESIST(op, ATNR_DEPLETE, IVAL);
^resist_death{S}	SET_RESIST(op, ATNR_DEATH, IVAL);
^resist_chaos{S}	SET_RESIST(op, ATNR_CHAOS, IVAL);
^resist_counterspell{S}	SET_RESIST(op, ATNR_COUNTERSPELL, IVAL);
^resist_godpower{S}	SET_RESIST(op, ATNR_GODPOWER, IVAL);
^resist_purity{S}	SET_RESIST(op, ATNR_HOLYWORD, IVAL);
^resist_blind{S}	SET_RESIST(op, ATNR_BLIND, IVAL);
^resist_internal{S}	SET_RESIST(op, ATNR_INTERNAL, IVAL);
^resist_life_steal{S}	SET_RESIST(op, ATNR_LIFE_STEALING, IVAL);
^resist_slash{S}	SET_RESIST(op, ATNR_SLASH, IVAL);
^resist_cleave{S}	SET_RESIST(op, ATNR_CLEAVE, IVAL);
^resist_pierce{S}	SET_RESIST(op, ATNR_PIERCE, IVAL);
^resist_nether{S}	SET_RESIST(op, ATNR_NETHER, IVAL);
^resist_sonic{S}	SET_RESIST(op, ATNR_SONIC, IVAL);
^resist_demonic{S}	SET_RESIST(op, ATNR_DEMONIC, IVAL);
^resist_psionic{S}	SET_RESIST(op, ATNR_PSIONIC, IVAL);

^protect_impact{S}	SET_PROTECTION(op, PROTECT_PHYSICAL, IVAL);
^protect_slash{S}	SET_PROTECTION(op, PROTECT_SLASH, IVAL);
^protect_cleave{S}	SET_PROTECTION(op, PROTECT_CLEAVE, IVAL);
^protect_pierce{S}	SET_PROTECTION(op, PROTECT_PIERCE, IVAL);
^protect_weapon_magic{S}	SET_PROTECTION(op, PROTECT_WEAPON_MAGIC, IVAL);

^protect_fire{S}		SET_PROTECTION(op, PROTECT_FIRE, IVAL);
^protect_cold{S}		SET_PROTECTION(op, PROTECT_COLD, IVAL);
^protect_electricity{S}		SET_PROTECTION(op, PROTECT_ELECTRICITY, IVAL);
^protect_poison{S}		SET_PROTECTION(op, PROTECT_POISON, IVAL);
^protect_acid{S}		SET_PROTECTION(op, PROTECT_ACID, IVAL);

^protect_magic{S}		SET_PROTECTION(op, PROTECT_MAGIC, IVAL);
^protect_mind{S}		SET_PROTECTION(op, PROTECT_MIND, IVAL);
^protect_body{S}		SET_PROTECTION(op, PROTECT_BODY, IVAL);
^protect_psionic{S}		SET_PROTECTION(op, PROTECT_PSIONIC, IVAL);
^protect_force{S}		SET_PROTECTION(op, PROTECT_FORCE, IVAL);

^protect_nether{S}		SET_PROTECTION(op, PROTECT_NETHER, IVAL);
^protect_chaos{S}		SET_PROTECTION(op, PROTECT_CHAOS, IVAL);
^protect_death{S}		SET_PROTECTION(op, PROTECT_DEATH, IVAL);
^protect_spiritual{S}		SET_PROTECTION(op, PROTECT_SPIRITUAL, IVAL);
^protect_corruption{S}		SET_PROTECTION(op, PROTECT_CORRUPTION, IVAL);

^attack_movement{S} op->move_type = IVAL;
^move_state{S}	    op->move_status = IVAL;
^confused{S}	    SET_OR_CLEAR_FLAG(op, FLAG_CONFUSED, IVAL);
^stealth{S}			SET_OR_CLEAR_FLAG(op, FLAG_STEALTH, IVAL);
^connected{S}	    add_button_link(op, op->map, IVAL);
^cursed{S}	    SET_OR_CLEAR_FLAG(op, FLAG_CURSED, IVAL);
^damned{S}	    SET_OR_CLEAR_FLAG(op, FLAG_DAMNED, IVAL);
^see_anywhere{S}    SET_OR_CLEAR_FLAG(op, FLAG_SEE_ANYWHERE, IVAL);
^known_magical{S}   SET_OR_CLEAR_FLAG(op, FLAG_KNOWN_MAGICAL, IVAL);
^known_cursed{S}    SET_OR_CLEAR_FLAG(op, FLAG_KNOWN_CURSED, IVAL);
^can_use_skill{S}   SET_OR_CLEAR_FLAG(op, FLAG_CAN_USE_SKILL, IVAL);
^been_applied{S}    SET_OR_CLEAR_FLAG(op, FLAG_BEEN_APPLIED, IVAL);
^title{S}	    {	char *y=yval();
			if (*y=='\0') LOG(llevBug,"BUG: loader(): Title without value.\n");
			else FREE_AND_COPY_HASH(op->title, y);
		    }
^unique{S}		SET_OR_CLEAR_FLAG(op, FLAG_UNIQUE, IVAL);
^inv_locked{S}		SET_OR_CLEAR_FLAG(op, FLAG_INV_LOCKED, IVAL);
^is_wooded{S}		SET_OR_CLEAR_FLAG(op, FLAG_IS_WOODED, IVAL);
^is_hilly{S}		SET_OR_CLEAR_FLAG(op, FLAG_IS_HILLY, IVAL);
^has_ready_skill{S}	SET_OR_CLEAR_FLAG(op, FLAG_READY_SKILL, IVAL);
^has_ready_weapon{S}	SET_OR_CLEAR_FLAG(op, FLAG_READY_WEAPON, IVAL);
^no_skill_ident{S}	SET_OR_CLEAR_FLAG(op, FLAG_NO_SKILL_IDENT, IVAL);
^glow_radius{S}		op->glow_radius = IVAL;
^is_blind{S}		SET_OR_CLEAR_FLAG(op, FLAG_BLIND, IVAL);
^can_see_in_dark{S}	SET_OR_CLEAR_FLAG(op, FLAG_SEE_IN_DARK, IVAL);
^is_cauldron{S}		SET_OR_CLEAR_FLAG(op, FLAG_IS_CAULDRON, IVAL);
^randomitems{S}		op->randomitems = find_treasurelist(yval());
^is_dust{S}		SET_OR_CLEAR_FLAG(op, FLAG_DUST, IVAL);
^no_steal{S}		SET_OR_CLEAR_FLAG(op, FLAG_NO_STEAL, IVAL);
^one_hit{S}		SET_OR_CLEAR_FLAG(op, FLAG_ONE_HIT, IVAL);

^is_vul_elemental{S}	SET_OR_CLEAR_FLAG(op, FLAG_VUL_ELEMENTAL, IVAL);
^is_proof_elemental{S}	SET_OR_CLEAR_FLAG(op, FLAG_PROOF_ELEMENTAL, IVAL);
^is_vul_magic{S}	SET_OR_CLEAR_FLAG(op, FLAG_VUL_MAGIC, IVAL);
^is_proof_magic{S}	SET_OR_CLEAR_FLAG(op, FLAG_PROOF_MAGIC, IVAL);
^is_vul_physical{S}	    	SET_OR_CLEAR_FLAG(op, FLAG_VUL_PHYSICAL, IVAL);
^is_proof_physical{S}	SET_OR_CLEAR_FLAG(op, FLAG_PROOF_PHYSICAL, IVAL);
^is_vul_sphere{S}	SET_OR_CLEAR_FLAG(op, FLAG_VUL_SPHERE, IVAL);
^is_proof_sphere{S}	SET_OR_CLEAR_FLAG(op, FLAG_PROOF_SPHERE, IVAL);
^is_indestructible{S}	SET_OR_CLEAR_FLAG(op, FLAG_INDESTRUCTIBLE, IVAL);


^editor_folder{S}	{ }
^sub_type{S}		op->sub_type1 = IVAL;
^terrain_flag{S}	op->terrain_flag = IVAL;
^terrain_type{S}	op->terrain_type = IVAL;
^item_quality{S}	op->item_quality = IVAL;
^item_condition{S}	op->item_condition = IVAL;
^item_race{S}		op->item_race = IVAL;
^item_skill{S}		op->item_skill = IVAL;
^item_level{S}		op->item_level = IVAL;
^item_level_art{S}	{ /* artifact list token - NEVER USE IN REAL ARCHES OR MAPS */
					int ival_tmp = IVAL;
				    /* when we have a artifact which has a min. use level of 10
					 * but we give it a mithril armour of default use level 50,
					 * we will create artifact mithril armour usable for level 10!
					 * this command prevent it
					 */
					if(ival_tmp <=0 ) /* this is forcing the value! */
						op->item_level = -ival_tmp;
					else if(ival_tmp >op->item_level)
						op->item_level = ival_tmp;
					}
^material_real{S}	op->material_real = IVAL;
^mpart_id{S}		op->quick_pos |= (((char)IVAL)<<4);
^mpart_nr{S}		op->quick_pos |= (char)IVAL;
^weapon_speed{S}	{
						op->weapon_speed = FVAL;
						op->weapon_speed_left = 0;
						op->weapon_speed_add = op->weapon_speed;
					}


<*>(^{WS})|{NL}	{/* ignore empty lines, newlines we don't do above */}
#.*{NL}			{}

<<EOF>>			{/* If we got an error, return the error.  Otherwise, return that we got EOF */
			    if (lex_error!=0) return lex_error; else return LL_EOF;}
.*			{ yyerror( "BUG: loader(): Unrecognized string"); lex_error= -1; }
%%


int yyerror(char *s)
{
  LOG(llevBug, "%s: %s\n", s, yytext);
  return -1;
}


/* Our save file syntax is very simple, so we can use a very simple
 * processing mechanism here instead using something like bison
 * This skips over the space and returns the value, or "" if no value
 * is found.  Modified 4/26/2000 to also strip spaces at end of
 * line
 */
static char *yval()
{
    static char *em="";
    char *cp,*end;

    /* First skip over start of line, like animation or name */
   for (cp=yytext; *cp!=' '; cp++) {
	if (*cp=='\0') {
	    return em;
	}
   }

    /* Skip over whitespace */
    for (; *cp==' '; cp++) {
	if (*cp=='\0') {
	    return em;
	}
    }
    /* Got last character before null and strip
     * off tailing whitespace
     */
    for (end=cp+strlen(cp)-1; *end==' ' || *end == 0x0A || *end==0x0D; end--) {
	if (end==cp) return em;
	*end='\0';
    }
    return cp;
}

/* buffer functions - needed to make load_object()
 * recursive save.
 */
void delete_loader_buffer(void *buffer)
{
	if(buffer)
		yy_delete_buffer(buffer);
	else
		yy_delete_buffer(YY_CURRENT_BUFFER);
	cur_buffer = NULL;
}

void *create_loader_buffer(void *fp)
{
	cur_buffer = yy_create_buffer(fp, YY_BUF_SIZE);
	yy_switch_to_buffer(cur_buffer);
	return cur_buffer;
}


/*
 * Loads an object from the given file-pointer.
 * Variables will be read and parsed and patched into the object
 * until the string "end" is reached, or the end of the file.
 *
 * bufstat is used to determine various file attributes:
 *  LO_REPEAT (0): We are reading from the same buffer as the last call.
 *  LO_LINEMODE (1): file that is being read from is multi purpose (ie, other functions
 *	will also be reading from this (treasure file, artifacts.)
 *  LO_NEWFILE (2): disabled
 *  LO_NOREAD (3): disabled
 *  LO_MEMORYMODE (4): read from a memory buffer instead of a file.
 */
/* load_object() is now recursive save - this kind of call can happens when a multi
 * arch is expanded after loading in a map and out_of_map() triggers a map loading for
 * parts in a different map. Because we don't need any order of objects when we load a map,
 * this will work fine.
 */
int load_object(void *fp, object *op, void *mybuffer, int bufstate, int map_flags) {
    int retval = 0;
	float old_speed, old_sleft;
    char inbuf[MAX_BUF];

	/* note: at this point we have perhaps a "arch" parse or a "object"
	 * parse - we don't know. "arch" parse means, arches are loaded - we
	 * hit a arch command and pre-load our object data - then we go on and
	 * parse the difference in. If we hit a "object" command, we had outside
	 * this function pre-loaded the arch. in the first case, the next 2
	 * values are undefined - in the last case, these values are the arch
	 * pre sets. We need to track this down to adjust for the "real ingame"
	 * object the right speed & speed_left.
	 */
	old_speed=op->speed;
	old_sleft=op->speed_left;
	object_load_flag = 0; /* we assume a "arch" load as default */

	*msgbuf='\0';

    if (bufstate==LO_REPEAT)
	{
		if(cur_buffer != mybuffer) /* be sure we work on the right buffer */
		{
			cur_buffer = mybuffer;
			yy_switch_to_buffer(mybuffer);
		}
	    retval=lex_load(op, map_flags);
	}
    else if (bufstate==LO_MEMORYMODE)
	{
		char *strptr = (char*)fp;
		YY_BUFFER_STATE  yybufstate;
		int lcount=0;
		object_load_flag = 1; /* we are object load but object is not called */
		while (1) /* we can be sure this will work - InitArtifact has tested this before */
		{
			strptr +=lcount;
			lcount = strlen(strptr)+1; /*we can even put this value at string start to save the strlen */
			yybufstate=yy_scan_string(strptr);
			retval=lex_load(op, map_flags); /* hopefully we have here a "end" - or we stay in this while a bit */
			yy_delete_buffer(yybufstate);
			if (retval==LL_NORMAL)
				break;
		}

		if(cur_buffer) /* be sure we work on the right buffer */
			yy_switch_to_buffer(cur_buffer);
    }
    else /* if (bufstate==LO_LINEMODE) */
	{
		YY_BUFFER_STATE  yybufstate;
		while (fgets(inbuf, MAX_BUF-3, (FILE *)fp))
		{
			yybufstate=yy_scan_string(inbuf);
			retval=lex_load(op, map_flags);
			yy_delete_buffer(yybufstate);
			if (retval==LL_NORMAL)
			{
				if(cur_buffer) /* be sure we work on the right buffer */
					yy_switch_to_buffer(cur_buffer);
				return retval;
			}
		}
		LOG(llevDebug,"Got eof while scanning strings (%d)\n",retval);
		if(cur_buffer) /* be sure we work on the right buffer */
			yy_switch_to_buffer(cur_buffer);
		return LL_EOF;
    }

	if(QUERY_FLAG(op,FLAG_IDENTIFIED))
	{
		SET_FLAG(op,FLAG_KNOWN_MAGICAL);
		SET_FLAG(op,FLAG_KNOWN_CURSED);
	}
  /* we set here the material real values.
   * we do: If item_quality == 0 AND material defines a base
   * material, then use material_real to set quality & condition.
   * So, we don't set it for floor or walls or some else objects
   * were we don't need it.
   */
  if(op->material && !op->item_quality)
  {
	  op->item_quality = material_real[op->material_real].quality;
	  op->item_condition = op->item_quality;
  }

	/* ok... now we have our old speed values and our new ones.
	 */
	if (!(map_flags & MAP_STYLE))
	{
	  /* if set, our old speed values comes from a valid source (arches?) outside. */
	  if(object_load_flag)
	  {
			/* ok - ONLY change something when we
			 * a.) don't have fix set speed_left in the loading
			 * b.) we have negative speed
			 */
			if(old_sleft==op->speed_left && op->speed<0.0f)
			{
				/* if we have now a negative speed - use default arch
				 * speed left for new random set
				 */
				if(op->speed<0.0f)
					op->speed_left=op->speed_left+(float)(RANDOM()%90)/100.0f;
			}
	  }
	  else /* arch load... perhaps a map */
	  {
	    /* only do this when we have not loaded a fix speed_left and speed is <0 */
		if(op->speed<0.0f && op->speed_left==op->arch->clone.speed_left)
			op->speed_left=op->speed_left+(float)(RANDOM()%90)/100.0f;
	  }
	  update_ob_speed(op); /* now we have build a new object - and its not arch loading
	                        * nor artifact list or style map. (MAP_STYLE is used to exlude them)
							*/
	}

    /*LOG(llevDebug," load completed, object=%s %s (%d - %d)\n",op->name,op->title, op->item_quality, op->item_condition);*/
    return retval;
}


/* This takes a buffer, scans it for variables, and sets those variables
 * as appropriate in op.
 *
 * This function appears to be used in only 2 places - in crossedit to
 * override values and in c_wiz to mutate values. */
int set_variable(object *op,char *buf)
{
    YY_BUFFER_STATE yybufstate, yycurbuf = YY_CURRENT_BUFFER;
    int retval;

    strcpy(msgbuf, "");
    yy_push_state(INITIAL);
    yybufstate = yy_scan_string(buf);
    retval = lex_load(op, 0);

    if (yycurbuf)
    	yy_switch_to_buffer(yycurbuf);

    yy_delete_buffer(yybufstate);
    yy_pop_state();
    return retval;
}


/* This is a list of pointers that correspond to the FLAG_.. values.
 * This is a simple 1:1 mapping - if FLAG_FRIENDLY is 15, then
 * the 15'th element of this array should match that name.
 * If an entry is NULL, that is a flag not to loaded/saved. */
static char *flag_names[NUM_FLAGS + 1] = {
	"sleep", 				"confused", 			NULL, 				"scared", 			"is_blind",
	"is_invisible", 		"is_ethereal",			"is_good", 			"no_pick", 			"walk_on",
	"no_pass",				"is_animated", 			NULL, 				"flying", 			"monster",
	"friendly", 			NULL, 					"been_applied", 	"auto_apply", 		"treasure",
	"is_neutral",			"see_invisible",		"can_roll", 		"generator", 		"is_turnable",
	"walk_off",				"fly_on", 				"fly_off", 			"is_used_up", 		"identified",
	"reflecting",			"changing", 			"splitting", 		"hitback", 			"startequip",
	"blocksview", 			"undead", 				NULL, 				"unaggressive",		"reflect_missile",
	"reflect_spell",		"no_magic", 			"no_fix_player", 	"is_evil", 			"tear_down",
	"run_away",				"pass_thru", 			"can_pass_thru", 	"pick_up", 			"unique",
	"no_drop",				"is_indestructible", 	"can_cast_spell", 	"can_use_scroll", 	"can_use_range",
	"can_use_bow",  		"can_use_armour", 		"can_use_weapon",	"can_use_ring", 	"has_ready_range",
	"has_ready_bow",		"xrays", 				NULL, 				"is_floor", 		"lifesave",
	"is_magical", 			"alive",				"stand_still", 		"random_move", 		"only_attack",
	"wiz",					"stealth", 				NULL, 				NULL, 				"cursed",
	"damned",				"see_anywhere", 		"known_magical", 	"known_cursed",		"can_use_skill",
	"is_thrown",			"is_vul_sphere", 		"is_proof_sphere", 	"is_male",			"is_female",
	"applied",  			"inv_locked", 			"is_wooded",		"is_hilly", 		"has_ready_skill",
	"has_ready_weapon",		"no_skill_ident",		"was_wiz", 			"can_see_in_dark", 	"is_cauldron",
	"is_dust", 				"no_steal", 			"one_hit", 			NULL, 				"berserk",
	"no_attack",			"invulnerable", 		"quest_item", 		"is_traped", 		"is_vul_elemental",
	"is_proof_elemental",	"is_vul_magic", 		"is_proof_magic", 	"is_vul_physical", 	"is_proof_physical",
	"sys_object",			"use_fix_pos",			"unpaid",			"is_aged",			"make_invisible",
	"make_ethereal",		"is_player",			"is_named",			NULL, 				"no_teleport",
	"corpse", 				"corpse_forced",		"player_only", 		"no_cleric", 		"one_drop",
	"cursed_perm", 			"damned_perm", 			"door_closed",		"was_reflected", 	"is_missile",
	"can_reflect_missile",	"can_reflect_spell",	"is_assassin",		NULL, 				"no_save"
};

void save_double(char *buf, char *name, double v)
{
	char tbuf[200];

	sprintf(tbuf, "%s %f\n", name, v);
	strcat(buf, tbuf);
}

/* Initialises the array of variable-names.  Needed before any
 * objects can be loaded.  Called by init_library(). */
void init_vars()
{
}

/* Returns a pointer to a static string which contains all variables
 * which are different in the two given objects.  op is the what object
 * the different values will be taken from.  This function is
 * typically used to dump objects (op2=empty object), or to save objects
 * (op2 is the objects original archetype) */

/* TODO: Optimize this! */
char *get_ob_diff(object *op, object *op2)
{
  	static char buf2[HUGE_BUF];
  	static char buf[HUGE_BUF];
  	int tmp;

  	buf[0] = '\0';
	if (op->name && op->name != op2->name)
	{
		sprintf(buf2, "name %s\n", op->name);
		strcat(buf, buf2);
	}

	if (op->title && op->title != op2->title)
	{
		sprintf(buf2, "title %s\n", op->title);
		strcat(buf, buf2);
	}

	if (op->race && op->race != op2->race)
	{
		sprintf(buf2, "race %s\n", op->race);
		strcat(buf, buf2);
	}

	if (op->slaying && op->slaying != op2->slaying)
	{
		sprintf(buf2, "slaying %s\n", op->slaying);
		strcat(buf, buf2);
	}

	if (op->msg && op->msg != op2->msg)
	{
		strcat(buf, "msg\n");
		strcat(buf, op->msg);
		/* we skipped last \n when we read the map, to avoid clear line in client */
		strcat(buf, "\n");
		strcat(buf, "endmsg\n");
	}

	if (op->other_arch != op2->other_arch && op->other_arch != NULL && op->other_arch->name)
	{
		sprintf(buf2, "other_arch %s\n", op->other_arch->name);
		strcat(buf, buf2);
	}

	if (op->face != op2->face)
	{
		sprintf(buf2, "face %s\n", op->face->name);
		strcat(buf, buf2);
	}

	if (op->inv_face != op2->inv_face)
	{
		sprintf(buf2, "inv_face %s\n", op->inv_face->name);
		strcat(buf, buf2);
	}

	if (op->animation_id != op2->animation_id)
	{
		if (op->animation_id)
			sprintf(buf2, "animation %s\n", animations[GET_ANIM_ID(op)].name);
		else
			sprintf(buf2, "animation NONE\n");

		strcat(buf, buf2);
	}

	if (op->inv_animation_id != op2->inv_animation_id)
	{
		if (op->inv_animation_id)
			sprintf(buf2, "inv_animation %s\n", animations[GET_INV_ANIM_ID(op)].name);
		else
			sprintf(buf2, "inv_animation NONE\n");

		strcat(buf, buf2);
	}


	if (op->stats.wc_range != op2->stats.wc_range)
		save_long(buf, "wc_range", op->stats.wc_range);

	if (op->stats.Str != op2->stats.Str)
		save_long(buf, "Str", op->stats.Str);

	if (op->stats.Dex != op2->stats.Dex)
		save_long(buf, "Dex", op->stats.Dex);

	if (op->stats.Con != op2->stats.Con)
		save_long(buf, "Con", op->stats.Con);

	if (op->stats.Wis != op2->stats.Wis)
		save_long(buf, "Wis", op->stats.Wis);

	if (op->stats.Pow != op2->stats.Pow)
		save_long(buf, "Pow", op->stats.Pow);

	if (op->stats.Cha != op2->stats.Cha)
		save_long(buf, "Cha", op->stats.Cha);

	if (op->stats.Int != op2->stats.Int)
		save_long(buf, "Int", op->stats.Int);

	if (op->stats.hp != op2->stats.hp)
		save_long(buf, "hp", op->stats.hp);

	if (op->stats.maxhp != op2->stats.maxhp)
		save_long(buf, "maxhp", op->stats.maxhp);

	if (op->stats.sp != op2->stats.sp)
		save_long(buf, "sp", op->stats.sp);

	if (op->stats.maxsp != op2->stats.maxsp)
		save_long(buf, "maxsp", op->stats.maxsp);

	if (op->stats.grace != op2->stats.grace)
		save_long(buf, "grace", op->stats.grace);

	if (op->stats.maxgrace != op2->stats.maxgrace)
		save_long(buf, "maxgrace", op->stats.maxgrace);

	if (op->stats.exp != op2->stats.exp)
		save_long(buf, "exp", op->stats.exp);

	if (op->stats.food != op2->stats.food)
		save_long(buf, "food", op->stats.food);

	if (op->stats.dam != op2->stats.dam)
		save_long(buf, "dam", op->stats.dam);

	if (op->stats.luck != op2->stats.luck)
		save_long(buf, "luck", op->stats.luck);

	if (op->stats.wc != op2->stats.wc)
		save_long(buf, "wc", op->stats.wc);

	if (op->stats.ac != op2->stats.ac)
		save_long(buf, "ac", op->stats.ac);

	if (op->x != op2->x)
		save_long(buf, "x", op->x);

	if (op->y != op2->y)
		save_long(buf, "y", op->y);

	if (op->speed != op2->speed)
	{
		sprintf(buf2, "speed %f\n", op->speed);
		strcat(buf, buf2);
	}

	if (op->speed_left != op2->speed_left || op->speed_left)
	{
		sprintf(buf2, "speed_left %f\n", op->speed_left);
		strcat(buf, buf2);
	}

	if (op->material_real != op2->material_real)
	{
		sprintf(buf2, "material_real %d\n", op->material_real);
		strcat(buf, buf2);
	}

	if (op->sub_type1 != op2->sub_type1)
	{
		sprintf(buf2, "sub_type %d\n", op->sub_type1);
		strcat(buf, buf2);
	}

	if (op->terrain_flag != op2->terrain_flag)
	{
		sprintf(buf2, "terrain_flag %d\n", op->terrain_flag);
		strcat(buf, buf2);
	}

	if (op->terrain_type != op2->terrain_type)
	{
		sprintf(buf2, "terrain_type %d\n", op->terrain_type);
		strcat(buf, buf2);
	}

	if (op->item_quality != op2->item_quality)
	{
		sprintf(buf2, "item_quality %d\n", op->item_quality);
		strcat(buf, buf2);
	}

	if (op->item_condition != op2->item_condition)
	{
		sprintf(buf2, "item_condition %d\n", op->item_condition);
		strcat(buf, buf2);
	}

	if (op->item_race != op2->item_race)
	{
		sprintf(buf2, "item_race %d\n", op->item_race);
		strcat(buf, buf2);
	}

	if (op->item_skill != op2->item_skill)
	{
		sprintf(buf2, "item_skill %d\n", op->item_skill);
		strcat(buf, buf2);
	}

	if (op->item_level != op2->item_level)
	{
		sprintf(buf2, "item_level %d\n", op->item_level);
		strcat(buf, buf2);
	}

	if (op->weapon_speed != op2->weapon_speed)
	{
		sprintf(buf2, "weapon_speed %f\n", op->weapon_speed);
		strcat(buf, buf2);
	}

	if (op->enemy_count != op2->enemy_count)
	{
		sprintf(buf2, "object_int1 %d\n", op->enemy_count);
		strcat(buf, buf2);
	}

	if (op->attacked_by_count != op2->attacked_by_count)
	{
		sprintf(buf2, "object_int2 %d\n", op->attacked_by_count);
		strcat(buf, buf2);
	}

	if (op->ownercount != op2->ownercount)
	{
		sprintf(buf2, "object_int3 %d\n", op->ownercount);
		strcat(buf, buf2);
	}

	if(op->move_status != op2->move_status)
		save_long(buf, "move_state", op->move_status);

	if (op->move_type != op2->move_type)
		save_long(buf, "attack_movement", op->move_type);

	if (op->nrof != op2->nrof)
		save_long(buf, "nrof", op->nrof);

	if (op->level != op2->level)
		save_long(buf, "level", op->level);

	if (op->direction != op2->direction)
		save_long(buf, "direction", op->direction);

	if (op->type != op2->type)
		save_long(buf, "type", op->type);

	if (op->attacktype != op2->attacktype)
		save_long(buf, "attacktype", op->attacktype);

  	for (tmp = 0; tmp < NROFATTACKS; tmp++)
	{
   		if (op->resist[tmp] != op2->resist[tmp])
		{
			sprintf(buf2, "resist_%s %d\n", resist_save[tmp], op->resist[tmp]);
			strcat(buf, buf2);
    	}
  	}

	/* resist_save[] holds the attack form also for attack_xx */
	for (tmp = 0; tmp < NROFATTACKS; tmp++)
	{
		if (op->attack[tmp] != op2->attack[tmp])
		{
			sprintf(buf2, "attack_%s %d\n", resist_save[tmp], op->attack[tmp]);
			strcat(buf, buf2);
		}
	}

	for (tmp = 0; tmp < NROFPROTECTIONS; tmp++)
	{
		if (op->protection[tmp] != op2->protection[tmp])
		{
			sprintf(buf2, "protect_%s %d\n", protection_save[tmp], op->protection[tmp]);
			strcat(buf, buf2);
		}
	}

	if (op->layer != op2->layer)
		save_long(buf, "layer", op->layer);

	if (op->hide != op2->hide)
		save_long(buf, "hidden", op->hide);

	if (op->path_attuned != op2->path_attuned)
		save_long(buf, "path_attuned", op->path_attuned);

	if (op->path_repelled != op2->path_repelled)
		save_long(buf, "path_repelled", op->path_repelled);

	if (op->path_denied != op2->path_denied)
		save_long(buf, "path_denied", op->path_denied);

	if (op->material != op2->material)
		save_long(buf, "material", op->material);

	if (op->value != op2->value)
		save_long(buf, "value", op->value);

	if (op->carrying != op2->carrying)
		save_long(buf, "carrying", op->carrying);

	if (op->weight != op2->weight)
		save_long(buf, "weight", op->weight);

	if (op->state != op2->state)
		save_long(buf, "state", op->state);

	if (op->magic != op2->magic)
		save_long(buf, "magic", op->magic);

	if (op->last_heal != op2->last_heal)
		save_long(buf, "last_heal", op->last_heal);

	if (op->last_sp != op2->last_sp)
		save_long(buf, "last_sp", op->last_sp);

	if (op->last_grace != op2->last_grace)
		save_long(buf, "last_grace", op->last_grace);

	if (op->last_eat != op2->last_eat)
		save_long(buf, "last_eat", op->last_eat);

	if (QUERY_FLAG(op, FLAG_IS_LINKED) && (tmp = get_button_value(op)))
		save_long(buf, "connected", tmp);

	if (op->glow_radius != op2->glow_radius)
		save_long(buf, "glow_radius", op->glow_radius);

	if (op->randomitems != op2->randomitems)
	{
		sprintf(buf2, "randomitems %s\n", (op->randomitems ? op->randomitems->name : "none"));
		strcat(buf, buf2);
	}

	if (op->run_away != op2->run_away)
		save_long(buf, "run_away", op->run_away);

	if (op->pick_up != op2->pick_up)
		save_long(buf, "pick_up", op->pick_up);

	if (op->weight_limit != op2->weight_limit)
		save_long(buf, "container", op->weight_limit);

	if (op->will_apply != op2->will_apply)
		save_long(buf, "will_apply", op->will_apply);

	if (op->quickslot != op2->quickslot)
	{
		sprintf(buf2, "quickslot %d\n", op->quickslot);
		strcat(buf, buf2);
	}

  	for (tmp = 0; tmp <= NUM_FLAGS; tmp++)
	{
		if (flag_names[tmp] && (QUERY_FLAG(op, tmp) != QUERY_FLAG(op2, tmp)))
		{
			if (QUERY_FLAG(op, tmp))
				sprintf(buf2, "%s 1\n", flag_names[tmp]);
			else
				sprintf(buf2, "%s 0\n", flag_names[tmp]);

			strcat(buf, buf2);
    	}
  	}

  	/* The old code was buggy - it would lose custom SLOW_PENALTY if the archetype
   	 * had a different slow_penalty (eg, when saving, it would also reset to the
   	 * archetype value. */
  	if (QUERY_FLAG(op, FLAG_SLOW_MOVE))
	{
    	if (!QUERY_FLAG(op2, FLAG_SLOW_MOVE) || SLOW_PENALTY(op) != SLOW_PENALTY(op2))
		{
			sprintf(buf2, "slow_move %f\n", SLOW_PENALTY(op));
			strcat(buf, buf2);
    	}
  	}

  	if (buf[0] == '\0')
    	return NULL;

  	return buf;
}

/* Dumps all variables in an object to a file.
 * If bit 0 of flag is set, unpaid objects will be saved.  As of now,
 * the only place this is not set is when saving the player.
 * If bit 1 of flag is set, don't remove the object after save.  As of now,
 * all of the callers are setting this. */
void save_map_object(FILE *fp, object *op, int flag)
{
    archetype *at;
    char *cp;
    object *tmp, *old;

    /* Even if the object does have an owner, it would seem that we should
     * still save it. */
    /*if(op->owner!=NULL || fp == NULL) this will wreck item we throw - they get player as owner */
    if (fp == NULL)
		return;

    /* If it is unpaid and we don't want to save those, just return. */
    if (!(flag & 1) && (QUERY_FLAG(op, FLAG_UNPAID)))
		return;

    if ((at = op->arch) == NULL)
		at = empty_archetype;

    fprintf(fp, "arch %s\n", at->name);

	/* We really should do some status checking on this */
    if ((cp = get_ob_diff(op, &at->clone)) != NULL)
		fputs(cp, fp);

    /* Eneq(@csd.uu.se): Added this to allow containers being saved with contents */
    old = NULL;

    if (flag & 2)
		for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	    	save_map_object(fp, tmp, flag);

    /* Slightly different logic because tmp/op will be removed by
     * the save_object we call.  So we just keep looking at op->inv
     * until there is nothing left.  In theory, the variable old
     * should not be needed, as recursive loops shouldn't happen. */
    else
		while ((tmp = op->inv) != NULL)
		{
			if (old == tmp)
			{
	    		LOG(llevError, "Recursive loop in inventory\n");
	    		break;
			}
			save_map_object(fp, tmp, flag);
			old = tmp;
    	}

    if (!(flag & 2))
		remove_ob(op);

    fprintf(fp, "end\n");
}

/* Like the above, but dumps the objects to a buffer.
 * TODO: save_player_object() and save_map_object()
 * should really be merged together. */
int save_player_object(char *buf, object *op, int flag, size_t len)
{
    archetype *at;
    char *cp;
    object *tmp, *old;
	int i;

    /* If it is unpaid and we don't want to save those, just return. */
    if (!(flag & 1) && (QUERY_FLAG(op, FLAG_UNPAID)))
		return;

    if ((at = op->arch) == NULL)
		at = empty_archetype;

	cp = get_ob_diff(op, &at->clone);

	if (strlen(buf) + strlen(at->name) + 10 + (cp ? strlen(cp) : 0) > len)
		return strlen(buf) + strlen(at->name) + 9 + (cp ? strlen(cp) : 0);

    sprintf(buf, "%sarch %s\n", buf, at->name);

	/* We really should do some status checking on this */
    if (cp != NULL)
		sprintf(buf, "%s%s", buf, cp);

    /* Eneq(@csd.uu.se): Added this to allow containers being saved with contents */
    old = NULL;

    if (flag & 2)
	{
		for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
		{
	    	i = save_player_object(buf, tmp, flag, len);

			if (i != 0)
				return i;
		}
	}

    /* Slightly different logic because tmp/op will be removed by
     * the save_object we call.  So we just keep looking at op->inv
     * until there is nothing left.  In theory, the variable old
     * should not be needed, as recursive loops shouldn't happen. */
    else
		while ((tmp = op->inv) != NULL)
		{
			if (old == tmp)
			{
	    		LOG(llevError, "Recursive loop in inventory\n");
	    		break;
			}

			i = save_player_object(buf, tmp, flag, len);

			if (i != 0)
				return i;

			old = tmp;
    	}

    if (!(flag & 2))
		remove_ob(op);

    sprintf(buf, "%send\n", buf);

	return 0;
}
