<?php

/**
 * @file
 * Command handling. */

/**
 * All the possible commands that can be used by the script when running
 * from the CLI. */
$commands = array(
	'maps-cache' => array(
		'args' => array(
			'autocrop' => array(
				'type' => 'bool',
				'default' => 0,
				'doc' => 'Do automatic cropping of empty borders on every image. Slow.',
			),
			'start-dir' => array(
				'type' => 'string',
				'default' => '',
				'doc' => 'Starting point in maps directory. Useful for testing.',
			),
		),
		'doc' => 'Regenerate the cached map images that are used for the world viewer.',
	),
	'arch-cache' => array(
		'doc' => 'Regenerate the information parsed from arch directory files like animations, archetypes and stored in cached PHP file as arrays.',
	),
	'render-map' => array(
		'args' => array(
			'autocrop' => array(
				'type' => 'bool',
				'default' => 0,
				'doc' => 'Do automatic cropping of empty borders on the output image.',
			),
			'file' => array(
				'type' => 'string',
				'required' => true,
				'doc' => 'Path to the map file.',
			),
			'output' => array(
				'type' => 'string',
				'default' => 'output.png',
				'doc' => 'Name of the output file.',
			),
		),
		'doc' => 'Render a single map.',
	),
	'overview' => array(
		'args' => array(
			'resize' => array(
				'required' => true,
				'type' => 'range',
				'min' => 2,
				'max' => 6,
				'doc' => 'How much to resize the overview from the real size of the map, ie, twice, thrice, etc.',
			),
			'filename' => array(
				'type' => 'string',
				'default' => 'overview.png',
				'doc' => 'The overview filename.',
			),
			'no-autocrop' => array(
				'type' => 'bool',
				'default' => 0,
				'doc' => 'If set, don\'t do automatic cropping of empty borders. The cropping can be slow.',
			),
			'first-map' => array(
				'required' => true,
				'type' => 'string',
				'doc' => 'Name of the first map for the overview, from which to look for other tiled maps.',
			),
		),
		'doc' => 'Generate overview image from a bunch of tiled maps. The maps must all be properly named using the Atrinik map naming scheme.',
	),
	'help' => array(
		'doc' => 'Show usage and known commands.',
	),
);

/**
 * The map-cache command. Used to (re)generate the cached images of maps
 * used for the world viewer. */
function command_maps_cache()
{
	global $maps_path, $arguments, $maps_info;

	// If the map cache dir doesn't exist, create it.
	if (!file_exists(MAP_CACHE_DIR))
	{
		mkdir(MAP_CACHE_DIR);
	}
	// If it exists but is not a directory, complain.
	elseif (!is_dir(MAP_CACHE_DIR))
	{
		die('ERROR: \'' . MAP_CACHE_DIR . '\' exists, but is not a directory.' . "\n");
	}

	$path = $maps_path;

	if (!empty($arguments['start-dir']))
	{
		$path .= '/' . $arguments['start-dir'];

		if (!file_exists($path) || !is_dir($path))
		{
			die('ERROR: Invalid start-dir argument: \'' . $path . '\' doesn\'t exist or is not a directory.' . "\n");
		}
	}

	// Maps to scan.
	$to_scan = array($path);
	$maps = array();
	$maps_info = array();

	// Get the length of the maps path.
	$maps_path_len = strlen($maps_path);

	// Scan all the maps.
	for ($i = 0; $i < count($to_scan); $i++)
	{
		$file = $to_scan[$i];

		// If this is a directory...
		if (is_dir($file))
		{
			// Open the directory.
			$dir = opendir($file);

			// Read the directory.
			while (($filename = readdir($dir)) !== false)
			{
				// Ignore anything starting with '.', and also ignore
				// some obvious non-map files and directories.
				if ($filename[0] == '.' || $filename == 'styles' || $filename == 'python' || $filename == 'Doxyfile')
				{
					continue;
				}

				// Add it to the scan array so we can parse it.
				$to_scan[] = $file . '/' . $filename;
			}

			// Close the directory.
			closedir($dir);
		}
		// Otherwise a file.
		else
		{
			$fp = fopen($file, 'r');

			// Try to parse the file as map
			if (($map_info = parse_map_header($fp)) !== false)
			{
				// The substr will remove maps_path, so instead of /home/xxx/atrinik/shattered_islands/world_xxxx
				// we will have shattered_islands/world_xxxx.
				$maps_info[substr($file, strlen($maps_path) + 1)] = $map_info;
				$maps[] = $file;
			}

			fclose($fp);
		}
	}

	// Loop through all the maps.
	foreach ($maps as $map)
	{
		// Make image of the map.
		$map_img = make_map_image(parse_map_file($map));

		// Get the name of the file that will be used to store the cached
		// image.
		$cache_map_file = MAP_CACHE_DIR . substr($map, $maps_path_len) . '.png';
		// Get the path to the file.
		$cache_map_path = dirname($cache_map_file);

		// If the directory doesn't exist, recursively create it.
		if (!file_exists($cache_map_path))
		{
			mkdir($cache_map_path, 0750, true);
		}
		// If it exists but is not a directory, complain.
		elseif (!is_dir($cache_map_path))
		{
			die('ERROR: \'' . $cache_map_path . '\' exists but is not a directory.' . "\n");
		}

		// Save the image.
		imagepng($map_img, $cache_map_file);
		// Free resources.
		imagedestroy($map_img);

		// If autocrop argument was set, remove empty borders from the
		// image.
		if ($arguments['autocrop'])
		{
			image_remove_empty_borders($cache_map_file);
		}

		echo 'Rendered and saved map: ', $cache_map_file, "\n";

		// Sleep for a bit.
		usleep(5000);
	}

	make_cache(array($maps_info), MAPS_CACHE_FILE);
}

/**
 * The arch-cache command. Regenerates the arch cache used by the
 * script. */
function command_arch_cache()
{
	global $arch_path, $archetypes, $faces, $animations;

	// Parse all the needed files.
	parse_archetypes($arch_path . '/archetypes');
	parse_facetree($arch_path . '/facetree');
	parse_animations($arch_path . '/animations');
	parse_artifacts($arch_path . '/artifacts');

	// Generate the cache.
	make_cache(array($archetypes, $faces, $animations), ARCH_CACHE_FILE);
}

/**
 * Render a single map. */
function command_render_map()
{
	global $arguments;

	if (!file_exists($arguments['file']) || !is_file($arguments['file']))
	{
		die('ERROR: The file \'' . $arguments['file'] . '\' does not exist or is not a file.' . "\n");
	}

	// Make image of the map.
	$map_img = make_map_image(parse_map_file($arguments['file']));

	// Save the image.
	imagepng($map_img, $arguments['output']);
	// Free resources.
	imagedestroy($map_img);

	// If autocrop argument was set, remove empty borders from the
	// image.
	if ($arguments['autocrop'])
	{
		image_remove_empty_borders($arguments['output']);
	}

	echo 'Rendered and saved map: ', $arguments['output'], "\n";
}

/**
 * The overview command. Generates image overview of a given mapset. */
function command_overview()
{
	global $maps, $to_scan, $maps_path, $arguments;

	$maps = array();
	$base_filename = basename($maps_path . '/' . $arguments['first-map']);
	$to_scan = array($base_filename);
	$dir = dirname($maps_path . '/' . $arguments['first-map']);

	for ($i = 0; $i < count($to_scan); $i++)
	{
		$map = $dir . '/' . $to_scan[$i];

		// Open the map.
		$fp = fopen($map, 'r');

		$got_map = false;
		$tile_paths = array();

		// Loop through the lines.
		while ($line = fgets($fp))
		{
			// Check if this is map arch, which should be the first line.
			if ($line == 'arch map' . "\n")
			{
				$got_map = true;
			}
			elseif ($line == 'end' . "\n")
			{
				break;
			}

			// We haven't got "arch map" as the first line, return an error.
			if (!$got_map)
			{
				die('ERROR: Generating overview failed, file is not a map: ' . $map . "\n");
			}

			// Otherwise scan for tile paths.
			if (sscanf($line, 'tile_path_%d %s' . "\n", $tile_path_id, $tile_path_map))
			{
				$tile_paths[$tile_path_id] = $tile_path_map;

				// If it's not in the $to_scan array yet, add it there.
				if (!in_array($tile_path_map, $to_scan))
				{
					$to_scan[] = $tile_path_map;
				}
			}
		}

		// Add it to the maps array.
		$maps[] = basename($map);

		// Close the file.
		fclose($fp);
	}

	$lowest_xx = $lowest_yy = $highest_xx = $highest_yy = 1;

	$base_map_filename = substr($base_filename, 0, -5);

	// Go through the maps to detect lowest XX, YY and highest XX, YY.
	foreach ($maps as $map)
	{
		$positions_xx = substr($map, -4, 2);
		$positions_yy = substr($map, -2, 2);

		if ((int) $positions_xx > $highest_xx)
		{
			$highest_xx = (int) $positions_xx;
		}
		elseif ((int) $positions_xx < $lowest_xx)
		{
			$lowest_xx = (int) $positions_xx;
		}

		if ((int) $positions_yy > $highest_yy)
		{
			$highest_yy = (int) $positions_yy;
		}
		elseif ((int) $positions_yy < $lowest_yy)
		{
			$lowest_yy = (int) $positions_yy;
		}
	}

	// Determine sizes.
	$size_x = (MAP_TILE_POS_XOFF * (24 + 1) * ($highest_xx > $highest_yy ? $highest_xx : $highest_yy)) / $arguments['resize'];
	$size_xpos = $size_x / 2;
	$size_y = (MAP_TILE_POS_YOFF * (24 + 3) * ($highest_yy > $highest_xx ? $highest_yy : $highest_xx)) / $arguments['resize'];
	$size_ypos = MAP_TILE_POS_YOFF * 2;

	// Create a new image.
	$world_img = imagecreatetruecolor($size_x, $size_y);

	// Turn on alpha blending and save alpha.
	imagealphablending($world_img, true);
	imagesavealpha($world_img, true);

	// Fill the image with transparency.
	imagefill($world_img, 0, 0, IMG_COLOR_TRANSPARENT);

	// Go through all the maps, starting from the top left one.
	for ($y = $highest_yy; $y >= $lowest_yy; $y--)
	{
		for ($x = $lowest_xx; $x <= $highest_xx; $x++)
		{
			// Get the map name.
			$map = $dir . '/' . $base_map_filename . '_' . ($x < 10 ? '0' . $x : $x) . ($y < 10 ? '0' . $y : $y);

			// If it doesn't exist, continue.
			if (!file_exists($map) || !is_file($map))
			{
				continue;
			}

			// Make an image of the map.
			$map_img = make_map_image(parse_map_file($map));

			// Get width and height.
			$width = imagesx($map_img);
			$height = imagesy($map_img);

			// Calculate new width and height.
			$newwidth = $width / $arguments['resize'] + 2;
			$newheight = $height / $arguments['resize'] + 2;

			// Calculate X and Y positions.
			$xpos = $size_xpos + ((($x - 1) * MAP_TILE_XOFF * MAP_TILE_XOFF * 4 - ($highest_yy - $y + 1) * MAP_TILE_YOFF * MAP_TILE_YOFF)) / $arguments['resize'];
			$ypos = ($size_ypos + (($highest_yy - $y) * MAP_TILE_YOFF * (MAP_TILE_YOFF / 2)) + (($x - 1) * MAP_TILE_XOFF * MAP_TILE_XOFF * 2)) / $arguments['resize'];

			// Copy the image.
			imagecopyresampled($world_img, $map_img, $xpos, $ypos, 0, 0, $newwidth, $newheight, $width, $height);
			// Free resources.
			imagedestroy($map_img);

			echo 'Rendered and copied image of map: ', $base_map_filename . '_' . ($x < 10 ? '0' . $x : $x) . ($y < 10 ? '0' . $y : $y), "\n";

			// Sleep for a bit.
			usleep(5000);
		}
	}

	// Save the image.
	imagepng($world_img, $arguments['filename']);
	// Free resources.
	imagedestroy($world_img);

	// If no-autocrop argument was not set, remove empty borders from the
	// overview image.
	if (!$arguments['no-autocrop'])
	{
		image_remove_empty_borders($arguments['filename']);
	}
}

/**
 * The help command. Used to show the usage of the script. */
function command_help()
{
	show_usage();
}

?>
