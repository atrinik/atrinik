"""Temple.py: Implements temple-related functions."""

from Atrinik import *
from collections import OrderedDict
from Interface import InterfaceBuilder

temple_services = OrderedDict((
    ("remove curse", [
        3000,
        "Removal of curse from all cursed items",
        "I will remove any curse from all cursed items in your inventory. Note that the curse may still come back, if the item is permanently cursed...",
    ]),
    ("remove damnation", [
        5000,
        "Removal of damnation from all damned items",
        "I will remove any damnation from all damned items in your inventory. Note that the damnation may still come back, if the item is permanently damned...",
    ]),
    ("cure disease", [
        1000,
        "Curing of disease",
        "I can attempt to cure any disease that is troubling you.",
    ]),
    ("cure poison", [
        500,
        "Curing of poison",
        "I can heal your poisoning, if you wish.",
    ]),
    ("food", [
        0,
        "Spare bit of food",
    ]),
))

class Temple(InterfaceBuilder):
    """Temple interface builder."""

    enemy_temple_name = None
    enemy_temple_desc = None

    def subdialog_services(self):
        """Show the available temple services."""

        for service in temple_services:
            self.add_link(temple_services[service][1], dest = service)

        self.add_link("Tell me about {}.".format(self.temple_name), dest = self.temple_name)

        if self.enemy_temple_name:
            self.add_link("Tell me about {}.".format(self.enemy_temple_name), dest = self.enemy_temple_name)

    def dialog_hello(self):
        """Default hello dialog handler."""

        self.add_msg("Welcome to the church of {god}. I am {npc.name}, a devoted servant of {god}.", god = self.temple_name)

        if self.enemy_temple_name:
            self.add_msg("Beware that followers of {enemy_god} are not welcome here.", enemy_god = self.enemy_temple_name)

        self.add_msg("I can offer you the following services.")
        self.subdialog_services()

    def dialog(self, msg):
        """Handle services and speaking about particular gods."""

        if msg == self.temple_name:
            self.add_msg(self.temple_desc)
        elif msg == self.enemy_temple_name:
            self.add_msg(self.enemy_temple_desc)
        else:
            if msg.startswith("buy "):
                msg = msg[4:]
                is_buy = True
            else:
                is_buy = False

            service = temple_services.get(msg)

            if not service:
                return

            if msg == "food":
                if self._activator.food < 500:
                    self._activator.food = 500
                    self.add_msg("Your stomach is filled again.")
                else:
                    self.add_msg("You don't look very hungry to me...")
            else:
                self.add_msg("[title]{service[1]}[/title]", service = service)

                if not is_buy:
                    self.add_msg(service[2])
                    self.add_msg("This will cost you {cost}.", cost = CostString(service[0]))
                    self.add_link("Confirm", dest = "buy " + msg)
                else:
                    if self._activator.PayAmount(service[0]):
                        if service[0]:
                            self.add_msg("You pay {cost}.", cost = CostString(service[0]), color = COLOR_YELLOW)

                        self.add_msg("Okay, I will cast [green]{origmsg}[/green] on you now.", origmsg = msg)
                        self._npc.Cast(GetArchetype("spell_" + msg.replace(" ", "_")).clone.sp, self._activator)
                    else:
                        self.add_msg("You do not have enough money...")

class TempleGrunhilde(Temple):
    """Grunhilde temple."""
    temple_name = "Grunhilde"
    temple_desc = "I am a servant of the Valkyrie Queen and the Goddess of Victory, Grunhilde."

class TempleDalosha(Temple):
    """Dalosha temple."""
    temple_name = "Dalosha"
    temple_desc = "I am a servant of the first Queen of the Drow and Spider Goddess, Dalosha."
    enemy_temple_name = "Tylowyn"
    enemy_temple_desc = "The high elves and their oppressive queen! Do not be swayed by her traps, she started the war with her attempt to enforce proper elven conduct in war. Tylowyn was too cowardly and weak to realize that it was our destiny to rule the world, so now she and her elves shall also perish!"

class TempleDrolaxi(Temple):
    """Drolaxi temple."""
    temple_name = "Drolaxi"
    temple_desc = "I am a servant of Queen of the Chaotic Seas and the Goddess of Water, Drolaxi."
    enemy_temple_name = "Shaligar"
    enemy_temple_desc = "Flames and terror does he seek to spread. Do not be deceived, although the flame be kin to the Lady, he is complerely mad. Avoid the scorching flames or they will consume you. We shall rule the world and all shall be seas!"

class TempleElathiel(Temple):
    """Elathiel temple."""
    temple_name = "Elathiel"
    temple_desc = "I am a servant of the God of Light and King of the Angels, Elathiel."
    enemy_temple_name = "Rashindel"
    enemy_temple_desc = "Caution child, for you speak of the Fallen One. In the days before the worlds were created by our Lord Elathiel, the archangel Rashindel stood at his right hand. In that day, however, Rashindel sought to claim the throne of Heaven and unseat the Mighty Elathiel. The Demon King was quickly defeated and banished to Hell with the angels he managed to deceive and they were transformed into the awful demons and devils which threaten the lands to this day."

class TempleGrumthar(Temple):
    """Grumthar temple."""
    temple_name = "Grumthar"
    temple_desc = "I am a servant of the First Dwarven Lord and the God of Smithery, Grumthar."
    enemy_temple_name = "Jotarl"
    enemy_temple_desc = "Do not be speaking of that Giant tyrant amongst us. Him and his giants have long sought to crush the little folk. He has those goblin vermin under his wing also."

class TempleJotarl(Temple):
    """Jotarl temple."""
    temple_name = "Jotarl"
    temple_desc = "I am a servant of the Titan King and the God of the Giants, Jotarl."
    enemy_temple_name = "Grumthar"
    enemy_temple_desc = "Puny dwarves do not scare Jotarl with their technology and mithril weapons, we shall rule the caves! The Dwarves shall fall and we shall claim their gold for ourselves."

class TempleRashindel(Temple):
    """Rashindel temple."""
    temple_name = "Rashindel"
    temple_desc = "I am a servant of the Demonic King and the Overlord of Hell, Rashindel."
    enemy_temple_name = "Elathiel"
    enemy_temple_desc = "Accursed fool, do not mention that name in our presence! In the days before this world, the Tyrant sought to oppress us with the his oppressive ideals of truth and justice. After our master freed us from the simpleton lots who follow him, he was bound into the darkness which is now our glorious kingdom."

class TempleRogroth(Temple):
    """Rogroth temple."""
    temple_name = "Rogroth"
    temple_desc = "I am a servant of the King of the Stormy Skies and the God of Lightning, Rogroth."

class TempleShaligar(Temple):
    """Shaligar temple."""
    temple_name = "Shaligar"
    temple_desc = "I am a servant of King of the Lava and the God of Flame, Shaligar."
    enemy_temple_name = "Drolaxi"
    enemy_temple_desc = "Ah, the weak and cowardly sister of the Flame Lord. One day, she shall no longer be able to keep our flames from consuming all things and our flames shall make all subjects to our will."

class TempleTerria(Temple):
    """Terria temple."""
    temple_name = "Terria"
    temple_desc = "I am a servant of Mother Earth and the Goddess of Life, Terria."
    enemy_temple_name = "Zechna"
    enemy_temple_desc = "Speak not of the Dark Lord here! The King of Death with his awful necromantic minions that rise from the sleep of death are not to be trifled with, for they are dangerous. Our Lady has long sought to remove the plague of death from the lands after that foul Lich ascended."

class TempleTylowyn(Temple):
    """Tylowyn temple."""
    temple_name = "Tylowyn"
    temple_desc = "I am a servant of the first Queen of Elven Kind and Elven Goddess of Luck, Tylowyn."
    enemy_temple_name = "Dalosha"
    enemy_temple_desc = "That rebellious heretic! In the days of the First Elven Kings, the first daughter of our gracious Tylowyn sought to overthrow the Elven Kingdoms with her lies and treachery. After she was routed from the Elven lands, she took her band of rebel dark elves and hid in the caves, but unfortunately managed to survive there. Avoid those drow if you know what is best for you."

class TempleZechna(Temple):
    """Zechna temple."""
    temple_name = "Zechna"
    temple_desc = "I am a servant of the Lord of the Grave and King of Undeath, Zechna."
    enemy_temple_name = "Terria"
    enemy_temple_desc = "Do you honestly believe the lies of those naturists? The powers of undeath will rule the universe and the servants of Nature will fail. The Dark Lord shall not fail to dominate the land and all be consumed in glorious Death."
