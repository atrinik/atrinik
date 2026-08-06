"""Operator tools for managing player quest state."""

from Atrinik import *
import InterfaceQuests


def remove_quest_items(quest):
    """Remove unfinished quest items described by a quest definition."""

    removed = 0

    for part in quest.get("parts", {}).values():
        item = part.get("item")

        if item:
            for obj in activator.FindObjects(
                    INVENTORY_CONTAINERS, item["arch"], item.get("name")):
                if obj.f_quest_item:
                    removed += max(1, obj.nrof)
                    obj.Destroy()

        removed += remove_quest_items(part)

    return removed


def main():
    args = WhatIsMessage().split()

    if len(args) != 2 or args[0] != "reset":
        pl.DrawInfo("Usage: /quest reset <quest-uid>", COLOR_WHITE)
        return

    uid = args[1]
    quest = getattr(InterfaceQuests, uid, None)

    if not isinstance(quest, dict) or quest.get("uid") != uid:
        pl.DrawInfo("Unknown quest UID: {}".format(uid), COLOR_RED)
        return

    quest_object = pl.quest_container.FindObject(name=uid)

    if not quest_object:
        pl.DrawInfo("No quest state found for: {}".format(uid), COLOR_RED)
        return

    removed = remove_quest_items(quest)
    quest_object.Destroy()
    pl.DrawInfo(
        "Reset quest '{}' and removed {} quest item{}.".format(
            quest["name"], removed, "" if removed == 1 else "s"),
        COLOR_WHITE
    )


main()
