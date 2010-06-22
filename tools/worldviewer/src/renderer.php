<?php

/**
 * @file
 * Handles rendering code. */

/**
 * Create a map image out of an object array.
 * @param map_array The array containing objects on the map.
 * @return The created image (which must be freed by imagedestroy() by
 * the caller, NULL on some kind of failure. */
function make_map_image($map_array)
{
	global $faces, $arch_path;

	// Sanity check.
	if (empty($map_array))
	{
		return null;
	}

	// Determine sizes.
	$size_x = MAP_TILE_POS_XOFF * (24 + 2);
	$size_xpos = MAP_TILE_POS_XOFF2 * (24 + 1);
	$size_y = MAP_TILE_POS_YOFF * (24 + 5);
	$size_ypos = MAP_TILE_POS_YOFF * 4;

	// Create a new image.
	$img = imagecreatetruecolor($size_x, $size_y);

	// Turn on alpha blending and save alpha.
	imagealphablending($img, true);
	imagesavealpha($img, true);

	// Fill the image with transparency.
	imagefill($img, 0, 0, IMG_COLOR_TRANSPARENT);

	// For all Y coordinates...
	for ($y = 0; $y < 24; $y++)
	{
		// For all X coordinates...
		for ($x = 0; $x < 24; $x++)
		{
			// Sanity check for empty map squares.
			if (empty($map_array[$y][$x]))
			{
				continue;
			}

			// Loop through all the objects on this square.
			foreach ($map_array[$y][$x] as $object)
			{
				// If this is a spawn point object with inventory,
				// exchange the spawn point with its inventory, so
				// instead of the spawn point showing up on the map, the
				// actual monster is shown.
				if ($object['type'] == SPAWN_POINT && !empty($object['inv']))
				{
					$object = $object['inv'][0];
				}

				// Create a temporary image from the face of the object.
				$img_face = imagecreatefrompng($faces[$object['face']] . '/' . $object['face'] . '.png');

				// Get image X and Y.
				$img_x = imagesx($img_face);
				$img_y = imagesy($img_face);

				// Handle multi arch object a bit differently.
				if ($object['parts'] > 1)
				{
					// Now it won't be treated as multi arch in the above
					// if anymore.
					$object['parts'] = 1;

					// For multi arch objects with one or more X, adjust
					// the resulting xpos.
					if ($object['multi_x'] >= 1 && $object['multi_y'] >= 0)
					{
						if ($img_y > MAP_TILE_POS_YOFF)
						{
							// Should work for about any multi arch
							// object.
							$object['xpos'] = MAP_TILE_POS_XOFF2 * $object['multi_x'];
						}
					}

					// Move it further down in the array to be processed
					// later, so it can be rendered properly.
					$map_array[$y + $object['multi_y']][$x + $object['multi_x']][] = $object;
				}
				else
				{
					// Calculate positions.
					$xpos = $size_xpos + $x * MAP_TILE_YOFF - $y * MAP_TILE_YOFF;
					$ypos = (($size_ypos + $x * MAP_TILE_XOFF + $y * MAP_TILE_XOFF) + MAP_TILE_POS_YOFF) - $img_y;

					// If this is not a multi arch, adjust $xpos
					// depending on the size of the image.
					if (!$object['is_multi'] && $img_x > MAP_TILE_POS_XOFF)
					{
						$xpos -= ($img_x - MAP_TILE_POS_XOFF) / 2;
					}

					// If the object had xpos set, subtract it from the
					// calculated $xpos.
					if (!empty($object['xpos']))
					{
						$xpos -= $object['xpos'];
					}

					// Copy the temporary image on the big image.
					imagecopy($img, $img_face, $xpos, $ypos, 0, 0, $img_x, $img_y);

					// If this is a wall and the setting is set, we'll
					// do double drawing of it.
					if ($object['type'] == WALL && DRAW_DOUBLE_WALLS)
					{
						imagecopy($img, $img_face, $xpos, $ypos - 22, 0, 0, $img_x, $img_y);
					}
				}

				// Free the temporary image.
				imagedestroy($img_face);
			}
		}
	}

	// Return the image for further processing.
	return $img;
}

/**
 * Remove empty borders from a specified image file. Similar to GIMP's
 * autocrop functionality.
 * @note This function is fairly slow, since it goes through every single
 * pixel of the image to detect if it's empty or not.
 * @warning Only works on PNGs.
 * @param filename Name of the image to remove empty borders from. */
function image_remove_empty_borders($filename)
{
	$imageinfo = getimagesize($filename);
	$height = $newStartX = $imageinfo[1];
	$width = $newStartY = $imageinfo[0];
	$newStopX = $newStopY = 0;
	$palette = $palette_num = 0;
	$img = imagecreatefrompng($filename);

	// Go through the image to get color palette.
	for ($i = 0; $i < $width; $i++)
	{
		for ($ii = 0; $ii < $height; $ii++)
		{
			$color = imagecolorat($img, $i, $ii);
			$palette += $color;
			$palette_num++;
		}
	}

	// Get the peak color.
	$peak_color = round($palette / $palette_num) * 0.95;
	unset($palette);

	// Go through the image again, this time to detect the empty pixels.
	for ($i = 0; $i < $width; $i++)
	{
		for ($ii = 0; $ii < $height; $ii++)
		{
			if (imagecolorat($img, $i, $ii) < $peak_color)
			{
				if ($i > $newStopX)
				{
					$newStopX = $i;
				}

				if ($ii > $newStopY)
				{
					$newStopY = $ii;
				}

				if ($i < $newStartX)
				{
					$newStartX = $i;
				}

				if ($ii < $newStartY)
				{
					$newStartY = $ii;
				}
			}
		}
	}

	// Create a new image of size based on where to crop the original.
	$cropped = imagecreatetruecolor($newStopX - $newStartX, $newStopY - $newStartY);

	// Turn on alpha blending and save alpha.
	imagealphablending($cropped, true);
	imagesavealpha($cropped, true);

	// Fill the image with transparency.
	imagefill($cropped, 0, 0, IMG_COLOR_TRANSPARENT);

	// Now copy the original, adjusting the positions where empty
	// borders are so they don't get copied.
	imagecopyresampled($cropped, $img, 0, 0, $newStartX, $newStartY, $newStopX - $newStartX, $newStopY - $newStartY, $newStopX - $newStartX, $newStopY - $newStartY);

	// Save the image.
	imagepng($cropped, $filename);
}

?>
