#include <GDML.hpp>
#include <lang/State.hpp>

using namespace gdml;
using namespace gdml::lang;
using namespace geode::prelude;

void gdml::loadGDMLFromFile(CCNode* node, ghc::filesystem::path const& path) {
    auto parser = Parser::create(path);
    parser->compile();
    parser->populate(node);
}
