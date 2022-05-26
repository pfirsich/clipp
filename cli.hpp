#pragma once

#include <cassert>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef CLI_DEBUG
#include <sstream>
#endif

namespace cli {
struct ArgBase {
    ArgBase(std::string name, char shortOpt = 0)
        : name_(std::move(name))
        , shortOpt_(shortOpt)
    {
    }

    virtual ~ArgBase()
    {
    }

    const std::string& name() const
    {
        return name_;
    }

    char shortOpt() const
    {
        return shortOpt_;
    }

    const std::vector<std::string>& choices() const
    {
        return choices_;
    }

    const std::string& help() const
    {
        return help_;
    }

    size_t min() const
    {
        return min_;
    }

    size_t max() const
    {
        return max_;
    }

    size_t size() const
    {
        return size_;
    }

    virtual bool parse(std::string_view) = 0;

protected:
    std::string name_;
    char shortOpt_ = 0;
    size_t min_ = 0;
    size_t max_ = 0;
    std::string help_;
    std::vector<std::string> choices_;
    size_t size_ = 0;
};

template <typename Derived>
struct ArgBuilderMixin : public ArgBase {
    ArgBuilderMixin(std::string name, char shortOpt = 0)
        : ArgBase(std::move(name), shortOpt)
    {
    }

    // Take a list of strings, because with custom types, we might not be able to
    // give a nice error message or conversion might be lossy somehow.
    Derived& choices(std::vector<std::string> c)
    {
        choices_ = std::move(c);
        return derived();
    }

    Derived& help(std::string help)
    {
        help_ = std::move(help);
        return derived();
    }

    // only relevant for flags
    Derived& required()
    {
        if (min_ == 0)
            return min(1);
        return derived();
    }

    Derived& optional()
    {
        return min(0);
    }

    Derived& min(size_t n)
    {
        min_ = n;
        return derived();
    }

    Derived& max(size_t n)
    {
        max_ = n;
        return derived();
    }

    // For flags this does something like "--foo 1 2 3"
    Derived& num(size_t n)
    {
        return min(n).max(n);
    }

    Derived& derived()
    {
        return *static_cast<Derived*>(this);
    }
};

template <typename T>
struct Flag;

template <>
struct Flag<bool> : public ArgBuilderMixin<Flag<bool>> {
    Flag(bool& value, std::string name, char shortOpt = 0)
        : ArgBuilderMixin(std::move(name), shortOpt)
        , value_(value)
    {
    }

    bool parse(std::string_view str) override
    {
        assert(str.empty());
        value_ = true;
        return true;
    }

private:
    bool& value_;
};

template <>
struct Flag<size_t> : public ArgBuilderMixin<Flag<size_t>> {
    Flag(size_t& value, std::string name, char shortOpt = 0)
        : ArgBuilderMixin(std::move(name), shortOpt)
        , value_(value)
    {
    }

    bool parse(std::string_view str) override
    {
        assert(str.empty());
        value_++;
        return true;
    }

private:
    size_t& value_;
};

template <>
struct Flag<std::optional<std::string>> : public ArgBuilderMixin<Flag<std::optional<std::string>>> {
    Flag(std::optional<std::string>& value, std::string name, char shortOpt = 0)
        : ArgBuilderMixin(std::move(name), shortOpt)
        , value_(value)
    {
        num(1);
    }

    bool parse(std::string_view str) override
    {
        value_ = std::string(str);
        return true;
    }

private:
    std::optional<std::string>& value_;
};

template <typename T>
struct Param;

template <>
struct Param<std::string> : public ArgBuilderMixin<Param<std::string>> {
    Param(std::string& value, std::string name)
        : ArgBuilderMixin(std::move(name))
        , value_(value)
    {
        num(1);
    }

    bool parse(std::string_view str) override
    {
        value_ = std::string(str);
        size_++;
        return true;
    }

private:
    std::string& value_;
};

struct ArgsBase {
    // A simple flag like. "--foo" => v = true
    auto& flag(bool& v, const std::string& name, char shortOpt = 0)
    {
        // How can I do this nicely without new?
        auto arg = new Flag<bool>(v, name, shortOpt);
        flags_.emplace_back(arg);
        return *arg;
    }

    // Counts flags. "-vvvv" => v = 4
    auto& flag(size_t& v, const std::string& name, char shortOpt = 0)
    {
        auto arg = new Flag<size_t>(v, name, shortOpt);
        flags_.emplace_back(arg);
        return *arg;
    }

    /*// An optional flag. "--foo 42" => v = 42
    Arg &flag(std::optional<int64_t> &v, const std::string & name,
              char shortOpt = 0);

    // An optional flag. "--foo 42" => v = 42
    Arg &flag(std::optional<uint64_t> &v, const std::string & name,
              char shortOpt = 0);

    // An optional flag. "--foo 42.00" => v = 42.0
    Arg &flag(std::optional<double> &v, const std::string & name, char shortOpt =
    0);*/

    // An optional flag. "--foo cool" => v = cool
    auto& flag(std::optional<std::string>& v, const std::string& name, char shortOpt = 0)
    {
        auto arg = new Flag<std::optional<std::string>>(v, name, shortOpt);
        flags_.emplace_back(arg);
        return *arg;
    }

    /*// Specify multiple times. "--foo 42 --foo 42 --foo 42" => v = { 42, 43, 44
    } Arg &flag(std::vector<int64_t> &v, const std::string & name, char shortOpt =
    0);

    // Specify multiple times. "--foo 42 --foo 42 --foo 42" => v = { 42, 43, 44 }
    Arg &flag(std::vector<uint64_t> &v, const std::string & name, char shortOpt =
    0);

    // Specify multiple times. "--foo 42.0 --foo 43.0" => v = { 42.0, 43.0 }
    Arg &flag(std::vector<double> &v, const std::string & name, char shortOpt =
    0);*/

    // Specify multiple times. "--foo a --foo b" => v = {"a", "b"}
    /*Arg &flag(std::vector<std::string> &v, const std::string & name,
              char shortOpt = 0);*/

    /*Arg &param(int64_t &v, const std::string & name);
    Arg &param(uint64_t &v, const std::string & name);
    Arg &param(double &v, const std::string & name);*/
    auto& param(std::string& v, const std::string& name)
    {
        auto arg = new Param<std::string>(v, name);
        positionals_.emplace_back(arg);
        return *arg;
    }

    /*Arg &param(std::optional<int64_t> &v, const std::string & name);
    Arg &param(std::optional<uint64_t> &v, const std::string & name);
    Arg &param(std::optional<double> &v, const std::string & name);*/
    // Arg &param(std::optional<std::string> &v, const std::string & name);

    // Specify arguments multiple times
    /*Arg &param(std::vector<int64_t> &v, const std::string & name);
    Arg &param(std::vector<uint64_t> &v, const std::string & name);
    Arg &param(std::vector<double> &v, const std::string & name);*/
    // Arg &param(std::vector<std::string> &v, const std::string & name);

    void remaining(std::vector<std::string>& args);

    // Add a way for custom types. Need custom output type and custom parse
    // function (also error messages)

    // FROM PARSER
    std::vector<std::unique_ptr<ArgBase>>& flags()
    {
        return flags_;
    }

    ArgBase* flag(std::string_view name)
    {
        for (const auto& f : flags_) {
            if (f->name() == name) {
                return f.get();
            }
        }
        return nullptr;
    }

    ArgBase* flag(char shortOpt)
    {
        for (const auto& f : flags_) {
            if (f->shortOpt() == shortOpt) {
                return f.get();
            }
        }
        return nullptr;
    }

    std::vector<std::unique_ptr<ArgBase>>& positionals()
    {
        return positionals_;
    }

private:
    std::vector<std::unique_ptr<ArgBase>> flags_;
    std::vector<std::unique_ptr<ArgBase>> positionals_;
    bool error_ = false;
};

namespace detail {
    template <typename... Args>
    void debug([[maybe_unused]] Args&&... args)
    {
#ifdef CLI_DEBUG
        std::stringstream ss;
        (ss << ... << args);
        puts(ss.str().c_str());
#endif
    }
}

struct Parser {
    Parser(std::string programName)
        : programName_(std::move(programName))
    {
    }

    void description(std::string description)
    {
        description_ = std::move(description);
    }

    void usage(std::string usage)
    {
        usage_ = std::move(usage);
    }

    void epilog(std::string epilog)
    {
        epilog_ = std::move(epilog);
    }

    void version(std::string version)
    {
        version_ = std::move(version);
    }

    void addHelp(bool addHelp)
    {
        addHelp_ = addHelp;
    }

    void exitOnError(bool exitOnError)
    {
        exitOnError_ = exitOnError;
    }

    template <typename ArgsType>
    std::optional<ArgsType> parse(const std::vector<std::string>& args)
    {
        detail::debug(">>> parse");
        ArgsType at;
        at.args();

        bool help = false;
        if (addHelp_) {
            at.flag(help, "help", 'h').help("Show this help message and exit");
        }

        bool versionFlag = false;
        if (!version_.empty()) {
            at.flag(versionFlag, "version").help("Show version string and exit");
        }

        if (usage_.empty()) {
            usage_ = getUsage(at);
        }

        bool afterPosDelim = false;
        for (size_t argIdx = 0; argIdx < args.size(); ++argIdx) {
            std::string_view arg = args[argIdx];
            detail::debug("arg: '", arg, "'");

            if (!afterPosDelim && arg.size() > 1 && arg[0] == '-') {
                ArgBase* flag = nullptr;
                std::string optName;

                if (arg[1] == '-') {
                    if (arg == "--") {
                        afterPosDelim = true;
                        continue;
                    }

                    // parse long option
                    flag = at.flag(arg.substr(2));
                    if (!flag) {
                        error("Invalid option '" + std::string(arg) + "'");
                        return std::nullopt;
                    }
                    optName = arg;
                } else {
                    // parse short option(s)
                    // parse all except the last as flags
                    for (const auto c : arg.substr(1, arg.size() - 2)) {
                        detail::debug("short: ", std::string(1, c));
                        auto flag = at.flag(c);
                        if (!flag) {
                            error("Invalid option '" + std::string(1, c) + "'");
                            return std::nullopt;
                        }
                        if (flag->max() != 0) {
                            const auto argStr = flag->max() == 1
                                ? std::string("an argument")
                                : std::to_string(flag->max()) + " arguments";
                            error("Option '" + std::string(1, c) + "' requires " + argStr);
                            return std::nullopt;
                        }
                        flag->parse("");
                    }

                    const auto lastOpt = arg[arg.size() - 1];
                    detail::debug("lastOpt: ", std::string(1, lastOpt));
                    flag = at.flag(lastOpt);
                    if (!flag) {
                        error("Invalid option '" + std::string(1, lastOpt) + "'");
                        return std::nullopt;
                    }
                    optName = std::string(1, lastOpt);
                }

                assert(flag);
                if (flag->max() == 0) {
                    detail::debug("flag");
                    flag->parse("");
                } else {
                    size_t availableArgs = 0;
                    for (size_t i = argIdx + 1; i < std::min(args.size(), argIdx + 1 + flag->max());
                         ++i) {
                        if (!args[i].empty() && args[i][0] == '-') {
                            break;
                        }
                        availableArgs++;
                    }
                    detail::debug("available: ", availableArgs);
                    assert(availableArgs <= flag->max());

                    if (availableArgs < flag->min()) {
                        error("Option '" + optName + "' requires at least "
                            + std::to_string(flag->min())
                            + (flag->min() > 1 ? " arguments" : " argument"));
                        return std::nullopt;
                    }

                    for (size_t i = 0; i < availableArgs; ++i) {
                        const auto name = "option '" + optName + "'";
                        if (!parseArg(*flag, name, args[argIdx + 1 + i])) {
                            return std::nullopt;
                        }
                    }
                    argIdx += availableArgs;
                }
            } else {
                bool consumed = false;
                for (auto& positional : at.positionals()) {
                    if (positional->size() < positional->max()) {
                        const auto name = "argument '" + positional->name() + "'";
                        if (!parseArg(*positional, name, args[argIdx])) {
                            return std::nullopt;
                        }
                        consumed = true;
                        break;
                    }
                }
                if (!consumed) {
                    error("Superfluous argument '" + args[argIdx] + "'");
                    return std::nullopt;
                }
            }
        }

        if (help) {
            puts(getHelp(at).c_str());
            std::exit(0);
        }

        if (versionFlag) {
            puts(version_.c_str());
            std::exit(0);
        }

        for (auto& positional : at.positionals()) {
            if (positional->size() < positional->min()) {
                error("Missing argument '" + positional->name() + "'");
                return std::nullopt;
            }
        }

        return at;
    }

    template <typename ArgsType>
    std::optional<ArgsType> parse(int argc, char** argv)
    {
        return parse<ArgsType>(std::vector<std::string>(argv + 1, argv + argc));
    }

private:
    bool parseArg(ArgBase& arg, const std::string& name, const std::string& argStr)
    {
        if (arg.choices().size() > 0) {
            bool found = false;
            for (const auto& choice : arg.choices()) {
                if (choice == argStr) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::string valStr;
                bool first = false;
                for (const auto& choice : arg.choices()) {
                    if (!first) {
                        valStr.append(", ");
                    }
                    valStr.append(choice);
                }
                error(
                    "Invalid value for " + name + ": '" + argStr + "'. Possible values: " + valStr);
                return false;
            }
        }

        if (!arg.parse(argStr)) {
            error("Invalid value for " + name);
            return false;
        }
        return true;
    }

    void error(const std::string& message)
    {
        std::fputs(message.c_str(), stderr);
        std::fputs("\n", stderr);
        if (!usage_.empty()) {
            std::fputs("Usage: ", stderr);
            std::fputs(usage_.c_str(), stderr);
            std::fputs("\n", stderr);
        }
        if (exitOnError_) {
            std::exit(1);
        }
    }

    std::string getUsage(ArgsBase&)
    {
        std::string usage = programName_;
        usage.append(" ");
        return usage;
    }

    static size_t getSpacing(size_t size)
    {
        const auto offset = 35;
        const auto minSpacing = 4;
        return size > offset - minSpacing ? minSpacing : offset - size;
    }

    std::string getHelp(ArgsBase& at)
    {
        std::string help = "Usage: " + getUsage(at);
        help.append("\n\n");
        if (!description_.empty()) {
            help.append(description_);
            help.append("\n\n");
        }
        if (!at.positionals().empty()) {
            help.append("Positional Arguments:\n");
            for (const auto& arg : at.positionals()) {
                help.append("  ");
                help.append(arg->name());
                help.append(getSpacing(arg->name().size()), ' ');
                help.append(arg->help());
                help.append("\n");
            }
            help.append("\n");
        }
        if (!at.positionals().empty()) {
            help.append("Optional Arguments:\n");
            for (const auto& arg : at.flags()) {
                help.append("  ");
                size_t size = 0;
                if (arg->shortOpt()) {
                    help.append("-");
                    help.append(std::string(1, arg->shortOpt()));
                    help.append(", ");
                    size += 4;
                }
                help.append("--");
                help.append(arg->name());
                size += 2 + arg->name().size();
                help.append(getSpacing(size), ' ');
                help.append(arg->help());
                help.append("\n");
            }
            help.append("\n");
        }
        if (!epilog_.empty()) {
            help.append("\n");
            help.append(epilog_);
            help.append("\n");
        }
        return help;
    }

    std::string programName_;
    std::string description_;
    std::string epilog_;
    std::string version_;
    std::string usage_;
    bool addHelp_ = true;
    bool exitOnError_ = true;
};
}
