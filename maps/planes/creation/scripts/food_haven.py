## @file
## Experimental script testing the power of CreateMap().

import time

# Create 17x17 map with unique name
m = CreateMap(17, 17, activator.name + "-food-" + str(time.time()))
# Change its name.
m.name = "Food Haven"
# Full light.
m.darkness = 7
# Temporary object we need for object.CreateTreasure()
cont = CreateObject("sack")

# Fill the map.
for x in range(m.width):
	for y in range(m.height):
		# Add walls around the edges of the map.
		if x == 0 and y == 0:
			m.CreateObject("wall_stone15_8", x, y)
		elif x == m.width - 1 and y == m.height - 1:
			m.CreateObject("wall_stone15_4", x, y)
		elif x in (0, m.width - 1) and y != 0:
			m.CreateObject("wall_stone15_3", x, y)
		elif y in (0, m.height - 1):
			m.CreateObject("wall_stone15_1", x, y)
		else:
			# A way out...
			if x == 1 and y == 1:
				portal = m.CreateObject("exit_green", x, y)
				portal.slaying = activator.map.path
				portal.hp = activator.x
				portal.sp = activator.y
				portal.msg = "The portal bounces you around, and soon you find yourself back where you were."
			elif x == 2 and y == 1:
				sign = m.CreateObject("sign", x, y)
				sign.name = "Escape Portal"
				sign.msg = "Back to where you came from"
			# Treasure
			else:
				food = None

				# Keep generating food until we get food that gives at
				# least 100 food points.
				while food == None:
					cont.CreateTreasure("random_food", 1, GT_NO_VALUE | GT_ONLY_GOOD)
					food = cont.inv

					if food.food < 100:
						food.Remove()
						food = None

				# Identify it and insert on the map.
				food.f_identified = True
				m.Insert(food, x, y)

		# Use 'blocked' for north and west edges.
		if x == 0 or y == 0:
			m.CreateObject("blocked", x, y)
		else:
			m.CreateObject("floor_rhomb", x, y)

# Teleport the player to the new map.
activator.TeleportTo(m.path, m.width // 2, m.height // 2)
