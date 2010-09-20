<?php

/**
 * @file
 * Utility related functions that can be reused across the sources,
 * and have no extra source code dependencies. */

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
 * Write an array to file. Used by make_cache().
 * @note This function is recursive, and will call itself over and over
 * for other arrays in the initial array.
 * @param array The array to write to the file.
 * @param fp File handle where to write the array. */
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
			fwrite($fp, '\'' . addslashes($value) . '\',');
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
 * Write arrays to file.
 * @param cache Array of arrays to write.
 * @param cache_file File name to write. */
function make_cache($cache, $cache_file)
{
	// Open the file.
	$fp = fopen($cache_file, 'w');

	// Write out PHP start tag.
	fwrite($fp, '<?php ');

	// Loop through the arrays to cache.
	foreach ($cache as $cache_array)
	{
		$array_name = '';

		foreach ($GLOBALS as $glob => $glob_arr)
		{
			if ($glob_arr == $cache_array)
			{
				$array_name = $glob;
				break;
			}
		}

		// If it's empty at this point, something is very wrong.
		if (empty($array_name))
		{
			echo 'ERROR: Could not find array\'s name in $GLOBALS.', "\n";
			continue;
		}

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

?>
