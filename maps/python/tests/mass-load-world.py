from Atrinik import *
import os

def main():
	path = "/shattered_islands"
	pl = GetFirst("player")

	for entry in os.listdir(CreatePathname(path)):
		pl.ExecuteCommand("/tpto {}/{}".format(path, entry))
