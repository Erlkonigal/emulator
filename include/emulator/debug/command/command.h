#pragma once

#include <sstream>
#include <string>

class DebugContext;

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string name() const = 0;
    virtual std::string help() const = 0;
    virtual void execute(std::istringstream& args, DebugContext& ctx) = 0;
};