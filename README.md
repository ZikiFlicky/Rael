# Rael
Rael is an interpreted programming language written in C.

The language is made for people who prefer not to name their variables.

Rael was started, but not finished in [LangJam](https://github.com/langjam/jam0001).

## Dependencies
* Git - to clone this repository
* GNU Make - to build the project
* GCC - any C99 compiler should work, though
* Python 2.7 or higher - for running tests

Note: this was tested only on Linux (Ubuntu 20.04).

The language doesn't use Linux specific things, so it should work on other platforms, too.

For build problems and questions, please open an issue.

## Build
To build, run `make`.

To run the tests, run `python runtests.py`

## Usage
To run a file, `./rael --file filename.rael`

## Examples
### Fibonacci:
```rael
:n 22
:1 1
:2 1
:3 0
loop :n {
    log :2
    :3 :1 + :2
    :1 :2
    :2 :3
    :n :n - 1 %% decrease :n by 1
}
```