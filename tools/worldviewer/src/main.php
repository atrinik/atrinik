<?php

/**
 * @file
 * Main file for CLI usage of the script. */

/** Faces array, initialized by parse_facetree(). */
$faces = array();
/** Archetypes array, initialized by parse_archetypes(). */
$archetypes = array();
/** Animations array, initialized by parse_animations(). */
$animations = array();

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

?>
