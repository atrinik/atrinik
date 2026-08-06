"""Tests for the authored-content identity catalog."""

import json
import tempfile
import unittest
from pathlib import Path

from tools.content_catalog import ContentCatalog, ContentId, load_catalog


class ContentCatalogTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)
        (self.root / "arch").mkdir()
        (self.root / "maps" / "interfaces" / "quests" / "sample_quest").mkdir(
            parents=True
        )

    def tearDown(self):
        self.temporary_directory.cleanup()

    def write(self, relative_path, contents):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        return path

    def create_valid_tree(self):
        self.write(
            "arch/objects.arc",
            """Object base
type 1
end
Object spell_minor_healing
type 29
end
Object skill_literacy
type 43
end
Object multipart_head
type 1
end
More
Object multipart_tail
type 1
end
""",
        )
        self.write(
            "arch/items.art",
            """Allowed all
artifact special_item
def_arch base
Object
end
""",
        )
        self.write(
            "arch/items.trs",
            """treasure basic_loot
  arch special_item
end
""",
        )
        self.write(
            "maps/world.factions",
            """faction world
  faction citizens
  end
end
""",
        )
        self.write(
            "maps/regions.reg",
            """region world
map_first /start
end
region town
parent world
end
""",
        )
        self.write(
            "maps/start",
            """arch map
region town
tile_path_1 next
end
arch base
end
arch base
end
arch special_item
end
""",
        )
        self.write(
            "maps/next",
            """arch map
region town
end
arch base
end
""",
        )
        self.write(
            "maps/interfaces/quests/sample_quest/quest.xml",
            """<?xml version="1.0"?>
<interfaces>
  <quest name="Sample Quest">
    <part uid="first_part">
      <part uid="nested_part"/>
      <interface>
        <action start="first_part::nested_part" cast="minor healing"
                teleport="/start 1 1" region_map="town"/>
        <object arch="special_item"/>
      </interface>
    </part>
  </quest>
</interfaces>
""",
        )

    def test_loads_domain_qualified_definitions_and_references(self):
        self.create_valid_tree()

        catalog = load_catalog(self.root)

        self.assertFalse(catalog.has_errors, [item.format() for item in catalog.diagnostics])
        ids = {definition.content_id for definition in catalog.definitions}
        self.assertIn(ContentId("archetype", "base"), ids)
        self.assertIn(ContentId("artifact", "special_item"), ids)
        self.assertIn(ContentId("spell", "spell_minor_healing"), ids)
        self.assertIn(ContentId("skill", "skill_literacy"), ids)
        self.assertIn(ContentId("quest", "sample_quest"), ids)
        self.assertIn(
            ContentId("quest-part", "sample_quest::first_part::nested_part"), ids
        )
        self.assertNotIn(ContentId("archetype", "multipart_tail"), ids)

        map_arch_references = [
            reference
            for reference in catalog.references
            if reference.source == ContentId("map", "/start")
            and reference.field == "map arch"
            and reference.key == "base"
        ]
        self.assertEqual(1, len(map_arch_references))

    def test_reports_duplicate_missing_wrong_domain_and_cycles(self):
        catalog = ContentCatalog(self.root)
        first = catalog.location(self.root / "one", 1)
        second = catalog.location(self.root / "two", 2)
        catalog.add_definition("archetype", "shared", first)
        catalog.add_definition("archetype", "shared", second)
        catalog.add_definition("faction", "world", first)
        catalog.add_definition("faction", "town", second)
        catalog.add_reference("shared", ("region",), second, "region parent")
        catalog.add_reference("absent", ("map",), second, "teleport")
        catalog.check_cycles(
            "faction", {"world": ("town", first), "town": ("world", second)}
        )
        catalog.resolve_references()

        codes = {diagnostic.code for diagnostic in catalog.diagnostics}
        self.assertEqual(
            {
                "duplicate-id",
                "identity-cycle",
                "missing-reference",
                "wrong-domain-reference",
            },
            codes,
        )

    def test_rejects_quest_part_ids_that_would_be_silently_changed(self):
        self.create_valid_tree()
        quest = self.root / "maps/interfaces/quests/sample_quest/quest.xml"
        quest.write_text(
            """<interfaces><quest name="Sample"><part uid="Not Stable!"/>
</quest></interfaces>
""",
            encoding="utf-8",
        )

        catalog = load_catalog(self.root)

        self.assertIn(
            "invalid-quest-part-id",
            {diagnostic.code for diagnostic in catalog.diagnostics},
        )

    def test_serialized_catalog_is_deterministic(self):
        self.create_valid_tree()

        first = json.dumps(load_catalog(self.root).to_dict(), sort_keys=True)
        second = json.dumps(load_catalog(self.root).to_dict(), sort_keys=True)

        self.assertEqual(first, second)

    def test_display_name_changes_do_not_change_identity(self):
        self.create_valid_tree()
        first_ids = {
            definition.content_id for definition in load_catalog(self.root).definitions
        }
        quest = self.root / "maps/interfaces/quests/sample_quest/quest.xml"
        quest.write_text(
            quest.read_text(encoding="utf-8").replace(
                'name="Sample Quest"', 'name="A Translated Display Name"'
            ),
            encoding="utf-8",
        )

        second_ids = {
            definition.content_id for definition in load_catalog(self.root).definitions
        }

        self.assertEqual(first_ids, second_ids)


if __name__ == "__main__":
    unittest.main()
