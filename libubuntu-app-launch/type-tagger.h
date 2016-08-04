#pragma once

namespace ubuntu
{
namespace app_launch
{

/** \brief A small template to make it clearer when special types are being used

    The TypeTagger a small piece of C++ so that we can have custom types
    for things in the Ubuntu App Launch API that should be handled in
    special ways, but really have basic types at their core. In this way
    there is explicit code to convert these items into their fundamental type
    so that is obvious and can be easily searched for.
*/
template <typename Tag, typename T>
class TypeTagger
{
public:
    /** Function to build a TypeTagger object from a fundamental type */
    static TypeTagger<Tag, T> from_raw(const T& value)
    {
        return TypeTagger<Tag, T>(value);
    }
    /** Getter to get the fundamental type out of the TypeTagger wrapper */
    const T& value() const
    {
        return _value;
    }
    /** Getter to get the fundamental type out of the TypeTagger wrapper */
    operator T() const
    {
        return _value;
    }
    ~TypeTagger()
    {
    }

private:
    /** Private constructor used by from_raw() */
    TypeTagger(const T& value)
        : _value(value)
    {
    }
    T _value; /**< The memory allocation for the fundamental type */
};

}  // namespace app_launch
}  // namespace ubuntu
