## @file
## Generic script to handle merchants.

from Seller import Seller

class InterfaceDialog(Seller):
    """
    Dialog when talking to the merchant.
    """

ib = InterfaceDialog(activator, me)
ib.finish(locals(), msg)
