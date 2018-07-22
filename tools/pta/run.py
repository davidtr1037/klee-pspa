import optparse
import os
import subprocess
import csv
from evaluate import PTALogParser


def run_klee(use_strong_updates, create_unique_as, pta_target, app_args):
    base_args = [
        "klee",
        "-libc=uclibc",
        "-posix-runtime",
        "-no-output",
        "-stat=0",
        "-run-static-pta=0",
        "-collect-pta-results",
        "-use-strong-updates=%d" % use_strong_updates,
        "-create-unique-as=%d" % create_unique_as,
        "-pta-target=%s" % pta_target,
    ]
    p = subprocess.Popen(base_args + app_args)
    p.communicate()
    return p.returncode


def parse_command_file(path):
    with open(path) as f:
        line = f.readline()
        return line.split()

def main():
    p = optparse.OptionParser()
    opts, args = p.parse_args()
    if len(args) != 5:
        print "Usage: <use_strong_updates> <create_unique_as> <command> <functions> <out_csv>"
        return 1

    use_strong_updates, create_unique_as, command_file, functions_file, csv_file = args
    
    app_args = parse_command_file(command_file)
    bc_dir = os.path.dirname(app_args[0])

    targets = []
    with open(functions_file) as f:
        for line in f.readlines():
            targets.append(line.strip())

    with open(csv_file, "w+") as out:
        writer = csv.writer(out)
        writer.writerow(["Function", "Unique", "Total"])
        for pta_target in targets:
            status = run_klee(int(use_strong_updates),
                              int(create_unique_as),
                              pta_target,
                              app_args)
            if status != 0:
                print "failed (exit code = %d)" % status
                return

            klee_out_dir = os.path.join(bc_dir, "klee-last")
            pta_log_file = os.path.join(klee_out_dir, "pta.log")

            parser = PTALogParser(pta_log_file)
            parser.parse()

            writer.writerow(
                [
                    pta_target,
                    parser.get_unique_results_count(),
                    parser.get_results_count(),
                ]
            )

if __name__ == '__main__':
    main()

