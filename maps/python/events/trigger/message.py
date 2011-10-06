## @file
## Simple trigger script to show a message to the activator.
##
## Used for objects that cannot have message to show defined, such as
## containers.

def main():
	activator.Write(WhatIsEvent().msg, COLOR_WHITE)

main()
