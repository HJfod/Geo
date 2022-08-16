#include "Compiler.hpp"
#include "GDML.hpp"
#include <fstream>
#include <parser/AST.hpp>
#include "Instance.hpp"
#include <ranges>

using namespace gdml;
using namespace gdml::io;

Value::Value(Compiler& compiler) : m_compiler(compiler) {}


Scope::Scope(Compiler& compiler) : compiler(compiler) {}

void Scope::pushType(std::shared_ptr<Type> type) {
    types.push_back(type);
}

void Scope::pushNamedType(std::string const& name, std::shared_ptr<Type> type) {
    namedTypes.insert({ compiler.getNameSpace() + name, type });
}

NamedEntity* Scope::pushVariable(std::string const& name, NamedEntity const& var) {
    auto fullName = compiler.getNameSpace() + name;
    variables.insert({ fullName, var });
    return &variables.at(fullName);
}

bool Scope::hasVariable(std::string const& name) const {
    return variables.count(name);
}


Error Compiler::compile() {
    auto tres = m_ast->compile(m_instance);
    if (!tres) {
        auto err = tres.unwrapErr();
        m_instance.getShared().logError(err);
        return err.code;
    }
    return Error::OK;
}

std::vector<std::string> const& Compiler::getNameSpaceStack() const {
    return m_namespace;
}

std::string Compiler::getNameSpace() const {
    std::string res {};
    for (auto& ns : m_namespace) {
        res += ns + "::";
    }
    return res;
}

void Compiler::pushNameSpace(std::string const& name) {
    m_namespace.push_back(name);
}

void Compiler::popNameSpace(std::string const& name) {
    if (m_namespace.back() == name) {
        m_namespace.pop_back();
    } else {
        std::string stack = "";
        for (auto const& s : m_namespace) {
            stack += s + "::";
        }
        stack.erase(stack.end() - 2, stack.end());
        m_instance.getShared().logError({
            Error::InternalError,
            "Attempted to pop \"" + name + "\" off the top of "
            "the namespace stack, but it wasn't there. This is "
            "likely a bug within the compiler itself.",
            "",
            "Current stack: " + stack,
            Position { 0, 0 },
            Position { 0, 0 },
            m_instance.getSource()
        });
    }
}

void Compiler::pushScope() {
    m_scope.push_back(Scope(*this));
}

void Compiler::popScope() {
    m_scope.pop_back();
}

Scope& Compiler::getScope() {
    return m_scope.back();
}

NamedEntity const* Compiler::getVariable(std::string const& name) const {
    auto fullName = getNameSpace() + name;
    for (auto& scope : std::ranges::reverse_view(m_scope)) {
        if (scope.variables.count(fullName)) {
            return &(scope.variables.at(fullName));
        }
    }
    return nullptr;
}

bool Compiler::variableExists(std::string const& name) const {
    for (auto& scope : std::ranges::reverse_view(m_scope)) {
        if (scope.variables.count(name)) {
            return true;
        }
    }
    return false;
}

bool Compiler::typeExists(std::string const& name) const {
    for (auto& scope : std::ranges::reverse_view(m_scope)) {
        if (scope.namedTypes.count(name)) {
            return true;
        }
        std::string testNameSpace = "";
        for (auto& nameSpace : m_namespace) {
            if (scope.namedTypes.count(testNameSpace + name)) {
                return true;
            }
            testNameSpace += nameSpace + "::";
        }
    }
    return false;
}

std::shared_ptr<Type> Compiler::getType(std::string const& name) const {
    for (auto& scope : std::ranges::reverse_view(m_scope)) {
        if (scope.namedTypes.count(name)) {
            return scope.namedTypes.at(name);
        }
    }
    return nullptr;
}

std::shared_ptr<Type> Compiler::getBuiltInType(types::DataType type) const {
    return getType(types::dataTypeToString(type));
}

void Compiler::codegen(std::ostream& stream) const noexcept {
    m_ast->codegen(m_instance, stream);
}

Compiler::Compiler(Instance& shared, ast::AST* ast)
 : m_instance(shared), m_ast(ast),
   m_formatter(*this), m_scope({ Scope(*this) }) {
    loadBuiltinTypes();
    loadConstValues();
}

Compiler::~Compiler() {
    for (auto& value : m_values) {
        delete value;
    }
}

Instance& Compiler::getInstance() const {
    return m_instance;
}

void Compiler::loadBuiltinTypes() {
    static std::array<types::DataType, 12> STATIC_CASTABLE {
        types::DataType::I8,  types::DataType::I16,
        types::DataType::I32, types::DataType::I64,
        types::DataType::U8,  types::DataType::U16,
        types::DataType::U32, types::DataType::U64,
        types::DataType::F32, types::DataType::F64,
        types::DataType::Bool,
        types::DataType::Char,
    };

    size_t i = 0;
    for (auto& type : types::DATATYPES) {
        makeNamedType(
            types::DATATYPE_STRS[i],
            type
        );
        i++;
    }
    for (auto& fromDataType : STATIC_CASTABLE) {
        auto fromType = getBuiltInType(fromDataType);

        for (auto& intoDataType : STATIC_CASTABLE) {
            auto intoType = getBuiltInType(intoDataType);

            fromType->addCastOperatorFor(
                intoType,
                "static_cast<" + intoType->codegenName() + ">"
            );
        }
    }
}

void Compiler::loadConstValues() {
    m_constValues = {
        {
            ConstValue::True,
            makeValue<BuiltInValue<types::Bool>>(true)
        },

        {
            ConstValue::False,
            makeValue<BuiltInValue<types::Bool>>(false)
        },

        {
            ConstValue::EmptyString,
            makeValue<BuiltInValue<types::String>>("")
        },

        {
            ConstValue::Zero,
            makeValue<BuiltInValue<types::I32>>(0)
        },

        {
            ConstValue::Null,
            makeValue<PointerValue>(nullptr)
        },
    };
}

Value* Compiler::getConstValue(ConstValue value) const {
    return m_constValues.at(value);
}


Formatter::Formatter(Compiler& compiler) : m_compiler(compiler) {}

Formatter& Compiler::getFormatter() {
    return m_formatter;
}

void Formatter::pushIndent() {
    m_indentation += 4;
}

void Formatter::popIndent() {
    m_indentation -= 4;
}

void Formatter::newline(std::ostream& stream) const {
    if (m_compiler.getInstance().getShared().getFlag(Flags::PrettifyOutput)) {
        stream << "\n" << std::string(m_indentation, ' ');
    }
}


PointerValue::PointerValue(
    Compiler& compiler,
    Value* value
) : Value(compiler), m_value(value) {}

Value* PointerValue::copy() {
    return m_compiler.makeValue<PointerValue>(m_value);
}

Value* PointerValue::getValue() const {
    return m_value;
}

void PointerValue::setValue(Value* value) {
    m_value = value;
}
