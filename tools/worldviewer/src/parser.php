<?php

/**
 * @file
 * Handles parsing related functions. */

/**
 * Parse the facetree file, which holds locations of every single face.
 * Parsed into the @ref $faces array.
 * @param file The file to parse. */
function parse_facetree($file)
{
	global $faces, $arch_path;

	$files = getfiles($arch_path);

	foreach ($files as $file => $path)
	{
		if (substr($file, -4) != '.png')
		{
			continue;
		}

		$faces[substr($file, 0, -4)] = $path;
	}
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
 * Parse a single archetype line, either from the archetypes file or from map.
 * @param line The line to parse.
 * @param[out] archetype Archetype's data array that will contain the information from
 * line, if successfully parsed.
 * @param type Type, marks where the line is from. */
function parse_archetype_line($line, $archetype, $type = 'archetypes')
{
	global $arch_attributes;

	foreach ($arch_attributes as $attribute => $data)
	{
		if (isset($data['type']) && ($data['type'] === null || $data['type'] != $type))
		{
			continue;
		}

		$value_type = '%d';

		if (is_string($data['default']))
		{
			$value_type = '%s';
		}

		if (sscanf($line, $attribute . ' ' . $value_type . "\n", $archetype[$attribute]) == 1)
		{
			break;
		}
	}
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
			archetype_init($archname);

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

				parse_archetype_line($line, &$archetypes[$archname]);

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
 * Attempt to parse file as map and load its header.
 * @param $fp File pointer to read from.
 * @return False if we failed to load the map header, the map header as
 * an array otherwise. */
function parse_map_header($fp)
{
	$got_map = false;
	$map_info = array();
	$msg_buf = '';
	$in_msg = false;

	while ($line = fgets($fp))
	{
		if ($line == 'arch map' . "\n")
		{
			$got_map = true;
			continue;
		}
		elseif ($line == 'end' . "\n")
		{
			return $map_info;
		}

		if (!$got_map)
		{
			return false;
		}

		if ($line == 'msg' . "\n")
		{
			$in_msg = true;
		}
		elseif ($line == 'endmsg' . "\n")
		{
			$map_info['msg'] = substr($msg_buf, 0, -1);
			$in_msg = false;
		}
		elseif ($in_msg)
		{
			$msg_buf .= $line;
		}
		else
		{
			$space_pos = strpos($line, ' ');
			$attribute = substr($line, 0, $space_pos);
			$value = substr($line, $space_pos + 1, -1);

			$map_info[$attribute] = $value;
		}
	}

	return false;
}

?>
