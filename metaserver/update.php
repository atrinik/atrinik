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
* This file is used by the server to update metaserver information.     *
************************************************************************/

// Load the common functions
require_once('common.php');

// The fields we need to look through
$fields = array(
	'hostname' => array(
		'type' => 'string',
	),
	'num_players' => array(
		'type' => 'int',
		'empty' => true,
	),
	'version' => array(
		'type' => 'string',
	),
	'text_comment' => array(
		'type' => 'string',
	),
	'port' => array(
		'type' => 'int',
	),
	'players' => array(
		'type' => 'string',
		'required' => false,
		'empty' => true,
	),
	'name' => array(
		'type' => 'string',
	),
);

// Check the POST values. All data is sanitized here.
foreach ($fields as $field => $field_data)
{
	// Not set but it's required?
	if (!isset($_POST[$field]))
	{
		if (!isset($field_data['required']) || $field_data['required'])
		{
			echo 'Did not get required post variable: ', $field, "\n";
			log_message('ERROR: Did not get required POST variable from server: ' . $field . "\n");
			die;
		}
		else
		{
			$_POST[$field] = '';
		}
	}

	// Trim the right and left whitespace.
	$_POST[$field] = trim($_POST[$field]);

	// Sanitize the data
	if ($field_data['type'] == 'string')
	{
		$_POST[$field] = db_sanitize($_POST[$field]);
	}
	elseif ($field_data['type'] == 'int')
	{
		$_POST[$field] = (int) $_POST[$field];
	}

	// Set, but we need a value for it?
	if ((!isset($field_data['empty']) || !$field_data['empty']) && empty($_POST[$field]))
	{
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
    echo 'Neither forward nor reverse DNS look corresponding to incoming IP address.', "\n";
    echo 'Incoming IP: ', $_SERVER['REMOTE_ADDR'], ', DNS of that: ', $hostname, "\n";;
    echo 'User specified hostname: ', $_POST['hostname'], ' IP of that hostname: ', $ip, "\n";;

    log_message('WARNING: ' . $_SERVER['REMOTE_ADDR'] . ' does not have correct hostname set' . "\n");
    die;
}

// Send the query
$request = db_query('
	SELECT *
	FROM blacklist
	WHERE
		(\'' . $_SERVER['REMOTE_ADDR'] . '\' REGEXP hostname)
		OR (\'' . $hostname . '\' REGEXP hostname)
		OR (\'' . $_POST['hostname'] . '\' REGEXP hostname)');

// Go through the returned rows, any first row returned is a match
while ($row = db_fetch_assoc($request))
{
	echo 'Your system has been blacklisted.  Matching entry: ', $row['hostname'], "\n";
	log_message('WARNING: Attempt to connect from blacklisted host: ' . $_SERVER['REMOTE_ADDR'] . '/' . $hostname . "\n");
	die;
}

db_free_result($request);

// Query to update the server info
db_query('
	UPDATE servers
	SET
		ip_address = \'' . $_SERVER['REMOTE_ADDR'] . '\',
		num_players = ' . $_POST['num_players'] . ',
		version = \'' . $_POST['version'] . '\',
		text_comment = \'' . $_POST['text_comment'] . '\',
		last_update = ' . time() . ',
		players = \'' . $_POST['players'] . '\',
		name = \'' . $_POST['name'] . '\'
	WHERE
		hostname = \'' . $_POST['hostname'] . '\' AND
		port = ' . $_POST['port']);

// If no rows were affected (there wasn't a row to update), this is a new insert
if (db_affected_rows() < 1)
{
	db_query('
		INSERT INTO servers
		(ip_address, port, hostname, num_players, version, text_comment, last_update, players, name)
		VALUES
		(\'' . $_SERVER['REMOTE_ADDR'] . '\', ' . $_POST['port'] . ', \'' . $_POST['hostname'] . '\', ' . $_POST['num_players'] . ', \'' . $_POST['version'] . '\', \'' . $_POST['text_comment'] . '\', ' . time() . ', \'' . $_POST['players'] . '\', \'' . $_POST['name'] . '\')');
}

?>
