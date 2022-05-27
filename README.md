# clipp
A command line argument parser library for C++17

## Another one? 🤨
There are already countless argument parsing libraries, but I always found it annoying and redundant to take their output, validate it and plumb it into some struct that contains the arguments. Those structs also looked the same every time and I thought that the fields of that struct essentially already encode the CLI interface description. This is an attempt to build an CLI argument parser library that uses the types of the target variables to derive what kind of argument should be passed.

I also used the opportunity to build a parser that behaves how I think such parsers should behave, which they often don't exactly.

## Example
```cpp
// from examples/intro.cpp
#include <iostream>

#include "clipp.hpp"

struct Args : public clipp::ArgsBase {
    bool dryRun = false;
    size_t verbose = 0;
    std::optional<int64_t> num;
    std::optional<std::string> output;
    std::vector<std::string> input;

    void args()
    {
        flag(dryRun, "dry-run", 'd').help("Only log potential filesystem changes");
        flag(verbose, "verbose", 'v').help("Output more debugging information");
        flag(num, "num").help("The number of things to do");
        flag(output, "output", 'o').help("The output file");
        positional(input, "input");
    }
};

int main(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const Args args = parser.parse<Args>(argc, argv).value();
    std::cout << "dry-run: " << args.dryRun;
    std::cout << ", verbose: " << args.verbose;
    std::cout << ", num: " << args.num.value_or(42);
    std::cout << ", output: " << (args.output ? *args.output : std::string("<none>"));
    std::cout << ", input: " << clipp::detail::join(args.input, ", ") << std::endl;
    return 0;
}
```

Example Output:

```shell
$ build/intro
Missing argument 'input'
Usage: build/intro [--help] [--dry-run] [--verbose] [--num NUM] [--output OUTPUT] input [input...]

$ build/intro file1 file2 file3 -ooutfile -vvvvv --dry-run --num=5
dry-run: 1, verbose: 5, num: 5, output: outfile, input: file1, file2, file3

$ build/intro --help
Usage: build/intro [--help] [--dry-run] [--verbose] [--num NUM] [--output OUTPUT] input [input...]

Positional Arguments:
  input

Optional Arguments:
  -h, --help                         Show this help message and exit
  -d, --dry-run                      Only log potential filesystem changes
  -v, --verbose                      Output more debugging information
      --num NUM                      The number of things to do
  -o, --output OUTPUT                The output file
```

## Overview / Features
* `--flag`: `flag(bool&)` (saves true if given) or `flag(size_t&)` (counts occurences).
* `-rf`: You can "stack" short options.
* `-fg VALUE`: All except the last option must be flags without arguments.
* `--flag VALUE` or `--flag=VALUE` or `-fVALUE`: `flag(optional<T>&)`. `--flag=VALUE` and `-fVALUE` only work for flags with exactly one argument.
* `--flag VALUE1 VALUE2 VALUE3`: `flag(vector<T>&).num(size_t)` - You can have flags that take multiple values (though it must be a fixed number of arguments).
* `-abf VALUE1 VALUE2 VALUE3`: Multiple arguments also work for the last short option in a stack.
* `--flag VALUE1 --flag VALUE2`: `flag(vector<T>&)` - If a flag is given multiple times, all the values are collected. If something other than a vector is passed to `flag()` only the last value is saved.
* `--flag VALUE1 VALUE2 --flag VALUE3 VALUE4`: `flag(vector<T>&).num(size_t).collect()` - You can also collect all values for multiple occurences of a flag with multiple arguments.
* Use `--` to all arguments after it be interpreted as positional arguments.
* `bar`: `positional(T&)` - Mandatory positional argument.
* `[bar]`: `positional(optional<T>&)` or `positional(T&).optional()` - Optional positional arguments.
* `bar [bar...]`: `positional(vector<T>&)` - Mandatory positional argument that may have many values (`[1, inf)`).
* `[bar...]`: `positional(vector<T>&).optional()` - Optional positional argument that may have many values (`[0, inf]`).
* `source [source...] dest`: This will match all except the last positional argument to `source` and the last to `dest` (and error if there is only 0 or 1). clipp will try to match the positional arguments such that parsing does not fail, while favoring the earlier arguments. E.g. passing `{"1", "2", "3", "4", "5", "6"}` to `a [a....] b [b...] c [c...]` will result in `a` having elements 1 through 4 and `b` and `c` having 5 and 6 respectively. `--` can be used additional times to move on to the next positional argument. See [test.cpp](./test.cpp) (`PosDelimArgs`).
* `.help(string)` - Can be used for every argument to specify a help text.
* `.choices(vector<string>)` - Can be used for every argument to specify the valid values. It takes strings instead of `T`s, so it is easier (possible) to have the help text and error message contain the possible values.
* `Arg.halt()` - Can be used for every argument to make parsing stop as soon as this argument is encountered. For flags this is useful for e.g. `--version` or `--help` so parsing will not fail because e.g. of missing positional arguments. For positional arguments this is useful for subcommands.
* Subcommands can be handled nicely without any special functionality. See [examples/subcommands.cpp](./examples/subcommands.cpp)
* `Parser::version(string)`: to specify a version string and automatically add a `--version` flag which will halt, print the given string and exit with status code 0 if encountered.

The type `T` mentioned above a few times can be either `std::string`, `int64_t` or `double` by default. Additional types can be added by specializing `clipp::Value`. See [examples/customtypes.cpp](./examples/customtypes.cpp) for an example of an enum, an even integer and a path to an existing file.

Also have a look at the [reference](./reference.md) to see all the other things you can do.

Though it is *very* messy [test.cpp](./test.cpp) tests mostly everything clipp can do, so "how do I do X?" might have an answer in this file.

## Notes / Caveats / Limitations
* If you specify a flag with a digit as a short option, all negative number arguments will be recognized as flags. I *could* only recognize the registered short options as flags, but I find it very weird to have a program understand e.g. `-1` as a flag and `-2` as a regular argument (a negative number). I also try to not have the meaning of an argument change by adding another flag that is not part of that argument (yet).
* You can't specify a flag value for the last flag in a stack in the same argument, e.g. `-doVALUE` (`-d` is a bool flag, `VALUE` is the argument to `-o`), because as with the previous point, I do not like that adding another flag can change the meaning of a string, i.e. if I added `-V` to the interface, suddenly `ALUE` would be the value of `-V`.
* Because `ArgsBase` keeps references to the variables that were passed to `flag` and `positional`, it is not copyable and consequently your derived `Args` class isn't either.

## Building / Integration
Usually I am not a fan of header-only libraries and I much prefer a single header and a single source file, but considering an argument parsing library like this one is almost always included in only a single translation unit, I think all the inline functions are worth the added convenience.

Long story short, just download [clipp.hpp](./clipp.hpp) (or add this repo as a submodule or use CMake's `FetchContent` or whatever) and include [clipp.hpp](./clipp.hpp) in your code.

## To Do
* More Examples. I know I want more, but I am not sure what exactly I should add. Suggestions welcome.
* Test for error message in tests where parsing fails
* Add tests for help and usage
* Add tests for subcommands
* Be more systematic about testing. Ideally test every branch intentionally and not just try out random stuff.
* Add a way to configure the name of a flag value in the help/usage string ("metavar" in Python's argparse)
* Hex/Octal/Binary Numbers (though if you need them, you can add your own Value specialization)
