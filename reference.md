# Reference
## Terminology
* `Args Struct`: The struct is given as the template parameter of `clipp::Parser::parse`, derived from `clipp::ArgsBase`.
* `Argument`: An element of the `argv` array.
* `Flag`: An argument beginning with `-`. Can be anywhere in the `argv` array.
* `Positional`: The meaning of a positional argument is determined by it's position in the `argv` array.

## `clipp::ArgsBase`
NOTE: This class is not copyable, because it keeps references to the variables you passed to `flag` and `positional`. Consequently the class you derive from it is also not copyable.

The built-in supported value types are `int64_t`, `double` and `string`.

### `Flag<T>& flag<T>(T&, std::string name, char shortOpt = 0)`
The behaviour of this flag is dependent on the template parameter `T` (the whole point of this library):
* `T = bool`: Usage is `[--flag]`. If the flag is given, the value of the referenced variable will be set to `true`. You need to initialize it with `false` yourself. `choices` doesn't have any effect.
* `T = size_t`: Usage is `[--flag]`. The referenced variable is increased for each time the flag is encountered. You need to initialize it to something yourself (likely `0`). `choices` doesn't have any effect.
* `T = std::optional<U>`: Usage is `[--flag FLAG]`, `[--flag=FLAG]` or `[-fFLAG]`. The referenced optional is set to the given value if the flag is given. `U` may be any of the built-in value types.
* `T = std::vector<U>`: Usage is: `[--flag FLAG]` or `[--flag FLAG1 FLAG2 FLAG3]` (with `.num()`, see below). `U` may be any of the built-in value types.

See below for `Flag<T>`'s methods.

### `Positional<T>& positional(T&, std::string name)`
The behaviour of this argument is also dependent on `T`:

### `const std::vector<std::string>& remaining()`
Contains the remaining arguments, when parsing is stopped after encountering an argument with `.halt()` (see below) or if `Parser::errorOnExtraArgs` is set to `false` (see below).

### `virtual std::string description() const`
Override this method to specify the description of the help text. Returns an empty string by default.

### `virtual std::string epilog() const`
Override this method to specify the epilog of the help text. Returns an empty string by default.

### `virtual std::string usage() const`
Override this method to customize the usage string. Returns a generated usage string by default.

### `virtual size_t helpOffset() const`
By overriding this method you can specify at which column the help text of each option should be printed. I consider this a temporary solution. By default the offset is 35.

### `virtual std::string help() const`
Override this method to customize the help text. Returns a generated help text by default.

## `Flag<T>`
### `Flag<T>& help(std::string)`
Specify the help text of the flag.

### `Flag<T>& choices(std::vector<std::string>)`
If this is given every flag value will be checked against the given vector of strings and value validation will fail if the flag value is not any of the choices. This has no effect on `T = bool` or `T = size_t`.

### `Flag<T>& halt(bool = true)`
If given argument parsing is aborted immediately if the flag is encountered. This is useful for flags like `--version` or `--help` to suppress parsing errors e.g. for missing positional arguments. Any remaining arguments that need to be parsed are saved and can be retrieved with `const std::vector<std::string>& ArgsBase::remaining()`.

## `Flag<vector<U>>`
### `Flag<vector<U>>& num(size_t num)`
Use this to have a flag take a fixed number of values, e.g. `[--flag FLAG1 FLAG2]`. After successful parsing the referenced vector will either be empty or contain a multiple of `num` elements. This function will set `collect` to `false`. By default this is 1.

### `Flag<vector<U>>& collect(bool = true)`
If set to true, the flag may be specified multiple times and all values will be appended to the vector. If not given the last flag value will be the value of the referenced variable. By default this is `true`.

## `Positional<T>`
### `Positional<T>& help(std::string)`
Specify the help text of the positional argument.

### `Positional<T>& choices(std::vector<std::string>)`
See flag.

### `Positional<T>& halt(bool = true)`
See flag. Additionally it should be noted that for positional arguments this is most useful for subcommands. See [examples/subcommands.cpp](./examples/subcommands.cpp).

### `Positional<vector<U>>& optional(bool = true)`
If the positional is optional, it may be given 0 times. By default every positional argument has to be given at least once.

## `clipp::Parser`
### `clipp::Parser(std::string programName)`
The argument `programName` should contain the argument as it will show up in the usage strings. Likely you want to pass `argv[0]` here.

### `std::optional<ArgsType> parse<ArgsType>(int argc, char** argv)`
This function takes `argc` and `argv` as they are passed to your program's entrypoint `main` (including `argv[0]`).

`ArgsType` must be derived from `clipp::ArgsBase`.

If `exitOnError` is `true`, which it is by default, then this function will never return an empty optional, but exit with status code 1 instead, so you can call `.value()` on it's return value right away.

If `exitOnError` is `false` a parsing error will result in `std::nullopt` being returned.

### `std::optional<ArgsType> parse<ArgsType>(std::vector<std::string>)`
The vector passed to this function should **NOT** include `argv[0]`! Everything else is the same as the other overload.

### `void version(std::string)`
If this method is called, a `--version` flag will automatically be added and, if given, will result in the string passed to this function being printed and your program exiting with status code 0.

### `void addHelp(bool)`
This function controls, whether a `--help`/`-h` flag should be added, which will print the help text and exit with status code 0 if given. This is enabled by default, so you only need this function to disable it.

### `void exitOnError(bool)`
By default clipp will exit with status code 1, if a parsing error occurs. With this function you disable this behaviour. If turned off, `parse` will return a `std::nullopt` in the case of error.

### `void errorOnExtraArgs(bool)`
If this is set to false, clipp will not error when a positional argument is encountered that cannot be matched. The remaining arguments will be saved to `ArgsBase` and can be retrieved via the `remaining()` method. Internally this will `halt()` when a superfluous positional argument is encountered. This is useful for "wrapper" commands that forward arguments, like ssh for example.

### `output(std::shared_ptr<OutputBase>)`
Instead of writing to stdout/stderr, you may customize the output by passing a `std::shared_ptr` to an instance of a class derived from `clipp::OutputBase`, which has the pure virtual methods `void out(std::string_view)` for writing to the equivalent of `stdout`
and `void err(std::string_view)` for writing to the equivalent of `stderr`. See [test.cpp](test.cpp) for an example.

### `exit(std::function<void(int)>)`
By default "exiting" means calling `std::exit`, but with this function you may overwrite the function being called to exit the program. If the new function, in contrast to `std::exit` *does* return, a `std::nullopt` will be returned from `parse` in error cases and a non-empty optional if the parsing was `halt`-ed (such as for `--version` and `--help`).

## Custom Values
To parse custom values and add custom validation, you can specialize `clipp::Value<T>` with your type. For an example of this (an enum, even integers and existing files) see [examples/customtypes.cpp](./examples/customtypes.cpp).
