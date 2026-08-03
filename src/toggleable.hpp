#ifndef FW_TOGGLEABLE_HPP
#define FW_TOGGLEABLE_HPP

class Toggleable
{
private:
    std::string _name {};
    bool        _enabled {};

protected:
    explicit Toggleable(std::string name);

    virtual bool internalEnable()  = 0;
    virtual bool internalDisable() = 0;

public:
    virtual ~Toggleable() = default;
    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] bool               enabled() const;
    bool                             enable();
    bool                             disable();
    bool                             toggle();
};

using PToggleable = std::shared_ptr<Toggleable>;

#endif // FW_TOGGLEABLE_HPP
