import os
import subprocess
import sys

BUILD_DIR = '/llvm-assignment3/build/'
LLVM = '/usr/local/llvm10ra/'
TESTCASE_DIR = '/llvm-assignment3/testcase/'

def execute_cmd(cmd,out=True):
    if out:
        print('\033[1;34m', cmd, '\033[0m')
    process = subprocess.Popen(cmd,shell=True, stdout=subprocess.PIPE,stderr=subprocess.PIPE);
    process.wait()
    output = process.stdout.read().decode()
    errs = process.stderr.read().decode()
    output.replace('\\n','\n')
    errs.replace('\\n','\n')

    if out:
        if errs != '':
            print("ERROR!")
            print(errs)
        print(output)
    return output,errs


def build():
    os.chdir(BUILD_DIR)
    cmd = 'cmake -DLLVM_DIR={} -DCMAKE_CXX_FLAGS="-std=c++14" ..'.format(LLVM)
    execute_cmd(cmd)
    execute_cmd('make')

# target should be a string like 'test00'
def build_testcase(target,out=True):
    os.chdir(TESTCASE_DIR)
    cmd = 'clang -emit-llvm -g -c {0}.c -o {0}.bc'.format(target)
    execute_cmd(cmd,out)

def run_testcase(target,out=True):
    os.chdir(BUILD_DIR)
    cmd = './assignment3 {}.bc'.format(TESTCASE_DIR + target)
    return execute_cmd(cmd,out)


def check(target,out=True):
    build_testcase(target,False)
    output,res = run_testcase(target,False)
    print(output)
    print('-------OUTPUT-------')
    print(res)
    print('-------ANSWER-------')
    with open(TESTCASE_DIR + target + '.c','r') as f:
        for line in f.readlines():
            line = line.strip()
            if line[:2] == '//':
                print(line.replace('/','').strip())

def USAGE():
    print('USAGE:')
    print('\tBUILD:\tpython test.py build')
    print('\tTEST:\tpython test.py test00')

if __name__ == '__main__':
    if len(sys.argv) == 2:
        if sys.argv[1] == 'build':
            build()
        elif len(sys.argv[1]) == 6:
            check(sys.argv[1])
        else:
            USAGE()
    else:
        USAGE()

