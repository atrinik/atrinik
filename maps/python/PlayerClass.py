## @file
## Handles player classes.

from Atrinik import *

class PlayerClass:
    def __init__(self, activator):
        self._activator = activator

    def get_classes(self):
        return ["warrior", "archer", "sorcerer", "warlock", "priest", "paladin"]

    def get_class_name_gender(self, name):
        female = self._activator.GetGender() == Gender.FEMALE

        if not female:
            return name

        if name == "sorcerer":
            return "sorceress"
        elif name == "priest":
            return "priestess"

        return name

    def get_class_bonuses(self, name):
        if name == "warrior":
            return ["+20% hit points", "+2 AC", "+2 Strength", "+1 Dexterity", "+1 Constitution"]
        elif name == "archer":
            return ["+5% hit points", "+2 Strength", "+3 Dexterity", "+1 Intelligence"]
        elif name == "sorcerer":
            return ["+20% spell points", "+3 Power", "+2 Intelligence"]
        elif name == "warlock":
            return ["+10% hit points", "+10% spell points", "+1 Strength", "+2 Power"]
        elif name == "priest":
            return ["+20% grace points", "+3 Wisdom", "+2 Intelligence"]
        elif name == "paladin":
            return ["+10% hit points", "+10% grace points", "+1 Strength", "+2 Wisdom"]

    def get_class(self):
        return self._activator.Controller().class_ob

    def set_class(self, name):
        class_ob = self.get_class()

        # If the class object already exists, remove it.
        if class_ob:
            class_ob.Destroy()

        # Create new class object.
        self._activator.CreateObject("class_" + name)
        # Make sure the activator gets the bonuses.
        self._activator.Update()
        # Mark the player's title for update.
        self._activator.Controller().s_ext_title_flag = True
