import subprocess


def run_klee(use_strong_updates, create_unique_as, pta_target, pta_args, app_args):
    base_args = [
        "klee",
        "-libc=uclibc",
        "-posix-runtime",
        "-search=dfs",
        "-no-output",
        "-stat=0",
        "-use-strong-updates=%d" % use_strong_updates,
        "-create-unique-as=%d" % create_unique_as,
        "-pta-target=%s" % pta_target,
    ]
    args = base_args + pta_args + app_args
    p = subprocess.Popen(args)
    p.communicate()
    return p.returncode

def parse_command_file(path):
    with open(path) as f:
        line = f.readline()
        return line.split()

def get_targets(path):
    with open(path) as f:
        lines = [line.strip() for line in f.readlines()]
        targets = [line for line in lines if line != "" and not line.startswith("#")]
        return targets

