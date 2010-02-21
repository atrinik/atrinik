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

?>