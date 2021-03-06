from os import listdir
import subprocess

TESTDIR = "tests/"
RAELPATH = "build/rael"

def main():
    files = listdir(TESTDIR)
    failed = []
    i = 1
    while True:
        file = "test{}.rael".format(i)
        if file in files:
            files.remove(file)
            exp_file = file + ".exp"
            if not exp_file in files:
                print("no .exp file for '{}'".format(file))
            else:
                with open(TESTDIR + exp_file, "rb") as f:
                    try:
                        output = subprocess.check_output("{} {}".format(RAELPATH, TESTDIR + file),
                            shell=True, stderr=subprocess.STDOUT)
                    except subprocess.CalledProcessError as ex:
                        output = ex.output
                    expected = f.read()
                    test_number = file[4:file.find(".")]
                    if output == expected:
                        print("Test {} passed!".format(test_number))
                    else:
                        failed.append(test_number)
                        print("Test {} failed!".format(test_number))
                        print("expected:")
                        print(expected.decode())
                        print("output:")
                        print(output.decode())
        else:
            break
        i += 1

    if failed:
        print("Failed tests: {}".format(", ".join(failed)))

if __name__ == "__main__":
    main()
