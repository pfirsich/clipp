#include <iostream>

// #define CLIPP_DEBUG
#include "clipp.hpp"

struct Args : public clipp::ArgsBase {
    bool foo = false;
    std::optional<std::string> opt;
    size_t verbose = 0;
    std::string pos = "def";

    void args()
    {
        flag(foo, "foo", 'f');
        flag(opt, "opt", 'o');
        flag(verbose, "verbose", 'v');
        positional(pos, "pos").optional();
    }

    std::string description() const override
    {
        return "An example application";
    }

    std::string epilog() const override
    {
        return "TODO: Add usage examples";
    }
};

int main(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<Args>(argc, argv).value();
    std::cout << "foo: " << args.foo << std::endl;
    std::cout << "verbose: " << args.verbose << std::endl;
    std::cout << "opt: " << (args.opt ? *args.opt : std::string("<none>")) << std::endl;
    std::cout << "pos: " << args.pos << std::endl;
    return 0;
}
