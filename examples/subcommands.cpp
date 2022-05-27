#include <iostream>

// #define CLIPP_DEBUG
#include "clipp.hpp"

struct ParentArgs : public clipp::ArgsBase {
    std::optional<std::string> device;
    std::string command;

    void args()
    {
        flag(device, "device", 'd').help("Which device to start the system on");
        positional(command, "command").choices({ "start", "stop" }).halt();
    }
};

struct StartArgs : public clipp::ArgsBase {
    std::optional<std::string> power;
    std::string system;

    void args()
    {
        flag(power, "power", 'p').help("With how much power to start the system");
        positional(system, "system").help("The system to start");
    }
};

struct StopArgs : public clipp::ArgsBase {
    bool force = false;
    std::string system;

    void args()
    {
        flag(force, "force", 'f').help("Force stopping of system");
        positional(system, "system").help("The system to stop");
    }
};

int main(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<ParentArgs>(argc, argv).value();
    if (args.device) {
        std::cout << "Device: " << *args.device << std::endl;
    }

    auto subParser = clipp::Parser(std::string(argv[0]) + " " + args.command);
    if (args.command == "start") {
        const auto subArgs = subParser.parse<StartArgs>(args.remaining()).value();
        if (subArgs.power) {
            std::cout << "power: " << *subArgs.power << std::endl;
        }
        std::cout << "Starting system: " << subArgs.system << std::endl;
    } else if (args.command == "stop") {
        const auto subArgs = subParser.parse<StopArgs>(args.remaining()).value();
        std::cout << "force: " << subArgs.force << std::endl;
        std::cout << "Stopping system: " << subArgs.system << std::endl;
    }
    return 0;
}
