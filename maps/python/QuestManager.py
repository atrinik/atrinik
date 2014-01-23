## @file
## This file has the QuestManager class, which is used by all Python
## quests to provide common quest-related functions.

from Atrinik import *

## Create a new quest container object.
## @param where Where to insert the object.
## @param q_type Type of the quest.
## @param name Name of the quest.
## @param msg Description of the quest.
## @param num Number of objects/monsters to collect/kill.
def _create_quest_object(where, q_type, name, msg = None, num = 0):
    obj = where.CreateObject("quest_container")
    obj.magic = 0
    obj.name = name
    obj.sub_type = q_type
    obj.msg = msg
    obj.last_grace = num
    return obj

## Find a quest object inside obj's quest container.
## @param obj Player in which to search.
## @param quest Quest name to look for.
## @return The quest container if found, None otherwise.
def get_quest_object(obj, quest):
    try:
        return obj.Controller().quest_container.FindObject(name = quest)
    except:
        return None

## Base quest manager class, from which the actual quest managers inherit.
class QuestManagerBase:
    ## The quest object inside player's quest container.
    quest_object = None
    ## Information about the quest.
    quest = {}
    ## Activator object.
    activator = None

    ## Initialize function.
    ## @param activator The activator object.
    ## @param quest Information about the quest as a dictionary.
    def __init__(self, activator, quest):
        self.quest_object = get_quest_object(activator, quest["quest_name"])
        self.quest = quest
        self.activator = activator

    ## Check if the quest has been started.
    def started(self):
        return self.quest_object != None

    ## Check if a quest has been completed before.
    def completed(self):
        if not self.started():
            return 0

        # Check the quest object's magic field to see if the quest has
        # been completed.
        if self.quest_object.magic == QUEST_STATUS_COMPLETED:
            return 1

        return 0

    def _get_quest_item_num(self, obj, quest):
        # Just one item, easy.
        if not obj.last_grace:
            quest_item = self.activator.FindObject(INVENTORY_CONTAINERS, quest["arch_name"], quest["item_name"])

            if quest_item:
                return 1
        # Got to count the objects otherwise.
        else:
            num = 0

            # Find all matching objects, and count them.
            for tmp in self.activator.FindObject(INVENTORY_CONTAINERS, quest["arch_name"], quest["item_name"], multiple = True):
                num += max(1, tmp.nrof)

            return num

        return 0

    ## Check if (part of a) quest has been finished.
    ## @param obj Quest container object.
    ## @param quest Information about the quest.
    def _finished(self, obj, quest):
        # For the kill type quest check if we have killed enough
        # monsters.
        if quest["type"] == QUEST_TYPE_KILL:
            if obj.last_sp >= obj.last_grace:
                return True
        # For the kill item quest type check for the quest item.
        elif quest["type"] == QUEST_TYPE_KILL_ITEM:
            if self._get_quest_item_num(obj, quest) >= max(1, obj.last_grace):
                return True

        return False

    def _num2finish(self, obj, quest):
        if quest["type"] == QUEST_TYPE_KILL:
            return obj.last_grace - obj.last_sp
        elif quest["type"] == QUEST_TYPE_KILL_ITEM:
            return max(1, obj.last_grace) - self._get_quest_item_num(obj, quest)

        return 0

    ## Complete a quest object.
    ## @param obj The quest object.
    ## @param quest Information about the quest.
    def _complete(self, obj, quest):
        # Special handling for kill item quests.
        if quest["type"] == QUEST_TYPE_KILL_ITEM:
            # Are we going to keep the quest item(s)?
            keep = "quest_item_keep" in quest and quest["quest_item_keep"]

            # Find all matching objects.
            for tmp in self.activator.FindObject(INVENTORY_CONTAINERS, quest["arch_name"], quest["item_name"], multiple = True):
                # Keeping the quest item(s), make sure quest-related flags
                # are not set.
                if keep:
                    tmp.f_quest_item = False
                    tmp.f_startequip = False
                else:
                    tmp.Remove()

## The Quest Manager class.
class QuestManager(QuestManagerBase):
    ## Start a quest.
    ## @param sound If not None, will play this sound effect.
    def start(self, sound = "learnspell.ogg"):
        self.quest_object = _create_quest_object(self.activator.Controller().quest_container, self.quest["type"], self.quest["quest_name"], "message" in self.quest and self.quest["message"] or None, self.quest["type"] == QUEST_TYPE_KILL and self.quest["kills"] or 0)

        if sound:
            self.activator.Controller().Sound(sound)

    ## Check if the quest has been finished.
    ## @return True if the quest has been finished, False otherwise.
    def finished(self):
        if not self.started():
            return False

        return self._finished(self.quest_object, self.quest)

    ## Complete a quest.
    ## @param sound If not None, will play this sound effect.
    def complete(self, sound = "learnspell.ogg"):
        # Mark the quest as completed.
        self.quest_object.magic = QUEST_STATUS_COMPLETED

        if sound:
            self.activator.Controller().Sound(sound)

        # Do special handling for different quest types.
        self._complete(self.quest_object, self.quest)

    ## Get number of monsters the player needs to kill to complete the
    ## quest.
    ## @return Number of monsters needed to kill, -1 if the quest does not
    ## require killing monsters.
    def num_to_kill(self):
        if not self.quest_object or self.quest["type"] != QUEST_TYPE_KILL:
            return -1

        return self.quest_object.last_grace - self.quest_object.last_sp

## More complex quest manager interface, which allows multi-part quests.
class QuestManagerMulti(QuestManagerBase):
    ## Internal function to start part of a quest.
    ## @param part Information about this quest part.
    ## @param num ID of the part.
    def _start_part(self, part, num):
        _create_quest_object(self.quest_object, part["type"], "part " + str(num), "message" in part and part["message"] or None, "num" in part and part["num"] or 0)

    ## Start (part of) the quest.
    ## @param part If None, will start all parts of the quest, otherwise
    ## only the specified part ID.
    ## @warning No checking whether part was already started or not is
    ## performed.
    def start(self, part = None, sound = "learnspell.ogg"):
        # Create the quest object holding the parts.
        if not self.quest_object:
            self.quest_object = _create_quest_object(self.activator.Controller().quest_container, QUEST_TYPE_MULTI, self.quest["quest_name"], "message" in self.quest and self.quest["message"] or None)

        # Start specified part?
        if part:
            self._start_part(self.quest["parts"][part - 1], part)
        # Start all parts.
        else:
            num = 1

            for part in self.quest["parts"]:
                self._start_part(part, num)
                num += 1

        if sound:
            self.activator.Controller().Sound(sound)

    ## Check if the specified quest part has already been started.
    ## @param part ID of the part to check.
    ## @return True if the quest part has been started, False otherwise.
    def started_part(self, part):
        if not self.quest_object:
            return False

        if not self.quest_object.FindObject(name = "part " + str(part)):
            return False

        return True

    ## Check if the specified (part of) quest has been finished.
    ## @param part If None, will check all parts, otherwise only the
    ## specified part ID.
    ## @return True if (part of) the quest has been finished, False
    ## otherwise.
    def finished(self, part = None):
        if not self.started():
            return False

        # Check part of the quest.
        if part:
            obj = self.quest_object.FindObject(name = "part " + str(part))

            if not obj:
                return False

            return self._finished(obj, self.quest["parts"][part - 1])
        # Otherwise all.
        else:
            for i in range(len(self.quest["parts"])):
                obj = self.quest_object.FindObject(name = "part " + str(i + 1))

                if not obj or not self._finished(obj, self.quest["parts"][i]):
                    return False

            return True

        return False

    ## Complete (part of) the quest.
    ## @param part If None, complete all parts of the quest, otherwise
    ## only the specified part.
    def complete(self, part = None, sound = "learnspell.ogg", skip_completion = False):
        if sound:
            self.activator.Controller().Sound(sound)

        # Complete specified part of the quest.
        if part:
            # Find the quest part object.
            obj = self.quest_object.FindObject(name = "part " + str(part))

            # Mark the quest part as completed.
            if obj:
                obj.magic = QUEST_STATUS_COMPLETED
                self._complete(obj, self.quest["parts"][part - 1])

            if skip_completion:
                return

            # Check all quest parts. If all are completed, complete the
            # entire quest.
            for tmp in self.quest_object.FindObject(archname = "quest_container", multiple = True):
                if tmp.magic != QUEST_STATUS_COMPLETED:
                    return
        # Complete all parts of the quest.
        else:
            for obj in self.quest_object.FindObject(archname = "quest_container", multiple = True):
                if obj.magic != QUEST_STATUS_COMPLETED:
                    obj.magic = QUEST_STATUS_COMPLETED
                    self._complete(obj, self.quest["parts"][int(obj.name[5:]) - 1])

        # Complete the quest itself now.
        self.quest_object.magic = QUEST_STATUS_COMPLETED

    ## Check if part of the quest has been completed.
    ## @param part ID of the part to check.
    def completed_part(self, part):
        if not self.quest_object:
            return False

        obj = self.quest_object.FindObject(name = "part " + str(part))

        if not obj:
            return False

        if obj.magic != QUEST_STATUS_COMPLETED:
            return False

        return True

    def num2finish(self, part):
        if not self.quest_object:
            return -1

        obj = self.quest_object.FindObject(name = "part " + str(part))

        if not obj:
            return -1

        return self._num2finish(obj, self.quest["parts"][part - 1])
