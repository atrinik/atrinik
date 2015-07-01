## @file
## Script for the gate closer monster.

from Atrinik import *

def main():
    me.Apply(activator.map.LocateBeacon("uc_torch_switch").env, APPLY_NORMAL)

main()
