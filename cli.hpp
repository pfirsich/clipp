#pragma once

#include <cassert>
#include <charconv>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef CLI_DEBUG
#include <sstream>
#endif

namespace cli {
namespace detail {
    template <typename... Args>
    void debug([[maybe_unused]] Args&&... args)
    {
#ifdef CLI_DEBUG
        std::stringstream ss;
        (ss << ... << args);
        std::puts(ss.str().c_str());
#endif
    }
}

// For all intents and purposes this value is infinity
constexpr size_t infinity = std::numeric_limits<size_t>::max();

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

    bool halt() const
    {
        return halt_;
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
    bool halt_ = false;
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

    // Only relevant for params
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

    Derived& halt(bool halt = true)
    {
        halt_ = halt;
        return derived();
    }

    Derived& derived()
    {
        return *static_cast<Derived*>(this);
    }
};

template <typename T>
struct Value;

template <>
struct Value<std::string> {
    static std::optional<std::string> parse(std::string_view str)
    {
        return std::string(str);
    }
};

template <>
struct Value<int64_t> {
    static std::optional<int64_t> parse(std::string_view str)
    {
        int64_t val;
        const auto res = std::from_chars(str.data(), str.data() + str.size(), val);
        if (res.ec != std::errc() || res.ptr < str.data() + str.size()) {
            return std::nullopt;
        }
        return val;
    }
};

template <>
struct Value<double> {
    static std::optional<double> parse(std::string_view str)
    {
        double val;
        const auto res = std::from_chars(str.data(), str.data() + str.size(), val);
        if (res.ec != std::errc() || res.ptr < str.data() + str.size()) {
            return std::nullopt;
        }
        return val;
    }
};

template <typename T>
struct Flag;

// A simple flag like. "--foo" => v = true
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

// Counts flags. "-vvvv" => v = 4
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

// An optional flag. "--foo cool" => v = cool
template <typename T>
struct Flag<std::optional<T>> : public ArgBuilderMixin<Flag<std::optional<T>>> {
    Flag(std::optional<T>& value, std::string name, char shortOpt = 0)
        // For some reason the template args need to be specified explicitly
        : ArgBuilderMixin<Flag<std::optional<T>>>(std::move(name), shortOpt)
        , value_(value)
    {
        this->num(1);
    }

    bool parse(std::string_view str) override
    {
        const auto res = Value<T>::parse(str);
        if (!res) {
            return false;
        }
        value_ = res.value();
        return true;
    }

private:
    std::optional<T>& value_;
};

template <typename T>
struct Flag<std::vector<T>> : public ArgBuilderMixin<Flag<std::vector<T>>> {
    Flag(std::vector<T>& values, std::string name, char shortOpt = 0)
        // For some reason the template args need to be specified explicitly
        : ArgBuilderMixin<Flag<std::vector<T>>>(std::move(name), shortOpt)
        , values_(values)
    {
        this->min(1);
        this->max(infinity);
    }

    bool parse(std::string_view str) override
    {
        const auto res = Value<T>::parse(str);
        if (!res) {
            return false;
        }
        values_.push_back(res.value());
        return true;
    }

private:
    std::vector<T>& values_;
};

template <typename T>
struct Param : public ArgBuilderMixin<Param<T>> {
    Param(T& value, std::string name)
        : ArgBuilderMixin<Param<T>>(std::move(name))
        , value_(value)
    {
        this->num(1);
    }

    bool parse(std::string_view str) override
    {
        const auto res = Value<T>::parse(str);
        if (!res) {
            return false;
        }
        value_ = res.value();
        this->size_++;
        return true;
    }

private:
    T& value_;
};

template <typename T>
struct Param<std::optional<T>> : public ArgBuilderMixin<Param<std::optional<T>>> {
    Param(std::optional<T>& value, std::string name, char shortOpt = 0)
        // For some reason the template args need to be specified explicitly
        : ArgBuilderMixin<Param<std::optional<T>>>(std::move(name), shortOpt)
        , value_(value)
    {
        this->min(0);
        this->max(1);
    }

    bool parse(std::string_view str) override
    {
        const auto res = Value<T>::parse(str);
        if (!res) {
            return false;
        }
        value_ = res.value();
        this->size_++;
        return true;
    }

private:
    std::optional<T>& value_;
};

struct ArgsBase {
    template <typename T>
    auto& flag(T& v, const std::string& name, char shortOpt = 0)
    {
        assert(!name.empty());
        assert(nameUnique(name));
        assert(shortOptUnique(shortOpt));
        // new is bad, but the alternative looks worse and a bit confusing
        auto arg = new Flag<std::decay_t<T>>(v, name, shortOpt);
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
    template <typename T>
    auto& param(T& v, const std::string& name)
    {
        assert(!name.empty());
        assert(nameUnique(name));
        auto arg = new Param<std::decay_t<T>>(v, name);
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

    // Add a way for custom types. Need custom output type and custom parse
    // function (also error messages)

    const auto& remaining() const
    {
        return remaining_;
    }

    // FROM PARSER
    auto& flags()
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

    auto& positionals()
    {
        return positionals_;
    }

    auto& remaining()
    {
        return remaining_;
    }

private:
    bool nameUnique(const std::string& name, const std::vector<std::unique_ptr<ArgBase>>& args)
    {
        for (const auto& arg : args) {
            if (arg->name() == name) {
                return false;
            }
        }
        return true;
    }

    bool nameUnique(const std::string& name)
    {
        return nameUnique(name, flags_) && nameUnique(name, positionals_);
    }

    bool shortOptUnique(char shortOpt)
    {
        if (shortOpt == 0) {
            return true;
        }
        for (const auto& flag : flags_) {
            if (flag->shortOpt() == shortOpt) {
                return false;
            }
        }
        return true;
    }

    std::vector<std::unique_ptr<ArgBase>> flags_;
    std::vector<std::unique_ptr<ArgBase>> positionals_;
    std::vector<std::string> remaining_;
};

struct OutputBase {
    virtual ~OutputBase() = default;
    virtual void out(std::string_view) = 0;
    virtual void err(std::string_view) = 0;
};

struct StdOutErr : public OutputBase {
    void out(std::string_view str) override
    {
        std::fwrite(str.data(), 1, str.size(), stdout);
    }

    void err(std::string_view str) override
    {
        std::fwrite(str.data(), 1, str.size(), stderr);
    }
};

struct Parser {
    Parser(std::string programName)
        : programName_(std::move(programName))
        , output_(std::make_shared<StdOutErr>())
        , exit_([](int status) { std::exit(status); })
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

    void helpOffset(size_t helpOffset)
    {
        helpOffset_ = helpOffset;
    }

    // These two are mostly for testing, but maybe they are useful for other stuff
    void output(std::shared_ptr<OutputBase> output)
    {
        output_ = std::move(output);
    }

    void exit(std::function<void(int)> exit)
    {
        exit_ = std::move(exit);
    }

    template <typename ArgsType>
    std::optional<ArgsType> parse(const std::vector<std::string>& args)
    {
        detail::debug(">>> parse");
        ArgsType at;

        bool help = false;
        if (addHelp_) {
            at.flag(help, "help", 'h').halt().help("Show this help message and exit");
        }

        bool versionFlag = false;
        if (!version_.empty()) {
            at.flag(versionFlag, "version").halt().help("Show version string and exit");
        }

        at.args();

        if (usage_.empty()) {
            usage_ = getUsage(at);
        }

        bool afterPosDelim = false;
        bool halted = false;
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
                if (flag->halt()) {
                    at.remaining().insert(
                        at.remaining().end(), args.begin() + argIdx + 1, args.end());
                    halted = true;
                    break;
                }
            } else {
                bool consumed = false;
                for (auto& arg : at.positionals()) {
                    if (arg->size() < arg->max()) {
                        const auto name = "argument '" + arg->name() + "'";
                        if (!parseArg(*arg, name, args[argIdx])) {
                            return std::nullopt;
                        }
                        consumed = true;
                        break;
                    } else if (arg->halt()) {
                        at.remaining().insert(
                            at.remaining().end(), args.begin() + argIdx + 1, args.end());
                        halted = true;
                        break;
                    }
                }
                if (halted) {
                    break;
                }
                if (!consumed) {
                    error("Superfluous argument '" + args[argIdx] + "'");
                    return std::nullopt;
                }
            }
        }

        if (help) {
            output_->out(getHelp(at));
            exit_(0);
            return at; // exit_ might return
        }

        if (versionFlag) {
            output_->out(version_);
            output_->out("\n");
            exit_(0);
            return at; // exit_ might return
        }

        if (halted) {
            return at;
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
                for (size_t i = 0; i < arg.choices().size(); ++i) {
                    if (i > 0) {
                        valStr.append(", ");
                    }
                    valStr.append(arg.choices()[i]);
                }
                error("Invalid value '" + argStr + "' for " + name + ": '" + argStr
                    + "'. Possible values: " + valStr);
                return false;
            }
        }

        if (!arg.parse(argStr)) {
            error("Invalid value '" + argStr + "' for " + name);
            return false;
        }
        return true;
    }

    void error(const std::string& message)
    {
        output_->err(message);
        output_->err("\n");
        if (!usage_.empty()) {
            output_->err("Usage: ");
            output_->err(usage_);
            output_->err("\n");
        }
        if (exitOnError_) {
            exit_(1);
        }
    }

    static char toUpperCase(char ch)
    {
        if (ch >= 'a' && ch <= 'z') {
            assert('a' > 'A');
            return ch - ('a' - 'A');
        }
        return ch;
    }

    static std::string toUpperCase(std::string_view str)
    {
        std::string upper;
        for (const auto ch : str) {
            upper.push_back(toUpperCase(ch));
        }
        return upper;
    }

    static std::string repeatedArgs(const std::string& name, size_t min, size_t max)
    {
        std::string ret;
        for (size_t i = 0; i < min; ++i) {
            if (i > 0) {
                ret.append(" ");
            }
            ret.append(name);
        }
        if (max > min) {
            if (min > 0) {
                ret.append(" ");
            }
            ret.append("[");
            ret.append(name);
            // This could use improvement
            if (max - min > 1) {
                ret.append("..");
            }
            ret.append("]");
        }
        return ret;
    }

    std::string getUsage(ArgsBase& at)
    {
        std::string usage = programName_;
        usage.append(" ");
        for (const auto& arg : at.flags()) {
            usage.append("[--");
            usage.append(arg->name());
            const auto repArgs = repeatedArgs(toUpperCase(arg->name()), arg->min(), arg->max());
            if (!repArgs.empty()) {
                usage.append(" ");
                usage.append(repArgs);
            }
            usage.append("] ");
        }

        for (const auto& arg : at.positionals()) {
            std::string name = arg->name();
            if (!arg->choices().empty()) {
                name = "{";
                for (size_t i = 0; i < arg->choices().size(); ++i) {
                    if (i > 0) {
                        name.append(",");
                    }
                    name.append(arg->choices()[i]);
                }
                name.append("}");
            }
            usage.append(repeatedArgs(name, arg->min(), arg->max()));
            usage.append(" ");
        }
        return usage;
    }

    size_t getSpacing(size_t size)
    {
        const auto minSpacing = 4;
        return size > helpOffset_ - minSpacing ? minSpacing : helpOffset_ - size;
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
                if (!arg->choices().empty()) {
                    help.append("{");
                    for (size_t i = 0; i < arg->choices().size(); ++i) {
                        if (i > 0) {
                            help.append(",");
                        }
                        help.append(arg->choices()[i]);
                    }
                    help.append("}\n");
                } else {
                    help.append(arg->name());
                    help.append(getSpacing(arg->name().size()), ' ');
                    help.append(arg->help());
                    help.append("\n");
                }
            }
            help.append("\n");
        }
        if (!at.positionals().empty()) {
            help.append("Optional Arguments:\n");
            for (const auto& arg : at.flags()) {
                help.append("  ");
                if (arg->shortOpt()) {
                    help.append("-");
                    help.append(std::string(1, arg->shortOpt()));
                    help.append(", ");
                } else {
                    help.append("    ");
                }
                help.append("--");
                help.append(arg->name());
                size_t size = 4 + 2 + arg->name().size();
                const auto repArgs = repeatedArgs(toUpperCase(arg->name()), arg->min(), arg->max());
                if (!repArgs.empty()) {
                    help.append(" ");
                    help.append(repArgs);
                    size += 1 + repArgs.size();
                }
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
    size_t helpOffset_ = 40;
    std::shared_ptr<OutputBase> output_;
    std::function<void(int)> exit_;
};
}
