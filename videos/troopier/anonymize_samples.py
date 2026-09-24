"""Swaps the machine, user and workload names in copies of the sample traces.

The tutorial footage shows host names, home folders and workload names stored
in the sample databases. record_tutorials.ps1 runs this on the copies it opens,
so neutral names appear on screen; the files in sample/ are left untouched.

Usage: python anonymize_samples.py <database> [<database> ...]
"""

import sqlite3
import sys

REPLACEMENTS = [
    ("gliff-dev-lnx", "demo-node"),
    ("asrock-1w300-h2-1", "demo-node"),
    ("/home/gliff/", "/home/user/"),
    ("/home/drchen/", "/home/user/"),
    ("drchen_pi", "monte_carlo_pi"),
]


def anonymize(path):
    db = sqlite3.connect(path)
    tables = [row[0] for row in db.execute("select name from sqlite_master where type = 'table'")]
    changed = 0
    for table in tables:
        for column in [row[1] for row in db.execute(f'pragma table_info("{table}")')]:
            for old, new in REPLACEMENTS:
                cursor = db.execute(
                    f'update "{table}" set "{column}" = replace("{column}", ?, ?) '
                    f"where typeof(\"{column}\") = 'text' and instr(\"{column}\", ?) > 0",
                    (old, new, old),
                )
                changed += cursor.rowcount
    db.commit()
    db.close()
    print(f"{path}: {changed} values anonymized")


if __name__ == "__main__":
    for database in sys.argv[1:]:
        anonymize(database)
