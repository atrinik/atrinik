'''
Utility functions.
'''

from html.parser import HTMLParser
from html import entities

class HTMLTextExtractor(HTMLParser):
    def __init__(self):
        HTMLParser.__init__(self)
        self.result = [ ]

    def handle_data(self, d):
        self.result.append(d)

    def handle_charref(self, number):
        codepoint = int(number[1:], 16) if number[0] in ('x', 'X') else int(number)
        self.result.append(chr(codepoint))

    def handle_entityref(self, name):
        codepoint = entities.name2codepoint[name]
        self.result.append(chr(codepoint))

    def get_text(self):
        return "".join(self.result)

def html2text(html):
    s = HTMLTextExtractor()
    s.feed(html)
    return s.get_text()
