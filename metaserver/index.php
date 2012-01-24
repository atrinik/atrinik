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

/************************************************************************
* This file is used to display all servers for clients.                 *
* It displays the data in an easy-to-grab way for the client.           *
************************************************************************/

header('Content-type: text/plain');

require_once('common.php');

// Select the servers
$request = db_query('
	SELECT ip_address, port, name, hostname, num_players, version, text_comment
	FROM servers
	WHERE last_update > (' . (time() - $last_update_timeout) . ')
	ORDER BY roworder ASC');

// Calculate the number of rows
$num_rows = db_num_rows($request);

// No rows?
if ($num_rows < 1)
{
	die;
}

$is_legacy_client = substr($_SERVER['HTTP_USER_AGENT'], -1, 1) != ')';
$i = 0;

// Now go through the rows
while ($row = db_fetch_assoc($request))
{
	// Output the data, format:
	// IP:Port:Name:Number_of_players:Version:Comment
	if ($is_legacy_client)
	{
		echo $row['ip_address'], ':', $row['port'], ':', $row['hostname'], ':', $row['num_players'], ':', $row['version'], ':', $row['text_comment'];
	}
	else
	{
		echo $row['ip_address'], ':', $row['port'], ':', $row['name'], ':', $row['num_players'], ':', $row['version'], ':', $row['text_comment'];
	}

	$i++;

	// If this is not last row, echo a line break
	if ($i < $num_rows)
	{
		echo "\n";
	}
}

// Free the request
db_free_result($request);

?>

