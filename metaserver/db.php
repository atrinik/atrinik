<?php
/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2015 Alex Tokar and Atrinik Development Team    *
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
* This file is used to hold all database related functions, so you can  *
* easily create support for other databases other than MySQL, for       *
* example, SQLite or PostgreSQL. It is also used to initiate a database *
* connection, or exit on failure to do so.                              *
************************************************************************/

require_once('db-cfg.php');

// Initate a connection
$db_connection = mysql_connect($db_server, $db_user, $db_passwd);

// If failed to initiate a connection, or could not select correct database, log error and die.
if (!$db_connection || !mysql_select_db($db_name, $db_connection))
{
	log_message('ERROR: ' . mysql_error() . "\n");
	die('Failed to connect to database.' . "\n");
}

// Send SQL query
function db_query($query)
{
	global $db_connection;

    return mysql_query($query, $db_connection);
}

// Fetch a result row as an associative array
function db_fetch_assoc($request)
{
    return mysql_fetch_assoc($request);
}

// Get number of rows in result
function db_num_rows($request)
{
	return mysql_num_rows($request);
}

// Free result memory
function db_free_result($request)
{
	mysql_free_result($request);
}

// Get number of affected rows in previous SQL operation
function db_affected_rows()
{
	global $db_connection;

	return mysql_affected_rows($db_connection);
}

// Escapes special characters in a string for use in SQL statement
function db_sanitize($input)
{
	return mysql_real_escape_string($input);
}

// Closes the DB connection.
function db_close()
{
	global $db_connection;

	mysql_close($db_connection);
}

?>
