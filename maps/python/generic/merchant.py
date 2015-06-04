## @file
## Generic script to handle merchants.

from Atrinik import *
from Seller import Seller

class InterfaceDialog(Seller):
    """
    Dialog when talking to the merchant.
    """

ib = InterfaceDialog(activator, me)
ib.finish(locals(), msg)
