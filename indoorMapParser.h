#pragma once

#include "rapidxml.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <iostream>
#include <sstream>
#include <string>
#include <functional>
#include <fstream>
#include <exception>

#include "indoorMap.h"

namespace Indoor::Map
{
    // Can be used to simply obtain a Map object from XML.
    class MapListener : public IndoorListener
    {
    public:
        std::shared_ptr<Map> map;

        void leaveMap(Map& map) override
        {
            this->map = std::make_shared<Map>(map);
        };
    };

    // The actual parser.
    // You can use readMapFromFile() to simply obtain a Map object.
    // Or use readFromFile() with any IndoorListener implementation for custom logic. (see indoorSvgListener.h)
    class MapParser
    {
    private:
        std::shared_ptr<IndoorListener> listener;

        using xml_node = rapidxml::xml_node<>;
        using xml_attribute = rapidxml::xml_attribute<>;

    public:
        MapParser()
        {

        }

        std::shared_ptr<Map> readMapFromFile(const std::string& filename)
        {
            auto mapListener = std::make_shared<MapListener>();

            readFromFile(filename, mapListener);

            return mapListener->map;
        }

        void readFromFile(const std::string& filename, std::shared_ptr<IndoorListener> listener)
        {
            if (!listener)
                listener = std::make_shared<IndoorListener>(); // create a nop listener

            this->listener = listener;

            std::ifstream fileStream(filename);
            if (fileStream.is_open())
            {
                std::stringstream fileBuffer;
                fileBuffer << fileStream.rdbuf();
                std::string fileContent = fileBuffer.str();

                try
                {
                    rapidxml::xml_document xmlDoc;
                    xmlDoc.parse<0>(fileContent.data());
                    xml_node* xMap = xmlDoc.first_node("map");
                    processMap(xMap);
                }
                catch (const rapidxml::parse_error& e)
                {
                    std::cout << "XML Parser error: " << e.what() << std::endl;
                    throw;
                }
            }
            else
            {
                std::stringstream msg;
                msg << "Indoor map file not found: '" << filename << "'\n";

                std::cout << msg.str();

                throw std::runtime_error(msg.str().c_str());
            }
        }
        
    private:
        static bool tryGetAttribute(const xml_node* node, const std::string& attName, char** value)
        {
            xml_attribute* att = node->first_attribute(attName.c_str());

            if (att)
            {
                *value = att->value();
                return true;
            }

            return false;
        }

        static float floatAttribute(const xml_node* node, const std::string& attName, float defaultValue = 0.0f)
        {
            char* v;
            if (tryGetAttribute(node, attName, &v))
            {
                return std::stof(v);
            }

            return defaultValue;
        }

        static int intAttribute(const xml_node* node, const std::string& attName, int defaultValue = 0)
        {
            char* v;
            if (tryGetAttribute(node, attName, &v))
            {
                return std::stoi(v);
            }

            return defaultValue;
        }

        static bool boolAttribute(const xml_node* node, const std::string& attName, bool defaultValue = false)
        {
            char* v;
            if (tryGetAttribute(node, attName, &v))
            {
                bool result;
                std::istringstream buffer(v);
                buffer >> std::boolalpha >> result;
                return result;
            }

            return defaultValue;
        }

        static std::string strAttribute(const xml_node* node, const std::string& attName, std::string defaultValue = "")
        {
            char* v;
            if (tryGetAttribute(node, attName, &v))
            {
                return std::string(v);
            }

            return defaultValue;
        }

        static void foreachNode(xml_node* node, const std::function<void(xml_node* e)> action)
        {
            for (xml_node* n = node->first_node(); n; n = n->next_sibling()) {
                action(n);
            }
        }

        static void foreachNode(xml_node* node, const std::string& nodeName, const std::function<void(xml_node* n)> action)
        {
            foreachNode(node, [&nodeName, &action](auto n)
            {
                if (std::strcmp(n->name(), nodeName.c_str()) == 0)
                {
                    action(n);
                }
            });
        }

        void processMap(xml_node* xMap)
        {
            Map map;
            map.width = floatAttribute(xMap, "width");
            map.depth = floatAttribute(xMap, "depth");

            listener->enterMap(map);
            
            xml_node* xEarthReg = xMap->first_node("earthReg");
            if (xEarthReg)
            {
                map.earthRegistration = processEarthRegistration(xEarthReg);
            }

            xml_node* xFloors = xMap->first_node("floors");
            if (xFloors)
            {
                foreachNode(xFloors, "floor", [this, &map](xml_node* e) {
                    Floor floor;
                    if (processFloor(e, floor))
                    {
                        map.floors.push_back(floor);
                    }
                });
            }

            listener->leaveMap(map);
        }

        EarthRegistration processEarthRegistration(xml_node* xEarthReg)
        {
            EarthRegistration earthReg;

            listener->enterEarthRegistration(earthReg);

            xml_node* xCorrespondences = xEarthReg->first_node("correspondences");
            if (xCorrespondences)
            {
                foreachNode(xCorrespondences, "point", [this, &earthReg](xml_node* e)
                {
                    EarthPosMapPos pos;
                    pos.lat = floatAttribute(e, "lat");
                    pos.lon = floatAttribute(e, "lon");
                    pos.alt = floatAttribute(e, "alt");

                    pos.x = floatAttribute(e, "mx");
                    pos.y = floatAttribute(e, "my");
                    pos.z = floatAttribute(e, "mz");

                    this->listener->enterEarthPosMapPos(pos);
                    earthReg.correspondences.push_back(pos);
                    this->listener->leaveEarthPosMapPos(pos);
                });
            }

            listener->leaveEarthRegistration(earthReg);
            return earthReg;
        }

        bool processFloor(xml_node* xFloor, Floor& floor)
        {
            floor.atHeight = floatAttribute(xFloor,"atHeight");
            floor.height = floatAttribute(xFloor, "height");
            floor.name = strAttribute(xFloor, "name");

            if (!listener->enterFloor(floor))
            {
                return false;
            }
            else
            {
                // outline
                xml_node* xOutline = xFloor->first_node("outline");
                if (xOutline)
                {
                    Outline outline;
                    if (processOutline(xOutline, outline))
                    {
                        floor.outline = outline;
                    }
                }

                // obstacles
                xml_node* xObstacles = xFloor->first_node("obstacles");
                if (xObstacles)
                {
                    processObstacles(xObstacles, floor);
                }

                // pois
                xml_node* xPois = xFloor->first_node("pois");
                if (xPois)
                {
                    processPointOfInterests(xPois, floor.pois);
                }

                // gtpoints
                xml_node* xGT = xFloor->first_node("gtpoints");
                if (xGT)
                {
                    processGroundtruthPoints(xGT, floor);
                }

                // accesspoints
                xml_node* xAP = xFloor->first_node("accesspoints");
                if (xAP)
                {
                    processAccessPoints(xAP, floor);
                }

                // beacons
                xml_node* xBeacons = xFloor->first_node("beacons");
                if (xBeacons)
                {
                    processBeacons(xBeacons, floor);
                }

                // fingerprints
                xml_node* xFingerprints = xFloor->first_node("fingerprints");
                if (xFingerprints)
                {
                    processFingerprints(xFingerprints, floor);
                }

                // TODO underlays (editor only?)
                // TODO stairs
                // TODO elevators

                listener->leaveFloor(floor);
                return true;
            }
        }

        bool processOutline(xml_node* xOutline, Outline& outline)
        {           
            if (listener->enterOutline(outline))
            {
                foreachNode(xOutline, "polygon", [this, &outline](xml_node* xPolygon)
                {
                    Polygon2D polygon;
                    polygon.name = strAttribute(xPolygon, "name");
                    polygon.method = (PolygonMethod)intAttribute(xPolygon, "method");
                    polygon.isOutdoor = boolAttribute(xPolygon, "outdoor");

                    foreachNode(xPolygon, "point", [&polygon](xml_node* xPoint) {
                        float x = floatAttribute(xPoint, "x");
                        float y = floatAttribute(xPoint, "y");

                        polygon.points.push_back(Point2D(x, y));
                    });

                    outline.polygons.push_back(polygon);
                });
                listener->leaveOutline(outline);
                return true;
            }
            return false;
        }

        void processPointOfInterests(xml_node* xPois, std::vector<PointOfInterest>& pois)
        {
            listener->enterPointOfInterests(pois);
            foreachNode(xPois, "poi", [&pois](xml_node* xPoi)
            {
                PointOfInterest poi;
                poi.name = strAttribute(xPoi, "name");
                poi.type = (POIType)intAttribute(xPoi, "type");
                poi.x = floatAttribute(xPoi, "x");
                poi.y = floatAttribute(xPoi, "y");

                pois.push_back(poi);
            });

            listener->leavePointOfInterests(pois);
        }

        void processGroundtruthPoints(xml_node* xGT, Floor& floor)
        {
            listener->enterGrundtruthPoints(floor.groundtruthPoints);
            foreachNode(xGT, "gtpoint", [&floor](xml_node* xGTpoint)
            {
                GroundtruthPoint gtPoint;
                gtPoint.id = intAttribute(xGTpoint, "id");
                gtPoint.x = floatAttribute(xGTpoint, "x");
                gtPoint.y = floatAttribute(xGTpoint, "y");
                gtPoint.heightAboveFloor = floatAttribute(xGTpoint, "z");

                gtPoint.z = floor.atHeight + gtPoint.heightAboveFloor;

                floor.groundtruthPoints.push_back(gtPoint);
            });
            listener->leaveGrundtruthPoints(floor.groundtruthPoints);
        }

        void processAccessPoints(xml_node* xAP, Floor& floor)
        {
            listener->enterAccessPoints(floor.accessPoints);
            foreachNode(xAP, "accesspoint", [&floor](xml_node* xAccessPoint)
            {
                AccessPoint ap;
                ap.name = strAttribute(xAccessPoint, "name");
                ap.macAddress = strAttribute(xAccessPoint, "mac");
                ap.x = floatAttribute(xAccessPoint, "x");
                ap.y = floatAttribute(xAccessPoint, "y");
                ap.heightAboveFloor = floatAttribute(xAccessPoint, "z");

                ap.z = floor.atHeight + ap.heightAboveFloor;

                ap.mdl_txp = floatAttribute(xAccessPoint, "mdl_txp");
                ap.mdl_exp = floatAttribute(xAccessPoint, "mdl_exp");
                ap.mdl_waf = floatAttribute(xAccessPoint, "mdl_waf");

                floor.accessPoints.push_back(ap);
            });
            listener->leaveAccessPoints(floor.accessPoints);
        }

        void processBeacons(xml_node* xBeacons, Floor& floor)
        {
            listener->enterBeacons(floor.beacons);
            foreachNode(xBeacons, "beacon", [&floor](xml_node* xBeacon) {
                Beacon b;
                b.name = strAttribute(xBeacon, "name");
                b.macAddress = strAttribute(xBeacon, "mac");
                b.uuid = strAttribute(xBeacon, "uuid");

                b.major = strAttribute(xBeacon, "major");
                b.minor = strAttribute(xBeacon, "minor");
                
                b.x = floatAttribute(xBeacon, "x");
                b.y = floatAttribute(xBeacon, "y");
                b.heightAboveFloor = floatAttribute(xBeacon, "z");

                b.z = floor.atHeight + b.heightAboveFloor;

                b.mdl_txp = floatAttribute(xBeacon, "mdl_txp");
                b.mdl_exp = floatAttribute(xBeacon, "mdl_exp");
                b.mdl_waf = floatAttribute(xBeacon, "mdl_waf");

                floor.beacons.push_back(b);
            });
            listener->leaveBeacons(floor.beacons);
        }

        void processFingerprints(xml_node* xFingerprints, Floor& floor)
        {
            listener->enterFingerprintLocations(floor.fingerprintLocations);
            foreachNode(xFingerprints, "location", [&floor](xml_node* xLocation) {
                FingerprintLocation fl;
                fl.name = strAttribute(xLocation, "name");

                fl.x = floatAttribute(xLocation, "x");
                fl.y = floatAttribute(xLocation, "y");
                fl.heightAboveFloor = floatAttribute(xLocation, "dz");

                fl.z = floor.atHeight + fl.heightAboveFloor;

                floor.fingerprintLocations.push_back(fl);
            });

            listener->leaveFingerprintLocations(floor.fingerprintLocations);
        }

        // Converts Walls to wall segments.
        // This method assumes that doors and windows do not overlap!
        void generateWallSegments(Wall& wall)
        {
            if (wall.doors.size() == 0 && wall.windows.size() == 0)
            {
                wall.segments.push_back(WallSegment2D(WallSegmentType::Wall, -1, wall.start(), wall.end()));
                return;
            }

            std::vector<WallSegment2D> segments;

            // Generate door segments
            for (size_t i = 0; i < wall.doors.size(); i++)
            {
                const WallDoor& door = wall.doors[i];

                WallSegment2D segDoor(WallSegmentType::Door, static_cast<int>(i));

                const Point2D dir = wall.end() - wall.start();

                segDoor.start = wall.start() + dir * door.atLinePos;
                segDoor.end = segDoor.start + dir.normalized() * (door.leftRight ? -door.width : +door.width);

                if (segDoor.end.x < segDoor.start.x)
                    std::swap(segDoor.start, segDoor.end);

                segments.push_back(segDoor);
            }

            // Generate window segments
            for (size_t i = 0; i < wall.windows.size(); i++)
            {
                const WallWindow& window = wall.windows[i];

                WallSegment2D segWindow(WallSegmentType::Window, static_cast<int>(i));

                const Point2D dir = wall.end() - wall.start();
                const Point2D center = wall.start() + dir * window.atLinePos;

                segWindow.start = center - dir.normalized() * window.width/2.0f;
                segWindow.end   = center + dir.normalized() * window.width/2.0f;

                if (segWindow.end.x < segWindow.start.x)
                    std::swap(segWindow.start, segWindow.end);

                segments.push_back(segWindow);
            }

            // Order by relative position
            std::sort(segments.begin(), segments.end(), [](WallSegment2D& a, WallSegment2D& b) {
                return a.start.x < b.start.x;
            });


            Point2D wStart = wall.start();
            Point2D wEnd = wall.end();

            if (wEnd.x < wStart.x)
                std::swap(wStart, wEnd);

            // Connect door/window segments with wall segments
            for (size_t i = 0; i < segments.size(); i++)
            {
                if (i == 0)
                {
                    // First wall segment
                    wall.segments.push_back(WallSegment2D(WallSegmentType::Wall, -1, wStart, segments[i].start));
                }

                // Door or window
                wall.segments.push_back(segments[i]);

                if (i < segments.size()-1)
                {
                    // Connection wall
                    wall.segments.push_back(WallSegment2D(WallSegmentType::Wall, -1, segments[i].end, segments[i+1].start));
                }
                else
                {
                    // Last wall segment
                    wall.segments.push_back(WallSegment2D(WallSegmentType::Wall, -1, segments[i].end, wEnd));
                }
            }
        }

        void processObstacles(xml_node* xObstacles, Floor& floor)
        {
            // Other obstacles: line, circle, door, object
            // atm only walls are parsed.

            listener->enterWalls(floor.walls);
            foreachNode(xObstacles, "wall", [this, &floor](xml_node* xWall) {
                Wall wall;

                wall.material = (WallMaterial)intAttribute(xWall, "material");
                wall.type = (ObstacleType)intAttribute(xWall, "type");

                wall.x1 = floatAttribute(xWall, "x1");
                wall.y1 = floatAttribute(xWall, "y1");
                wall.x2 = floatAttribute(xWall, "x2");
                wall.y2 = floatAttribute(xWall, "y2");

                wall.height = floatAttribute(xWall, "height", NAN);
                if (std::isnan(wall.height) || wall.height == 0.0f)
                {
                    wall.height = floor.height;
                }

                wall.thickness = floatAttribute(xWall, "thickness", NAN);
                if (std::isnan(wall.thickness))
                {
                    wall.thickness = 0.15f;
                }

                if (this->listener->enterWall(wall))
                {
                    floor.walls.push_back(wall);

                    // Doors
                    foreachNode(xWall, "door", [this, &wall](xml_node* xDoor) {
                        WallDoor door;

                        door.type = (DoorType)intAttribute(xDoor, "type");
                        door.material = (WallMaterial)intAttribute(xDoor, "material");
                        door.atLinePos = floatAttribute(xDoor, "x01");
                        door.width = floatAttribute(xDoor, "width");
                        door.height = floatAttribute(xDoor, "heigth");
                        door.leftRight = boolAttribute(xDoor, "lr");
                        door.inOut = boolAttribute(xDoor, "io");

                        if (this->listener->enterWallDoor(door))
                        {
                            wall.doors.push_back(door);
                            this->listener->leaveWallDoor(door);
                        }
                    });
                    
                    // Windows
                    foreachNode(xWall, "window", [this, &wall](xml_node* xWindow) {
                        WallWindow window;

                        // window.type = xWindow->IntAttribute("type");
                        window.material = (WallMaterial)intAttribute(xWindow, "material");
                        window.atLinePos = floatAttribute(xWindow, "x01");
                        window.atHeigth = floatAttribute(xWindow, "y");
                        window.width = floatAttribute(xWindow, "width");
                        window.height = floatAttribute(xWindow, "height");
                        window.inOut = boolAttribute(xWindow, "io");

                        if (this->listener->enterWallWindow(window))
                        {
                            wall.windows.push_back(window);
                            this->listener->leaveWallWindow(window);
                        }
                    });

                    this->generateWallSegments(wall);
                    this->listener->leaveWall(wall);
                }
            });
            listener->leaveWalls(floor.walls);
        }
    };


}
