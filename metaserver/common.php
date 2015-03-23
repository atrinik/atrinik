<?php
/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team    *
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

/************************************************************************
* This file is used to hold all common functions used all around        *
* metaserver PHP files.                                                 *
************************************************************************/

// Log file for meta server
$logfile = 'metaserver.log';

// Maximum timeout for servers to disappear from the metaserver
$last_update_timeout = 3600;

// Load all database functions, and init connection
require_once('db.php');

// Logs a message to our log file.
function log_message($message)
{
	global $logfile;

	// No log file specified?
	if (!isset($logfile) || empty($logfile))
	{
		return;
	}

	// Open the log file
	$fp = fopen($logfile, 'a');

	// Failed to open?
	if (!$fp)
	{
		return;
	}

	// Write it to the file
	fwrite($fp, $message);

	// Close the file
	fclose($fp);
}

?>
