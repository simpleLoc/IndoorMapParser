#pragma once

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

#include "indoorMap.h"

namespace Indoor::Map
{
    struct SvgColor
    {
        int R, G, B, A;

        void setRGB(int r, int g, int b)
        {
            R = std::clamp(r, 0, 255);
            G = std::clamp(g, 0, 255);
            B = std::clamp(b, 0, 255);
        }

        std::string asSvgString() const
        {
            std::string str = "#000000";
            snprintf(&str[0], str.size() + 1, "#%02X%02X%02X", R, G, B);
            return str;
        }
    };

    class SvgListener : public IndoorListener
    {
    private:
        std::stringstream svg;
        int indentLevel = 0;
        std::map<WallMaterial, SvgColor> colorMap;

        std::string indent() const
        {
            return std::string(2 * indentLevel, ' ');
        }

        std::string qs(const std::string& str) const
        {
            return "'" + str + "'";
        }

        std::string qs(float value) const
        {
            return qs(std::to_string(value));
        }

        std::string qs(const SvgColor& value) const
        {
            return qs(value.asSvgString());
        }

        std::string svgPathStr(const Point2D& a, const Point2D& b)
        {
            return svgPathStr({ a, b }, false);
        }

        std::string svgPathStr(const std::vector<Point2D>& pts, bool pathClosed = false)
        {
            std::stringstream result;

            if (pts.size() > 1)
            {
                result << "M" << pts[0].x << " " << -pts[0].y;

                for (size_t i = 1; i < pts.size(); i++)
                {
                    result << " L" << pts[i].x << " " << -pts[i].y;
                }

                if (pathClosed)
                    result << " Z";
            }

            return result.str();
        }

        std::string svgArcStr(Point2D center, float radius, float startAngle, float endAngle)
        {
            center.y = -center.y;

            const Point2D start = Point2D::fromPolar(center, radius, startAngle);
            const Point2D end = Point2D::fromPolar(center, radius, endAngle);

            const std::string largeArcFlag = (endAngle - startAngle <= M_PI) ? "0" : "1";

            std::stringstream arcStr;
            arcStr << " M " << start.x << " " << start.y
                << " A " << radius << " " << radius << " "  // radius
                << 0 << " "  // roation x
                << largeArcFlag << " "  // large arc?
                << 1 << " "  // counter-clockwise?
                << end.x << " " << end.y;  // endpoint

            return arcStr.str();
        }

    public:
        SvgListener()
        {
            setMaterialColor(WallMaterial::Unknown, 0, 0, 0);
            setMaterialColor(WallMaterial::Drywall, 100, 100, 100);
            setMaterialColor(WallMaterial::Concrete, 50, 50, 50);
            setMaterialColor(WallMaterial::Glass, 0, 110, 255);
            setMaterialColor(WallMaterial::Metalized_Glas, 0, 220, 255);
            setMaterialColor(WallMaterial::Metal, 114, 159, 207);
            setMaterialColor(WallMaterial::Wood, 206, 92, 0);
        }

        void setMaterialColor(WallMaterial material, int r, int g, int b)
        {
            colorMap[material].setRGB(r, g, b);
        }

        std::string svgString() const
        {
            return svg.str();
        }

        void saveSvgToFile(const std::string& filename)
        {
            std::ofstream out(filename);
            out << svgString();
            out.close();
        }

        void enterMap(Map& map) override
        {
            svg << "<svg xmlns='http://www.w3.org/2000/svg'>" << std::endl;
            indentLevel++;
        };

        bool enterFloor(Floor& floor) override
        {
            svg << indent() << "<g id=" << qs("floor_" + floor.name) << ">" << std::endl;
            indentLevel++;

            return true;
        }

        void leaveOutline(Outline& outline) override
        {
            for (auto& polygon : outline.polygons)
            {
                std::string color;
                if (polygon.method == PolygonMethod::Remove) {
                    color = "#FFFFFF";
                }
                else if (polygon.isOutdoor) {
                    color = "#4E9A06"; // green
                }
                else {
                    color = "#C8C8C8"; // light gray
                }

                std::string svgPath = svgPathStr(polygon.points);

                svg << indent() << "<path d=" << qs(svgPath)
                    << " stroke='none'"
                    << " fill=" << qs(color)
                    << "/>" << std::endl;
            }
        }

        void leaveWall(Wall& wall) override
        {
            for (const WallSegment2D& seg : wall.segments)
            {
                switch (seg.type)
                {
                case WallSegmentType::Wall:
                {
                    svg << indent() << "<path d=" << qs(svgPathStr(seg.start, seg.end))
                        << " stroke=" << qs(colorMap[wall.material])
                        << " stroke-width=" << qs(wall.thickness)
                        << " fill='none'"
                        << "/>" << std::endl;
                    break;
                }
                case WallSegmentType::Window:
                {
                    svg << indent() << "<path d=" << qs(svgPathStr(seg.start, seg.end))
                        << " stroke='#0000FF'"
                        << " stroke-width=" << qs(wall.thickness - 0.1f)
                        << " stroke-dasharray='0.2, 0.1'"
                        << " fill='none'"
                        << "/>" << std::endl;
                    break;
                }
                case WallSegmentType::Door:
                {
                    const float M_PIf = static_cast<float>(M_PI);
                    const float M_PI_2f = static_cast<float>(M_PI_2);

                    // TODO different door types
                    const WallDoor& door = wall.doors[seg.listIndex];

                    Point2D openDir = (seg.end - seg.start).orthogonal().normalized();

                    if (door.inOut)
                        openDir = openDir * -1;

                    const Point2D doorHinge = door.leftRight ? seg.end : seg.start;
                    const Point2D doorLock = door.leftRight ? seg.start : seg.end;
                    const Point2D doorLeaf = doorHinge + openDir * door.width;

                    const float doorWidth = 0.9f * door.width;
                    float startAngle, endAngle;
                    if (door.inOut)
                    {
                        if (door.leftRight)
                        {
                            startAngle = M_PI_2f;
                            endAngle = M_PIf;
                        }
                        else
                        {
                            startAngle = 0;
                            endAngle = M_PI_2f;
                        }
                    }
                    else
                    {
                        if (door.leftRight)
                        {
                            startAngle = M_PIf;
                            endAngle = -M_PI_2f;
                        }
                        else
                        {
                            startAngle = -M_PI_2f;
                            endAngle = 0;
                        }
                    }

                    svg << indent() << "<path d=" << qs(svgPathStr(seg.start, seg.end))
                        << " stroke='#000000'"
                        << " stroke-width=" << qs(wall.thickness - 0.1f)
                        << " stroke-dasharray='0.2, 0.1'"
                        << " fill='none'"
                        << "/>" << std::endl;

                    svg << indent() << "<path d=" << qs(svgArcStr(doorHinge, doorWidth, startAngle, endAngle))
                        << " stroke='#000000'"
                        << " stroke-width=" << qs(wall.thickness - 0.1f)
                        << " fill='none'"
                        << "/>" << std::endl;


                    svg << indent() << "<path d=" << qs(svgPathStr(doorHinge, doorLeaf))
                        << " stroke='#000000'"
                        << " stroke-width=" << qs(wall.thickness - 0.1f)
                        << " fill='none'"
                        << "/>" << std::endl;

                    break;
                }
                }
            }


        }

        void leaveGrundtruthPoints(std::vector<GroundtruthPoint>& gtPoints) override
        {
            for (const auto& gtPt : gtPoints)
            {
                svg << indent() << "<circle "
                    << " cx=" << qs(gtPt.x)
                    << " cy=" << qs(-gtPt.y)
                    << " r='0.125'"
                    << " fill='#000000'"
                    << " stroke='none'"
                    << "/>" << std::endl;

                svg << indent() << "<text "
                    << " x=" << qs(gtPt.x + 0.25f)
                    << " y=" << qs(-gtPt.y)
                    << " style=" << qs("font: 0.5px sans-serif;")
                    << " text-anchor='start'"
                    << ">" << gtPt.id << "</text>"
                    << std::endl;
            }
        }

        void leaveAccessPoints(std::vector<AccessPoint>& accessPoints) override
        {
            for (const auto& apPt : accessPoints)
            {
                svg << indent() << "<circle "
                    << " cx=" << qs(apPt.x)
                    << " cy=" << qs(-apPt.y)
                    << " r='0.125'"
                    << " fill='#FF0000'"
                    << " stroke='none'"
                    << "/>" << std::endl;

                svg << indent() << "<text "
                    << " x=" << qs(apPt.x + 0.25f)
                    << " y=" << qs(-apPt.y)
                    << " style=" << qs("font: 0.5px sans-serif;")
                    << " text-anchor='start'"
                    << ">" << apPt.name << " (" << apPt.macAddress << ")" << "</text>"
                    << std::endl;
            }
        }

        void leavePointOfInterests(std::vector<PointOfInterest>& pois) override
        {
            for (const PointOfInterest& poi : pois)
            {
                svg << indent() << "<text "
                    << " x=" << qs(poi.x)
                    << " y=" << qs(-poi.y - 0.25f)
                    << " style=" << qs("font: 0.5px sans-serif;")
                    << " text-anchor='middle'"
                    << ">" << poi.name << "</text>"
                    << std::endl;
            }
        }

        void leaveFloor(Floor& floor) override
        {
            indentLevel--;
            svg << indent() << "</g>" << std::endl;
        }

        void leaveMap(Map& map) override
        {
            indentLevel--;
            svg << "</svg>" << std::endl;
        };
    };
}
