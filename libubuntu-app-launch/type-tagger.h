#pragma once

namespace ubuntu
{
namespace app_launch
{

template <typename Tag, typename T>
class TypeTagger
{
public:
    static TypeTagger<Tag, T> from_raw(const T& value)
    {
        return TypeTagger<Tag, T>(value);
    }
    const T& value() const
    {
        return _value;
    }
    operator T() const
    {
        return _value;
    }
    ~TypeTagger(void)
    {
    }

private:
    TypeTagger(const T& value)
        : _value(value)
    {
    }
    T _value;
};

};  // namespace app_launch
};  // namespace ubuntu
