# Rael
Rael is an interpreted programming language written in C.

The language is made for people who prefer not to name their variables.

Rael was started, but not finished in [LangJam](https://github.com/langjam/jam0001).

The name Rael has no actual meaning, if you were wondering ;)

## Dependencies
* Git - to clone this repository (you can also just download it manually if you want)
* GNU Make - to build the project on unix systems
* GCC or Clang - any decent C99 compiler should work, though
* Python 2.7 or higher - for running tests (`runtests.py`)

The default compiler is GCC, but it can be changed in the makefile's configuration
at the top to whatever you like.

Note: the language was only tested on Linux (Ubuntu 20.04, to be exact).

The language's code currently uses some Posix libraries and functions and doesn't currently support OSes without Posix support.

In the future I may or may not port it to Windows.

For build problems and questions regarding the language, please open an issue on this GitHub page.

## Build
To build, run `make`.

To run the tests, run `python runtests.py`

## Usage
To run a file, `build/rael filename.rael`

## Examples
Examples can be found in the examples directory.

They can be run like this: `build/rael examples/beer.rael`
