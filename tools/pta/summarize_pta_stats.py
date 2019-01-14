import optparse
import csv
from stats_parser import PTAStatsParser


def generate(records, benchmark, mode, out_path):
    joined_records = {}
    for r in records:
        if r.function not in joined_records:
            joined_records[r.function] = []
        joined_records[r.function].append(r)

    with open(out_path, "w+") as out:
        writer = csv.writer(out)
        writer.writerow(["Benchmark", "Mode", "Function", "Average", "Max", "Mod", "Ref"])
        for function, records in joined_records.iteritems():
            avg_size = sum([r.avg_size for r in records]) / len(records)
            max_size = sum([r.max_size for r in records]) / len(records)
            mod_size = sum([r.mod_size for r in records]) / len(records)
            ref_size = sum([r.ref_size for r in records]) / len(records)
            writer.writerow([benchmark, mode, function, avg_size, max_size, mod_size, ref_size])


def main():
    p = optparse.OptionParser()
    opts, args = p.parse_args()
    if len(args) != 4:
        print "Usage: <input_csv> <output_csv> <benchmark> <mode>"
        return 1

    input_csv, output_csv, benchmark, mode = args

    parser = PTAStatsParser()
    records = parser.parse(input_csv)
    generate(records, benchmark, mode, output_csv)

if __name__ == '__main__':
    main()

