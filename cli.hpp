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
template <typename T>
struct Value;

template <>
struct Value<std::string> {
    static constexpr std::string_view typeName = "";

    static std::optional<std::string> parse(std::string_view str)
    {
        return std::string(str);
    }
};

template <>
struct Value<int64_t> {
    static constexpr std::string_view typeName = "integer";

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
    static constexpr std::string_view typeName = "real number";

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

    // For all intents and purposes this value is infinity
    constexpr size_t infinity = std::numeric_limits<size_t>::max();

    class ArgBase {
    public:
        ArgBase(std::string name, std::string_view typeName, char shortOpt = 0)
            : name_(std::move(name))
            , typeName_(std::move(typeName))
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

        std::string_view typeName() const
        {
            return typeName_;
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
        std::string_view typeName_;
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
        ArgBuilderMixin(std::string name, std::string_view typeName, char shortOpt = 0)
            : ArgBase(std::move(name), typeName, shortOpt)
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
    class Flag;

    // A simple flag like. "--foo" => v = true
    template <>
    class Flag<bool> : public ArgBuilderMixin<Flag<bool>> {
    public:
        Flag(bool& value, std::string name, char shortOpt = 0)
            : ArgBuilderMixin(std::move(name), "", shortOpt)
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
    class Flag<size_t> : public ArgBuilderMixin<Flag<size_t>> {
    public:
        Flag(size_t& value, std::string name, char shortOpt = 0)
            : ArgBuilderMixin(std::move(name), "", shortOpt)
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
    class Flag<std::optional<T>> : public ArgBuilderMixin<Flag<std::optional<T>>> {
    public:
        Flag(std::optional<T>& value, std::string name, char shortOpt = 0)
            : ArgBuilderMixin<Flag<std::optional<T>>>(std::move(name), Value<T>::typeName, shortOpt)
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
    class Flag<std::vector<T>> : public ArgBuilderMixin<Flag<std::vector<T>>> {
    public:
        Flag(std::vector<T>& values, std::string name, char shortOpt = 0)
            : ArgBuilderMixin<Flag<std::vector<T>>>(std::move(name), Value<T>::typeName, shortOpt)
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
    class Param : public ArgBuilderMixin<Param<T>> {
    public:
        Param(T& value, std::string name)
            : ArgBuilderMixin<Param<T>>(std::move(name), Value<T>::typeName)
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
    class Param<std::optional<T>> : public ArgBuilderMixin<Param<std::optional<T>>> {
    public:
        Param(std::optional<T>& value, std::string name)
            : ArgBuilderMixin<Param<std::optional<T>>>(std::move(name), Value<T>::typeName)
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

    template <typename T>
    class Param<std::vector<T>> : public ArgBuilderMixin<Param<std::vector<T>>> {
    public:
        Param(std::vector<T>& values, std::string name)
            : ArgBuilderMixin<Param<std::vector<T>>>(std::move(name), Value<T>::typeName)
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
            this->size_++;
            return true;
        }

    private:
        std::vector<T>& values_;
    };

    char toUpperCase(char ch)
    {
        if (ch >= 'a' && ch <= 'z') {
            assert('a' > 'A');
            return ch - ('a' - 'A');
        }
        return ch;
    }

    std::string toUpperCase(std::string_view str)
    {
        std::string upper;
        for (const auto ch : str) {
            upper.push_back(toUpperCase(ch));
        }
        return upper;
    }

    std::string repeatedArgs(const std::string& name, size_t min, size_t max)
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
}

class Parser;

class ArgsBase {
public:
    ArgsBase() = default;
    virtual ~ArgsBase() = default;

    // Automatically deleted because of vector<unique_ptr> members, but this gives nicer errors
    ArgsBase(const ArgsBase&) = delete;
    ArgsBase& operator=(const ArgsBase&) = delete;

    ArgsBase(ArgsBase&&) = default;
    ArgsBase& operator=(ArgsBase&&) = default;

    template <typename T>
    detail::Flag<std::decay_t<T>>& flag(T& v, const std::string& name, char shortOpt = 0)
    {
        assert(!name.empty());
        assert(nameUnique(name));
        assert(shortOptUnique(shortOpt));
        // new is bad, but the alternative looks worse and a bit confusing
        auto arg = new detail::Flag<std::decay_t<T>>(v, name, shortOpt);
        flags_.emplace_back(arg);
        return *arg;
    }

    template <typename T>
    detail::Param<std::decay_t<T>>& param(T& v, const std::string& name)
    {
        assert(!name.empty());
        assert(nameUnique(name));
        auto arg = new detail::Param<std::decay_t<T>>(v, name);
        positionals_.emplace_back(arg);
        return *arg;
    }

    const auto& remaining() const
    {
        return remaining_;
    }

    virtual std::string description() const
    {
        return "";
    }

    virtual std::string epilog() const
    {
        return "";
    }

    virtual std::string usage(std::string_view programName) const
    {
        std::string usage = std::string(programName);
        usage.append(" ");
        for (const auto& arg : flags_) {
            usage.append("[--");
            usage.append(arg->name());
            const auto repArgs
                = detail::repeatedArgs(detail::toUpperCase(arg->name()), arg->min(), arg->max());
            if (!repArgs.empty()) {
                usage.append(" ");
                usage.append(repArgs);
            }
            usage.append("] ");
        }

        for (const auto& arg : positionals_) {
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
            usage.append(detail::repeatedArgs(name, arg->min(), arg->max()));
            usage.append(" ");
        }
        return usage;
    }

    virtual size_t helpOffset() const
    {
        return 35;
    }

    virtual std::string help(std::string_view programName) const
    {
        auto getSpacing = [this](size_t size) {
            const auto minSpacing = 2;
            return size > helpOffset() - minSpacing ? minSpacing : helpOffset() - size;
        };

        std::string help = "Usage: " + usage(programName);
        help.append("\n\n");

        const auto descr = description();
        if (!descr.empty()) {
            help.append(descr);
            help.append("\n\n");
        }

        if (!positionals_.empty()) {
            help.append("Positional Arguments:\n");
            for (const auto& arg : positionals_) {
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

        if (!positionals_.empty()) {
            help.append("Optional Arguments:\n");
            for (const auto& arg : flags_) {
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
                const auto repArgs = detail::repeatedArgs(
                    detail::toUpperCase(arg->name()), arg->min(), arg->max());
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

        const auto epi = epilog();
        if (!epi.empty()) {
            help.append("\n");
            help.append(epi);
            help.append("\n");
        }

        return help;
    }

private:
    // I don't want useless and potentially "dangerous" stuff to show up in autocomplete
    // (generally), so I need to make some functions/data only accessible from Parser.
    // This is the easiest way to do it, even though it's not squeaky clean.
    friend class Parser;

    detail::ArgBase* flag(std::string_view name)
    {
        for (const auto& f : flags_) {
            if (f->name() == name) {
                return f.get();
            }
        }
        return nullptr;
    }

    detail::ArgBase* flag(char shortOpt)
    {
        for (const auto& f : flags_) {
            if (f->shortOpt() == shortOpt) {
                return f.get();
            }
        }
        return nullptr;
    }

    bool nameUnique(
        const std::string& name, const std::vector<std::unique_ptr<detail::ArgBase>>& args)
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

    std::vector<std::unique_ptr<detail::ArgBase>> flags_;
    std::vector<std::unique_ptr<detail::ArgBase>> positionals_;
    std::vector<std::string> remaining_;
};

class Parser {
public:
    Parser(std::string programName)
        : programName_(std::move(programName))
        , output_(std::make_shared<StdOutErr>())
        , exit_([](int status) { std::exit(status); })
    {
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

    // These two are mostly for testing, but maybe they are useful for other stuff
    void output(std::shared_ptr<OutputBase> output)
    {
        output_ = std::move(output);
    }

    void exit(std::function<void(int)> exit)
    {
        exit_ = std::move(exit);
    }

    template <typename Args>
    std::optional<Args> parse(const std::vector<std::string>& argv)
    {
        static_assert(std::is_base_of_v<ArgsBase, Args>);
        detail::debug(">>> parse");

        Args args;

        bool help = false;
        if (addHelp_) {
            args.flag(help, "help", 'h').halt().help("Show this help message and exit");
        }

        bool versionFlag = false;
        if (!version_.empty()) {
            args.flag(versionFlag, "version").halt().help("Show version string and exit");
        }

        args.args();

        bool afterPosDelim = false;
        bool halted = false;
        for (size_t argIdx = 0; argIdx < argv.size(); ++argIdx) {
            std::string_view arg = argv[argIdx];
            detail::debug("arg: '", arg, "'");

            if (!afterPosDelim && arg.size() > 1 && arg[0] == '-') {
                detail::ArgBase* flag = nullptr;
                std::string optName;

                if (arg[1] == '-') {
                    if (arg == "--") {
                        afterPosDelim = true;
                        continue;
                    }

                    // parse long option
                    flag = args.flag(arg.substr(2));
                    if (!flag) {
                        error(args, "Invalid option '" + std::string(arg) + "'");
                        return std::nullopt;
                    }
                    optName = arg;
                } else {
                    // parse short option(s)
                    // parse all except the last as flags
                    for (const auto c : arg.substr(1, arg.size() - 2)) {
                        detail::debug("short: ", std::string(1, c));
                        auto flag = args.flag(c);
                        if (!flag) {
                            error(args, "Invalid option '" + std::string(1, c) + "'");
                            return std::nullopt;
                        }
                        if (flag->max() != 0) {
                            const auto argStr = flag->max() == 1
                                ? std::string("an argument")
                                : std::to_string(flag->max()) + " arguments";
                            error(args, "Option '" + std::string(1, c) + "' requires " + argStr);
                            return std::nullopt;
                        }
                        flag->parse("");
                    }

                    const auto lastOpt = arg[arg.size() - 1];
                    detail::debug("lastOpt: ", std::string(1, lastOpt));
                    flag = args.flag(lastOpt);
                    if (!flag) {
                        error(args, "Invalid option '" + std::string(1, lastOpt) + "'");
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
                    for (size_t i = argIdx + 1; i < std::min(argv.size(), argIdx + 1 + flag->max());
                         ++i) {
                        if (!argv[i].empty() && argv[i][0] == '-') {
                            break;
                        }
                        availableArgs++;
                    }
                    detail::debug("available: ", availableArgs);
                    assert(availableArgs <= flag->max());

                    if (availableArgs < flag->min()) {
                        error(args,
                            "Option '" + optName + "' requires at least "
                                + std::to_string(flag->min())
                                + (flag->min() > 1 ? " arguments" : " argument"));
                        return std::nullopt;
                    }

                    for (size_t i = 0; i < availableArgs; ++i) {
                        const auto name = "option '" + optName + "'";
                        if (!parseArg(args, *flag, name, argv[argIdx + 1 + i])) {
                            return std::nullopt;
                        }
                    }
                    argIdx += availableArgs;
                }
                if (flag->halt()) {
                    args.remaining_.insert(
                        args.remaining_.end(), argv.begin() + argIdx + 1, argv.end());
                    halted = true;
                    break;
                }
            } else {
                bool consumed = false;
                for (auto& arg : args.positionals_) {
                    if (arg->size() < arg->max()) {
                        const auto name = "argument '" + arg->name() + "'";
                        if (!parseArg(args, *arg, name, argv[argIdx])) {
                            return std::nullopt;
                        }
                        consumed = true;
                        break;
                    } else if (arg->halt()) {
                        args.remaining_.insert(
                            args.remaining_.end(), argv.begin() + argIdx + 1, argv.end());
                        halted = true;
                        break;
                    }
                }
                if (halted) {
                    break;
                }
                if (!consumed) {
                    error(args, "Superfluous argument '" + argv[argIdx] + "'");
                    return std::nullopt;
                }
            }
        }

        if (help) {
            output_->out(args.help(programName_));
            exit_(0);
            return args; // exit_ might return
        }

        if (versionFlag) {
            output_->out(version_);
            output_->out("\n");
            exit_(0);
            return args; // exit_ might return
        }

        if (halted) {
            return args;
        }

        for (auto& positional : args.positionals_) {
            if (positional->size() < positional->min()) {
                error(args, "Missing argument '" + positional->name() + "'");
                return std::nullopt;
            }
        }

        return args;
    }

    template <typename Args>
    std::optional<Args> parse(int argc, char** argv)
    {
        return parse<Args>(std::vector<std::string>(argv + 1, argv + argc));
    }

private:
    bool parseArg(const ArgsBase& args, detail::ArgBase& arg, const std::string& name,
        const std::string& argStr)
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
                error(args,
                    "Invalid value '" + argStr + "' for " + name + " (choice): '" + argStr
                        + "'. Possible values: " + valStr);
                return false;
            }
        }

        if (!arg.parse(argStr)) {
            std::string message = "Invalid value '" + argStr + "' for " + name;
            if (!arg.typeName().empty()) {
                message.append(" (" + std::string(arg.typeName()) + ")");
            }
            error(args, message);
            return false;
        }
        return true;
    }

    void error(const ArgsBase& args, const std::string& message)
    {
        output_->err(message);
        output_->err("\n");
        const auto usage = args.usage(programName_);
        if (!usage.empty()) {
            output_->err("Usage: ");
            output_->err(usage);
            output_->err("\n");
        }
        if (exitOnError_) {
            exit_(1);
        }
    }

    std::string programName_;
    std::string version_;
    std::shared_ptr<OutputBase> output_;
    std::function<void(int)> exit_;
    bool addHelp_ = true;
    bool exitOnError_ = true;
};
}
