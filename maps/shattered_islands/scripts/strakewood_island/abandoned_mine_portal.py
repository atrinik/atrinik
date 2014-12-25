## @file
## Graphical effect for the portal to Moroch Temple in Abandoned Mine.
##
## Makes the portal shrink, then disappear.

# Shrink it.
me.zoom_x -= 5
me.zoom_y -= 5
# Adjust X and Y align.
me.align += 1
me.z += 1
me.map.Redraw(me.x, me.y, me.layer, me.sub_layer)

# Shrinked to a very small size? Then set the is_used_up flag so it'll
# get removed the next tick.
if me.zoom_x == 30:
    me.f_is_used_up = True
