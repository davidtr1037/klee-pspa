import optparse
import os
from run import run_klee, parse_command_file, get_targets


BASE_ARGS = [
    "-use-forked-solver=0",
    "-no-output",
    "-use-strong-updates=1",
    "-create-unique-as=1",
    "-collect-pta-stats=1",
]

def main():
    p = optparse.OptionParser()
    opts, args = p.parse_args()
    if len(args) != 3:
        print "Usage: <command> <functions> <out_csv>"
        return 1

    command_file, functions_file, csv_file = args

    if os.path.exists(csv_file):
        os.remove(csv_file)
    
    # get application parameters
    app_args = parse_command_file(command_file)
    # get bitcode file path
    bc_dir = os.path.dirname(app_args[0])
    # get function targets
    targets = get_targets(functions_file)

    for pta_target in targets:
        args = []
        args += BASE_ARGS
        args += [
            "-pta-target=%s" % pta_target,
            "-pta-log=%s" % csv_file,
        ]
        args += app_args
        status = run_klee(args)
        if status != 0:
            print "failed (exit code = %d)" % status
            return

if __name__ == '__main__':
    main()
