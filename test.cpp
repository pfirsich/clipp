#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// #define CLI_DEBUG
#include "cli.hpp"

struct Args : public cli::ArgsBase {
    bool foo = false;
    std::optional<std::string> opt;
    size_t verbose = 0;
    std::string pos;

    void args()
    {
        flag(foo, "foo", 'f');
        flag(opt, "opt", 'o');
        flag(verbose, "verbose", 'v');
        param(pos, "pos");
    }
};

template <typename ArgsType>
auto parse(std::vector<std::string> args)
{
    auto parser = cli::Parser("test");
    parser.exitOnError(false);
    return parser.parse<ArgsType>(args);
}

TEST_CASE("no arg (Args)s")
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
