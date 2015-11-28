"""
Generic script for smiths in shops.
"""

from Atrinik import *
from Smith import Smith


class InterfaceDialog(Smith):
    """
    Dialog when talking to the smith.
    """

ib = InterfaceDialog(activator, me)
ib.finish(locals(), msg)
