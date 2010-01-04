<?php

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the Free Software           *
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
*                                                                       *
* The author can be reached at admin@atrinik.org                        *
************************************************************************/

/**
 * @file
 * This script provides access to world viewer and world image generator,
 * depending whether it was ran from HTTP server or CLI.
 * @author Alex Tokar */

if (!file_exists('config.php'))
{
	die('ERROR: Could not find config.php.' . "\n");
}
else
{
	require_once('config.php');
}

// Set infinite time limit.
set_time_limit(0);
// Report all errors.
error_reporting(E_ALL);
ini_set('display_errors', 1);

/**
 * @defgroup type_defines Type defines
 * Defines for common object types being used.
 *@{*/

/** Spawn point. */
define('SPAWN_POINT', 81);
/** Wall. */
define('WALL', 77);
/*@}*/

// Some common defines dealing with map tiles.
define('MAP_TILE_POS_YOFF', 23);
define('MAP_TILE_POS_XOFF', 48);
define('MAP_TILE_POS_XOFF2', 24);
define('MAP_TILE_XOFF', 12);
define('MAP_TILE_YOFF', 24);

/** Faces array, initialized by parse_facetree(). */
$faces = array();
/** Archetypes array, initialized by parse_archetypes(). */
$archetypes = array();
/** Animations array, initialized by parse_animations(). */
$animations = array();

if (empty($main_path) || !file_exists($main_path) || !is_dir($main_path))
{
	die('ERROR: Incorrectly configured main path to your Atrinik directory. Either it doesn\'t exist or is not a directory.' . "\n");
}

/** Path to maps. */
$maps_path = $main_path . '/maps';

/** Path to arch. */
$arch_path = $main_path . '/arch';

if (!file_exists(ARCH_CACHE_FILE))
{
	command_arch_cache();
}
else
{
	require_once(ARCH_CACHE_FILE);
}

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
		),
		'doc' => 'Regenerate the cached map images that are used for the world viewer.',
	),
	'arch-cache' => array(
		'doc' => 'Regenerate the information parsed from arch directory files like animations, archetypes and stored in cached PHP file as arrays.',
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

// Determine whether or not we're running from the CLI, and call
// appropriate main function.
if (php_sapi_name() == 'cli')
{
	world_main();
}
else
{
	world_viewer();
}

/**
 * The main function used when running from a server. */
function world_viewer()
{
	$map = empty($_GET['map']) ? FIRST_MAP : $_GET['map'];

	// Get the directory name.
	$dirname = dirname(__FILE__) . '/' . MAP_CACHE_DIR;

	// Sanity check.
	if (!file_exists($dirname . '/' . $map . '.png') || substr(realpath($dirname . '/' . $map . '.png'), 0, strlen($dirname)) != $dirname)
	{
		die;
	}

	// Get the XX and YY positions from the map name, and the map name
	// without the positions.
	$positions_xx = (int) substr($map, -4, 2);
	$positions_yy = (int) substr($map, -2, 2);
	$map_without_pos = substr($map, 0, -4);

	// Maps array of tiled maps.
	$maps = array();

	// Check for every single possibility of tiled map.
	for ($x = -1; $x <= 1; $x++)
	{
		for ($y = -1; $y <= 1; $y++)
		{
			// $x and $y both being zero means it's the map we're
			// displaying, so skip it.
			if ($x == 0 && $y == 0)
			{
				continue;
			}

			// Get the map file name.
			$map_file = $map_without_pos . ($positions_xx + $x < 10 ? '0' . ($positions_xx + $x) : $positions_xx + $x) . ($positions_yy + $y < 10 ? '0' . ($positions_yy + $y) : $positions_yy + $y);

			// If it exists, add it to the array.
			if (file_exists(MAP_CACHE_DIR . '/' . $map_file . '.png'))
			{
				$maps[$x][$y] = $map_file;
			}
		}
	}

	// Output the HTML.
	echo '<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html lang="eng">
	<head>
		<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
		<title>Atrinik World Viewer</title>
		<link href="world.css" rel="stylesheet" type="text/css" media="screen">
	</head>
	<body>
		<table id="world" width="100%">
			<tr>
				<td align="left" valign="top">
					<table id="compass" border="1">';

	// Ouput the compass.
	for ($i = 0; $i < 9; $i++)
	{
		$x = $y = 0;
		$classname = '';

		if (!($i % 3))
		{
			if ($i == 0)
			{
				echo '
						<tr>';
			}
			else
			{
				echo '
						</tr><tr>';
			}
		}

		switch ($i)
		{
			case 0:
				$x = -1;
				$classname = 'topleft';
				break;

			case 1:
				$x = -1;
				$y = 1;
				$classname = 'top';
				break;

			case 2:
				$y = 1;
				$classname = 'topright';
				break;

			case 3:
				$x = -1;
				$y = -1;
				$classname = 'left';
				break;

			case 4:
				$classname = '';
				break;

			case 5:
				$x = 1;
				$y = 1;
				$classname = 'right';
				break;

			case 6:
				$y = -1;
				$classname = 'bottomleft';
				break;

			case 7:
				$x = 1;
				$y = -1;
				$classname = 'bottom';
				break;

			case 8:
				$x = 1;
				$classname = 'bottomright';
				break;
		}

		if (!empty($classname))
		{
			$map_exists = !empty($maps[$x][$y]);

			echo '<td class="', $classname, ' ', !$map_exists ? 'disabled' : 'enabled', '">', $map_exists ? '<a href="?map=' . $maps[$x][$y] . '"></a>' : '', '</td>';
		}
		else
		{
			echo '<td></td>';
		}
	}


	echo '
						</tr>
					</table>
				</td>
				<td>
					<img src="', MAP_CACHE_DIR, '/', $map, '.png" alt="">
				</td>
			</tr>
		</table>
	</body>
</html>';
}

/**
 * The main function used when running from the CLI. */
function world_main()
{
	global $arguments, $argc, $commands, $argv;

	$arguments = array();
	$command = '';

	// Parse all the arguments.
	for ($i = 1; $i < $argc; $i++)
	{
		// If no command yet, and this is a valid command, store the
		// command.
		if (empty($command) && isset($commands[$argv[$i]]))
		{
			$command = $argv[$i];
		}
		// Otherwise this must be an argument for the previous command.
		elseif (!empty($command) && !empty($commands[$command]['args']))
		{
			// Get the argument name. Supports both -- and - prefixed arguments.
			$arg = substr($argv[$i], ($argv[$i][0] == '-' && $argv[$i][1] == '-' ? 2 : 1));

			// If it's a valid argument.
			if (!empty($commands[$command]['args'][$arg]))
			{
				// Switch the argument type, so we can validate the
				// input.
				switch ($commands[$command]['args'][$arg]['type'])
				{
					// String, an easy one.
					case 'string':
						if ($i + 1 < $argc)
						{
							$i++;
							$arguments[$arg] = $argv[$i];
						}

						break;

					// Range is a bit more tricky.
					case 'range':
						if ($i + 1 < $argc)
						{
							$i++;

							// Validate that it is between min and max.
							if ($argv[$i] < $commands[$command]['args'][$arg]['min'] || $argv[$i] > $commands[$command]['args'][$arg]['max'])
							{
								show_usage();
								die('ERROR: Argument ' . $argv[$i - 1] . ' must be between ' . $commands[$command]['args'][$arg]['min'] . ' and ' . $commands[$command]['args'][$arg]['max'] . '.' . "\n");
							}

							$arguments[$arg] = $argv[$i];
						}

						break;

					// Bool, just set the argument to the opposite of the default.
					case 'bool':
						$arguments[$arg] = !$commands[$command]['args'][$arg]['default'];
						break;
				}
			}
			// Otherwise show an error.
			else
			{
				show_usage();
				die('ERROR: Unknown argument for command ' . $command . ': ' . $argv[$i] . "\n");
			}
		}
	}

	// No valid command?
	if (empty($command))
	{
		show_usage();
		die('ERROR: Unknown command.' . "\n");
	}

	// If we've got arguments for this command, loop through them so we
	// can set the defaults, and check required ones.
	if (!empty($commands[$command]['args']))
	{
		foreach ($commands[$command]['args'] as $arg => $arg_values)
		{
			if (!isset($arguments[$arg]))
			{
				// Required command not set, show an error.
				if (isset($arg_values['required']) && $arg_values['required'])
				{
					show_usage();
					die('ERROR: Command requires argument \'' . $arg . '\' but it is not set. ' . "\n");
				}
				// Otherwise set the default.
				else
				{
					$arguments[$arg] = $arg_values['default'];
				}
			}
		}
	}

	// And finally, call the command's function.
	call_user_func('command_' . str_replace('-', '_', $command));
}

/**
 * Show the usage of the script when running from CLI, and, for example,
 * an unknown command was entered, or the help command was used. */
function show_usage()
{
	global $commands, $argv;

	// Show the usage.
	echo 'Usage: ./', $argv[0], ' <command> [command-arguments]', "\n\n";

	echo 'Known commands:', "\n\n";

	// And the known commands.
	foreach ($commands as $command_name => $command)
	{
		echo ' ', $command_name, ': ', $command['doc'], "\n";

		// And arguments for the known commands as well.
		if (!empty($command['args']))
		{
			echo '   Arguments:', "\n";

			foreach ($command['args'] as $arg_name => $arg)
			{
				echo '     --', $arg_name, ': ', $arg['doc'], "\n";
			}
		}

		echo "\n";
	}

	echo "\n";
}

/**
 * The map-cache command. Used to (re)generate the cached images of maps
 * used for the world viewer. */
function command_maps_cache()
{
	global $maps_path, $arguments;

	// If the map cache dir doesn't exist, create it.
	if (!file_exists(MAP_CACHE_DIR))
	{
		mkdir(MAP_CACHE_DIR);
	}
	// If it exists but is not a directory, complain.
	elseif (!is_dir(MAP_CACHE_DIR))
	{
		die('ERROR: \'' . MAP_CACHE_DIR . '\' exists, but is not a directory.');
	}

	// Maps to scan.
	$to_scan = array($maps_path);
	$maps = array();

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
			// Open the file in read only mode.
			$fp = fopen($file, 'r');

			// Get the first line.
			$line = fgets($fp);

			// If it is a legitimate map, add it to the maps array.
			if ($line == 'arch map' . "\n")
			{
				$maps[] = $file;
			}

			// Close the file.
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
		usleep(50000);
	}
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
	make_cache(array('archetypes' => $archetypes, 'faces' => $faces, 'animations' => $animations), ARCH_CACHE_FILE);
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
			usleep(50000);
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

/**
 * Adjusts an object's face by the facing direction.
 *
 * The @ref $animations array is used to look up animations and number
 * of facings.
 * @param archname Name of the arch.
 * @param direction Direction the object is facing.
 * @param face Fallback face name.
 * @return Face name relevant to the facing direction if found, the
 * fallback face name otherwise. */
function adjust_face_by_dir($archname, $direction, $face)
{
	global $animations;

	// Sanity check
	if (empty($animations[$archname]))
	{
		return $face;
	}

	// Get the correct direction number for the anims array.
	$dir_num = $direction * count($animations[$archname]['anims']) / $animations[$archname]['facings'];

	// If the direction number is set, return it.
	if (!empty($animations[$archname]['anims'][$dir_num]))
	{
		return $animations[$archname]['anims'][$dir_num];
	}

	return $face;
}

/**
 * Recursively parse an archetype from a file, loading the archetype on
 * the map, and its inventory.
 * @param archname Name of the arch to consider.
 * @param fp File pointer to get more lines from opened by fopen().
 * @param return If true, we return the found values instead of adding
 * it to the $map array initialized by caller. Used when loading
 * inventories of objects.
 * @return The found values if $return is true, nothing otherwise. */
function parse_arch_recursive($archname, $fp, $return = false)
{
	global $map, $archetypes;

	// Some defaults.
	$archetype = !empty($archetypes[$archname]) ? $archetypes[$archname] : array();
	$object_x = 0;
	$object_y = 0;
	$face = isset($archetype['face']) ? $archetype['face'] : 'bug.101';
	$inv = array();
	$type = isset($archetype['type']) ? $archetype['type'] : 0;
	$sys_object = isset($archetype['sys_object']) ? $archetype['sys_object'] : 0;
	$direction = isset($archetype['direction']) ? $archetype['direction'] : -1;
	$parts = isset($archetype['parts']) ? $archetype['parts'] : 1;
	$multi_x = !empty($archetype['multi_x']) ? $archetype['multi_x'] : 0;
	$multi_y = !empty($archetype['multi_y']) ? $archetype['multi_y'] : 0;

	// Loop through the lines.
	while ($line = fgets($fp))
	{
		// Hit an end, break out.
		if ($line == 'end' . "\n")
		{
			break;
		}

		// Another arch inside another arch means it's inside an
		// inventory, so load it up.
		if (sscanf($line, 'arch %s' . "\n", $inv_archname) == 1)
		{
			$inv[] = parse_arch_recursive($inv_archname, $fp, true);
		}

		// Parse values.
		sscanf($line, 'x %d' . "\n", $object_x);
		sscanf($line, 'y %d' . "\n", $object_y);
		sscanf($line, 'face %s' . "\n", $face);
		sscanf($line, 'type %d' . "\n", $type);
		sscanf($line, 'sys_object %d' . "\n", $sys_object);
		sscanf($line, 'direction %d' . "\n", $direction);
	}

	// If direction was set, adjust the face by the direction. Note we
	// check for -1, because 0 is a valid direction.
	if ($direction != -1)
	{
		$face = adjust_face_by_dir($archname, $direction, $face);
	}

	// We ignore system objects which are not of type SPAWN_POINT.
	if ($sys_object && $type != SPAWN_POINT)
	{
		return;
	}

	if (!isset($map[$object_y]))
	{
		$map[$object_y] = array();
	}

	if (!isset($map[$object_y][$object_x]))
	{
		$map[$object_y][$object_x] = array();
	}

	// Initialize the object's array.
	$object_array = array(
		'arch' => $archname,
		'face' => $face,
		'type' => $type,
		'parts' => $parts,
		'multi_x' => $multi_x,
		'multi_y' => $multi_y,
		'is_multi' => $parts > 1,
		'inv' => $inv,
	);

	// If $return was set, return the array.
	if ($return)
	{
		return $object_array;
	}

	// Otherwise add it to the $map array initialize by caller.
	$map[$object_y][$object_x][] = $object_array;
}

/**
 * Parse a single map file.
 * @param file File where to load the map stuff from.
 * @return An array containing all the objects on the map. */
function parse_map_file($file)
{
	global $archetypes, $map;

	// Open the file.
	$fp = fopen($file, 'r');

	$got_map = $got_map_end = false;

	// Initialize our array where objects will be stored.
	$map = array();

	// Loop through the lines.
	while ($line = fgets($fp))
	{
		// Check if this is map arch, which should be the first line.
		if ($line == 'arch map' . "\n")
		{
			$got_map = true;
		}
		// If we are inside the map, and we haven't hit the end yet until
		// now, set it.
		elseif ($got_map && !$got_map_end && $line == 'end' . "\n")
		{
			$got_map_end = true;
			continue;
		}

		// We haven't got "arch map" as the first line, so break out.
		if (!$got_map)
		{
			break;
		}

		// We still haven't got a map end, so continue to the next line.
		if (!$got_map_end)
		{
			continue;
		}

		// If this is a new arch, call the parsing function.
		if (sscanf($line, 'arch %s' . "\n", $archname) == 1)
		{
			parse_arch_recursive($archname, $fp);
		}
	}

	// Close the file.
	fclose($fp);

	// Return the objects.
	return $map;
}

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
 * Get an array of all files in a directory.
 * @param dir The directory to get files for.
 * @return Array of all the files in the directory and its
 * subdirectories. */
function getfiles($dir)
{
	$files = array();

	$dh = opendir($dir);

	while (($file = readdir($dh)))
	{
		if ($file == '.' || $file == '..')
		{
			continue;
		}

		if (is_dir($dir . '/' . $file))
		{
			$files = array_merge($files, getfiles($dir . '/' . $file));
		}
		elseif (is_file($dir . '/' . $file))
		{
			$files[$file] = $dir;
		}
	}

	closedir($dh);

	return $files;
}

/**
 * Parse the facetree file, which holds locations of every single face.
 * Parsed into the @ref $faces array.
 * @param file The file to parse. */
function parse_facetree($file)
{
	global $faces, $arch_path;

	// Open the file.
	$fp = fopen($file, 'r');

	$files = getfiles($arch_path);

	// Loop through the lines.
	while ($line = fgets($fp))
	{
		// Get the face name and the directory location where it is.
		$face_name = substr(strrchr($line, '/'), 1, -1);
		// Add it to the array.
		$faces[$face_name] = $files[$face_name . '.png'];
	}

	// Close the file.
	fclose($fp);
}

/**
 * Parse the animations file, adding the entries to the @ref $animations
 * array.
 * @param file The file to parse. */
function parse_animations($file)
{
	global $animations;

	// Open the file.
	$fp = fopen($file, 'r');

	// Loop through the lines.
	while ($line = fgets($fp))
	{
		// If this is a start of an animation.
		if (sscanf($line, 'anim %s' . "\n", $animname) == 1)
		{
			// Value defaults.
			$facings = 1;

			// Initialize the defaults for this animation.
			$animations[$animname] = array(
				'facings' => $facings,
				'anims' => array(),
			);

			// Loop through more lines.
			while ($line = fgets($fp))
			{
				// If we hit the end of the animation, break out.
				if ($line == 'mina' . "\n")
				{
					break;
				}

				// Parse the lines.
				if (sscanf($line, 'facings %d' . "\n", $facings) == 1)
				{
					$animations[$animname]['facings'] = $facings;
				}
				else
				{
					$animations[$animname]['anims'][] = substr($line, 0, -1);
				}
			}
		}
	}

	// Close the file.
	fclose($fp);
}

/**
 * Parse the archetypes file into the @ref $archetypes array.
 * @param file The file. */
function parse_archetypes($file)
{
	global $archetypes;

	// Indicates whether there is more to the arch.
	$is_more = false;
	// Name of the last arch parsed.
	$last_archname = '';

	// Open the file.
	$fp = fopen($file, 'r');

	// Loop through the lines.
	while ($line = fgets($fp))
	{
		// Is there more to the last archetype?
		if ($line == 'More' . "\n")
		{
			$is_more = true;
		}
		// Otherwise it's start of the archetype.
		elseif (sscanf($line, 'Object %s' . "\n", $archname) == 1)
		{
			// Value defaults.
			$face = 'bug.101';
			$type = 0;
			$sys_object = 0;
			$direction = -1;

			// If there is more to the archetype, increase number of
			// parts the last archname had.
			if ($is_more && !empty($last_archname))
			{
				$archetypes[$last_archname]['parts']++;
			}

			// Loop through more lines.
			while ($line = fgets($fp))
			{
				// If we hit the end, break out.
				if ($line == 'end' . "\n")
				{
					break;
				}

				// Scan for values.
				sscanf($line, 'face %s' . "\n", $face);
				sscanf($line, 'type %d' . "\n", $type);
				sscanf($line, 'sys_object %d' . "\n", $sys_object);
				sscanf($line, 'direction %d' . "\n", $direction);

				// X or Y being set in the archetype indicates it's a
				// part of a multi archetype.
				if (sscanf($line, 'x %d' . "\n", $multi_x))
				{
					$archetypes[$last_archname]['multi_x'] = $multi_x;
				}
				elseif (sscanf($line, 'y %d' . "\n", $multi_y))
				{
					$archetypes[$last_archname]['multi_y'] = $multi_y;
				}
			}

			// We ignore archetypes that are just information for the
			// main multi archetype.
			if ($is_more)
			{
				$is_more = false;
				continue;
			}

			// Set the last archname.
			$last_archname = $archname;

			// Add the archetype to the array.
			$archetypes[$archname] = array(
				'face' => $face,
				'type' => $type,
				'sys_object' => $sys_object,
				'direction' => $direction,
				'parts' => 1,
			);
		}
	}

	// Close the file.
	fclose($fp);
}

/**
 * Parse the artifacts file.
 *
 * Entries in the artifacts file are parsed, and artifacts are added to
 * the @ref $archetypes array.
 * @param file The artifacts file to parse. */
function parse_artifacts($file)
{
	global $archetypes;

	// Open the file.
	$fp = fopen($file, 'r');

	// Loop through the lines.
	while ($line = fgets($fp))
	{
		// Ignore comments and empty lines.
		if ($line[0] == '#' || $line[0] == "\n")
		{
			continue;
		}

		// Every artifacts starts with 'Allowed %s'.
		if (sscanf($line, 'Allowed %s' . "\n", $allowed) == 1)
		{
			$artifact_name = $def_arch = $face = '';
			$type = -1;

			// Loop through more lines.
			while ($line = fgets($fp))
			{
				// If we hit, break out.
				if ($line == 'end' . "\n")
				{
					break;
				}

				// Scan for the needed values.
				sscanf($line, 'artifact %s', $artifact_name);
				sscanf($line, 'def_arch %s', $def_arch);
				sscanf($line, 'face %s', $face);
				sscanf($line, 'type %d', $type);
			}

			// Sanity check.
			if (empty($artifact_name) || empty($def_arch))
			{
				continue;
			}

			// First load the defaults, and then replace with any custom
			// values in the artifacts file.
			$archetypes[$artifact_name] = $archetypes[$def_arch];

			if ($type != -1)
			{
				$archetypes[$artifact_name]['type'] = $type;
			}

			if (!empty($face))
			{
				$archetypes[$artifact_name]['face'] = $face;
			}
		}
	}

	// Close the file.
	fclose($fp);
}

/**
 * Write an array to file.
 *
 * Used by make_cache() to generate cache of arrays.
 * @note This function is recursive, and will call itself over and over
 * for other arrays in the initial array.
 * @param array The array to write to the file.
 * @param fp File pointer opened by fopen() to file where to write the
 * array. */
function array_to_file($array, $fp)
{
	// Loop through the array values.
	foreach ($array as $key => $value)
	{
		// If the key is not empty, write it out.
		if (!empty($key))
		{
			fwrite($fp, '\'' . $key . '\'=>');
		}

		// Write out the values, depending on their type.
		if (is_string($value))
		{
			fwrite($fp, '\'' . $value . '\',');
		}
		elseif (is_int($value) || is_float($value))
		{
			fwrite($fp, $value . ',');
		}
		elseif (is_bool($value))
		{
			fwrite($fp, ($value ? 'true' : 'false') . ',');
		}
		elseif (is_array($value))
		{
			fwrite($fp, 'array(');
			array_to_file($value, $fp);
			fwrite($fp, '),');
		}
	}
}

/**
 * Make cache of common arrays needed by the script.
 * @param cache The array of what arrays to cache, and their names.
 * @param cache_file File name where to store the cached arrays. */
function make_cache($cache, $cache_file)
{
	// Open the file.
	$fp = fopen($cache_file, 'w');

	// Write out PHP start tag.
	fwrite($fp, '<?php ');

	// Loop through the arrays to cache.
	foreach ($cache as $array_name => $cache_array)
	{
		// Write out the variable name of the array, and the array
		// initializer.
		fwrite($fp, '$' . $array_name . ' = array(');
		// Write out all the array values.
		array_to_file($cache_array, $fp);
		// Close the array.
		fwrite($fp, ');');
	}

	// Close the PHP tag.
	fwrite($fp, ' ?>');

	// Close the file.
	fclose($fp);
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
