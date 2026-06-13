
#include "Geometry.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double Epsilon = 1e-9;

    bool isZero(const double value)
    {
        return std::abs(value) < Epsilon;
    }

    bool haveDifferentSigns(const double first, const double second)
    {
        return (first > Epsilon && second < -Epsilon)
            || (first < -Epsilon && second > Epsilon);
    }

    bool isBetween(const double value, const double min, const double max)
    {
        return value >= min - Epsilon && value <= max + Epsilon;
    }

    bool onSegment(const Node& a, const Node& b, const Node& p)
    {
        return isZero(Geometry::orientation(a, b, p))
            && isBetween(p.x, std::min(a.x, b.x), std::max(a.x, b.x))
            && isBetween(p.y, std::min(a.y, b.y), std::max(a.y, b.y));
    }
}

namespace Geometry
{
    double orientation(const Node& a, const Node& b, const Node& c)
    {
        return (b.x - a.x) * (c.y - a.y)
             - (b.y - a.y) * (c.x - a.x);
    }

    bool segmentsIntersect(
        const Node& a,
        const Node& b,
        const Node& c,
        const Node& d
    )
    {
        const double o1 = orientation(a, b, c);
        const double o2 = orientation(a, b, d);
        const double o3 = orientation(c, d, a);
        const double o4 = orientation(c, d, b);

        if (haveDifferentSigns(o1, o2) && haveDifferentSigns(o3, o4))
        {
            return true;
        }

        if (isZero(o1) && onSegment(a, b, c))
        {
            return true;
        }

        if (isZero(o2) && onSegment(a, b, d))
        {
            return true;
        }

        if (isZero(o3) && onSegment(c, d, a))
        {
            return true;
        }

        if (isZero(o4) && onSegment(c, d, b))
        {
            return true;
        }

        return false;
    }
}
