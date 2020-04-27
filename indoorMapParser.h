#pragma once

#include "tinyxml2.h"

#include <algorithm>
#include <memory>
#include <iostream>
#include <sstream>
#include <string>
#include <functional>

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

            tinyxml2::XMLDocument xmlDoc;
            tinyxml2::XMLError result = xmlDoc.LoadFile(filename.c_str());

            if (result == tinyxml2::XMLError::XML_SUCCESS)
            {
                tinyxml2::XMLElement* xMap = xmlDoc.FirstChildElement();
                processMap(xMap);
            }
            else
            {
                std::stringstream msg;
                msg << "Error reading indoor map xml '" << filename << "' with error: " << std::endl;
                msg << xmlDoc.ErrorStr() << std::endl;

                std::cout << msg.str();

                throw std::exception(msg.str().c_str());
            }
        }

    private:
        static void foreachNode(tinyxml2::XMLElement* element, const std::function<void(tinyxml2::XMLElement* e)> action)
        {
            for (tinyxml2::XMLElement* e = element->FirstChildElement(); e; e = e->NextSiblingElement())
            {
                action(e);
            }
        }

        static void foreachNode(tinyxml2::XMLElement* element, const std::string& elementName, const std::function<void(tinyxml2::XMLElement* e)> action)
        {
            foreachNode(element, [&elementName, &action](auto e)
            {
                if (std::strcmp(e->Name(), elementName.c_str()) == 0)
                {
                    action(e);
                }
            });
        }

        void processMap(tinyxml2::XMLElement* xMap)
        {
            Map map;
            map.width = xMap->FloatAttribute("width");
            map.depth = xMap->FloatAttribute("depth");

            listener->enterMap(map);
            
            tinyxml2::XMLElement* xEarthReg = xMap->FirstChildElement("earthReg");
            if (xEarthReg)
            {
                map.earthRegistration = processEarthRegistration(xEarthReg);
            }

            tinyxml2::XMLElement* xFloors = xMap->FirstChildElement("floors");
            if (xFloors)
            {
                foreachNode(xFloors, "floor", [this, &map](tinyxml2::XMLElement* e) {
                    Floor floor;
                    if (processFloor(e, floor))
                    {
                        map.floors.push_back(floor);
                    }
                });
            }

            listener->leaveMap(map);
        }

        EarthRegistration processEarthRegistration(tinyxml2::XMLElement* xEarthReg)
        {
            EarthRegistration earthReg;

            listener->enterEarthRegistration(earthReg);

            tinyxml2::XMLElement* xCorrespondences = xEarthReg->FirstChildElement("correspondences");
            if (xCorrespondences)
            {
                foreachNode(xCorrespondences, "point", [this, &earthReg](tinyxml2::XMLElement* e)
                {
                    EarthPosMapPos pos;
                    pos.lat = e->FloatAttribute("lat");
                    pos.lon = e->FloatAttribute("lon");
                    pos.alt = e->FloatAttribute("alt");

                    pos.x = e->FloatAttribute("mx");
                    pos.y = e->FloatAttribute("my");
                    pos.z = e->FloatAttribute("mz");

                    this->listener->enterEarthPosMapPos(pos);
                    earthReg.correspondences.push_back(pos);
                    this->listener->leaveEarthPosMapPos(pos);
                });
            }

            listener->leaveEarthRegistration(earthReg);
            return earthReg;
        }

        bool processFloor(tinyxml2::XMLElement* xFloor, Floor& floor)
        {
            floor.atHeight = xFloor->FloatAttribute("atHeight");
            floor.height = xFloor->FloatAttribute("height");
            floor.name = xFloor->Attribute("name");

            if (!listener->enterFloor(floor))
            {
                return false;
            }
            else
            {
                // outline
                tinyxml2::XMLElement* xOutline = xFloor->FirstChildElement("outline");
                if (xOutline)
                {
                    Outline outline;
                    if (processOutline(xOutline, outline))
                    {
                        floor.outline = outline;
                    }
                }

                // obstacles
                tinyxml2::XMLElement* xObstacles = xFloor->FirstChildElement("obstacles");
                if (xObstacles)
                {
                    processObstacles(xObstacles, floor);
                }

                // pois
                tinyxml2::XMLElement* xPois = xFloor->FirstChildElement("pois");
                if (xPois)
                {
                    processPointOfInterests(xPois, floor.pois);
                }

                // gtpoints
                tinyxml2::XMLElement* xGT = xFloor->FirstChildElement("gtpoints");
                if (xGT)
                {
                    processGroundtruthPoints(xGT, floor);
                }

                // accesspoints
                tinyxml2::XMLElement* xAP = xFloor->FirstChildElement("accesspoints");
                if (xAP)
                {
                    processAccessPoints(xAP, floor);
                }

                // beacons
                tinyxml2::XMLElement* xBeacons = xFloor->FirstChildElement("beacons");
                if (xBeacons)
                {
                    processBeacons(xBeacons, floor);
                }

                // fingerprints
                tinyxml2::XMLElement* xFingerprints = xFloor->FirstChildElement("fingerprints");
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

        bool processOutline(tinyxml2::XMLElement* xOutline, Outline& outline)
        {           
            if (listener->enterOutline(outline))
            {
                foreachNode(xOutline, "polygon", [this, &outline](tinyxml2::XMLElement* xPolygon)
                {
                    Polygon2D polygon;
                    polygon.name = xPolygon->Attribute("name");
                    polygon.method = (PolygonMethod)xPolygon->IntAttribute("method");
                    polygon.isOutdoor = xPolygon->BoolAttribute("outdoor");

                    foreachNode(xPolygon, "point", [&polygon](tinyxml2::XMLElement* xPoint) {
                        float x = xPoint->FloatAttribute("x");
                        float y = xPoint->FloatAttribute("y");

                        polygon.points.push_back(Point2D(x, y));
                    });

                    outline.polygons.push_back(polygon);
                });
                listener->leaveOutline(outline);
                return true;
            }
            return false;
        }

        void processPointOfInterests(tinyxml2::XMLElement* xPois, std::vector<PointOfInterest>& pois)
        {
            listener->enterPointOfInterests(pois);
            foreachNode(xPois, "poi", [&pois](tinyxml2::XMLElement* xPoi)
            {
                PointOfInterest poi;
                poi.name = xPoi->Attribute("name");
                poi.type = (POIType)xPoi->IntAttribute("type");
                poi.x = xPoi->FloatAttribute("x");
                poi.y = xPoi->FloatAttribute("y");

                pois.push_back(poi);
            });

            listener->leavePointOfInterests(pois);
        }

        void processGroundtruthPoints(tinyxml2::XMLElement* xGT, Floor& floor)
        {
            listener->enterGrundtruthPoints(floor.groundtruthPoints);
            foreachNode(xGT, "gtpoint", [&floor](tinyxml2::XMLElement* xGTpoint)
            {
                GroundtruthPoint gtPoint;
                gtPoint.id = xGTpoint->IntAttribute("id");
                gtPoint.x = xGTpoint->FloatAttribute("x");
                gtPoint.y = xGTpoint->FloatAttribute("y");
                gtPoint.heightAboveFloor = xGTpoint->FloatAttribute("z");

                gtPoint.z = floor.atHeight + gtPoint.heightAboveFloor;

                floor.groundtruthPoints.push_back(gtPoint);
            });
            listener->leaveGrundtruthPoints(floor.groundtruthPoints);
        }

        void processAccessPoints(tinyxml2::XMLElement* xAP, Floor& floor)
        {
            listener->enterAccessPoints(floor.accessPoints);
            foreachNode(xAP, "accesspoint", [&floor](tinyxml2::XMLElement* xAccessPoint)
            {
                AccessPoint ap;
                ap.name = xAccessPoint->Attribute("name");
                ap.macAddress = xAccessPoint->Attribute("mac");
                ap.x = xAccessPoint->FloatAttribute("x");
                ap.y = xAccessPoint->FloatAttribute("y");
                ap.heightAboveFloor = xAccessPoint->FloatAttribute("z");

                ap.z = floor.atHeight + ap.heightAboveFloor;

                ap.mdl_txp = xAccessPoint->FloatAttribute("mdl_txp");
                ap.mdl_exp = xAccessPoint->FloatAttribute("mdl_exp");
                ap.mdl_waf = xAccessPoint->FloatAttribute("mdl_waf");

                floor.accessPoints.push_back(ap);
            });
            listener->leaveAccessPoints(floor.accessPoints);
        }

        void processBeacons(tinyxml2::XMLElement* xBeacons, Floor& floor)
        {
            listener->enterBeacons(floor.beacons);
            foreachNode(xBeacons, "beacon", [&floor](tinyxml2::XMLElement* xBeacon) {
                Beacon b;
                b.name = xBeacon->Attribute("name");
                b.macAddress = xBeacon->Attribute("mac");
                b.uuid = xBeacon->Attribute("uuid");

                b.major = xBeacon->Attribute("major");
                b.minor = xBeacon->Attribute("minor");
                
                b.x = xBeacon->FloatAttribute("x");
                b.y = xBeacon->FloatAttribute("y");
                b.heightAboveFloor = xBeacon->FloatAttribute("z");

                b.z = floor.atHeight + b.heightAboveFloor;

                b.mdl_txp = xBeacon->FloatAttribute("mdl_txp");
                b.mdl_exp = xBeacon->FloatAttribute("mdl_exp");
                b.mdl_waf = xBeacon->FloatAttribute("mdl_waf");

                floor.beacons.push_back(b);
            });
            listener->leaveBeacons(floor.beacons);
        }

        void processFingerprints(tinyxml2::XMLElement* xFingerprints, Floor& floor)
        {
            listener->enterFingerprintLocations(floor.fingerprintLocations);
            foreachNode(xFingerprints, "location", [&floor](tinyxml2::XMLElement* xLocation) {
                FingerprintLocation fl;
                fl.name = xLocation->Attribute("name");

                fl.x = xLocation->FloatAttribute("x");
                fl.y = xLocation->FloatAttribute("y");
                fl.heightAboveFloor = xLocation->FloatAttribute("dz");

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

        void processObstacles(tinyxml2::XMLElement* xObstacles, Floor& floor)
        {
            // Other obstacles: line, circle, door, object
            // atm only walls are parsed.

            listener->enterWalls(floor.walls);
            foreachNode(xObstacles, "wall", [this, &floor](tinyxml2::XMLElement* xWall) {
                Wall wall;

                wall.material = (WallMaterial)xWall->IntAttribute("material");
                wall.type = (ObstacleType)xWall->IntAttribute("type");

                wall.x1 = xWall->FloatAttribute("x1");
                wall.y1 = xWall->FloatAttribute("y1");
                wall.x2 = xWall->FloatAttribute("x2");
                wall.y2 = xWall->FloatAttribute("y2");

                wall.height = xWall->FloatAttribute("height", NAN);
                if (std::isnan(wall.height) || wall.height == 0.0f)
                {
                    wall.height = floor.height;
                }

                wall.thickness = xWall->FloatAttribute("thickness", NAN);
                if (std::isnan(wall.thickness))
                {
                    wall.thickness = 0.15f;
                }

                if (this->listener->enterWall(wall))
                {
                    floor.walls.push_back(wall);

                    // Doors
                    foreachNode(xWall, "door", [this, &wall](tinyxml2::XMLElement* xDoor) {
                        WallDoor door;

                        door.type = (DoorType)xDoor->IntAttribute("type");
                        door.material = (WallMaterial)xDoor->IntAttribute("material");
                        door.atLinePos = xDoor->FloatAttribute("x01");
                        door.width = xDoor->FloatAttribute("width");
                        door.height = xDoor->FloatAttribute("heigth");
                        door.leftRight = xDoor->BoolAttribute("lr");
                        door.inOut = xDoor->BoolAttribute("io");

                        if (this->listener->enterWallDoor(door))
                        {
                            wall.doors.push_back(door);
                            this->listener->leaveWallDoor(door);
                        }
                    });
                    
                    // Windows
                    foreachNode(xWall, "window", [this, &wall](tinyxml2::XMLElement* xWindow) {
                        WallWindow window;

                        // window.type = xWindow->IntAttribute("type");
                        window.material = (WallMaterial)xWindow->IntAttribute("material");
                        window.atLinePos = xWindow->FloatAttribute("x01");
                        window.atHeigth = xWindow->FloatAttribute("y");
                        window.width = xWindow->FloatAttribute("width");
                        window.height = xWindow->FloatAttribute("height");
                        window.inOut = xWindow->BoolAttribute("io");

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
