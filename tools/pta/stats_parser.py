import csv
from collections import namedtuple


Record = namedtuple(
    "Record",
    [
        "function",
        "line",
        "call_depth",
        "queries",
        "avg_size",
        "max_size",
        "mod_size",
        "ref_size",
    ]
)


class PTAStatsParser(object):

    def __init__(self):
        pass

    def parse(self, path):
        records = []
        with open(path, "r") as f:
            reader = csv.reader(f)
            for index, row in enumerate(reader):
                if index != 0:
                    f, line, c, q, avg_size, max_size, mod_size, ref_size = row
                    records.append(
                        Record(
                            f,
                            line,
                            c,
                            q,
                            float(avg_size),
                            float(max_size),
                            float(mod_size),
                            float(ref_size)
                        )
                    )
        return records

