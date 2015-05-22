/**
 * @file
 * This small program will extract information from Gridarta's types.xml
 * file to generate documentation about types and fields.
 *
 * Files are placed in types documentation dir by default.
 *
 * To build: <pre>gcc -O3 -Wall -W -pedantic gridarta-types-convert.c
 * -I../../server/src/include -o gridarta-types-convert</pre>
 * To run: <pre>./gridarta-types-convert
 * ../../arch/dev/editor/conf/types.xml</pre>
 * (adjust the path according to your setup)
 *
 * Note that someone wishing to tweak this program should know the format of
 * Gridarta's types.xml.
 *
 * Note that "attribute" is used for "field in a object/living structure".
 *
 * @author Nicolas Weeger */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "define.h"
#define EXTERN
#define INIT_C
#include "attack.h"

/** Root destination dir. */
const char *destination_dir = "../../server/doc";
/** Where the files about the fields will be stored. */
const char *field_dir = "fields";
/** Where the files about types will be stored. */
const char *type_dir = "types";

/** One attribute in a type. */
typedef struct
{
    char *field;
    char *name;
    char *description;
} type_attribute;

/** One object type. */
typedef struct
{
    int number;
    char *name;
    char *description;
    char *use;
    type_attribute **attributes;
    int attribute_count;
    char **required;
    int require_count;
} type_definition;

/** Defined types. */
type_definition **types = NULL;

int type_count = 0;

/** Definitions all types have by default. */
type_definition *default_type = NULL;

/** Dummy object type that non defined objects use. */
type_definition *fallback_type = NULL;

/** One list of fields to ignore. */
typedef struct
{
    char *name;
    int count;
    char **fields;
} ignore_list;

ignore_list **lists = NULL;
int list_count = 0;

/** One type for an attribute. */
typedef struct
{
    char **type;
    int *number;
    int count;
    char *description;
} attribute_type;

/** One attribute. */
typedef struct
{
    char *field;
    attribute_type **types;
    int type_count;
} attribute_definition;

attribute_definition **attributes = NULL;
int attribute_count = 0;

/** One flag. */
typedef struct
{
    const char *field;
    const char *code_name;
} flag_definition;

/** Flag mapping. */
static const flag_definition flags[] =
{
    {"sleep", "FLAG_SLEEP"},
    {"confused", "FLAG_CONFUSED"},
    {"scared", "FLAG_SCARED"},
    {"is_blind", "FLAG_BLIND"},
    {"is_invisible", "FLAG_IS_INVISIBLE"},
    {"is_ethereal", "FLAG_IS_ETHEREAL"},
    {"is_good", "FLAG_IS_GOOD"},
    {"no_pick", "FLAG_NO_PICK"},
    {"walk_on", "FLAG_WALK_ON"},
    {"no_pass", "FLAG_NO_PASS"},
    {"is_animated", "FLAG_ANIMATE"},
    {"slow_move", "FLAG_SLOW_MOVE"},
    {"flying", "FLAG_FLYING"},
    {"monster", "FLAG_MONSTER"},
    {"friendly", "FLAG_FRIENDLY"},
    {"been_applied", "FLAG_BEEN_APPLIED"},
    {"auto_apply", "FLAG_AUTO_APPLY"},
    {"is_neutral", "FLAG_IS_NEUTRAL"},
    {"see_invisible", "FLAG_SEE_INVISIBLE"},
    {"can_roll", "FLAG_CAN_ROLL"},
    {"connect_reset", "FLAG_CONNECT_RESET"},
    {"is_turnable", "FLAG_IS_TURNABLE"},
    {"walk_off", "FLAG_WALK_OFF"},
    {"fly_on", "FLAG_FLY_ON"},
    {"fly_off", "FLAG_FLY_OFF"},
    {"is_used_up", "FLAG_IS_USED_UP"},
    {"identified", "FLAG_IDENTIFIED"},
    {"reflecting", "FLAG_REFLECTING"},
    {"changing", "FLAG_CHANGING"},
    {"splitting", "FLAG_SPLITTING"},
    {"hitback", "FLAG_HITBACK"},
    {"startequip", "FLAG_STARTEQUIP"},
    {"blocksview", "FLAG_BLOCKSVIEW"},
    {"undead", "FLAG_UNDEAD"},
    {"can_stack", "FLAG_CAN_STACK"},
    {"unaggressive", "FLAG_UNAGGRESSIVE"},
    {"reflect_missile", "FLAG_REFL_MISSILE"},
    {"reflect_spell", "FLAG_REFL_SPELL"},
    {"no_magic", "FLAG_NO_MAGIC"},
    {"no_fix_player", "FLAG_NO_FIX_PLAYER"},
    {"is_evil", "FLAG_IS_EVIL"},
    {"run_away", "FLAG_RUN_AWAY"},
    {"pass_thru", "FLAG_PASS_THRU"},
    {"can_pass_thru", "FLAG_CAN_PASS_THRU"},
    {"outdoor", "FLAG_OUTDOOR"},
    {"unique", "FLAG_UNIQUE"},
    {"no_drop", "FLAG_NO_DROP"},
    {"is_indestructible", "FLAG_INDESTRUCTIBLE"},
    {"can_cast_spell", "FLAG_CAST_SPELL"},
    {"two_handed", "FLAG_TWO_HANDED"},
    {"can_use_bow", "FLAG_USE_BOW"},
    {"can_use_armour", "FLAG_USE_ARMOUR"},
    {"can_use_weapon", "FLAG_USE_WEAPON"},
    {"connect_no_push", "FLAG_CONNECT_NO_PUSH"},
    {"connect_no_release", "FLAG_CONNECT_NO_RELEASE"},
    {"has_ready_bow", "FLAG_READY_BOW"},
    {"xrays", "FLAG_XRAYS"},
    {"is_floor", "FLAG_IS_FLOOR"},
    {"lifesave", "FLAG_LIFESAVE"},
    {"is_magical", "FLAG_IS_MAGICAL"},
    {"stand_still", "FLAG_STAND_STILL"},
    {"random_move", "FLAG_RANDOM_MOVE"},
    {"only_attack", "FLAG_ONLY_ATTACK"},
    {"stealth", "FLAG_STEALTH"},
    {"cursed", "FLAG_CURSED"},
    {"damned", "FLAG_DAMNED"},
    {"is_buildable", "FLAG_IS_BUILDABLE"},
    {"no_pvp", "FLAG_NO_PVP"},
    {"is_thrown", "FLAG_IS_THROWN"},
    {"is_male", "FLAG_IS_MALE"},
    {"is_female", "FLAG_IS_FEMALE"},
    {"applied", "FLAG_APPLIED"},
    {"inv_locked", "FLAG_INV_LOCKED"},
    {"has_ready_weapon", "FLAG_READY_WEAPON"},
    {"no_skill_ident", "FLAG_NO_SKILL_IDENT"},
    {"can_see_in_dark", "FLAG_SEE_IN_DARK"},
    {"is_cauldron", "FLAG_IS_CAULDRON"},
    {"is_dust", "FLAG_DUST"},
    {"one_hit", "FLAG_ONE_HIT"},
    {"draw_double_always", "FLAG_DRAW_DOUBLE_ALWAYS"},
    {"berserk", "FLAG_BERSERK"},
    {"no_attack", "FLAG_NO_ATTACK"},
    {"invulnerable", "FLAG_INVULNERABLE"},
    {"quest_item", "FLAG_QUEST_ITEM"},
    {"is_trapped", "FLAG_IS_TRAPPED"},
    {"sys_object", "FLAG_SYS_OBJECT"},
    {"use_fix_pos", "FLAG_USE_FIX_POS"},
    {"unpaid", "FLAG_UNPAID"},
    {"hidden", "FLAG_HIDDEN"},
    {"make_invisible", "FLAG_MAKE_INVISIBLE"},
    {"make_ethereal", "FLAG_MAKE_ETHEREAL"},
    {"is_player", "FLAG_IS_PLAYER"},
    {"is_named", "FLAG_IS_NAMED"},
    {"no_teleport", "FLAG_NO_TELEPORT"},
    {"corpse", "FLAG_CORPSE"},
    {"corpse_forced", "FLAG_CORPSE_FORCED"},
    {"player_only", "FLAG_PLAYER_ONLY"},
    {"one_drop", "FLAG_ONE_DROP"},
    {"cursed_perm", "FLAG_PERM_CURSED"},
    {"damned_perm", "FLAG_PERM_DAMNED"},
    {"door_closed", "FLAG_DOOR_CLOSED"},
    {"is_spell", "FLAG_IS_SPELL"},
    {"is_missile", "FLAG_IS_MISSILE"},
    {"draw_direction", "FLAG_DRAW_DIRECTION"},
    {"draw_double", "FLAG_DRAW_DOUBLE"},
    {"is_assassin", "FLAG_IS_ASSASSINATION"},
    {"no_save", "FLAG_NO_SAVE"},
    {NULL, NULL}
};

/**
 * Find a flag in ::flags.
 * @param name Flag name.
 * @return The flag if found, NULL otherwise. */
const flag_definition *find_flag(const char *name)
{
    int flag;

    for (flag = 0; flags[flag].field; flag++) {
        if (!strcmp(flags[flag].field, name)) {
            return &flags[flag];
        }
    }

    return NULL;
}

/**
 * Names of attack types to use when saving them to file. */
char *attack_save[NROFATTACKS] = {
    "impact",   "slash", "cleave",      "pierce",    "weaponmagic",
    "fire",     "cold",  "electricity", "poison",    "acid",
    "magic",    "mind",  "blind",       "paralyze",  "force",
    "godpower", "chaos", "drain",       "slow",      "confusion",
    "internal"
};

/** One type. */
typedef struct
{
    const char *code_name;
    int value;
} type_name;

/** All the types we want to create documentation for. */
static type_name type_names[] = {
    {"PLAYER", PLAYER},
    {"BULLET", BULLET},
    {"ROD", ROD},
    {"TREASURE", TREASURE},
    {"POTION", POTION},
    {"FOOD", FOOD},
    {"REGION_MAP", REGION_MAP},
    {"BOOK", BOOK},
    {"CLOCK", CLOCK},
    {"MATERIAL", MATERIAL},
    {"DUPLICATOR", DUPLICATOR},
    {"LIGHTNING", LIGHTNING},
    {"ARROW", ARROW},
    {"BOW", BOW},
    {"WEAPON", WEAPON},
    {"ARMOUR", ARMOUR},
    {"PEDESTAL", PEDESTAL},
    {"CONFUSION", CONFUSION},
    {"DOOR", DOOR},
    {"KEY", KEY},
    {"MAP", MAP},
    {"MAGIC_MIRROR", MAGIC_MIRROR},
    {"SPELL", SPELL},
    {"SHIELD", SHIELD},
    {"HELMET", HELMET},
    {"GREAVES", GREAVES},
    {"MONEY", MONEY},
    {"CLASS", CLASS},
    {"GRAVESTONE", GRAVESTONE},
    {"AMULET", AMULET},
    {"PLAYER_MOVER", PLAYER_MOVER},
    {"CREATOR", CREATOR},
    {"SKILL", SKILL},
    {"EXPERIENCE", EXPERIENCE},
    {"BLINDNESS", BLINDNESS},
    {"GOD", GOD},
    {"DETECTOR", DETECTOR},
    {"SKILL_ITEM", SKILL_ITEM},
    {"DEAD_OBJECT", DEAD_OBJECT},
    {"DRINK", DRINK},
    {"MARKER", MARKER},
    {"HOLY_ALTAR", HOLY_ALTAR},
    {"PEARL", PEARL},
    {"GEM", GEM},
    {"SOUND_AMBIENT", SOUND_AMBIENT},
    {"FIREWALL", FIREWALL},
    {"CHECK_INV", CHECK_INV},
    {"EXIT", EXIT},
    {"SHOP_FLOOR", SHOP_FLOOR},
    {"RING", RING},
    {"FLOOR", FLOOR},
    {"FLESH", FLESH},
    {"INORGANIC", INORGANIC},
    {"LIGHT_APPLY", LIGHT_APPLY},
    {"WALL", WALL},
    {"LIGHT_SOURCE", LIGHT_SOURCE},
    {"MISC_OBJECT", MISC_OBJECT},
    {"MONSTER", MONSTER},
    {"SPAWN_POINT", SPAWN_POINT},
    {"SPAWN_POINT_MOB", SPAWN_POINT_MOB},
    {"SPAWN_POINT_INFO", SPAWN_POINT_INFO},
    {"LIGHT_REFILL", LIGHT_REFILL},
    {"BOOK_SPELL", BOOK_SPELL},
    {"ORGANIC", ORGANIC},
    {"CLOAK", CLOAK},
    {"CONE", CONE},
    {"SPINNER", SPINNER},
    {"GATE", GATE},
    {"BUTTON", BUTTON},
    {"HANDLE", TYPE_HANDLE},
    {"WORD_OF_RECALL", WORD_OF_RECALL},
    {"SIGN", SIGN},
    {"BOOTS", BOOTS},
    {"GLOVES", GLOVES},
    {"BASE_INFO", BASE_INFO},
    {"RANDOM_DROP", RANDOM_DROP},
    {"BRACERS", BRACERS},
    {"POISONING", POISONING},
    {"SAVEBED", SAVEBED},
    {"WAND", WAND},
    {"ABILITY", ABILITY},
    {"SCROLL", SCROLL},
    {"DIRECTOR", DIRECTOR},
    {"GIRDLE", GIRDLE},
    {"FORCE", FORCE},
    {"POTION_EFFECT", POTION_EFFECT},
    {"JEWEL", JEWEL},
    {"NUGGET", NUGGET},
    {"EVENT_OBJECT", EVENT_OBJECT},
    {"WAYPOINT_OBJECT", WAYPOINT_OBJECT},
    {"QUEST_CONTAINER", QUEST_CONTAINER},
    {"CONTAINER", CONTAINER},
    {"WEALTH", WEALTH},
    {"BEACON", BEACON},
    {"MAP_EVENT_OBJ", MAP_EVENT_OBJ},
    {"COMPASS", COMPASS},
    {"MAP_INFO", MAP_INFO},
    {"SWARM_SPELL", SWARM_SPELL},
    {"RUNE", RUNE},
    {"POWER_CRYSTAL", POWER_CRYSTAL},
    {"CORPSE", CORPSE},
    {"DISEASE", DISEASE},
    {"SYMPTOM", SYMPTOM},
    {NULL, 0}
};

/** Maximum object types. */
#define OBJECT_TYPE_MAX 160

/**
 * Gets the attribute for the specified type. If it doesn't exist, create
 * it.
 *
 * If the attribute is already defined, return the existing one, after
 * cleaning its fields.
 * @param type Type to get attribute for.
 * @param attribute The attribute.
 * @return The new type attribute. */
type_attribute *get_attribute_for_type(type_definition *type, const char *attribute)
{
    type_attribute *ret;
    int test;

    for (test = 0; test < type->attribute_count; test++) {
        if (!strcmp(type->attributes[test]->field, attribute)) {
            ret = type->attributes[test];
            free(ret->name);
            ret->name = NULL;
            free(ret->description);
            ret->description = NULL;
            return ret;
        }
    }

    ret = calloc(1, sizeof(type_attribute));
    ret->field = strdup(attribute);

    type->attribute_count++;
    type->attributes = realloc(type->attributes, type->attribute_count * sizeof(type_attribute *));
    type->attributes[type->attribute_count - 1] = ret;

    return ret;
}

/**
 * Free a type attribute.
 * @param attr Attribute to free. */
void free_attribute(type_attribute *attr)
{
    free(attr->field);
    free(attr->name);
    free(attr->description);
    free(attr);
}

/**
 * Copy attributes from one type definition to another.
 * @param source Where to copy attributes from.
 * @param type Where to copy attributes to. */
void copy_attributes(const type_definition *source, type_definition *type)
{
    int attr;
    type_attribute *add;

    if (!source || source->attribute_count == 0) {
        return;
    }

    for (attr = 0; attr < source->attribute_count; attr++) {
        add = get_attribute_for_type(type, source->attributes[attr]->field);
        add->name = strdup(source->attributes[attr]->name);

        if (source->attributes[attr]->description) {
            add->description = strdup(source->attributes[attr]->description);
        }
    }
}

/**
 * Copy default attributes to a type definition.
 * @param type The type definition to copy default attribute to. */
void copy_default_attributes(type_definition *type)
{
    if (!default_type) {
        return;
    }

    copy_attributes(default_type, type);
}

/**
 * Returns a new type_definition having the default attributes. */
type_definition *get_type_definition()
{
    type_definition *ret = calloc(1, sizeof(type_definition));

    ret->attribute_count = 0;
    ret->attributes = NULL;

    if (default_type) {
        copy_default_attributes(ret);
    }

    return ret;
}

/**
 * Used for type import.
 * @param name Name of the type to import.
 * @return Type definition from ::types if found, NULL otherwise. */
type_definition *find_type_definition(const char *name)
{
    int type;

    for (type = 0; type < type_count; type++) {
        if (!strcmp(types[type]->name, name)) {
            return types[type];
        }
    }

    printf("Type not found for import: %s\n", name);
    return NULL;
}

/** To sort attributes using qsort(). */
int sort_type_attribute(const void *a, const void *b)
{
    const type_attribute **la = (const type_attribute **) a;
    const type_attribute **lb = (const type_attribute **) b;

    return strcmp((*la)->name, (*lb)->name);
}

/** Used for ::fake_names. */
typedef struct
{
    /** Fake name. */
    const char *fake_name;

    /** Real name. */
    char *real_name;
} fake_name_definition;

/**
 * Fake names, because things like "animation" actually is "animation_id"
 * and "object_int1" is "enemy_count". */
static fake_name_definition fake_names[] =
{
    {"animation", "animation_id"},
    {"object_int1", "enemy_count"},
    {"object_int2", "attacked_by_count"},
    {"object_int3", "ownercount"},
    {"movement_type", "move_type"},
    {"sub_type", "sub_type"},
    {"container", "weight_limit"},
    {NULL, NULL}
};

/**
 * Find real name from fake name in the ::fake_names array.
 * @param name Fake name to find.
 * @return The real name, or the passed name if not found. */
static char *find_fake_attr_name(char *name)
{
    int i;

    for (i = 0; fake_names[i].fake_name; i++) {
        if (!strcmp(name, fake_names[i].fake_name)) {
            return fake_names[i].real_name;
        }
    }

    return name;
}

/**
 * Find ignore list from ::lists.
 * @param name The ignore list to find.
 * @return The ignore list if found, NULL otherwise. */
ignore_list *find_ignore_list(const char *name)
{
    int list;

    for (list = 0; list < list_count; list++) {
        if (strcmp(lists[list]->name, name) == 0) {
            return lists[list];
        }
    }

    return NULL;
}

/**
 * Remove an attribute from type definition.
 *
 * Used for ignoring attributes.
 * @param type Type definition to remove attribute from.
 * @param attribute The attribute to remove. */
void ignore_attribute(type_definition *type, const char *attribute)
{
    int find;

    for (find = 0; find < type->attribute_count; find++) {
        if (!strcmp(attribute, type->attributes[find]->field)) {
            free_attribute(type->attributes[find]);

            if (find < type->attribute_count - 1) {
                type->attributes[find] = type->attributes[type->attribute_count-1];
            }

            type->attribute_count--;
            return;
        }
    }
}

/**
 * Remove all attributes in the specified list from the type.
 * @param type Type definition to remove ignored attributes.
 * @param list Ignore list. */
void ignore_attributes(type_definition *type, ignore_list *list)
{
    int attr;

    if (!list) {
        printf("%s has empty ignore list?\n", type->name);
        return;
    }

    for (attr = 0; attr < list->count; attr++) {
        ignore_attribute(type, list->fields[attr]);
    }
}

/**
 * Add a required parameter to the specified type.
 * @param type Type definition to.
 * @param buf Line read from the file, non processed. */
void add_required_parameter(type_definition *type, const char *buf)
{
    char *sn, *en, *sv, *ev;
    char value[200], name[200], temp[200];
    const flag_definition *flag;

    if (type == fallback_type) {
        /* the "Misc" type has dummy requirements, don't take that into account.
         * */
        return;
    }

    sn = strstr(buf, "arch");

    if (!sn) {
        return;
    }

    sn = strchr(sn, '"');
    en = strchr(sn + 1, '"');
    sv = strstr(buf, "value");
    sv = strchr(sv, '"');
    ev = strchr(sv + 1, '"');

    name[en - sn - 1] = '\0';
    strncpy(name, sn + 1, en - sn - 1);
    value[ev - sv - 1] = '\0';
    strncpy(value, sv + 1, ev - sv - 1);

    type->require_count++;
    type->required = realloc(type->required, type->require_count * sizeof(char *));

    flag = find_flag(name);

    if (flag) {
        snprintf(temp, sizeof(temp), "@ref %s %s", flag->code_name, strcmp(value, "0") ? "set" : "unset");
    }
    else {
        snprintf(temp, sizeof(temp), "@ref object::%s = %s", name, value);
    }

    type->required[type->require_count - 1] = strdup(temp);
}

/**
 * Read all lines related to a type.
 * @param type Type definition.
 * @param file File to read from.
 * @param block_end If encountered on a line, will stop reading. */
void read_type(type_definition *type, FILE *file, const char *block_end)
{
    char buf[200], tmp[200];
    char *find, *end;
    type_attribute *attr;

    while (fgets(buf, sizeof(buf), file)) {
        if (strstr(buf, block_end) != NULL) {
            if (type->attribute_count) {
                qsort(type->attributes, type->attribute_count, sizeof(type_attribute *), sort_type_attribute);
            }

            return;
        }

        if (strstr(buf, "<description>") != NULL) {
            while (fgets(buf, sizeof(buf), file)) {
                if (strstr(buf, "</description>") != NULL) {
                    break;
                }
                else if (strstr(buf, "<![CDATA[")) {
                    continue;
                }

                if (type->description) {
                    type->description = realloc(type->description, strlen(type->description) + strlen(buf) + 1);
                    strcat(type->description, buf);
                }
                else {
                    type->description = strdup(buf);
                }
            }

            find = strstr(type->description, "]]>");

            if (find) {
                type->description[find-type->description] = '\0';
            }
            while (type->description[strlen(type->description) - 1] == '\n') {
                type->description[strlen(type->description) - 1] = '\0';
            }
        }

        if (strstr(buf, "<ignore_list") != NULL) {
            find = strstr(buf, "name=");

            if (!find) {
                return;
            }

            find = strchr(find + 1, '"');

            if (!find) {
                return;
            }

            end = strchr(find + 1, '"');

            if (!end) {
                return;
            }

            tmp[end - find - 1] = '\0';
            strncpy(tmp, find + 1, end-find - 1);
            ignore_attributes(type, find_ignore_list(tmp));
        }

        if (strstr(buf, "<ignore>") != NULL) {
            while (fgets(buf, sizeof(buf), file)) {
                if (strstr(buf, "</ignore>") != NULL) {
                    break;
                }

                find = strstr(buf, "arch=");

                if (!find) {
                    continue;
                }

                find = strchr(find + 1, '"');

                if (!find) {
                    continue;
                }

                end = strchr(find + 1, '"');

                if (!end) {
                    continue;
                }

                tmp[end-find - 1] = '\0';
                strncpy(tmp, find + 1, end - find - 1);
                ignore_attribute(type, tmp);
            }
        }

        if (strstr(buf, "<required>") != NULL) {
            while (fgets(buf, sizeof(buf), file)) {
                if (strstr(buf, "</required>") != NULL) {
                    break;
                }

                add_required_parameter(type, buf);
            }
        }

        if (strstr(buf, "<import_type") != NULL) {
            type_definition *import;

            find = strstr(buf, "name=");

            if (!find) {
                return;
            }

            find = strchr(find + 1, '"');

            if (!find) {
                return;
            }

            end = strchr(find + 1, '"');

            if (!end) {
                return;
            }

            tmp[end - find - 1] = '\0';
            strncpy(tmp, find + 1, end - find - 1);
            import = find_type_definition(tmp);

            if (import) {
                copy_attributes(import, type);
            }
            else {
                printf("%s: import %s not found\n", type->name, tmp);
            }
        }

        if (strstr(buf, "<attribute") != NULL) {
            if (strstr(buf, "/>") != NULL) {
                continue;
            }

            find = strstr(buf, "arch");

            if (!find) {
                continue;
            }

            find = strchr(find, '"');
            end = strchr(find + 1, '"');

            if (end == find + 1) {
                /* Empty arch, meaning inventory or such, ignore. */
                continue;
            }

            tmp[end - find - 1] = '\0';
            strncpy(tmp, find + 1, end - find - 1);

            attr = get_attribute_for_type(type, tmp);

            find = strstr(buf, "editor");
            find = strchr(find, '"');
            end = strchr(find + 1, '"');
            tmp[end - find - 1] = '\0';
            strncpy(tmp, find + 1, end - find - 1);
            attr->name = strdup(tmp);

            while (fgets(buf, sizeof(buf), file)) {
                if (strstr(buf, "</attribute>") != NULL) {
                    break;
                }

                if (attr->description) {
                    attr->description = realloc(attr->description, strlen(attr->description) + strlen(buf) + 1);
                    strcat(attr->description, buf);
                }
                else {
                    attr->description = strdup(buf);
                }
            }

            if (attr->description) {
                while (attr->description[strlen(attr->description) - 1] == '\n') {
                    attr->description[strlen(attr->description) - 1] = '\0';
                }
            }
        }
    }
}

/**
 * Get an attribute, create it if it doesn't exist yet.
 * @param name Name of the attribute to get/create.
 * @return Found/created attribute. */
attribute_definition *get_attribute(const char *name)
{
    int attr;
    attribute_definition *ret;

    for (attr = 0; attr < attribute_count; attr++) {
        if (!strcmp(attributes[attr]->field, name)) {
            return attributes[attr];
        }
    }

    ret = calloc(1, sizeof(attribute_definition));
    attribute_count++;
    attributes = realloc(attributes, attribute_count * sizeof(attribute_definition *));
    attributes[attribute_count - 1] = ret;

    ret->field = strdup(name);

    return ret;
}

/**
 * Gets a type description for specified attribute, create it if doesn't
 * exist.
 * @param attribute Attribute to get description for.
 * @param description The description.
 * @return Attribute type. */
attribute_type *get_description_for_attribute(attribute_definition *attribute, const char *description)
{
    int desc;
    attribute_type *add;

    for (desc = 0; desc < attribute->type_count; desc++) {
        if (!description && !attribute->types[desc]->description) {
            return attribute->types[desc];
        }

        if (description && attribute->types[desc]->description && !strcmp(description, attribute->types[desc]->description)) {
            return attribute->types[desc];
        }
    }

    add = calloc(1, sizeof(attribute_type));
    attribute->type_count++;
    attribute->types = realloc(attribute->types, attribute->type_count * sizeof(attribute_type));
    attribute->types[attribute->type_count - 1] = add;

    if (description) {
        add->description = strdup(description);
    }

    return add;
}

/**
 * Add type to attribute.
 * @param attribute Attribute defition to add to.
 * @param type Type definition.
 * @param attr Attribute ID. */
void add_type_to_attribute(attribute_definition *attribute, type_definition *type, int attr)
{
    attribute_type *att;

    att = get_description_for_attribute(attribute, type->attributes[attr]->description);
    att->count++;
    att->type = realloc(att->type, att->count * sizeof(const char *));
    att->number = realloc(att->number, att->count * sizeof(int));
    att->type[att->count - 1] = strdup(type->name);
    att->number[att->count - 1] = type->number;
}

/**
 * Read the contents of a <code>\<ignore_list\></code> tag.
 * @param name Name of the ignore list.
 * @param file File to read from. */
void read_ignore_list(const char *name, FILE *file)
{
    char buf[200], tmp[200];
    char *start, *end;
    ignore_list *list;

    list = calloc(1, sizeof(ignore_list));
    list_count++;
    lists = realloc(lists, list_count * sizeof(ignore_list *));
    lists[list_count - 1] = list;
    list->name = strdup(name);

    while (fgets(buf, sizeof(buf), file)) {
        if (strstr(buf, "</ignore_list>") != NULL) {
            return;
        }

        start = strstr(buf, "arch=");

        if (!start) {
            continue;
        }

        start = strchr(start + 1, '"');

        if (!start) {
            continue;
        }

        end = strchr(start + 1, '"');

        if (!end) {
            continue;
        }

        tmp[end - start - 1] = '\0';
        strncpy(tmp, start + 1, end - start - 1);

        list->count++;
        list->fields = realloc(list->fields, list->count * sizeof(char *));
        list->fields[list->count - 1] = strdup(tmp);
    }
}

/** Fields part of the living structure. */
static const char *in_living[] =
{
    "exp",

    "hp",
    "maxhp",
    "sp",
    "maxsp",

    "food",
    "dam",
    "wc",
    "ac",
    "wc_range",

    "str",
    "dex",
    "con",
    "wis",
    "cha",
    "int",
    "pow",
    NULL
};

/** Custom attributes we know about, to point to the right page. */
static const char *custom_attributes[] =
{
    "faction",
    "faction_kill_penalty",
    "faction_rep",
    "notification_action",
    "notification_delay",
    "notification_message",
    "notification_shortcut",
    "spawn_time",
    "match",
    NULL
};

int is_custom_attribute(const char *attribute)
{
    int val, i;

    for (val = 0; custom_attributes[val] != NULL; val++) {
        if (!strcmp(custom_attributes[val], attribute)) {
            return 1;
        }
    }

    if (!strncmp(attribute, "attack_", 7) || !strncmp(attribute, "protection_", 11)) {
        attribute += (*attribute == 'a' ? 7 : 11);

        for (i = 0; i < NROFATTACKS; i++) {
            if (!strcmp(attribute, attack_save[i])) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Write the part to the right of a \@ref for the specified attribute.
 * @param attribute Attribute.
 * @param file File to write to. */
void write_attribute_reference(char *attribute, FILE *file)
{
    const flag_definition *flag = find_flag(attribute);
    int val;

    if (flag) {
        fprintf(file, "%s", flag->code_name);
        return;
    }

    for (val = 0; in_living[val] != NULL; val++) {
        if (!strcmp(in_living[val], attribute)) {
            fprintf(file, "liv::%s", attribute);
            return;
        }
    }

    if (is_custom_attribute(attribute)) {
        fprintf(file, "page_custom_attributes \"%s\"", attribute);
        return;
    }

    if (!strcmp(attribute, "connected")) {
        fprintf(file, "page_connected \"connection value\"");
        return;
    }

    fprintf(file, "obj::%s", find_fake_attr_name(attribute));
}

/**
 * Write a type definition file.
 * @param type Type definition to write. */
void write_type_file(type_definition *type)
{
    FILE *file;
    char buf[200];
    int attr, req;

    snprintf(buf, sizeof(buf), "%s/%s/type_%d.dox", destination_dir, type_dir, type->number);
    file = fopen(buf, "w+");

    fprintf(file, "/**\n");

    /* Auto-generate documentation for the type, so no need to change define.h
     * */
    if (type->number > 0) {
        for (req = 0; type_names[req].code_name != NULL; req++) {
            if (type_names[req].value == type->number) {
                fprintf(file, "@var %s\nSee @ref page_type_%d\n*/\n\n/**\n", type_names[req].code_name, type->number);
                break;
            }
        }
    }

    fprintf(file, "@page page_type_%d %s\n\n", type->number, type->name);
    fprintf(file, "\n@section Description\n");
    fprintf(file, "%s\n\n", type->description);

    if (type != fallback_type) {
        fprintf(file, "\n\nType defined by:\n");

        if (type->number && type->number < OBJECT_TYPE_MAX) {
            fprintf(file, "- @ref object::type = %d\n", type->number);
        }

        for (req = 0; req < type->require_count; req++) {
            fprintf(file, "- %s\n", type->required[req]);
        }
    }

    fprintf(file, "\n\n@section Attributes\n\n");
    fprintf(file, "<table>\n\t<tr>\n\t\t<th>Attribute</th>\n\t\t<th>Field</th>\n\t\t<th>Description</th>\n\t</tr>\n");

    for (attr = 0; attr < type->attribute_count; attr++) {
        fprintf(file, "\t<tr>\n\t\t<td>%s</td>\n\t\t<td>@ref ", type->attributes[attr]->name);
        write_attribute_reference(type->attributes[attr]->field, file);
        fprintf(file, "</td>\n\t\t<td>%s\n\t\t</td>\n\t</tr>\n", type->attributes[attr]->description ? type->attributes[attr]->description : "(no description)");
    }

    fprintf(file, "</table>\n*/\n");

    fclose(file);
}

/** Write index of all types. */
void write_type_index()
{
    FILE *index;
    int type;
    char buf[200];

    snprintf(buf, sizeof(buf), "%s/%s/types.dox", destination_dir, type_dir);
    index = fopen(buf, "w+");

    if (index == NULL) {
        printf("Could not open %s\n", buf);
        exit(0);
    }

    fprintf(index, "/**\n@page type_index Type index\n");
    fprintf(index, "Types not listed here have the attributes defined in @ref page_type_0 \"this page\".\n\n");

    for (type = 0; type < type_count; type++) {
        if (types[type]) {
            fprintf(index, "- @ref page_type_%d \"%s\"\n", types[type]->number, types[type]->name);
        }
    }

    fprintf(index, "*/\n");

    fclose(index);
}

/**
 * Write the description of a field.
 * @param attribute Attribute definition to write. */
void write_attribute_file(attribute_definition *attribute)
{
    FILE *file;
    char buf[200];
    int type, desc;
    const char *end;

    snprintf(buf, sizeof(buf), "%s/%s/field_%s.dox", destination_dir, field_dir, attribute->field);
    file = fopen(buf, "w+");

    fprintf(file, "/**\n@var ");
    write_attribute_reference(attribute->field, file);
    fprintf(file, "\n@sa @ref page_field_%s\n*/\n\n", attribute->field);

    fprintf(file, "/**\n");
    fprintf(file, "@page page_field_%s ", attribute->field);
    write_attribute_reference(attribute->field, file);
    fprintf(file, " Uses\n");

    fprintf(file, "<table>\n\t<tr>\n\t\t<th>Type(s)</th>\n\t\t<th>Description</th>\n\t</tr>");

    for (desc = 0; desc < attribute->type_count; desc++) {
        fprintf(file, "\t<tr>\n\t\t<td>\n");

        for (type = 0; type < attribute->types[desc]->count; type++) {
            if (type < attribute->types[desc]->count-1) {
                end = ", ";
            }
            else {
                end = "\n";
            }

            fprintf(file, "@ref page_type_%d%s", attribute->types[desc]->number[type], end);
        }

        fprintf(file, "\t\t</td><td>%s</td>\n\t</tr>\n", attribute->types[desc]->description ? attribute->types[desc]->description : "(no description)");
    }

    fprintf(file, "\n*/\n");
    fclose(file);
}

/**
 * Main function of the program. */
int main(int argc, char **argv)
{
    FILE *xml;
    int number, attr, dummy;
    char buf[200], tmp[200];
    char *start, *end;
    type_definition *type;

    if (argc < 2) {
        printf("Syntax: %s /path/to/Gridarta/types.xml\n", argv[0]);
        return 1;
    }

    /* Dummy type number for special types. */
    dummy = OBJECT_TYPE_MAX+50;
    xml = fopen(argv[1], "r");

    if (!xml) {
        printf("Could not open file: %s\n", argv[1]);
        return 1;
    }

    while (fgets(buf, sizeof(buf), xml) != NULL) {
        if (buf[0] == '#') {
            continue;
        }

        if (strstr(buf, "<default_type>")) {
            default_type = get_type_definition();
            default_type->name = strdup("(default type)");
            read_type(default_type, xml, "</default_type>");
            continue;
        }

        if (strstr(buf, "<ignore_list") != NULL) {
            start = strstr(buf, "name=");
            start = strchr(start + 1, '"');
            end = strchr(start + 1, '"');
            tmp[end - start - 1] = '\0';
            strncpy(tmp, start + 1, end - start - 1);
            read_ignore_list(tmp, xml);
            continue;
        }

        start = strstr(buf, "<type number");

        if (start) {
            start = strchr(start, '"');
            end = strchr(start + 1, '"');
            tmp[end - start - 1] = '\0';
            strncpy(tmp, start + 1, end - start - 1);

            number = atoi(tmp);
            start = strstr(end, "name=");
            start = strchr(start, '"');
            end = strchr(start + 1, '"');
            tmp[end - start - 1] = '\0';
            strncpy(tmp, start + 1, end - start - 1);

            if (!strcmp(tmp, "Misc")) {
                fallback_type = get_type_definition();
                type = fallback_type;
            }
            else {
                if (number == 0) {
                    number = dummy++;
                }

                type = get_type_definition();
                type_count++;
                types = realloc(types, type_count * sizeof(type_definition *));
                types[type_count - 1] = type;
            }

            type->number = number;
            type->name = strdup(tmp);

            read_type(type, xml, "</type>");
        }
    }

    free(fallback_type->description);
    fallback_type->description = strdup("This type regroups all types who don't have a specific definition.");

    for (number = 0; number < type_count; number++) {
        for (attr = 0; attr < types[number]->attribute_count; attr++) {
            add_type_to_attribute(get_attribute(types[number]->attributes[attr]->field), types[number], attr);
        }
    }

    write_type_index();

    for (number = 0; number < type_count; number++) {
        write_type_file(types[number]);
    }

    write_type_file(fallback_type);

    for (attr = 0; attr < attribute_count; attr++) {
        if (!is_custom_attribute(attributes[attr]->field)) {
            write_attribute_file(attributes[attr]);
        }
    }

    fclose(xml);
    free(types);
    return 0;
}
