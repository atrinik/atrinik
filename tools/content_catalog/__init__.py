"""Atrinik authored-content identity catalog."""

from .loaders import load_catalog
from .model import ContentCatalog, ContentId, Definition, Diagnostic, Reference, SourceLocation

__all__ = (
    "ContentCatalog",
    "ContentId",
    "Definition",
    "Diagnostic",
    "Reference",
    "SourceLocation",
    "load_catalog",
)
