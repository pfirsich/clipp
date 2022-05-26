#include <filesystem>
#include <iostream>

#include "clipp.hpp"

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

class EvenInt {
public:
    EvenInt() = default;

    EvenInt(int64_t value)
        : value_(value)
    {
        assert(value % 2 == 0);
    }

    int64_t value() const
    {
        return value_;
    }

private:
    int64_t value_;
};

template <>
struct clipp::Value<EvenInt> {
    static constexpr std::string_view typeName = "even integer";

    static std::optional<EvenInt> parse(std::string_view str)
    {
        const auto val = clipp::Value<int64_t>::parse(str);
        if (!val || val.value() % 2 != 0) {
            return std::nullopt;
        }
        return val.value();
    }
};

class ExistingFile {
public:
    ExistingFile() = default;

    ExistingFile(std::string path)
        : path_(std::move(path))
    {
        // If this was real code, I would be worried about race-conditions
        assert(std::filesystem::exists(path_));
    }

    const std::string& path() const
    {
        return path_;
    }

private:
    std::string path_;
};

template <>
struct clipp::Value<ExistingFile> {
    static constexpr std::string_view typeName = "existing file";

    static std::optional<ExistingFile> parse(std::string_view str)
    {
        if (!std::filesystem::exists(str)) {
            return std::nullopt;
        }
        return std::string(str);
    }
};

struct Args : public clipp::ArgsBase {
    MyEnum myEnum;
    EvenInt evenInt;
    ExistingFile existingFile;

    void args()
    {
        // Parsing will fail even without specifying `choices` here, but if you do, the error
        // message will tell you what strings are allowed.
        positional(myEnum, "enum").choices({ "a", "b", "c" });
        positional(evenInt, "even");
        positional(existingFile, "file");
    }
};

int main(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<Args>(argc, argv).value();
    std::cout << "enum: " << static_cast<int>(args.myEnum) << std::endl;
    std::cout << "even int: " << args.evenInt.value() << std::endl;
    std::cout << "file: " << args.existingFile.path() << std::endl;
    return 0;
}
