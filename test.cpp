#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// #define CLI_DEBUG
#include "cli.hpp"

struct Args : public cli::ArgsBase {
    bool foo = false;
    std::optional<std::string> opt;
    size_t verbose = 0;
    std::string pos;
    std::optional<int64_t> number;
    std::optional<double> fnum;

    void args()
    {
        flag(foo, "foo", 'f').help("a boolean flag");
        flag(opt, "opt", 'o').help("an optional string");
        flag(verbose, "verbose", 'v').help("a counted flag");
        flag(number, "number", 'n').help("a number flag");
        flag(fnum, "fnum").help("a real number flag");
        param(pos, "pos").help("a positional argument");
    }
};

template <typename ArgsType>
auto parse(std::vector<std::string> args)
{
    auto parser = cli::Parser("test");
    parser.version("0.1");
    parser.exitOnError(false);
    return parser.parse<ArgsType>(args);
}

TEST_CASE(R"(no args (Args))")
{
    const auto args = parse<Args>({});
    CHECK(!args);
}

TEST_CASE(R"({ "pos" } (Args))")
{
    const auto args = parse<Args>({ "pos" });
    CHECK(args);
    CHECK(!args->foo);
    CHECK(!args->opt);
    CHECK(args->verbose == 0);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "--foo", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--foo", "pos" });
    CHECK(args);
    CHECK(args->foo);
    CHECK(!args->opt);
    CHECK(args->verbose == 0);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "pos", "--foo" } (Args))")
{
    const auto args = parse<Args>({ "pos", "--foo" });
    CHECK(args);
    CHECK(args->foo);
    CHECK(!args->opt);
    CHECK(args->verbose == 0);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "-fvvv", "pos" } (Args))")
{
    const auto args = parse<Args>({ "-fvvv", "pos" });
    CHECK(args);
    CHECK(args->foo);
    CHECK(!args->opt);
    CHECK(args->verbose == 3);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "--opt" } (Args))")
{
    const auto args = parse<Args>({ "--opt" });
    CHECK(!args);
}

TEST_CASE(R"({ "-fvvv", "--opt", "optval", "--foo" } (Args))")
{
    const auto args = parse<Args>({ "-fvvv", "--opt", "optval", "pos" });
    CHECK(args);
    CHECK(args->foo);
    CHECK(args->opt);
    CHECK(args->opt.value() == "optval");
    CHECK(args->verbose == 3);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "-fvvvo", "optval", "pos" } (Args))")
{
    const auto args = parse<Args>({ "-fvvvo", "optval", "pos" });
    CHECK(args);
    CHECK(args->foo);
    CHECK(args->opt);
    CHECK(args->opt.value() == "optval");
    CHECK(args->verbose == 3);
    CHECK(!args->number);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "--number", "foo", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--number", "foo", "pos" });
    CHECK(!args);
}

TEST_CASE(R"({ "--number", "42x", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--number", "42x", "pos" });
    CHECK(!args);
}

TEST_CASE(R"({ "--number", "42", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--number", "42", "pos" });
    CHECK(args);
    CHECK(!args->foo);
    CHECK(!args->opt);
    CHECK(args->verbose == 0);
    CHECK(*args->number == 42);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "--fnum", "foo", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--fnum", "foo", "pos" });
    CHECK(!args);
}

TEST_CASE(R"({ "--fnum", "42", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--fnum", "42", "pos" });
    CHECK(args);
    CHECK(*args->fnum == 42.0);
}

TEST_CASE(R"({ "--fnum", "42.542", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--fnum", "42.542", "pos" });
    CHECK(args);
    CHECK(std::fabs(*args->fnum - 42.542) < 1e-8);
}

struct OptParam : public cli::ArgsBase {
    std::string pos = "def";

    void args()
    {
        param(pos, "pos").optional();
    }
};

TEST_CASE(R"(no args (OptParam))")
{
    const auto args = parse<OptParam>({});
    CHECK(args);
    CHECK(args->pos == "def");
}

TEST_CASE(R"({ "bar" } (OptParam))")
{
    const auto args = parse<OptParam>({ "bar" });
    CHECK(args);
    CHECK(args->pos == "bar");
}

TEST_CASE(R"({ "foo", "foo" } (OptParam))")
{
    const auto args = parse<OptParam>({ "foo", "foo" });
    CHECK(!args);
}

enum class MyEnum { A, B, C };

template <>
struct cli::Value<MyEnum> {
    static std::optional<MyEnum> parse(std::string_view str)
    {
        if (str == "a") {
            return MyEnum::A;
        } else if (str == "b") {
            return MyEnum::B;
        } else if (str == "c") {
            return MyEnum::C;
        }
        return std::nullopt;
    }
};

struct CustomType : public cli::ArgsBase {
    MyEnum val;

    void args()
    {
        param(val, "pos").choices({ "a", "b", "c" });
    }
};

TEST_CASE(R"({ "foo" } (CustomType))")
{
    const auto args = parse<CustomType>({ "foo" });
    CHECK(!args);
}

TEST_CASE(R"({ "a" } (CustomType))")
{
    const auto args = parse<CustomType>({ "a" });
    CHECK(args);
    CHECK(args->val == MyEnum::A);
}

TEST_CASE(R"({ "c" } (CustomType))")
{
    const auto args = parse<CustomType>({ "c" });
    CHECK(args);
    CHECK(args->val == MyEnum::C);
}

// How to test help and version (which exit)?
/* TEST_CASE(R"({ "--help" } (OptParam))")
{
    const auto args = parse<Args>({ "--help" });
    CHECK(!args);
} */
