from os import listdir, popen
import re

TESTDIR = "tests/"
RAELPATH = "./rael"

def main():
    files = listdir(TESTDIR)
    i = 1
    while True:
        file = "test{}.rael".format(i)
        if file in files:
            files.remove(file)
            exp_file = file + ".exp"
            if not exp_file in files:
                print("no .exp file for '{}'".format(file))
            else:
                with open(TESTDIR + exp_file, "r") as f:
                    output = popen("{} --file {}".format(RAELPATH, TESTDIR + file)).read()
                    expected = f.read()
                    if output == expected:
                        print("Test {} passed!".format(file[4:file.find(".")]))
                    else:
                        print("Test {} failed!".format(file[4:file.find(".")]))
                        print("expected:")
                        print(expected)
                        print("output:")
                        print(output)
        else:
            break
        i += 1

if __name__ == "__main__":
    main()
