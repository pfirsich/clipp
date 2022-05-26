#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// #define CLIPP_DEBUG
#include "clipp.hpp"

struct StringOutput : clipp::OutputBase {
    void out(std::string_view str)
    {
        output.append(str);
    }

    void err(std::string_view str)
    {
        error.append(str);
    }

    void clear()
    {
        output.clear();
        error.clear();
    }

    std::string output;
    std::string error;
};

std::shared_ptr<StringOutput> output;
int exitStatus;

template <typename ArgsType>
auto parse(std::vector<std::string> args)
{
    if (!output) {
        output = std::make_shared<StringOutput>();
    }
    output->clear();
    exitStatus = 0;

    auto parser = clipp::Parser("test");
    parser.version("0.1");
    parser.output(output);
    parser.exit([](int status) { exitStatus = status; });

    return parser.parse<ArgsType>(args);
}

struct Args : public clipp::ArgsBase {
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

TEST_CASE(R"(no args (Args))")
{
    const auto args = parse<Args>({});
    CHECK(!args);
}

TEST_CASE(R"({ "pos" } (Args))")
{
    const auto args = parse<Args>({ "pos" });
    REQUIRE(args);
    CHECK(!args->foo);
    CHECK(!args->opt);
    CHECK(args->verbose == 0);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "--foo", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--foo", "pos" });
    REQUIRE(args);
    CHECK(args->foo);
    CHECK(!args->opt);
    CHECK(args->verbose == 0);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "pos", "--foo" } (Args))")
{
    const auto args = parse<Args>({ "pos", "--foo" });
    REQUIRE(args);
    CHECK(args->foo);
    CHECK(!args->opt);
    CHECK(args->verbose == 0);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "-fvvv", "pos" } (Args))")
{
    const auto args = parse<Args>({ "-fvvv", "pos" });
    REQUIRE(args);
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
    REQUIRE(args);
    CHECK(args->foo);
    CHECK(args->opt);
    CHECK(args->opt.value() == "optval");
    CHECK(args->verbose == 3);
    CHECK(args->pos == "pos");
}

TEST_CASE(R"({ "-fvvvo", "optval", "pos" } (Args))")
{
    const auto args = parse<Args>({ "-fvvvo", "optval", "pos" });
    REQUIRE(args);
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
    REQUIRE(args);
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
    REQUIRE(args);
    CHECK(*args->fnum == 42.0);
}

TEST_CASE(R"({ "--fnum", "42.542", "pos" } (Args))")
{
    const auto args = parse<Args>({ "--fnum", "42.542", "pos" });
    REQUIRE(args);
    CHECK(std::fabs(*args->fnum - 42.542) < 1e-8);
}

struct OptParam : public clipp::ArgsBase {
    std::string pos = "def";

    void args()
    {
        param(pos, "pos").optional();
    }
};

TEST_CASE(R"(no args (OptParam))")
{
    const auto args = parse<OptParam>({});
    REQUIRE(args);
    CHECK(args->pos == "def");
}

TEST_CASE(R"({ "bar" } (OptParam))")
{
    const auto args = parse<OptParam>({ "bar" });
    REQUIRE(args);
    CHECK(args->pos == "bar");
}

TEST_CASE(R"({ "foo", "foo" } (OptParam))")
{
    const auto args = parse<OptParam>({ "foo", "foo" });
    CHECK(!args);
}

enum class MyEnum { A, B, C };

template <>
struct clipp::Value<MyEnum> {
    static constexpr std::string_view typeName = "MyEnum";

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

struct CustomType : public clipp::ArgsBase {
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
    REQUIRE(args);
    CHECK(args->val == MyEnum::A);
}

TEST_CASE(R"({ "c" } (CustomType))")
{
    const auto args = parse<CustomType>({ "c" });
    REQUIRE(args);
    CHECK(args->val == MyEnum::C);
}

TEST_CASE(R"({ "--version" } (Args))")
{
    const auto args = parse<Args>({ "--version" });
    CHECK(args);
    CHECK(exitStatus == 0);
    CHECK(output->output == "0.1\n");
}

TEST_CASE(R"({ "--help" } (Args))")
{
    const auto args = parse<Args>({ "--version" });
    CHECK(args);
    CHECK(exitStatus == 0);
}

struct StdOptParam : public clipp::ArgsBase {
    int64_t x = 1000;
    std::optional<int64_t> y;

    void args()
    {
        param(x, "x").optional();
        param(y, "y");
    }
};

TEST_CASE(R"({} (StdOptParam))")
{
    const auto args = parse<StdOptParam>({});
    REQUIRE(args);
    CHECK(args->x == 1000);
    CHECK(!args->y);
}

TEST_CASE(R"({ "42" } (StdOptParam))")
{
    const auto args = parse<StdOptParam>({ "42" });
    REQUIRE(args);
    CHECK(args->x == 42);
    CHECK(!args->y);
}

TEST_CASE(R"({ "42", "42" } (StdOptParam))")
{
    const auto args = parse<StdOptParam>({ "42", "42" });
    REQUIRE(args);
    CHECK(args->x == 42);
    CHECK(args->y);
    CHECK(args->y.value() == 42);
}

struct VecFlag : public clipp::ArgsBase {
    std::vector<int64_t> vec;

    void args()
    {
        flag(vec, "vec").num(3);
    }
};

TEST_CASE(R"({ "--vec" } (VecFlag))")
{
    const auto args = parse<VecFlag>({ "--vec" });
    CHECK(!args);
}

TEST_CASE(R"({ "--vec", "1" } (VecFlag))")
{
    const auto args = parse<VecFlag>({ "--vec", "1" });
    CHECK(!args);
}

TEST_CASE(R"({ "--vec", "1", "2" } (VecFlag))")
{
    const auto args = parse<VecFlag>({ "--vec", "1", "2" });
    CHECK(!args);
}

TEST_CASE(R"({ "--vec", "1", "2", "3" } (VecFlag))")
{
    const auto args = parse<VecFlag>({ "--vec", "1", "2", "3" });
    REQUIRE(args);
    CHECK(args->vec.size() == 3);
    CHECK(args->vec[0] == 1);
    CHECK(args->vec[1] == 2);
    CHECK(args->vec[2] == 3);
}

TEST_CASE(R"({ "--vec", "1", "2", "3", "4" } (VecFlag))")
{
    const auto args = parse<VecFlag>({ "--vec", "1", "2", "3", "4" });
    CHECK(!args);
}

struct VecParamZeroToInf : public clipp::ArgsBase {
    std::vector<std::string> params;

    void args()
    {
        param(params, "param").optional();
    }
};

TEST_CASE(R"({} (VecParamZeroToInf))")
{
    const auto args = parse<VecParamZeroToInf>({});
    REQUIRE(args);
    CHECK(args->params.size() == 0);
}

TEST_CASE(R"({ "a" } (VecParamZeroToInf))")
{
    const auto args = parse<VecParamZeroToInf>({ "a" });
    REQUIRE(args);
    CHECK(args->params.size() == 1);
    CHECK(args->params[0] == "a");
}

TEST_CASE(R"({ "a", "b" } (VecParamZeroToInf))")
{
    const auto args = parse<VecParamZeroToInf>({ "a", "b" });
    REQUIRE(args);
    CHECK(args->params.size() == 2);
    CHECK(args->params[0] == "a");
    CHECK(args->params[1] == "b");
}

struct VecParamOneToInf : public clipp::ArgsBase {
    std::vector<std::string> params;

    void args()
    {
        param(params, "param");
    }
};

TEST_CASE(R"({} (VecParamOneToInf))")
{
    const auto args = parse<VecParamOneToInf>({});
    CHECK(!args);
}

TEST_CASE(R"({ "a" } (VecParamOneToInf))")
{
    const auto args = parse<VecParamOneToInf>({ "a" });
    REQUIRE(args);
    CHECK(args->params.size() == 1);
    CHECK(args->params[0] == "a");
}

TEST_CASE(R"({ "a", "b" } (VecParamOneToInf))")
{
    const auto args = parse<VecParamOneToInf>({ "a", "b" });
    REQUIRE(args);
    CHECK(args->params.size() == 2);
    CHECK(args->params[0] == "a");
    CHECK(args->params[1] == "b");
}

struct VecFlagDontCollectArgs : public clipp::ArgsBase {
    std::vector<int64_t> vals;

    void args()
    {
        flag(vals, "vals");
    }
};

TEST_CASE(R"({ "--vals", "1", "--vals", "2", "--vals", "3" } (VecFlagDontCollectArgs))")
{
    const auto args
        = parse<VecFlagDontCollectArgs>({ "--vals", "1", "--vals", "2", "--vals", "3" });
    REQUIRE(args);
    CHECK(args->vals.size() == 1);
    CHECK(args->vals[0] == 3);
}

struct VecFlagCollectArgs : public clipp::ArgsBase {
    std::vector<int64_t> vals;

    void args()
    {
        flag(vals, "vals").collect();
    }
};

TEST_CASE(R"({ "--vals", "1", "--vals", "2", "--vals", "3" } (VecFlagCollectArgs))")
{
    const auto args = parse<VecFlagCollectArgs>({ "--vals", "1", "--vals", "2", "--vals", "3" });
    REQUIRE(args);
    CHECK(args->vals.size() == 3);
    CHECK(args->vals[0] == 1);
    CHECK(args->vals[1] == 2);
    CHECK(args->vals[2] == 3);
}
