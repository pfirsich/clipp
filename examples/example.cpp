#include <iostream>

// #define CLI_DEBUG
#include "cli.hpp"

struct Args : public cli::ArgsBase {
    bool foo = false;
    std::optional<std::string> opt;
    size_t verbose = 0;
    std::string pos = "def";

    void args()
    {
        flag(foo, "foo", 'f');
        flag(opt, "opt", 'o');
        flag(verbose, "verbose", 'v');
        param(pos, "pos").optional();
    }
};

int main(int argc, char** argv)
{
    auto parser = cli::Parser(argv[0]);
    const auto args = parser.parse<Args>(argc, argv).value();
    std::cout << "foo: " << args.foo << std::endl;
    std::cout << "verbose: " << args.verbose << std::endl;
    std::cout << "opt: " << (args.opt ? *args.opt : std::string("<none>")) << std::endl;
    std::cout << "pos: " << args.pos << std::endl;
    return 0;
}
