<?php

/**
 * @file
 * Handles the world viewer code. */

/**
 * The main function used when running from a server. */
function world_viewer()
{
	$map = empty($_GET['map']) ? FIRST_MAP : $_GET['map'];

	// Get the directory name.
	$dirname = dirname($_SERVER['SCRIPT_FILENAME']) . '/' . MAP_CACHE_DIR;

	// Sanity check.
	if (!file_exists($dirname . '/' . $map . '.png') || substr(realpath($dirname . '/' . $map . '.png'), 0, strlen($dirname)) != $dirname)
	{
		die;
	}

	if (!file_exists(MAP_CACHE_DIR) || !is_dir(MAP_CACHE_DIR))
	{
		die('ERROR: Map cache directory doesn\'t exist or is not a directory.');
	}

	if (!file_exists(MAPS_CACHE_FILE) || !is_file(MAPS_CACHE_FILE))
	{
		die('ERROR: Maps cache file doesn\'t exist or is not a file.');
	}

	require_once(MAPS_CACHE_FILE);

	if (!isset($maps_info[$map]))
	{
		die('ERROR: Could not find the provided map in $maps_info.');
	}

	$map_info = $maps_info[$map];

	// Maps array of tiled maps.
	$maps = array();

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
					<table id="compass" border="1">
						<tr>';

	// Ouput the compass.
	for ($i = 0; $i < 9; $i++)
	{
		if ($i && !($i % 3))
		{
			echo '
						</tr><tr>';
		}

		switch ($i)
		{
			case 0:
				$tile_id = 4;
				$classname = 'topleft';
				break;

			case 1:
				$tile_id = 8;
				$classname = 'top';
				break;

			case 2:
				$tile_id = 1;
				$classname = 'topright';
				break;

			case 3:
				$tile_id = 7;
				$classname = 'left';
				break;

			case 4:
				$classname = '';
				break;

			case 5:
				$tile_id = 5;
				$classname = 'right';
				break;

			case 6:
				$tile_id = 3;
				$classname = 'bottomleft';
				break;

			case 7:
				$tile_id = 6;
				$classname = 'bottom';
				break;

			case 8:
				$tile_id = 2;
				$classname = 'bottomright';
				break;
		}

		if (!empty($classname))
		{
			$map_exists = isset($map_info['tile_path_' . $tile_id]) && file_exists(MAP_CACHE_DIR . '/' . dirname($map) . '/' . $map_info['tile_path_' . $tile_id] . '.png');

			echo '<td class="', $classname, ' ', !$map_exists ? 'disabled' : 'enabled', '">', $map_exists ? '<a href="?map=' . dirname($map) . '/' . $map_info['tile_path_' . $tile_id] . '"></a>' : '', '</td>';
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

?>
