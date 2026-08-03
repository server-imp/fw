#include "toggleable.hpp"
#include "logger.hpp"

Toggleable::Toggleable(std::string name) : _name(std::move(name)) {}

const std::string& Toggleable::name() const
{
    return _name;
}

bool Toggleable::enabled() const
{
    return _enabled;
}

bool Toggleable::enable()
{
    LOG_INFO("Enabling \"{}\"", _name);

    if (_enabled)
    {
        LOG_WARN("Already enabled");
        return true;
    }

    if (!internalEnable())
    {
        LOG_ERR("Failed to enable \"{}\"", _name);
        return false;
    }

    _enabled = true;
    LOG_INFO("Enabled \"{}\"", _name);
    return true;
}

bool Toggleable::disable()
{
    LOG_INFO("Disabling \"{}\"", _name);

    if (!_enabled)
    {
        LOG_WARN("Already disabled");
        return true;
    }

    if (!internalDisable())
    {
        LOG_ERR("Failed to disable \"{}\"", _name);
        return false;
    }

    _enabled = false;
    LOG_INFO("Disabled \"{}\"", _name);
    return true;
}

bool Toggleable::toggle()
{
    return _enabled ? disable() : enable();
}
