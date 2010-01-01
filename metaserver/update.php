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

/************************************************************************
* This file is used by the server to update metaserver information.     *
************************************************************************/

// Load the common functions
require_once('common.php');

// All the data that _MUST_ be sent by the server
$fields = array(
	'hostname',
	'num_players',
	'version',
	'text_comment',
	'port',
);

// Check the POST values
foreach ($fields as $field)
{
	// If it's not set, or empty and not 0 (ie, number of players)
	if (!isset($_POST[$field]) || (empty($_POST[$field]) && $_POST[$field] != 0))
	{
		// Show a message, log it and die.
		echo 'Did not get required post variable: ', $field, "\n";
		log_message('ERROR: Did not get required POST variable from server: ' . $field . "\n");
		die;
	}
}

// Hostname still set to the default?
if ($_POST['hostname'] == 'put.your.hostname.here')
{
    echo 'You have not properly set up your metaserver configuration - hostname is set to default', "\n";
    die;
}

$hostname = gethostbyaddr($_SERVER['REMOTE_ADDR']);
$ip = gethostbyname($_POST['hostname']);

// Sanity checks
if ($ip != $_SERVER['REMOTE_ADDR'] && $hostname != $_POST['hostname'])
{
    echo 'Neither forward nor reverse DNS look corresponds to incoming IP address.', "\n";
    echo 'Incoming IP: ', $_SERVER['REMOTE_ADDR'], ', DNS of that: ', $hostname, "\n";;
    echo 'User specified hostname: ', $_POST['hostname'], ' IP of that hostname: ', $ip, "\n";;

    log_message('WARNING: ' . $_SERVER['REMOTE_ADDR'] . ' does not have correct hostname set', "\n");
    die;
}

// Are we blacklisted entry?
$query = '
	SELECT *
	FROM blacklist
	WHERE (\'' . db_sanitize($_SERVER['REMOTE_ADDR']) . '\' REGEXP hostname) OR (\'' .
	db_sanitize($hostname) . '\' REGEXP hostname) OR (\'' .
	db_sanitize($_POST['hostname']) . '\' REGEXP hostname)';

// Send the query
$request = db_query($query);

// Go through the returned rows, any first row returned is a match
while ($row = db_fetch_assoc($request))
{
	echo 'Your system has been blacklisted.  Matching entry: ', $row['hostname'], "\n";
	log_message('WARNING: Attempt to connect from blacklisted host: ' . $_SERVER['REMOTE_ADDR'] . '/' . $hostname . "\n");
	die;
}

db_free_result($request);

// Query to update the server info
$query = '
	UPDATE servers
	SET
		ip_address = \'' . db_sanitize($_SERVER['REMOTE_ADDR']) . '\',
		port = ' . (is_numeric($_POST['port']) ? (int) $_POST['port'] : 13327) . ',
		num_players = ' . (is_numeric($_POST['num_players']) ? (int) $_POST['num_players'] : 0) . ',
		version = \'' . db_sanitize($_POST['version']) . '\',
		text_comment = \'' . db_sanitize($_POST['text_comment']) . '\',
		last_update = ' . time() . '
	WHERE
		hostname = \'' . db_sanitize($_POST['hostname']) . '\'';

// Send the query
db_query($query);

// If no rows were affected (there wasn't a row to update), this is a new insert
if (db_affected_rows() < 1)
{
	// Insert query
	$query = '
	INSERT INTO servers
	(ip_address, port, hostname, num_players, version, text_comment, last_update)
	VALUES
	(\'' . db_sanitize($_SERVER['REMOTE_ADDR']) . '\', ' . (is_numeric($_POST['port']) ? (int) $_POST['port'] : 13327) . ', \'' . db_sanitize($_POST['hostname']) . '\', ' . (is_numeric($_POST['num_players']) ? (int) $_POST['num_players'] : 0) . ', \'' . db_sanitize($_POST['version']) . '\', \'' . db_sanitize($_POST['text_comment']) . '\', ' . time() . ')';

	// Run the query
	db_query($query);
}

?>
