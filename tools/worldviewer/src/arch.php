<?php

/**
 * @file
 * Handles archetype related code. */

/** Archetype attributes. */
$arch_attributes = array(
	'x' => array(
		'default' => 0,
		'type' => 'map',
	),
	'y' => array(
		'default' => 0,
		'type' => 'map',
	),
	'face' => array(
		'default' => 'bug.101',
	),
	'type' => array(
		'default' => 0,
	),
	'sys_object' => array(
		'default' => 0,
	),
	'direction' => array(
		'default' => -1,
	),
	'multi_x' => array(
		'default' => 0,
		'type' => null,
	),
	'multi_y' => array(
		'default' => 0,
		'type' => null,
	),
	'parts' => array(
		'default' => 1,
		'type' => null,
	),
);

/**
 * Initialize default values for a single archetype in the @ref $archetypes array.
 * @param archname Arch name to initialize. */
function archetype_init($archname)
{
	global $archetypes, $arch_attributes;

	$archetypes[$archname] = array();

	foreach ($arch_attributes as $attribute => $data)
	{
		$archetypes[$archname][$attribute] = $data['default'];
	}
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
	$inv = array();

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

		parse_archetype_line($line, &$archetype, 'map');
	}

	// If direction was set, adjust the face by the direction. Note we
	// check for -1, because 0 is a valid direction.
	if ($archetype['direction'] != -1)
	{
		$archetype['face'] = adjust_face_by_dir($archname, $archetype['direction'], $archetype['face']);
	}

	// We ignore system objects which are not of type SPAWN_POINT.
	if ($archetype['sys_object'] && $archetype['type'] != SPAWN_POINT)
	{
		return;
	}

	if (!isset($map[$archetype['y']]))
	{
		$map[$archetype['y']] = array();
	}

	if (!isset($map[$archetype['y']][$archetype['x']]))
	{
		$map[$archetype['y']][$archetype['x']] = array();
	}

	$archetype += array(
		'inv' => $inv,
		'is_multi' => $archetype['parts'] > 1,
	);

	// If $return was set, return the array.
	if ($return)
	{
		return $archetype;
	}

	// Otherwise add it to the $map array initialized by caller.
	$map[$archetype['y']][$archetype['x']][] = $archetype;
}

?>
