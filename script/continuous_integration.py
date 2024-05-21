import argparse 
import subprocess
import sys 
import os

CALLING_PATH = os.getcwd()
SCRIPT_PATH = os.path.dirname(os.path.realpath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_PATH)
CONTINUOUS_INTEGRATION_LOG = os.path.join(CALLING_PATH, 'continuous_integration_output.txt')

# open and truncate file
output = open(CONTINUOUS_INTEGRATION_LOG,'w');

def test_strings():
    class test_strings_class:
        def separator(self):
            return '[----------] '

        def run(self):
            return '[ RUN      ] '

        def ok(self):
            return '[       OK ] '

        def stderr(self):
            return '[  STDERR  ] '

        def failed(self):
            return '[  FAILED  ] ' 

    return test_strings_class()

def log(s):
    global output 
    if isinstance(s, str):
        sys.stdout.write(s)
        sys.stdout.flush()
        output.write(s)

def execute(cmd, cwd=None):
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,cwd=cwd)

    for line in process.stdout:
        print(str(line, 'utf-8'), flush=True, end='')
    
    stdout, stderr = process.communicate()
    
    return process.returncode, stderr

# print functions are intended to blend in with googletest print output
command_separator_newline_required = False 

def print_command_separator(string):
    global command_separator_newline_required
    if command_separator_newline_required == False:
        command_separator_newline_required = True 
    else:
        log('\n')
    log(test_strings().separator()+string+'\n')

def print_command_prepend(string):
    log(test_strings().run()+string+'\n')

def print_command_result(code, string, err=''):
    if code == 0:
        log(test_strings().ok()+string+'\n')
    else:
        if err != '':
            log(test_strings().separator()+'\n')
            log(test_strings().stderr()+'\n')
            log(err)
        log(test_strings().failed()+string+'\n')
        log('    ErrorCode: '+str(code)+'\n');

def validate(string):
    print_command_prepend(string)
    code, err = execute(string.split())
    if code == 0:
        print_command_result(code, string)
        return True 
    else:
        print_command_result(code, string, err)
        return False 

def validateCommands(commands):
    success = True
    for com in commands:
        if validate(com) != True:
            success = False 
            break
    return success

def buildAndExecuteTests():
    success = True
    os.chdir(PROJECT_ROOT)
   
    # erase previous logfile by opening with 'w' 
    print("CONTINUOUS_INTEGRATION_LOG["+CONTINUOUS_INTEGRATION_LOG+"]")
    output = open(CONTINUOUS_INTEGRATION_LOG,'w');
    output.close()

    print_command_separator('build and run unit test code')

    def verify_success(success):
        if success == False:
            log('Integration Validation FAILURE')
            sys.exit(1)

    # test gcc compiler
    os.environ['CC'] = '/usr/bin/gcc'
    os.environ['CXX'] = '/usr/bin/g++'
    commands = [ 'cmake ' + PROJECT_ROOT, 'make mce_ut', './tst/mce_ut' ]
    success = validateCommands(commands)
    verify_success(success)

    commands = [ 'cmake ' + PROJECT_ROOT, 'make mce_ut_minimal', './tst/mce_ut_minimal' ]
    success = validateCommands(commands)
    verify_success(success)

    print_command_separator('build and run example code')
    commands = [ 'make clean', 'make mce_ex' ]
    success = validateCommands(commands)
    verify_success(success)

    if success == True:
        examples = [ ]
        ex_path = os.path.join(PROJECT_ROOT, "ex")
        ex_files = os.listdir(ex_path)

        for f in ex_files:
            if 'example_' in f and 'human_only' not in f:
                ex = os.path.join(ex_path,f)
                examples.append(ex)

        examples.sort()

        # run example code 
        commands = examples
        success = validateCommands(commands)
    verify_success(success)

    # test clang compiler
    os.environ['CC'] = '/usr/bin/clang'
    os.environ['CXX'] = '/usr/bin/clang++'
    commands = [ 'cmake ' + PROJECT_ROOT, 'make mce_ut', './tst/mce_ut' ]
    success = validateCommands(commands)
    verify_success(success)

    commands = [ 'cmake ' + PROJECT_ROOT, 'make mce_ut_minimal', './tst/mce_ut_minimal' ]
    success = validateCommands(commands)
    verify_success(success)

    print_command_separator('build and run example code')
    commands = [ 'make clean', 'make mce_ex' ]
    success = validateCommands(commands)
    verify_success(success)

    if success == True:
        examples = [ ]
        ex_path = os.path.join(PROJECT_ROOT, "ex")
        ex_files = os.listdir(ex_path)

        for f in ex_files:
            if 'example_' in f and 'human_only' not in f:
                ex = os.path.join(ex_path,f)
                examples.append(ex)

        examples.sort()

        # run example code 
        commands = examples
        success = validateCommands(commands)

    verify_success(success)
        
    os.chdir(CALLING_PATH)

    if success == True:
        log('Integration Validation Success')
        sys.exit(0)
    else:
        log('Integration Validation FAILURE')
        sys.exit(1)

    
if __name__ == "__main__":
    buildAndExecuteTests()
    output.close()
