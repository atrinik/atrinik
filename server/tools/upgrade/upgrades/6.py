# Upgrade for fixing book msg contents.

def upgrade_func(arch):
    if arch["archname"] in ("book", "book_green", "book_red", "book_blue", "book_pink", "tome", "tome_black", "tome_blue", "tome_indigo", "tome_cyan", "card", "note", "letter", "diploma"):
        i = Upgrader.arch_get_attr_num(arch, "msg", None)

        if i != -1:
            arch["attrs"][i][1] = arch["attrs"][i][1].replace("\">", "\">\n")

    return arch

upgrader = Upgrader.ObjectUpgrader(files, upgrade_func)
upgrader.upgrade()
