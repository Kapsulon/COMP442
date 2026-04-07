#pragma once

#include <unordered_map>
#include <vector>
#include "Problems/Problems.hpp"

class Compiler
{
public:
    struct Settings {
        std::vector<std::string> files;
    };

    struct Output {
        std::string source_file;
        std::string assembly;
        lang::Problems problems;
    };

    Compiler(const Settings &settings);

    std::unordered_map<std::string, Output> compileAll();

private:
    Output compile(const std::string &file);

    const Settings m_settings;
};
