import optparse
import os
import csv
from evaluate import PTALogParser
from run import run_klee, parse_command_file, get_targets

PTA_ARGS = [
    "-run-static-pta=0",
    "-collect-pta-results=1",
]


def main():
    p = optparse.OptionParser()
    opts, args = p.parse_args()
    if len(args) != 5:
        print "Usage: <use_strong_updates> <create_unique_as> <command> <functions> <out_csv>"
        return 1

    use_strong_updates, create_unique_as, command_file, functions_file, csv_file = args
    
    # get application parameters
    app_args = parse_command_file(command_file)
    # get bitcode file path
    bc_dir = os.path.dirname(app_args[0])
    # get function targets
    targets = get_targets(functions_file)

    with open(csv_file, "w+") as out:
        writer = csv.writer(out)

        # write header
        writer.writerow(["Function", "Unique", "Total"])
        out.flush()

        for pta_target in targets:
            status = run_klee(int(use_strong_updates),
                              int(create_unique_as),
                              pta_target,
                              PTA_ARGS,
                              app_args)
            if status != 0:
                print "failed (exit code = %d)" % status
                return

            klee_out_dir = os.path.join(bc_dir, "klee-last")
            pta_log_file = os.path.join(klee_out_dir, "pta.log")

            parser = PTALogParser(pta_log_file)
            parser.parse()

            # add row
            writer.writerow(
                [
                    pta_target,
                    parser.get_unique_results_count(),
                    parser.get_results_count(),
                ]
            )
            out.flush()

if __name__ == '__main__':
    main()

