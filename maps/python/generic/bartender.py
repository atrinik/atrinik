## @file
## Generic script to handle bartenders, using the Tavern API.

from Interface import InterfaceBuilder
from Tavern import Bartender

class InterfaceDialog(Bartender, InterfaceBuilder):
    """
    Dialog when talking to the bartender.
    """

ib = InterfaceDialog(activator, me)
ib.finish(msg)
