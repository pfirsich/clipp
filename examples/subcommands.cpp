#include <iostream>

// #define CLI_DEBUG
#include "cli.hpp"

struct ParentArgs : public cli::ArgsBase {
    std::optional<std::string> device;
    std::string command;
    std::vector<std::string> remaining;

    void args()
    {
        flag(device, "device", 'd').help("Which device to start the system on");
        subCommand(command, remaining, "command").choices({ "start", "stop" });
    }
};

struct StartArgs : public cli::ArgsBase {
    std::optional<std::string> power;
    std::string system;

    void args()
    {
        flag(power, "power", 'p').help("With how much power to start the system");
        param(system, "system").help("The system to start");
    }
};

struct StopArgs : public cli::ArgsBase {
    bool force = false;
    std::string system;

    void args()
    {
        flag(force, "force", 'f').help("Force stopping of system");
        param(system, "system").help("The system to stop");
    }
};

int main(int argc, char** argv)
{
    auto parser = cli::Parser(argv[0]);
    const auto args = parser.parse<ParentArgs>(argc, argv).value();
    if (args.device) {
        std::cout << "Device: " << *args.device << std::endl;
    }

    auto subParser = cli::Parser(std::string(argv[0]) + " " + args.command);
    if (args.command == "start") {
        const auto subArgs = subParser.parse<StartArgs>(args.remaining).value();
        if (subArgs.power) {
            std::cout << "power: " << *subArgs.power << std::endl;
        }
        std::cout << "Starting system: " << subArgs.system << std::endl;
    } else if (args.command == "stop") {
        const auto subArgs = subParser.parse<StopArgs>(args.remaining).value();
        std::cout << "force: " << subArgs.force << std::endl;
        std::cout << "Stopping system: " << subArgs.system << std::endl;
    }
    return 0;
}
