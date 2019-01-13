import optparse
import csv
from summarize_pta_stats import Record


class SummarizedStatsParser(object):

    def __init__(self):
        self.records = {}

    def parse(self, path):
        with open(path, "r") as f:
            reader = csv.reader(f)
            for index, row in enumerate(reader):
                if index != 0:
                    f, avg_size, max_size, mod_size, ref_size = row
                    self.records[f] = Record(
                        f,
                        0,
                        float(avg_size),
                        float(max_size),
                        float(mod_size),
                        float(ref_size)
                    )


def get_ratios(records1, records2, functions, handler):
    ratios = []
    for f in functions:
        r1 = records1[f]
        r2 = records2[f]
        ratios.append(handler(r1, r2))
    return ratios

def compare(records1, records2):
    # get common functions
    functions = get_common_functions(records1, records2)

    def avg_handler(r1, r2):
        return (r1.avg_size + 1) / (r2.avg_size + 1)
    avg_ratios = get_ratios(records1, records2, functions, avg_handler)

    def max_handler(r1, r2):
        return (r1.max_size + 1) / (r2.max_size + 1)
    max_ratios = get_ratios(records1, records2, functions, max_handler)

    def mod_handler(r1, r2):
        return (r1.mod_size + 1) / (r2.mod_size + 1)
    mod_ratios = get_ratios(records1, records2, functions, mod_handler)

    def ref_handler(r1, r2):
        return (r1.ref_size + 1) / (r2.ref_size + 1)
    ref_ratios = get_ratios(records1, records2, functions, ref_handler)

    print sum([x for x in avg_ratios]) / len(avg_ratios)
    print sum([x for x in max_ratios]) / len(max_ratios)
    print sum([x for x in mod_ratios]) / len(mod_ratios)
    print sum([x for x in ref_ratios]) / len(ref_ratios)

def get_common_functions(records1, records2):
    return set(records1.keys()).intersection(records2.keys()) 

def get_records(path):
    parser = SummarizedStatsParser()
    parser.parse(path)
    return parser.records

def main():
    p = optparse.OptionParser()
    opts, args = p.parse_args()
    if len(args) != 2:
        print "Usage: <file1> <file2>"
        return 1

    path1, path2 = args

    records1 = get_records(path1)
    records2 = get_records(path2)
    compare(records1, records2)

if __name__ == '__main__':
    main()

