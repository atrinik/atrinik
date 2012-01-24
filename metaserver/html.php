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
* This file can be used to allow browsers to see the servers in nice    *
* HTML table output.                                                    *
************************************************************************/

require_once('common.php');

echo '
<html>
	<head>
		<title>Atrinik Servers List</title>
	</head>
	<body>';

// Select the servers
$request = db_query('
	SELECT ip_address, port, name, num_players, version, text_comment, players
	FROM servers
	WHERE last_update > (' . (time() - $last_update_timeout) . ')
	ORDER BY roworder DESC');

echo '
		<center>
			<h1>Atrinik Servers List</h1>
			<table border="1" width="70%">';
echo '
				<tr>
					<td><b>Name</b></td>
					<td><b>IP Address</b></td>
					<td><b>Port</b></td>
					<td width="10%"><b># of players</b></td>
					<td><b>Version</b></td>
					<td><b>Comment</b></td>
					<td width="20%"><b>Players</b></td>
				</tr>';

// If no servers...
if (db_num_rows($request) < 1)
	echo '
				<tr>
					<td colspan="7"><center>Currently, there are no servers active.</center></td>
				</tr>';

// Loop through the servers.
while ($row = db_fetch_assoc($request))
{
	echo '
				<tr>
					<td>', $row['name'], '</td>
					<td>', $row['ip_address'], '</td>
					<td>', $row['port'], '</td>
					<td>', $row['num_players'], '</td>
					<td>', $row['version'], '</td>
					<td>', $row['text_comment'], '</td>
					<td valign="top">';

	if (isset($_GET['players']) && !empty($row['players']))
	{
		echo str_replace(':', '<br>', $row['players']);
	}

	echo '
					&nbsp;</td>
				</tr>';
}

// Free the request
db_free_result($request);

echo '
			</table>';

if (!isset($_GET['players']))
{
	echo '<a href="', $_SERVER['PHP_SELF'], '?players">Show online players</a>';
}
else
{
	echo '<a href="', $_SERVER['PHP_SELF'], '">Hide online players</a>';
}

echo '
		</center>
	</body>
</html>';

?>
