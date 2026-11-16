# SPDX-License-Identifier: GPL-2.0-or-later
"""Every relative Markdown link in the repository must resolve."""
import re
import unittest
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
LINK_RE = re.compile(r"\]\(([^)]+)\)")


class LinkTests(unittest.TestCase):
    def test_relative_links_resolve(self):
        broken = []
        for md in PROJECT.rglob("*.md"):
            if "runs" in md.parts:
                continue
            text = md.read_text(encoding="utf-8")
            for match in LINK_RE.finditer(text):
                target = match.group(1).split("#")[0]
                if not target or target.startswith(
                        ("http://", "https://", "mailto:")):
                    continue
                if not (md.parent / target).resolve().exists():
                    broken.append(f"{md.relative_to(PROJECT)} -> {target}")
        self.assertEqual(broken, [], "\n".join(broken))


if __name__ == "__main__":
    unittest.main()
