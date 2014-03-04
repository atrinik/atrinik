## @file
## Implements the class chooser NPC.

from Interface import Interface
from PlayerClass import PlayerClass

inf = Interface(activator, me)
pc = PlayerClass(activator)

def main():
    enchant_removal_cost = 25000

    if msg == "hello":
        from Language import pluralize, l2s

        classes = pc.get_classes()
        classes.sort()
        classes_plural = [pluralize(x) for x in classes]

        inf.add_msg("Ah, welcome {}! My name is {}.".format(activator.name, me.name))
        inf.add_msg("I have traveled all over the world to learn about the ways of many professions adventurers choose, and, if you like, I can teach you more about {}.".format(l2s(classes_plural)))

        for idx, class_plural in enumerate(classes_plural):
            inf.add_link("Tell me about {}.".format(class_plural), dest = "tell " + classes[idx])

    elif msg == "tell warrior":
        inf.add_msg("Warriors are normally trained in the art of combat with weapons and archery.")
        inf.add_msg("Because of their training, they're stronger, more agile and hardier than they would be otherwise.")
        inf.add_msg("However, because of their dedication to their combat training, they have neglected studies in the magical arts and religious devotion, so their education, in general, lacks breadth.")
        inf.add_link("Teach me how to be a {}.".format(pc.get_class_name_gender("warrior")), dest = "teach warrior")

    elif msg == "tell archer":
        inf.add_msg("Archers are learned in the art of using long range weapons such as bows and crossbows.")
        inf.add_msg("Their training includes strengthening their body in order to be able to fire missiles from ranged weapons with enough strength to cause the most damage to their enemies.")
        inf.add_msg("Their education includes things such as reading the wind in order to be able to strike down enemies more effectively.")
        inf.add_link("Teach me how to be an {}.".format(pc.get_class_name_gender("archer")), dest = "teach archer")

    elif msg == "tell sorcerer":
        inf.add_msg("Sorcerers obsessively study the magical arts, and by frequently practicing their arts, they enhance their powers.")
        inf.add_msg("They are always looking for better ways to channel their energies, thus enormously sharpening their intellect.")
        inf.add_msg("On the other hand, they totally neglect any training with weaponry, so they're usually soft and weak.")
        inf.add_link("Teach me how to be a {}.".format(pc.get_class_name_gender("sorcerer")), dest = "teach sorcerer")

    elif msg == "tell warlock":
        inf.add_msg("Warlocks divide their time between learning magic and learning weapons, but they totally disregard religious devotion.")
        inf.add_msg("They're physically stronger and hardier because of their training, and they know how to use weapons and bows.")
        inf.add_msg("However, they're a bit clumsy in both weaponry and magic because they divided their time between the two.")
        inf.add_link("Teach me how to be a {}.".format(pc.get_class_name_gender("warlock")), dest = "teach warlock")

    elif msg == "tell priest":
        inf.add_msg("Priests are intensely devoted to their god, and they know how to channel the energies of their god.")
        inf.add_msg("They've been taught the use of weapons, but only cursorily, and their physical training has been lacking in general.")
        inf.add_link("Teach me how to be a {}.".format(pc.get_class_name_gender("priest")), dest = "teach priest")

    elif msg == "tell paladin":
        inf.add_msg("Paladins are militant priests, with an emphasis on 'priest'.")
        inf.add_msg("They've been taught archery and the use of weapons, but great care has been taken that they're doctrinally correct.")
        inf.add_msg("Normally, their mission is to go out into the world to convert the unrighteous and destroy the enemies of the faith.")
        inf.add_link("Teach me how to be a {}.".format(pc.get_class_name_gender("paladin")), dest = "teach paladin")

    elif msg.startswith("teach "):
        name = msg[6:]

        if not name in pc.get_classes():
            return

        from Language import pluralize

        inf.add_msg("[title]Profession: " + pc.get_class_name_gender(name) + "[/title]")
        inf.add_msg("If you wish, I will instruct you in the ways of {}.".format(pluralize(name)))
        class_ob = pc.get_class()

        if not class_ob:
            inf.add_msg("I will also cast an enchantment which will give you the bonuses of this profession - but you should be warned that your body is unlikely to handle two such enchantments, so if you ever want me to teach you in the ways of a different profession, you'll have to get that enchantment removed, and that could be... costly. So, are you sure about this?")
        elif class_ob.name == name:
            inf.add_msg("Ah, but you already know the ways of {}!".format(pluralize(name)))
            return
        else:
            inf.add_msg("Hm, you're already learned in the ways of {}. This means that I will need to remove the previous enchantment, and this will cost you {}. Is that okay?".format(pluralize(class_ob.name), CostString(enchant_removal_cost)))

        inf.add_msg("Bonuses:", COLOR_GREEN)
        inf.add_msg("\n[padding=10]" + "\n".join(pc.get_class_bonuses(name)) + "[/padding]", COLOR_GREEN, newline = False)

        inf.add_link("Yes, teach me the ways of {}.".format(pluralize(name)), dest = "teach2 " + name)

    elif msg.startswith("teach2 "):
        name = msg[7:]

        if not name in pc.get_classes():
            return

        from Language import pluralize

        inf.add_msg("[title]Profession: " + pc.get_class_name_gender(name) + "[/title]")
        class_ob = pc.get_class()

        if class_ob:
            if class_ob.name == name:
                inf.add_msg("Ah, but you already know the ways of {}!".format(pluralize(name)))
                return
            else:
                if activator.PayAmount(enchant_removal_cost):
                    inf.add_msg("You pay {}.".format(CostString(enchant_removal_cost)), COLOR_YELLOW)
                else:
                    inf.add_msg("Hm, you don't have enough money to remove the previous enchantment...")
                    return

        inf.add_msg("Alright. I shall teach you all I know about {}...".format(pluralize(name)))
        inf.add_msg("{} teaches you in length about {}.".format(me.name, pluralize(name)), COLOR_YELLOW)
        inf.add_msg("And now for the enchantment...")
        inf.add_msg("{} performs a complicated incantation...".format(me.name), COLOR_GREEN)
        inf.add_msg("There, all done!")
        pc.set_class(name)

main()
inf.finish()
