## @file
## The English to Pirate translator module.

import re, random

## The translation table. Keys must be strings and are matched by regex
## on word boundary basis in a case-insensitive manner (capitalization
## is done automatically). Values can be lists in which case an entry is
## randomly chosen from the list.
_translations = {
    "hello": "ahoy",
    "hey": "ahoy",
    "heya": "ahoy",
    "hi": "yo-ho-ho",
    "pardon me": "avast",
    "excuse me": "arrr",
    "yes": ["aye", "arrr"],
    "get off": "avast",
    "sure": "aye aye",
    "no way": "avast",
    "my": "me",
    "friend": ["me bucko", "me hearty", "matey"],
    "friends": ["me buckos", "me hearties", "mateys"],
    "sir": "matey",
    "madam": "proud beauty",
    "miss": "comely wench",
    "stranger": "scurvy dog",
    "strangers": "scurvy dogs",
    "officer": "foul blaggart",
    "officers": "foul blaggarts",
    "where": "whar",
    "is": "be",
    "are": "be",
    "am": "be",
    "im": "i be",
    "i'm": "i be",
    "i'll": "i be",
    "the": "th'",
    "you": "ye",
    "you'll": "ye'll",
    "your": "yer",
    "yours": "yers",
    "tell": "be tellin'",
    "know": "be knowin'",
    "how far": "how many leagues",
    "old": "barnacle-covered",
    "attractive": "comely",
    "happy": "grog-filled",
    "quickly": "smartly",
    "nearby": "broadside",
    "restroom": "head",
    "restaurant": "galley",
    "restaurant": "galleys",
    "hotel": "fleabag inn",
    "hotels": "fleabag inns",
    "pub": "Skull & Scuppers",
    "pubs": "Skull & Scuppers",
    "mall": "market",
    "bank": "buried treasure",
    "die": "visit Davey Jones' Locker",
    "died": "visited Davey Jones' Locker",
    "dying": "visitin' Davey Jones' Locker",
    "kill": "keel-haul",
    "killing": "keel-haulin'",
    "killed": "keel-hauled",
    "sleep": "take a caulk",
    "sleeping": "taking a caulk",
    "slept": "took a caulk",
    "stupid": "addled",
    "after": "aft",
    "stop": "belay",
    "nonsense": "bilge",
    "ocean": "briny deep",
    "song": "shanty",
    "money": "doubloons",
    "food": "grub",
    "nose": "prow",
    "left": "weighed anchor",
    "leave": "weigh anchor",
    "leaving": "weighin' anchor",
    "cheat": "hornswaggle",
    "forward": "fore",
    "child": "sprog",
    "children": "sprogs",
    "sailor": "swab",
    "sailors": "swabs",
    "lean": "careen",
    "find": "come across",
    "found": "came across",
    "finding": "coming across",
    "mother": "dear ol' mum, bless her black soul",
    "drink": "barrel o' rum",
    "of": "o'",
    "comes": "hails",
    "lawyer": "scurvy land lubber",
    "lawyers": "scurvy land lubbers",
    "beer": "grog",
    "girl": "lass",
    "girls": "lasses",
    "to": "t'",
    "together": "t'gether",
    "went": "be goin'",
    "new": "shiny",
    "with": "wi'",
    "there": "thar",
    "piracy": "sweet trade",
    "me": "my",
    "for": "fer",
    "perhaps": "p'raps",
}

## Translate a single match (word).
## @param match What matched.
## @return Translated string.
def _translate(match):
    # What are we translating?
    what = match.group(0)
    # Get the translation.
    to = _translations[what.lower()]

    # Value is a list, choose randomly.
    if type(to) == list:
        to = random.choice(to)

    # If it was capitalized, the end result string should be as well.
    if what.istitle():
        return to.capitalize()

    return to

## Replace ending 'g' or 'd' in words with a single quote, for example,
## "going" => "goin'".
def _translate2(match):
    return match.group(0)[:-1] + "'"

## Replace 'v's in words with a single quote.
def _translate3(match):
    return match.group(0).replace("v", "'")

## Translate English to Pirate.
## @param text What to translate.
## @return Translated string.
def english2pirate(text):
    rc = re.compile(r"\b(" + "|".join(map(re.escape, _translations)) + r")\b", re.I)
    msg = rc.sub(_translate, text)
    rc = re.compile(r"\b\w+[gd]\b", re.I)
    msg = rc.sub(_translate2, msg)
    rc = re.compile(r"\b\w+\b", re.I)
    msg = rc.sub(_translate3, msg)

    return msg
