#pragma once

#include "lang/Main.hpp"
#include <Geode/DefaultInclude.hpp>

namespace gdml {
    GDML_DLL void loadGDMLFromFile(cocos2d::CCNode* node, Path const& path);
}
