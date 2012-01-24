<?php

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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

if (empty($main_path) || !file_exists($main_path) || !is_dir($main_path))
{
	die('ERROR: Incorrectly configured main path to your Atrinik directory. Either it doesn\'t exist or is not a directory.' . "\n");
}

// Determine whether or not we're running from the CLI, and call
// appropriate main function.
if (php_sapi_name() == 'cli')
{
	require_once(SRC_DIR . '/utils.php');
	require_once(SRC_DIR . '/commands.php');
	require_once(SRC_DIR . '/renderer.php');
	require_once(SRC_DIR . '/arch.php');
	require_once(SRC_DIR . '/parser.php');
	require_once(SRC_DIR . '/main.php');
	world_main();
}
else
{
	require_once(SRC_DIR . '/viewer.php');
	world_viewer();
}

?>
