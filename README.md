# IndoorMapParser
C++ parser for indoor XML map format

# How to use
```cpp
#include "indoorMapParser.h"
#include "indoorSvgListener.h"

int main(int argc, char *argv[])
{
    Indoor::Map::MapParser p;

    // Read a map file
    std::shared_ptr<Indoor::Map::Map> map = p.readMapFromFile("example.xml");

    // Export map as svg
    auto svg = std::make_shared<Indoor::Map::SvgListener>();
    p.readFromFile("example.xml", svg);
    svg->saveSvgToFile("example.svg");
}
```

