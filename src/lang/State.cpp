#include <lang/State.hpp>
#include <lang/Expr.hpp>
#include "Debug.hpp"

using namespace geode::prelude;
using namespace gdml::lang;
using namespace gdml;

ParsedSrc::ParsedSrc(Rc<Src> src, Rc<AST> ast) : m_src(src), m_ast(ast) {}

Rc<AST> ParsedSrc::getAST() const {
    return m_ast;
}

bool ParsedSrc::addExportedType(UnitParser& state, Type const& type) {
    auto name = type.getName();
    if (!name) {
        return false;
    }
    auto path = state.resolve(name.value(), false);
    if (!path) {
        return false;
    }
    if (!m_exportedTypes.contains(path.unwrap())) {
        return false;
    }
    m_exportedTypes.insert({ path.unwrap(), type });
    return true;
}

Option<Type> ParsedSrc::getExportedType(FullIdentPath const& name) const {
    if (m_exportedTypes.contains(name)) {
        return m_exportedTypes.at(name);
    }
    return None;
}

Vec<Type> ParsedSrc::getExportedTypes() const {
    Vec<Type> types;
    for (auto& [_, ty] : m_exportedTypes) {
        types.push_back(ty);
    }
    return types;
}

Scope::Scope(Option<IdentPath> const& name, bool function, UnitParser& parser)
    : m_function(function), m_parser(parser)
{
    if (name) {
        m_name = parser.resolve(name.value(), false).ok();
    }
}

void Scope::push(Type const& type) {
    if (auto name = type.getName()) {
        if (auto path = m_parser.resolve(name.value(), false)) {
            this->m_entities.insert({ path.unwrap(), type });
        }
    }
}

void Scope::push(Var const& var) {
    if (auto path = m_parser.resolve(var.name, false)) {
        this->m_entities.insert({ path.unwrap(), var });
    }
}

void Scope::push(Fun const& fun) {
    if (auto path = m_parser.resolve(fun.name, false)) {
        this->m_entities.insert({ path.unwrap(), fun });
    }
}

void Scope::push(Namespace const& ns) {
    if (auto path = m_parser.resolve(ns.name, false)) {
        this->m_entities.insert({ path.unwrap(), ns });
    }
}

UnitParser::UnitParser(Parser& parser, Rc<Src> src)
  : m_parser(parser), m_src(src), m_scopes({ Scope(None, false, *this) })
{
    this->pushType(Primitive::Void);
    this->pushType(Primitive::Bool);
    this->pushType(Primitive::Int);
    this->pushType(Primitive::Float);
    this->pushType(Primitive::Str);
}

Rc<ParsedSrc> UnitParser::parse(Parser& shared, Rc<Src> src) {
    auto unit = UnitParser(shared, src);
    auto stream = src->read(unit);
    auto ast = AST::pull(stream).unwrapOr(nullptr);
    auto parsed = std::make_shared<ParsedSrc>(src, ast);
    if (ast) {
        log::debug("Succesfully parsed AST for {}", src->getName());
        log::debug("{}", ast->debug());
        unit.m_parsed = parsed;
        ast->typecheck(unit);
    }
    return parsed;
}

Parser& UnitParser::getShared() const {
    return m_parser;
}

Rc<Src> UnitParser::getSrc() const {
    return m_src;
}

Rc<ParsedSrc> UnitParser::getParsedSrc() const {
    return m_parsed;
}

bool UnitParser::verifyCanPush(Rc<IdentExpr> name) {
    auto path = this->resolve(name->path, false);
    if (!path) {
        this->error(name->range, "{}", path.unwrapErr());
        return false;
    }
    if (m_scopes.back().m_entities.contains(path.unwrap())) {
        this->error(name->range, "Type or variable \"{}\" already exists in this scope", name->path);
        return false;
    }
    return true;
}

Result<FullIdentPath> UnitParser::resolve(IdentPath const& name, bool existing) {
    if (name.isSingle() && !existing) {
        for (auto& scope : ranges::reverse(m_scopes)) {
            if (scope.m_name) {
                return Ok(scope.m_name.value().join(name.name));
            }
        }
        return Ok(FullIdentPath(name));
    }
    for (auto& scope : ranges::reverse(m_scopes)) {
        if (scope.m_name) {
            if (auto resolved = scope.m_name.value().resolve(name, existing)) {
                log::debug("Scope resolved {} -> {}", name.toString(), resolved.value().toString());
                return Ok(resolved.value());
            }
        }
        for (auto& [path, entity] : scope.m_entities) {
            if (auto resolved = path.resolve(name, existing)) {
                if (
                    std::holds_alternative<Namespace>(entity) ||
                    std::holds_alternative<Fun>(entity)
                ) {
                    log::debug("Entity resolved {} -> {}", name.toString(), resolved.value().toString());
                    return Ok(resolved.value());
                }
                else {
                    return Err("Cannot add sub-entities to a non-namespace or function");
                }
            }
        }
    }
    return Err("Unknown namespace \"{}\"", fmt::join(name.path, "::"));
}

void UnitParser::pushType(Type const& type) {
    m_scopes.back().push(type);
}

Type* UnitParser::getType(IdentPath const& name, bool topOnly) {
    if (auto path = this->resolve(name, true)) {
        // Prefer topmost scope
        for (auto& scope : ranges::reverse(m_scopes)) {
            if (scope.m_entities.contains(path.unwrap())) {
                return std::get_if<Type>(&scope.m_entities.at(path.unwrap()));
            }
            if (topOnly) {
                break;
            }
        }
    }
    return nullptr;
}

void UnitParser::pushVar(Var const& var) {
    m_scopes.back().push(var);
}

Var* UnitParser::getVar(IdentPath const& name, bool topOnly) {
    if (auto path = this->resolve(name, true)) {
        // Prefer topmost scope
        for (auto& scope : ranges::reverse(m_scopes)) {
            if (scope.m_entities.contains(path.unwrap())) {
                return std::get_if<Var>(&scope.m_entities.at(path.unwrap()));
            }
            if (topOnly) {
                break;
            }
        }
    }
    return nullptr;
}

void UnitParser::pushScope(Option<IdentPath> const& name, bool function) {
    m_scopes.push_back(Scope(name, function, *this));
}

void UnitParser::popScope(std::source_location const loc) {
    m_scopes.pop_back();
    if (m_scopes.empty()) {
        throw std::runtime_error(fmt::format("Scope stack is empty (tried to pop from {})", loc));
    }
}

bool UnitParser::isRootScope() const {
    return m_scopes.size() == 1;
}

Scope& UnitParser::scope(size_t depth) {
    try {
        return m_scopes.at(m_scopes.size() - depth - 1);
    }
    catch(...) {
        throw std::runtime_error(fmt::format(
            "Attempted to access scope past bounds ({}) at depth {}",
            m_scopes.size(), depth
        ));
    }
}

Parser::Parser(Rc<Src> src) : m_root(src) {
    this->autorelease();
}

Parser* Parser::create(Rc<Src> src) {
    return new Parser(src);
}

Parser* Parser::create(Path const& file) {
    auto src = SrcFile::from(file);
    auto ret = Parser::create(src.unwrapOr(nullptr));
    if (!src) {
        ret->log(Message {
            .level = Level::Error,
            .src = nullptr,
            .info = src.unwrapErr(),
            .range = Range(nullptr),
        });
    }
    return ret;
}

void Parser::compile() {
    if (!m_root) return;
    try {
        m_parsed = UnitParser::parse(*this, m_root);
    }
    catch(std::exception const& e) {
        this->log(Message {
            .level = Level::Error,
            .src = m_root,
            .info = "Internal Compiler Error: " + std::string(e.what()),
            .range = Range(m_root->getLocation(0)),
        });
    }
}

void Parser::populate(CCNode* node) {
    for (auto& created : m_created) {
        created->removeFromParent();
    }
    m_created.clear();
    this->dispatchLogs();
    if (!this->getErrors().empty()) {
        auto label = CCLabelBMFont::create(
            "There were errors loading GDML\n(See console output)",
            "bigFont.fnt"
        );
        label->setPosition(node->getContentSize() / 2);
        limitNodeSize(label, node->getContentSize(), 1.f, .1f);
        m_created.push_back(label);
    }
    for (auto& created : m_created) {
        node->addChild(created);
    }
}

void Parser::dispatchLogs() const {
    size_t errorCount = 0;
    size_t warnCount = 0;
    for (auto& [_, msg] : m_messages) {
        switch (msg.level) {
            default:
            case Level::Info:
                log::info("{}", msg.toString());
                break;

            case Level::Error:   
                log::error("{}", msg.toString());
                errorCount += 1;
                break;

            case Level::Warning: 
                log::warn("{}", msg.toString());
                warnCount += 1;
                break;
        }
    }
    log::info("Finished with {} errors and {} warnings", errorCount, warnCount);
}

void Parser::log(Message const& message, size_t level) {
    m_messages.push_back({ level, message });
}

size_t Parser::pushLogLevel() {
    return m_rollbackLevel++;
}

void Parser::popLogLevel() {
    m_rollbackLevel -= 1;
}

void Parser::popMessages(size_t level) {
    ranges::remove(m_messages, [=](std::pair<size_t, Message> const& pair) {
        return pair.first >= level;
    });
}

std::vector<Message> Parser::getErrors() const {
    std::vector<Message> errs;
    for (auto& [_, msg] : m_messages) {
        if (msg.level == Level::Error) {
            errs.push_back(msg);
        }
    }
    return errs;
}
