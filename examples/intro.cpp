#include <iostream>

#include "clipp.hpp"

struct Args : public clipp::ArgsBase {
    bool dryRun = false;
    size_t verbose = 0;
    std::optional<int64_t> num;
    std::optional<std::string> output;
    std::vector<std::string> input;

    void args()
    {
        flag(dryRun, "dry-run", 'd').help("Only log potential filesystem changes");
        flag(verbose, "verbose", 'v').help("Output more debugging information");
        flag(num, "num").help("The number of things to do");
        flag(output, "output", 'o').help("The output file");
        positional(input, "input");
    }
};

int main(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const Args args = parser.parse<Args>(argc, argv).value();
    std::cout << "dry-run: " << args.dryRun;
    std::cout << ", verbose: " << args.verbose;
    std::cout << ", num: " << args.num.value_or(42);
    std::cout << ", output: " << (args.output ? *args.output : std::string("<none>"));
    std::cout << ", input: " << clipp::detail::join(args.input, ", ") << std::endl;
    return 0;
}
