#pragma once

#include <string>
#include <ostream>
#include <vector>

namespace Indoor::Map
{
    // Simple 2D vector structure used by the parser
    struct Point2D
    {
        float x, y;

        Point2D()
            : x(0.0f), y(0.0f)
        {}

        Point2D(float x, float y)
            : x(x), y(y)
        {}

        Point2D operator+ (const Point2D& p) const { return Point2D(x + p.x, y + p.y); }
        Point2D operator- (const Point2D& p) const { return Point2D(x - p.x, y - p.y); }

        Point2D operator* (const float v) const { return Point2D(x * v, y * v); }
        Point2D operator/ (const float v) const { return Point2D(x / v, y / v); }

        bool operator== (const Point2D& p) const { return x == p.x && y == p.y; }
        bool operator!= (const Point2D& p) const { return !(*this == p); }

        Point2D orthogonal() const { return Point2D(-y, x); }

        float length() const { return std::sqrt(x*x + y*y); }
        Point2D normalized() const { return (*this) / length(); }


        static Point2D fromPolar(const Point2D& center, float radius, float angle)
        {
            return Point2D(center.x + radius * std::cos(angle), center.y + radius * std::sin(angle));
        }
    };

    static std::ostream& operator<<(std::ostream& os, const Point2D& p)
    {
        return os << "(" << p.x << "; " << p.y << ")";
    }

    enum class PolygonMethod : int
    {
        Add,
        Remove
    };

    // Represents a part of an outline.
    struct Polygon2D
    {
        std::string name;

        // PolygonMethod::Add is the default.
        // PolygonMethod::Remove can be used to remove the inner parts of other polygons.
        PolygonMethod method;

        // Represents an area wich is outside of the building like a yard.
        bool isOutdoor;

        // The points which define the polygon
        std::vector<Point2D> points;
    };

    // Represents the walkable area, i.e. the ground.
    // Each floor as one outline which is made off multiple polygons.
    // Non-walkable areas inside the floor can be modeled with PolygonMethod::Remove.
    struct Outline
    {
        std::vector<Polygon2D> polygons;
    };

    enum class POIType 
    {
        Room
    };

    // This object is used to mark rooms
    struct PointOfInterest
    {
        std::string name;
        POIType type;
        float x, y;
    };

    // Represents an orientation point for the walks.
    // At every turn of a walk a groundtruth point is placed on the map.
    // The groundtruth path is defined as a list of IDs.
    struct GroundtruthPoint
    {
        int id;
        float x, y, z;

        // z Position relative to the floor's ground
        float heightAboveFloor;
    };

    // Location where fingerprints are recorded. Not the fingerprints themselves.
    struct FingerprintLocation
    {
        std::string name;

        // Position
        float x, y, z;

        // z Position relative to the floor's ground
        float heightAboveFloor;
    };

    // Represents a Bluetooth beacon
    struct Beacon
    {
        std::string name;
        std::string macAddress;
        std::string uuid;

        std::string major;
        std::string minor;

        // Position
        float x, y, z;

        // z Position relative to the floor's ground
        float heightAboveFloor;

        // Model parameter
        float mdl_txp, mdl_exp, mdl_waf;
    };

    // Represents a WiFi access point
    struct AccessPoint
    {
        std::string name;
        std::string macAddress;

        // Position
        float x, y, z;

        // z Position relative to the floor's ground
        float heightAboveFloor;

        // Model parameter
        // see: Ebner, F.; Fetzer, T.; Deinzer, F.; Grzegorzek, M. 
        //      On Wi-Fi Model Optimizations for Smartphone-Based Indoor Localization.
        //      ISPRS Int. J. Geo-Inf. 2017, 6, 233.
        //      https://www.mdpi.com/2220-9964/6/8/233
        float mdl_txp; // sending power
        float mdl_exp; // path-loss-exponent
        float mdl_waf; // attenuation per ceiling/floor
    };

    enum class WallMaterial
    {
        Unknown,
        Concrete,
        Wood,
        Drywall,
        Glass,
        Metal,
        Metalized_Glas
    };

    enum class DoorType
    {
        Unknown,
        Swing,			// Normal
        DoubleSwing,
        Slide,			// "Schiebetuer"
        DoubleSlide,
        Revolving       // "Drehtuer"
    };

    enum class ObstacleType
    {
        Unknown,
        Wall,
        Window,
        Handrail,
        Pillar
    };

    // Base struct for doors and windows
    struct WallElement
    {
        WallMaterial material;

        // Door/Window geometry
        float width;
        float height;

        // Position relative to the walls start point.
        // The value ranges from 0 to 1.
        float atLinePos;
    };

    // Represents a door relatively positioned on a wall
    struct WallDoor : public WallElement
    {
        DoorType type;

        // Handle position; True if the hinge is on the right.
        bool leftRight;

        // Opening direction
        bool inOut;
    };

    // Represents a window relatively positioned on a wall
    struct WallWindow : public WallElement
    {
        // Relative Y position to the wall object
        float atHeigth;

        // Opening direction
        bool inOut;
    };

    enum class WallSegmentType
    {
        Wall,
        Door,
        Window
    };

    // Represents a wall segment, door or window with absolute start and end points.
    struct WallSegment2D
    {
        // Index for Wall.doors or Wall.windows
        // Or -1 if segment is of wall type
        int listIndex;

        WallSegmentType type;

        Point2D start;
        Point2D end;

        WallSegment2D(WallSegmentType type, int listIndex = -1, Point2D start = Point2D(), Point2D end = Point2D())
            : type(type), listIndex(listIndex), start(start), end(end)
        { }
    };

    // Represents a wall with optional doors and windows.
    struct Wall
    {
        WallMaterial material;
        ObstacleType type;

        // The wall as a 2D line.
        float x1, y1;
        float x2, y2;

        Point2D start() const { return Point2D(x1, y1); }
        Point2D end()   const { return Point2D(x2, y2); }

        // Wall thickness
        float thickness;

        // The individual height of the wall. If equal to zero in the xml the floor's height is used.
        float height;

        // Within our model doors and windows are parts of the wall.
        // The wall is defined as a line with thickness.
        // Doors are positioned relativly on the wall, likewise windows.
        std::vector<WallDoor> doors;
        std::vector<WallWindow> windows;

        // Segments of the wall. Each segment represents continuous piece of wall, door or window.
        std::vector<WallSegment2D> segments;
    };

    // Represents a single floor of the building.
    struct Floor
    {
        // Z position of the ground.
        float atHeight;

        // The height of this floor.
        // This also defines the default height of every wall.
        float height;

        std::string name;

        // Defines the walkable area
        Outline outline;

        // Contains all walls
        std::vector<Wall> walls;

        std::vector<AccessPoint> accessPoints;
        std::vector<Beacon> beacons;
        std::vector<GroundtruthPoint> groundtruthPoints;
        std::vector<FingerprintLocation> fingerprintLocations;
        std::vector<PointOfInterest> pois;


        bool gtPointById(int id, GroundtruthPoint& result)
        {
            auto it = std::find_if(groundtruthPoints.begin(), groundtruthPoints.end(), [id](const auto& gt) { return gt.id == id; });

            if (it != groundtruthPoints.end())
            {
                result = *it;
                return true;
            }
            else
            {
                return false;
            }
        }
    };

    struct EarthPosMapPos
    {
        // Position in earth coordinates
        float lat, lon, alt;

        // Position in map coordinates
        float x, y, z;
    };

    // This object is used to associate global coordinates to map coordinates.
    // This information is used to transform map coordinates to GPS compatible coordinates.
    struct EarthRegistration
    {
        std::vector<EarthPosMapPos> correspondences;
    };

    // This is the root object of every map file.
    struct Map
    {
        float width;
        float depth;

        EarthRegistration earthRegistration;
        std::vector<Floor> floors;
    };


    // This class can be used to as an interface to the parser.
    // Each enter* method is called after the XML tag is parsed.
    // At this point attributes are parsed but child tags are not processed yet.
    // Some enter* methods have a boolean return type to indicate the parser to skip this tag (return false).
    // When a leave* method is called the element is fully processed.
    // See indoorSvgListener.h for an example.
    class IndoorListener
    {
    public:
        virtual void enterMap(Map& map) {};
        virtual void leaveMap(Map& map) {};

        virtual void enterEarthRegistration(EarthRegistration& earthReg) {};
        virtual void leaveEarthRegistration(EarthRegistration& earthReg) {};

        virtual void enterEarthPosMapPos(EarthPosMapPos& earthMapPos) {};
        virtual void leaveEarthPosMapPos(EarthPosMapPos& earthMapPos) {};


        virtual bool enterFloor(Floor& floor) { return true; };
        virtual void leaveFloor(Floor& floor) {};

        virtual bool enterOutline(Outline& outline) { return true; };
        virtual void leaveOutline(Outline& outline) {};

        virtual void enterPointOfInterests(std::vector<PointOfInterest>& pois) {};
        virtual void leavePointOfInterests(std::vector<PointOfInterest>& pois) {};

        virtual void enterGrundtruthPoints(std::vector<GroundtruthPoint>& gtPoints) {};
        virtual void leaveGrundtruthPoints(std::vector<GroundtruthPoint>& gtPoints) {};

        virtual void enterAccessPoints(std::vector<AccessPoint>& accessPoints) {};
        virtual void leaveAccessPoints(std::vector<AccessPoint>& accessPoints) {};

        virtual void enterBeacons(std::vector<Beacon>& beacons) {};
        virtual void leaveBeacons(std::vector<Beacon>& beacons) {};

        virtual void enterFingerprintLocations(std::vector<FingerprintLocation>& fpLocations) {};
        virtual void leaveFingerprintLocations(std::vector<FingerprintLocation>& fpLocations) {};

        virtual void enterWalls(std::vector<Wall>& walls) {};
        virtual void leaveWalls(std::vector<Wall>& walls) {};

        virtual bool enterWall(Wall& wall) { return true; };
        virtual void leaveWall(Wall& wall) {};

        virtual bool enterWallDoor(WallDoor& wallDoor) { return true; };
        virtual void leaveWallDoor(WallDoor& wallDoor) {};

        virtual bool enterWallWindow(WallWindow& wallWindow) { return true; };
        virtual void leaveWallWindow(WallWindow& wallWindow) {};
    };

}