# Building Boost is a pain from CMake, use this instead 

import os
import sys
import subprocess
import argparse

script_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = os.path.realpath(os.path.join(script_dir,'..'))

def perror(s):
    print(f'ERROR:{s}')

def parse_args():
    parser = argparse.ArgumentParser(prog='setup-boost')
    parser.add_argument('--major', required=True)
    parser.add_argument('--minor', required=True)
    parser.add_argument('--patch', required=True)
    parser.add_argument('--url', required=True)
    parser.add_argument('--built-file', required=True)
    return parser.parse_args()

def boost_underscore_version(args):
    return f'boost_{args.major}_{args.minor}_{args.patch}'

def boost_tarball(args):
    return f'{boost_underscore_version(args)}.tar.gz'

def boost_dir(args):
    return f'boost/{boost_underscore_version(args)}'

def execute(cmd, cwd=None):
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,cwd=cwd)

    for line in process.stdout:
        print(str(line, 'utf-8'), flush=True, end='')
    
    _ = process.communicate()
    
    return True if process.returncode == 0 else False

def verify_file(path):
    return os.path.exists(path) and os.path.isfile(path)

def verify_dir(path):
    return os.path.exists(path) and os.path.isdir(path)

def download_boost(args):
    return execute(['wget', args.url])

def extract_tarball(tarball,destination):
    return execute(['tar','-xzvf',tarball,'-C',destination])

def touch_built(args):
    return execute(['touch',args.built_file])

def main():
    args = parse_args()

    if not verify_file(os.path.join(root_dir,args.built_file)):
        broot = os.path.join(root_dir,boost_dir(args))
        print(f'boost root:{broot}')

        if not verify_dir(broot):
            btarball = os.path.join(root_dir,boost_tarball(args))
            print(f'boost tarball:${btarball}')

            if not verify_file(btarball):
                if not download_boost(args):
                    perror('cannot download boost')
                    sys.exit(1)

                if not verify_file(btarball):
                    perror('boost tarball missing')
                    sys.exit(1)

            broot_root = os.path.join(root_dir,'boost')

            if not execute(['mkdir','-p',broot_root]):
                perror(f'cannot make directory {broot_root}')
                sys.exit(1)

            if not extract_tarball(btarball,broot_root):
                perror('cannot extract boost')
                sys.exit(1)

            if not verify_dir(broot):
                perror('boost directory missing')
                sys.exit(1)

        b2 = os.path.join(broot,'b2')
        print(f'b2:{b2}')

        if not verify_file(b2):
            bootstrap = os.path.join(broot,'bootstrap.sh')
            print(f'bootstrap:{bootstrap}')

            if not verify_file(bootstrap):
                perror('boost bootstrap.sh missing')
                sys.exit(1)

            if not execute([bootstrap],cwd=broot):
                perror('could not bootstrap boost')
                sys.exit(1)

            if not verify_file(b2):
                perror('b2 missing')
                sys.exit(1)

        if not execute([b2,'--with-context','--with-coroutine','--with-thread'],cwd=broot):
            perror('could not build boost')
            sys.exit(1)

        if not touch_built(args):
            perror(f'could not create {args.built_file}')
            sys.exit(1)

    print('boost built')
    sys.exit(0)

if __name__ == '__main__':
    main()
