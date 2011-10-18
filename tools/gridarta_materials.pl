#!/usr/bin/perl
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# This script will collect materials information from Atrinik server
# and the arch/materials file, and will output XML for use in lists.xml
# and bitmasks.xml files.

use strict;
use warnings;

print "<bitmask name=\"material\">\n";
open FILE, "../server/src/include/material.h" or die $!;

my $bit = 0;
my $in_materials = 0;
my %materials;

while (my $line = <FILE>)
{
	if ($line eq "/*@}*/\n")
	{
		$in_materials = 0;
	}
	elsif ($in_materials)
	{
		if ((substr $line, 0, 10) eq "#define M_")
		{
			my $material = lc(substr $line, 10, index((substr $line, 10), " "));
			my $material_id = substr($line, 10 + length($material), -1);
			$material_id =~ s/^\s+//;
			$material =~ s/_/ /;
			$material =~ s/\b(\w)/\u$1/g;
			$materials{$material_id} = $material;
			print "\t<bmentry bit=\"$bit\" name=\"$material\" />\n";
			$bit++;
		}
	}
	elsif ((substr $line, 0, 15) eq "#define M_NONE ")
	{
		$in_materials = 1;
	}
}

close(FILE);
print "</bitmask>\n\n\n";

open FILE, "../arch/materials" or die $!;

my $last_material = 0;
my %materials_real;

while (my $line = <FILE>)
{
	chomp($line);

	if ((substr $line, 0, 14) eq "material_real ")
	{
		$last_material = substr($line, 14);
		$materials_real{$last_material} = {
			"name", ""
		};
	}
	elsif ((substr $line, 0, 5) eq "name ")
	{
		$materials_real{$last_material}{"name"} = substr($line, 5);
	}
	elsif ((substr $line, 0, 5) eq "type ")
	{
		$materials_real{$last_material}{"type"} = substr($line, 5);
	}
}

close(FILE);

print "<list name=\"material_real\">\n";
print "\t<listentry value=\"0\" name=\"&lt;undefined&gt;\" />\n";

foreach my $material (sort {$a <=> $b} keys %materials_real)
{
	my $material_name = lc($materials{$materials_real{$material}{"type"}});
	print "\t<listentry value=\"$material\" name=\"$material_name: $materials_real{$material}{\"name\"}\" />\n";
}

print "</list>\n";

