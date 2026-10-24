#ifndef IFREEWB_H
#define IFREEWB_H

namespace freewb
{
class IFreewb
{
public:
    virtual ~IFreewb() = default;
    virtual const char *name() const = 0;
    virtual bool available() const = 0;
    virtual void changeAvailable() = 0;
};
} // namespace freewb

#endif