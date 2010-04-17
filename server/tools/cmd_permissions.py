#!/usr/bin/python
#
# Script for getting information about command permissions
# of specific (or all) players.

import getopt, sys, os

class colors:
	bold = "\033[1m"
	underscore = "\033[4m"
	red = "\033[91m"
	end = "\033[0m"

def usage():
	print("\n" + colors.bold + colors.underscore + "Use:" + colors.end + colors.end)
	print("\nScript to print information about players' command permissions.\n")
	print(colors.bold + colors.underscore + "Options:" + colors.end + colors.end)
	print("\n\t-h, --help:\n\t\tDisplay this help.")
	print("\n\t-d " + colors.underscore + "directory" + colors.end + ", --directory=" + colors.underscore + "directory" + colors.end + ":\n\t\tSpecify data directory where player files are. Default is '../data/players'.")
	print("\n\t-p " + colors.underscore + "player1,player2" + colors.end + ", --players=" + colors.underscore + "player1,player2" + colors.end + ":\n\t\tComma separated list of players to list permissions for.")

def get_cmd_permissions(file):
	perms = []

	try:
		fp = open(file)

		for line in fp:
			if line[:15] == "cmd_permission ":
				perms.append("/" + line[15:-1])

		fp.close()
	except IOError, err:
		print(err)

	return perms

def parse_file(path, player):
	file = path + "/" + player + "/" + player + ".pl"
	perms = get_cmd_permissions(file)

	if perms:
		print(colors.red + player + colors.end + ":\t\t" + ", ".join(perms))

try:
	opts, args = getopt.getopt(sys.argv[1:], "hd:p:", ["help", "directory=", "players="])
except getopt.GetoptError, err:
	print(err)
	usage()
	sys.exit(2)

path = "../data/players"
players = None

for o, a in opts:
	if o in ("-h", "--help"):
		usage()
		sys.exit()
	elif o in ("-d", "--directory"):
		path = a
	elif o in ("-p", "--players"):
		players = a

if players:
	list_players = players.split(",")

	for player in list_players:
		parse_file(path, player.strip())
else:
	files = os.listdir(path)

	for player in files:
		parse_file(path, player)