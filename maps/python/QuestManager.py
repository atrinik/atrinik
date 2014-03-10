## @file
## This file has the QuestManager class, which is used by all Python
## quests to provide common quest-related functions.

from Atrinik import *
import time

class QuestManager:
    """
    Quest manager implementation.
    """

    def _quest_adjust(self, quest):
        """Compatibility function for old quest definitions."""

        if "arch_name" in quest:
            quest["item"] = {"arch": quest["arch_name"], "name": quest["item_name"]}

        if "num" in quest:
            if "item" in quest:
                quest["item"]["nrof"] = quest["num"]
            else:
                quest["kill"] = {"nrof": quest["num"]}

        if "parts" in quest:
            for part in quest["parts"]:
                self._quest_adjust(quest["parts"][part])

    def __init__(self, activator, quest):
        """
        Initiate the quest manager class.

        Args:
            activator: Player object
            quest: Quest information dictionary
        """

        if activator.type != Type.PLAYER:
            return

        if not "parts" in quest:
            quest["parts"] = {
                quest["quest_name"]: quest,
            }

        # TODO remove at some point
        self._quest_adjust(quest)

        self.activator = activator
        self.quest = quest
        self.sound_last = None

        self.quest_container = activator.Controller().quest_container
        self.quest_object = self.quest_container.FindObject(name = self.get_quest_uid())

        if self.quest.get("repeat", False) and self.get_qp_remaining() != 0 and self.completed():
            if self.quest_object.exp == 0 or int(time.time()) >= self.quest_object.exp:
                self.quest_object.Remove()
                self.quest_object = None

    def get_quest_name(self, quest = None):
        """Acquire the quest's name."""

        if quest == None:
            quest = self.quest

        # TODO Remove 'uid' and 'quest_name' once all the old quests have
        # been updated to the new system.
        for attr in ("name", "uid", "quest_name"):
            val = quest.get(attr)

            if val:
                return val

        return None

    def get_quest_uid(self, quest = None):
        """Acquire the quest's UID."""

        if quest == None:
            quest = self.quest

        # TODO Remove 'quest_name' once all the old quests have been
        # updated to the new system.
        for attr in ("uid", "quest_name"):
            val = quest.get(attr)

            if val:
                return val

        return None

    def get_quest_msg(self, quest = None):
        """Acquire the quest's information message."""

        if quest == None:
            quest = self.quest

        # TODO Remove 'message' once all the old quests have been
        # updated to the new system.
        for attr in ("info", "message"):
            val = quest.get(attr)

            if val:
                return val

        return None

    def create_quest_object(self, where, quest, uid):
        obj = where.CreateObject("quest_container")
        obj.magic = 0
        obj.last_grace = 0
        obj.name = uid
        obj.race = self.get_quest_name(quest)
        obj.msg = self.get_quest_msg(quest)

        for attr, quest_type in (
            ("item", QUEST_TYPE_ITEM),
            ("kill", QUEST_TYPE_KILL),
            ("special", QUEST_TYPE_SPECIAL),
                                ):
            if attr in ("item", "kill"):
                val = quest.get(attr)

                if val:
                    obj.sub_type = quest_type
                    obj.last_grace = val.get("nrof", 1)
                    break
            elif attr == "special":
                obj.sub_type = quest_type

        return obj

    def sound(self, sound):
        if self.sound_last == sound:
            return

        self.activator.Controller().Sound(sound)
        self.sound_last = sound

    def get_qp_max(self):
        return int(max(1, self.activator.level * 0.35 + 0.5))

    def get_qp_restored(self):
        qp_max = self.get_qp_max()
        return min(int((time.time() - self.quest_container.exp) / (60 * 60) * (qp_max * 0.10)), qp_max)

    def get_qp_current(self):
        return self.quest_container.magic

    def get_qp_remaining(self):
        if not self.quest.get("repeat", False):
            return -1

        restored = self.get_qp_restored()

        if restored != 0:
            self.quest_container.magic = max(0, self.quest_container.magic - restored)
            self.quest_container.exp = int(time.time())

        return max(0, self.get_qp_max() - self.get_qp_current())

    def use_qp(self):
        if not self.quest.get("repeat", False):
            return

        self.quest_container.magic += 1

        if self.quest_container.exp == 0:
            self.quest_container.exp = int(time.time())

        delay = self.quest.get("repeat_delay", None)

        if delay:
            self.quest_object.exp = int(time.time()) + delay

    def get_quest_item_num(self, obj, quest):
        # Just one item, easy.
        if obj.last_grace <= 1:
            quest_item = self.activator.FindObject(INVENTORY_CONTAINERS, quest["item"]["arch"], quest["item"]["name"])

            if quest_item:
                return 1
        # Got to count the objects otherwise.
        else:
            num = 0

            # Find all matching objects, and count them.
            for tmp in self.activator.FindObject(INVENTORY_CONTAINERS, quest["item"]["arch"], quest["item"]["name"], multiple = True):
                num += max(1, tmp.nrof)

            return num

        return 0

    def num2finish(self, part):
        if not self.quest_object:
            return -1

        part, quest = self.get_part(part)
        obj = self.quest_object.FindObject(name = part)

        if not obj:
            return -1

        if obj.sub_type == QUEST_TYPE_KILL:
            return obj.last_grace - obj.last_sp
        elif obj.sub_type == QUEST_TYPE_ITEM:
            return obj.last_grace - self.get_quest_item_num(obj, quest)

        return 0

    def num_to_kill(self):
        """Deprecated; do not use."""

        # TODO remove me

        return self.num2finish(self.quest["quest_name"])

    def need_start(self, *args, **kwargs):
        return not self.started(*args, **kwargs)

    def need_finish(self, *args, **kwargs):
        return self.started(*args, **kwargs) and not self.finished(*args, **kwargs) and not self.completed(*args, **kwargs)

    def need_complete(self, *args, **kwargs):
        return self.started(*args, **kwargs) and self.finished(*args, **kwargs) and not self.completed(*args, **kwargs)

    def get_part(self, part):
        if type(part) is str:
            return part, self.quest["parts"][part]

        newpart = self.quest

        for val in part:
            newpart = newpart["parts"][val]

        return val, newpart

    def start(self, part = None, sound = "learnspell.ogg"):
        # TODO remove eventually, make part mandatory
        if part == None:
            part = self.quest["quest_name"]

        if not self.quest_object:
            self.quest_object = self.create_quest_object(self.quest_container, self.quest, self.get_quest_uid())

        part, quest = self.get_part(part)
        self.create_quest_object(self.quest_object, quest, part)

        self.sound(sound)

    def complete(self, part = None, sound = "learnspell.ogg", skip_completion = False):
        # TODO remove eventually, make part mandatory
        if part == None:
            part = self.quest["quest_name"]

        part, quest = self.get_part(part)
        obj = self.quest_object.FindObject(name = part)

        if not obj:
            return

        # Mark the quest part as completed.
        obj.magic = QUEST_STATUS_COMPLETED
        self.sound(sound)

        # Special handling for kill item quests.
        if obj.sub_type == QUEST_TYPE_ITEM:
            # Are we going to keep the quest item(s)?
            keep = quest["item"].get("keep")
            nrof = quest["item"].get("nrof", 1)
            removed = 0

            # Find all matching objects.
            for tmp in self.activator.FindObject(INVENTORY_CONTAINERS, quest["item"]["arch"], quest["item"]["name"], multiple = True):
                # Keeping the quest item(s), make sure quest-related flags
                # are not set.
                if keep:
                    tmp.f_quest_item = False
                    tmp.f_startequip = False
                else:
                    remove = min(max(1, tmp.nrof), nrof - removed)
                    tmp.Decrease(remove)
                    removed += remove

                    if removed == nrof:
                        break

        if skip_completion:
            return

        # Check all quest parts. If all are completed, complete the
        # entire quest.
        for obj in self.quest_object.inv:
            if obj.magic != QUEST_STATUS_COMPLETED:
                return

        # Complete the quest itself now.
        self.quest_object.magic = QUEST_STATUS_COMPLETED
        self.use_qp()

    def finished(self, part = None):
        if not self.started(part):
            return False

        # TODO remove eventually, make part mandatory
        if part == None:
            part = self.quest["quest_name"]

        part, quest = self.get_part(part)
        obj = self.quest_object.FindObject(name = part)

        if not obj:
            return False

        # For the kill type quest check if we have killed enough
        # monsters.
        if obj.sub_type == QUEST_TYPE_KILL:
            return obj.last_sp >= obj.last_grace
        # For the kill item quest type check for the quest item.
        elif obj.sub_type == QUEST_TYPE_ITEM:
            return self.get_quest_item_num(obj, quest) >= obj.last_grace

        return True

    def started(self, part = None):
        if self.quest_object == None:
            return False

        if part:
            part, quest = self.get_part(part)

            return self.quest_object.FindObject(name = part) != None

        return True

    def completed(self, part = None):
        if not self.started(part):
            return False

        if part == None:
            # If no more quest points, the quest is "completed for now".
            if self.get_qp_remaining() == 0:
                return True

            # Check the quest object's magic field to see if the quest has
            # been completed.
            if self.quest_object.magic == QUEST_STATUS_COMPLETED:
                return True
        else:
            part, quest = self.get_part(part)
            obj = self.quest_object.FindObject(name = part)

            if obj and obj.magic == QUEST_STATUS_COMPLETED:
                return True

        return False

class QuestManagerMulti(QuestManager):
    pass
