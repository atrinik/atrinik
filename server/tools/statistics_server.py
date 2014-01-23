## @file
## Implements the statistics server. Requires Python 3.0 or later.

import socket, shelve, sys, time
from datetime import datetime

## Get 64-bit integer from data.
## @param data Data.
## @return The integer.
def SockList_GetInt64(data):
    return ((data[0] << 56) + (data[1] << 48) + (data[2] << 40) + (data[3] << 32) + (data[4] << 24) + (data[5] << 16) + (data[6] << 8) + data[7])

## Handle a statistic update.
## @param database Database to update.
## @param type Type of the statistic to update.
## @param name Player's name.
## @param i Integer data.
## @param buf String data.
def handle_stat(database, type, name, i, buf):
    # Not yet in the database, initialize the player.
    if not name in database:
        database[name] = {
            "stats": {},
            "kills": {},
        }

    # Particular monster has been killed.
    if type == "kills":
        if not buf in database[name]["kills"]:
            database[name]["kills"][buf] = 0

        database[name]["kills"][buf] += i
    # Regular integer-based statistic.
    else:
        if not type in database[name]["stats"]:
            database[name]["stats"][type] = 0

        database[name]["stats"][type] += i

## Open the monthly database.
def open_monthly_db():
    global db_monthly, last_month

    db_monthly = shelve.open("../data/statistics_monthly_{}.db".format(datetime.now().strftime("%Y_%m")), writeback = True)
    last_month = datetime.now().month

## Check if it's time to create a new monthly database.
def check_monthly_db():
    if datetime.now().month != last_month:
        db_monthly.close()
        open_monthly_db()

## The main loop.
def main():
    last_time = time.time()

    while True:
        data, address = s.recvfrom(65565)
        check_monthly_db()

        # Get the data.
        type = data[:data.find(b"\0")].decode()
        data = data[len(type) + 1:]

        name = data[:data.find(b"\0")].decode()
        data = data[len(name) + 1:]

        i = SockList_GetInt64(data)
        data = data[8:]

        buf = data[:data.find(b"\0")].decode()
        data = data[len(buf) + 1:]

        # Store the data.
        handle_stat(db, type, name, i, buf)
        handle_stat(db_monthly, type, name, i, buf)

        # Flush cache every hour.
        if time.time() >= last_time + 60 * 60:
            db.sync()
            db_monthly.sync()
            last_time = time.time()

# Need at least Python 3.0.
if sys.version_info < (3, 0):
    raise StandardError("Must use Python 3.0 or better.")

try:
    # Create the server's socket.
    port = 13324
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("", port))

    # Open the databases.
    db = shelve.open("../data/statistics.db", writeback = True)
    open_monthly_db()
    main()
finally:
# Cleanup.
    s.close()
    db.close()
    db_monthly.close()
