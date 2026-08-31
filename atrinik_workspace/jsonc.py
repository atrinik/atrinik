"""Small JSONC compatibility helpers for configuration files."""

from __future__ import annotations

import json
from typing import Any


def _strip_comments(text: str) -> str:
    """Remove JSONC comments without changing string contents or line numbers."""
    result: list[str] = []
    in_string = False
    escaped = False
    line_comment = False
    block_comment = False
    index = 0

    while index < len(text):
        character = text[index]
        next_character = text[index + 1] if index + 1 < len(text) else ""

        if line_comment:
            if character in "\r\n":
                line_comment = False
                result.append(character)
            else:
                result.append(" ")
        elif block_comment:
            if character == "*" and next_character == "/":
                block_comment = False
                result.extend((" ", " "))
                index += 1
            elif character in "\r\n":
                result.append(character)
            else:
                result.append(" ")
        elif in_string:
            result.append(character)
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                in_string = False
        elif character == '"':
            in_string = True
            result.append(character)
        elif character == "/" and next_character == "/":
            line_comment = True
            result.extend((" ", " "))
            index += 1
        elif character == "/" and next_character == "*":
            block_comment = True
            result.extend((" ", " "))
            index += 1
        else:
            result.append(character)

        index += 1

    if block_comment:
        raise json.JSONDecodeError("unterminated JSONC block comment", text, len(text))

    return "".join(result)


def loads(text: str, **kwargs: Any) -> Any:
    """Parse JSON or JSONC using the same decoder options as ``json.loads``."""
    return json.loads(_strip_comments(text), **kwargs)
