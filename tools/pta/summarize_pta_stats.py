import optparse
import os
import csv
from collections import namedtuple


Record = namedtuple(
    "Record",
    [
        "function",
        "line",
        "avg_size",
        "max_size",
        "mod_size",
        "ref_size",
    ]
)


class PTAStatsParser(object):

    def __init__(self):
        self.records = []

    def parse(self, path):
        with open(path, "r") as f:
            reader = csv.reader(f)
            for index, row in enumerate(reader):
                if index != 0:
                    f, line, _, _, avg_size, max_size, mod_size, ref_size = row
                    self.records.append(
                        Record(
                            f,
                            line,
                            float(avg_size),
                            float(max_size),
                            float(mod_size),
                            float(ref_size)
                        )
                    )

    def generate(self, out_path):
        joined_records = {}
        for r in self.records:
            if r.function not in joined_records:
                joined_records[r.function] = []
            joined_records[r.function].append(r)

        with open(out_path, "w+") as out:
            writer = csv.writer(out)
            writer.writerow(["Function", "Average", "Max", "Mod", "Ref"])
            for function, records in joined_records.iteritems():
                avg_size = sum([r.avg_size for r in records]) / len(records)
                max_size = sum([r.max_size for r in records]) / len(records)
                mod_size = sum([r.mod_size for r in records]) / len(records)
                ref_size = sum([r.ref_size for r in records]) / len(records)
                writer.writerow([function, avg_size, max_size, mod_size, ref_size])


def main():
    p = optparse.OptionParser()
    opts, args = p.parse_args()
    if len(args) != 2:
        print "Usage: <out_csv> <summarized_csv>"
        return 1

    out_csv, summarized_csv = args

    parser = PTAStatsParser()
    parser.parse(out_csv)
    parser.generate(summarized_csv)

if __name__ == '__main__':
    main()

