"""
Generic script for spell sellers.
"""

from Atrinik import *
from Merchant import SpellSeller


class InterfaceDialog(SpellSeller):
    """
    Dialog when talking to the spell seller.
    """

ib = InterfaceDialog(activator, me)
ib.finish(locals(), msg)
