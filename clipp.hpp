#pragma once

#include <cassert>
#include <charconv>
#include <cstdio>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace clipp {
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
    std::string concat(Args&&... args)
    {
        std::stringstream ss;
        (ss << ... << args);
        return ss.str();
    }

    template <typename... Args>
    void debug([[maybe_unused]] Args&&... args)
    {
#ifdef CLIPP_DEBUG
        std::puts(concat(std::forward<Args>(args)...).c_str());
#endif
    }

    // For all intents and purposes this value is infinity
    constexpr size_t infinity = std::numeric_limits<size_t>::max();

    class ArgBase {
    public:
        ArgBase(std::string name, std::string_view typeName)
            : name_(std::move(name))
            , typeName_(std::move(typeName))
        {
        }

        virtual ~ArgBase()
        {
        }

        const std::string& name() const
        {
            return name_;
        }

        const std::string& help() const
        {
            return help_;
        }

        std::string_view typeName() const
        {
            return typeName_;
        }

        const std::vector<std::string>& choices() const
        {
            return choices_;
        }

        bool halt() const
        {
            return halt_;
        }

        virtual bool parse(std::string_view) = 0;

    protected:
        std::string name_;
        std::string_view typeName_;
        std::string help_;
        std::vector<std::string> choices_;
        bool halt_ = false;
    };

    class FlagBase : public ArgBase {
    public:
        FlagBase(std::string name, std::string_view typeName, char shortOpt)
            : ArgBase(std::move(name), typeName)
            , shortOpt_(shortOpt)
        {
        }

        char shortOpt() const
        {
            return shortOpt_;
        }

        size_t num() const
        {
            return num_;
        }

        bool collect() const
        {
            return collect_;
        }

        virtual void reset()
        {
        }

    protected:
        char shortOpt_;
        size_t num_ = 0;
        bool collect_ = false;
    };

    class PositionalBase : public ArgBase {
    public:
        PositionalBase(std::string name, std::string_view typeName)
            : ArgBase(std::move(name), typeName)
        {
        }

        bool optional() const
        {
            return optional_;
        }

        bool many() const
        {
            return many_;
        }

        size_t size() const
        {
            return size_;
        }

    protected:
        bool optional_ = false;
        bool many_ = false;
        size_t size_ = 0;
    };

    template <typename Derived>
    struct FlagBuilderMixin : public FlagBase {
        FlagBuilderMixin(std::string name, std::string_view typeName, char shortOpt)
            : FlagBase(std::move(name), typeName, shortOpt)
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

        Derived& halt(bool halt = true)
        {
            halt_ = halt;
            return derived();
        }

    private:
        Derived& derived()
        {
            return *static_cast<Derived*>(this);
        }
    };

    template <typename Derived>
    struct PositionalBuilderMixin : public PositionalBase {
        PositionalBuilderMixin(std::string name, std::string_view typeName)
            : PositionalBase(std::move(name), typeName)
        {
        }

        Derived& optional(bool optional = true)
        {
            optional_ = optional;
            return derived();
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

        Derived& halt(bool halt = true)
        {
            halt_ = halt;
            return derived();
        }

    private:
        Derived& derived()
        {
            return *static_cast<Derived*>(this);
        }
    };

    template <typename T>
    class Flag;

    // A simple flag like. "--foo" => v = true
    template <>
    class Flag<bool> : public FlagBuilderMixin<Flag<bool>> {
    public:
        Flag(bool& value, std::string name, char shortOpt)
            : FlagBuilderMixin(std::move(name), "", shortOpt)
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
    class Flag<size_t> : public FlagBuilderMixin<Flag<size_t>> {
    public:
        Flag(size_t& value, std::string name, char shortOpt)
            : FlagBuilderMixin(std::move(name), "", shortOpt)
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
    class Flag<std::optional<T>> : public FlagBuilderMixin<Flag<std::optional<T>>> {
    public:
        Flag(std::optional<T>& value, std::string name, char shortOpt)
            : FlagBuilderMixin<Flag<std::optional<T>>>(
                std::move(name), Value<T>::typeName, shortOpt)
            , value_(value)
        {
            this->num_ = 1;
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
    class Flag<std::vector<T>> : public FlagBuilderMixin<Flag<std::vector<T>>> {
    public:
        Flag(std::vector<T>& values, std::string name, char shortOpt)
            : FlagBuilderMixin<Flag<std::vector<T>>>(std::move(name), Value<T>::typeName, shortOpt)
            , values_(values)
        {
            this->num_ = 1;
            this->collect_ = true;
        }

        auto& num(size_t num)
        {
            this->num_ = num;
            this->collect_ = false;
            return *this;
        }

        auto& collect(bool collect = true)
        {
            this->collect_ = collect;
            return *this;
        }

        void reset() override
        {
            debug("reset");
            values_.clear();
        }

        bool parse(std::string_view str) override
        {
            debug("parse ", str);
            const auto res = Value<T>::parse(str);
            if (!res) {
                return false;
            }
            values_.push_back(res.value());
            debug("size after: ", values_.size());
            return true;
        }

    private:
        std::vector<T>& values_;
    };

    template <typename T>
    class Positional : public PositionalBuilderMixin<Positional<T>> {
    public:
        Positional(T& value, std::string name)
            : PositionalBuilderMixin<Positional<T>>(std::move(name), Value<T>::typeName)
            , value_(value)
        {
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
    class Positional<std::optional<T>>
        : public PositionalBuilderMixin<Positional<std::optional<T>>> {
    public:
        Positional(std::optional<T>& value, std::string name)
            : PositionalBuilderMixin<Positional<std::optional<T>>>(
                std::move(name), Value<T>::typeName)
            , value_(value)
        {
            this->optional();
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
    class Positional<std::vector<T>> : public PositionalBuilderMixin<Positional<std::vector<T>>> {
    public:
        Positional(std::vector<T>& values, std::string name)
            : PositionalBuilderMixin<Positional<std::vector<T>>>(
                std::move(name), Value<T>::typeName)
            , values_(values)
        {
            this->many_ = true;
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

    std::string repeated(const std::string& str, size_t num)
    {
        std::string ret;
        for (size_t i = 0; i < num; ++i) {
            if (i > 0) {
                ret.append(" ");
            }
            ret.append(str);
        }
        return ret;
    }

    template <typename Container>
    std::string join(Container&& container, std::string_view delim)
    {
        std::stringstream ss;
        bool first = true;
        for (const auto& elem : container) {
            if (!first) {
                ss << delim;
            }
            first = false;
            ss << elem;
        }
        return ss.str();
    }

    bool isNumber(std::string_view str)
    {
        double v;
        const auto res = std::from_chars(str.data(), str.data() + str.size(), v);
        return res.ec == std::errc {} && res.ptr == str.data() + str.size();
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
    detail::Flag<std::decay_t<T>>& flag(T& v, std::string name, char shortOpt = 0)
    {
        assert(!name.empty());
        assert(nameUnique(name));
        assert(shortOptUnique(shortOpt));
        // new is bad, but the alternative looks worse and a bit confusing
        auto arg = new detail::Flag<std::decay_t<T>>(v, std::move(name), shortOpt);
        flags_.emplace_back(arg);
        return *arg;
    }

    template <typename T>
    detail::Positional<std::decay_t<T>>& positional(T& v, std::string name)
    {
        assert(!name.empty());
        assert(nameUnique(name));
        auto arg = new detail::Positional<std::decay_t<T>>(v, std::move(name));
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
            const auto repArgs = detail::repeated(detail::toUpperCase(arg->name()), arg->num());
            if (!repArgs.empty()) {
                usage.append(" ");
                usage.append(repArgs);
            }
            usage.append("]");
            if (arg->collect()) {
                usage.append("...");
            }
            usage.append(" ");
        }

        for (const auto& arg : positionals_) {
            std::string name = arg->name();
            if (!arg->choices().empty()) {
                name = "{";
                name.append(detail::join(arg->choices(), ","));
                name.append("}");
            }
            if (arg->optional()) {
                usage.append("[");
                usage.append(name);
                if (arg->many()) {
                    usage.append("...");
                }
                usage.append("]");
            } else {
                usage.append(name);
                if (arg->many()) {
                    usage.append(" ");
                    usage.append("[");
                    usage.append(name);
                    usage.append("...]");
                }
            }
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
                    help.append(detail::join(arg->choices(), ","));
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
                const auto repArgs = detail::repeated(detail::toUpperCase(arg->name()), arg->num());
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

    detail::FlagBase* flag(std::string_view name)
    {
        for (const auto& f : flags_) {
            if (f->name() == name) {
                return f.get();
            }
        }
        return nullptr;
    }

    detail::FlagBase* flag(char shortOpt)
    {
        for (const auto& f : flags_) {
            if (f->shortOpt() == shortOpt) {
                return f.get();
            }
        }
        return nullptr;
    }

    template <typename Container>
    bool nameUnique(const std::string& name, Container&& args)
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

    std::vector<std::unique_ptr<detail::FlagBase>> flags_;
    std::vector<std::unique_ptr<detail::PositionalBase>> positionals_;
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

    // This is essential haltOnFirstSuperfluousPositional
    void errorOnExtraArgs(bool errorOnExtraArgs)
    {
        errorOnExtraArgs_ = errorOnExtraArgs;
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

        bool hasDigitShortOpt = false;
        for (const auto& arg : args.flags_) {
            assert('0' < '9');
            if (arg->shortOpt() >= '0' && arg->shortOpt() <= '9') {
                hasDigitShortOpt = true;
                break;
            }
        }

        auto isFlag = [&hasDigitShortOpt](std::string_view arg) {
            if (arg == "--" || arg.size() < 2 || arg[0] != '-') {
                return false;
            }
            if (!hasDigitShortOpt && detail::isNumber(arg)) {
                return false;
            }
            return true;
        };

        size_t positionalsLeft = 0;
        for (const auto& arg : argv) {
            if (!isFlag(arg)) {
                positionalsLeft++;
            }
        }

        size_t positionalsRequired = 0;
        for (const auto& arg : args.positionals_) {
            if (!arg->optional()) {
                positionalsRequired++;
            }
        }

        auto halt = [](Args& args, const std::vector<std::string>& argv, size_t argIdx) -> bool {
            detail::debug("halt");
            args.remaining_.clear();
            for (size_t i = argIdx; i < argv.size(); ++i) {
                detail::debug("remaining: ", argv[i]);
                args.remaining_.push_back(argv[i]);
            }
            return true;
        };

        bool afterPosDelim = false;
        bool halted = false;
        size_t positionalIdx = 0;
        for (size_t argIdx = 0; argIdx < argv.size(); ++argIdx) {
            std::string_view arg = argv[argIdx];
            detail::debug("arg: '", arg, "'");

            if (arg == "--") {
                detail::debug("sep");
                if (afterPosDelim) {
                    detail::debug("inc pos idx");
                    positionalIdx++;
                }
                afterPosDelim = true;
                continue;
            } else if (!afterPosDelim && isFlag(arg)) {
                detail::debug("flag");
                detail::FlagBase* flag = nullptr;
                std::vector<std::string_view> values;
                std::string_view optName;

                if (arg[1] == '-') {
                    // long option: --flag
                    const auto eq = arg.find('=');
                    if (eq != std::string_view::npos) {
                        // --flag=value
                        detail::debug("eq");
                        const auto name = arg.substr(0, eq);

                        flag = args.flag(name.substr(2));
                        if (!flag) {
                            error(args, "Invalid option '", name, "'");
                            return std::nullopt;
                        }

                        if (flag->num() != 1) {
                            error(args, "'='-syntax can not be used for '", name,
                                "' because it takes ", std::to_string(flag->num()), " arguments");
                            return std::nullopt;
                        }

                        values.push_back(arg.substr(eq + 1));
                        optName = name;
                    } else {
                        flag = args.flag(arg.substr(2));
                        if (!flag) {
                            error(args, "Invalid option '", arg, "'");
                            return std::nullopt;
                        }
                        optName = arg;
                    }
                } else {
                    // parse short option(s)
                    flag = args.flag(arg[1]);
                    if (!flag) {
                        error(args, "Invalid option '", std::string(1, arg[1]), "'");
                        return std::nullopt;
                    }

                    if (flag->num() == 1 && arg.size() > 2) {
                        detail::debug("short + value");
                        // -fVALUE
                        values.push_back(arg.substr(2));
                        optName = std::string(1, arg[1]);
                    } else {
                        // parse all except the last as bool flags
                        for (const auto c : arg.substr(1, arg.size() - 2)) {
                            detail::debug("short: ", std::string(1, c));
                            auto flag = args.flag(c);
                            if (!flag) {
                                error(args, "Invalid option '", std::string(1, c), "'");
                                return std::nullopt;
                            }

                            if (flag->num() != 0) {
                                const auto argStr = flag->num() == 1
                                    ? std::string("an argument")
                                    : std::to_string(flag->num()) + " arguments";
                                error(args, "Option '", std::string(1, c), "' requires ", argStr);
                                return std::nullopt;
                            }

                            flag->parse("");

                            // If we need to halt, we do not break, so we can finish this arg
                            // completely. If we don't finish it "remaining" is not quite right and
                            // parts of this argument would be remaining still.
                            if (flag->halt()) {
                                halted = halt(args, argv, argIdx + 1);
                            }
                        }

                        const auto lastOpt = arg[arg.size() - 1];
                        detail::debug("lastOpt: ", std::string(1, lastOpt));
                        flag = args.flag(lastOpt);
                        if (!flag) {
                            error(args, "Invalid option '", std::string(1, lastOpt), "'");
                            return std::nullopt;
                        }
                        optName = std::string(1, lastOpt);
                    }
                }

                assert(flag);
                if (flag->num() == 0) {
                    detail::debug("0 arg flag");
                    flag->parse("");
                } else {
                    if (values.empty()) {
                        for (size_t i = argIdx + 1;
                             i < std::min(argv.size(), argIdx + 1 + flag->num()); ++i) {
                            if (isFlag(argv[i])) {
                                break;
                            }
                            values.push_back(argv[i]);
                            detail::debug("flag value: ", values.back());
                        }
                        assert(values.size() <= flag->num());
                        argIdx += values.size();
                    }

                    if (values.size() < flag->num()) {
                        error(args, "Option '", optName, "' requires ", std::to_string(flag->num()),
                            (flag->num() > 1 ? " arguments" : " argument"));
                        return std::nullopt;
                    }

                    if (!flag->collect()) {
                        flag->reset();
                    }

                    for (const auto val : values) {
                        const auto name = detail::concat("option '", optName, "'");
                        if (!parseArg(args, *flag, name, val)) {
                            return std::nullopt;
                        }
                    }
                }

                if (flag->halt()) {
                    halted = halt(args, argv, argIdx + 1);
                }
            } else if (positionalIdx < args.positionals_.size()) {
                auto& arg = *args.positionals_[positionalIdx];

                detail::debug("positional ", arg.name());
                const auto name = "argument '" + arg.name() + "'";
                if (!parseArg(args, arg, name, argv[argIdx])) {
                    return std::nullopt;
                }

                if (arg.halt()) {
                    halted = halt(args, argv, argIdx + 1);
                } else if (!arg.many() || positionalsLeft == positionalsRequired) {
                    // If we don't have positionals to spare (we just have enough left to give one
                    // to every positional that needs one) we don't give any positional multiple
                    // anymore.
                    positionalIdx++;
                    positionalsRequired--;
                }

                positionalsLeft--;
            } else if (errorOnExtraArgs_) {
                error(args, "Superfluous argument '", argv[argIdx], "'");
                return std::nullopt;
            } else {
                halted = halt(args, argv, argIdx);
            }

            if (halted) {
                break;
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

        for (auto& arg : args.positionals_) {
            if (!arg->optional() && arg->size() == 0) {
                error(args, "Missing argument '", arg->name(), "'");
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
    bool parseArg(
        const ArgsBase& args, detail::ArgBase& arg, std::string_view name, std::string_view value)
    {
        if (arg.choices().size() > 0) {
            bool found = false;
            for (const auto& choice : arg.choices()) {
                if (choice == value) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::string valStr = detail::join(arg.choices(), ", ");
                error(
                    args, "Invalid value '", value, "' for ", name, ". Possible values: ", valStr);
                return false;
            }
        }

        if (!arg.parse(value)) {
            std::string message = detail::concat("Invalid value '", value, "' for ", name);
            if (!arg.typeName().empty()) {
                message.append(" (" + std::string(arg.typeName()) + ")");
            }
            error(args, message);
            return false;
        }
        return true;
    }

    template <typename... Args>
    void error(const ArgsBase& args, Args&&... msg)
    {
        output_->err(detail::concat(std::forward<Args>(msg)...));
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
    bool errorOnExtraArgs_ = true;
};
}
