#include "AST.hpp"
#include <compiler/GDML.hpp>
#include <compiler/Compiler.hpp>
#include <compiler/Instance.hpp>
#include <utils/Macros.hpp>

using namespace gdml;
using namespace gdml::ast;

#define PUSH_INDENT() \
    instance.getCompiler().getFormatter().pushIndent(); \
    instance.getCompiler().getFormatter().newline(stream)

#define POP_INDENT() \
    instance.getCompiler().getFormatter().popIndent(); \
    instance.getCompiler().getFormatter().newline(stream)

#define NEW_LINE() \
    instance.getCompiler().getFormatter().newline(stream)

#define PUSH_SCOPE() \
    instance.getCompiler().pushScope()

#define POP_SCOPE() \
    instance.getCompiler().popScope()

#define PUSH_NAMESPACE(name) \
    instance.getCompiler().pushNameSpace(name)

#define POP_NAMESPACE(name) \
    instance.getCompiler().popNameSpace(name)

#define EXPECT_TYPE(from) \
    if (!from->evalType.type) {\
        THROW_TYPE_ERR(\
            "Expected " #from " to have a type, "\
            "but it didn't",\
            "It was probably VariableDeclExpr. Make sure to "\
            "give your variable declaration a type",\
            "In " __FUNCTION__\
        );\
    }

#define DEBUG_LOG_TYPE() \
    instance.getShared().logDebug(__FUNCTION__ " -> " + evalType.toString())

#define DATA_TYPE_CHECK(typeName) \
    case types::DataType::typeName:\
        evalType = QualifiedType {\
            instance.getCompiler().getBuiltInType(types::DataType::typeName),\
            types::CONST_QUALIFIED\
        };\
        break

#define DATA_EVAL_VALUE(typeName) \
    case types::DataType::typeName:\
    return instance.getCompiler().makeValue<BuiltInValue<types::typeName>>(\
        static_cast<types::typeName>(value)\
    );\
    break

static bool matchBranchTypes(Option<QualifiedType> const& a, Option<QualifiedType> const& b) {
    // do both branches actually return something?
    if (a.has_value() && b.has_value()) {
        return a.value().convertibleTo(b.value());
    }
    return true;
}

// BoolLiteralExpr

TypeCheckResult BoolLiteralExpr::compile(Instance& instance) noexcept {
    evalType = QualifiedType {
        instance.getCompiler().getBuiltInType(types::DataType::Bool),
        types::CONST_QUALIFIED
    };
    DEBUG_LOG_TYPE();
    return Ok();
}

Value* BoolLiteralExpr::eval(Instance& instance) {
    return instance.getCompiler().getConstValue(
        value ? ConstValue::True : ConstValue::False
    );
}

// IntLiteralExpr

TypeCheckResult IntLiteralExpr::compile(Instance& instance) noexcept {
    switch (type) {
        DATA_TYPE_CHECK(I8);
        DATA_TYPE_CHECK(I16);
        DATA_TYPE_CHECK(I32);
        DATA_TYPE_CHECK(I64);
        default: THROW_TYPE_ERR(
            "Integer literal type is somehow not "
            "valid.",
            "",
            "This is an error in the compiler."
        );
    }
    DEBUG_LOG_TYPE();
    return Ok();
}

Value* IntLiteralExpr::eval(Instance& instance) {
    switch (type) {
        DATA_EVAL_VALUE(I8);
        DATA_EVAL_VALUE(I16);
        DATA_EVAL_VALUE(I32);
        DATA_EVAL_VALUE(I64);
        default: return nullptr;
    }
}

// UIntLiteralExpr

TypeCheckResult UIntLiteralExpr::compile(Instance& instance) noexcept {
    switch (type) {
        DATA_TYPE_CHECK(U8);
        DATA_TYPE_CHECK(U16);
        DATA_TYPE_CHECK(U32);
        DATA_TYPE_CHECK(U64);
        default: THROW_TYPE_ERR(
            "Unsigned integer literal type is somehow not "
            "valid.",
            "",
            "This is an error in the compiler."
        );
    }
    DEBUG_LOG_TYPE();
    return Ok();
}

Value* UIntLiteralExpr::eval(Instance& instance) {
    switch (type) {
        DATA_EVAL_VALUE(U8);
        DATA_EVAL_VALUE(U16);
        DATA_EVAL_VALUE(U32);
        DATA_EVAL_VALUE(U64);
        default: return nullptr;
    }
}

// FloatLiteralExpr

TypeCheckResult FloatLiteralExpr::compile(Instance& instance) noexcept {
    switch (type) {
        DATA_TYPE_CHECK(F32);
        DATA_TYPE_CHECK(F64);
        default: THROW_TYPE_ERR(
            "Unsigned integer literal type is somehow not "
            "valid.",
            "",
            "This is an error in the compiler."
        );
    }
    DEBUG_LOG_TYPE();
    return Ok();
}

Value* FloatLiteralExpr::eval(Instance& instance) {
    switch (type) {
        DATA_EVAL_VALUE(F32);
        DATA_EVAL_VALUE(F64);
        default: return nullptr;
    }
}

// StringLiteralExpr

TypeCheckResult StringLiteralExpr::compile(Instance& instance) noexcept {
    evalType = QualifiedType {
        instance.getCompiler().getBuiltInType(types::DataType::String),
        types::CONST_QUALIFIED
    };
    DEBUG_LOG_TYPE();
    return Ok();
}

Value* StringLiteralExpr::eval(Instance& instance) {
    if (value == "") {
        return instance.getCompiler().getConstValue(ConstValue::EmptyString);
    }
    return instance.getCompiler().makeValue<BuiltInValue<types::String>>(value);
}

// InterpolatedLiteralExpr

TypeCheckResult InterpolatedLiteralExpr::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILDREN(components);
    // todo: evalValue
    DEBUG_LOG_TYPE();
    return Ok();
}

Value* InterpolatedLiteralExpr::eval(Instance& instance) {
    // todo
    return nullptr;
}

void InterpolatedLiteralExpr::codegen(Instance& com, std::ostream& stream) const noexcept {
    for (size_t ix = 0; ix < rawStrings.size() + components.size(); ix++) {
        if (ix) {
            stream << " + ";
        }
        if (ix % 2) {
            // todo: convert codegenned result to string or smth
            components.at(ix / 2)->codegen(com, stream);
        } else {
            stream << "\"" << rawStrings.at(ix / 2) << "\"";
        }
    }
}

// NullLiteralExpr

TypeCheckResult NullLiteralExpr::compile(Instance& instance) noexcept {
    evalType = QualifiedType {
        instance.getCompiler().makeType<PointerType>(
            QualifiedType(
                instance.getCompiler().getBuiltInType(types::DataType::Void)
            ),
            types::PointerType::Pointer
        ),
        types::CONST_QUALIFIED
    };
    DEBUG_LOG_TYPE();
    return Ok();
}

Value* NullLiteralExpr::eval(Instance& instance) {
    return instance.getCompiler().getConstValue(ConstValue::Null);
}

// UnaryExpr

TypeCheckResult UnaryExpr::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILD(value);

    if (isLValueOperator(op) && value->evalType.qualifiers.isConst) {
        THROW_TYPE_ERR(
            "Invalid operands for binary expression: left-hand-side "
            "is const-qualified `" + value->evalType.toString() + "`, but "
            "the operator used " + TOKEN_STR_V(op) + " requires a "
            "modifiable value",
            "",
            ""
        );
    }

    evalType = value->evalType;

    return Ok();
}

// BinaryExpr

TypeCheckResult BinaryExpr::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILD(LHS);
    GDML_TYPECHECK_CHILD(RHS);

    EXPECT_TYPE(LHS);
    EXPECT_TYPE(RHS);

    if (!RHS->evalType.convertibleTo(LHS->evalType)) {
        THROW_TYPE_ERR(
            "Invalid operands for binary expression: left-hand-side "
            "has the type `" + LHS->evalType.toString() + "`, but "
            "right-hand-side has the type `" + RHS->evalType.toString() + "`",

            "Add an explicit type conversion on the right-hand-side: "
            "`as " + LHS->evalType.toString() + "`",

            "There are no implicit conversions in GDML. All types "
            "must match exactly!"
        );
    }

    if (isLValueOperator(op) && LHS->evalType.qualifiers.isConst) {
        THROW_TYPE_ERR(
            "Invalid operands for binary expression: left-hand-side "
            "is const-qualified `" + LHS->evalType.toString() + "`, but "
            "the operator used " + TOKEN_STR_V(op) + " requires a "
            "modifiable value",
            "",
            ""
        );
    }

    evalType = LHS->evalType;

    DEBUG_LOG_TYPE();

    return Ok();
}

void BinaryExpr::codegen(Instance& instance, std::ostream& stream) const noexcept {
    LHS->codegen(instance, stream);
    if (instance.getShared().getFlag(Flags::PrettifyOutput)) stream << " ";
    stream << tokenTypeToString(op);
    if (instance.getShared().getFlag(Flags::PrettifyOutput)) stream << " ";
    RHS->codegen(instance, stream);
}

// TernaryExpr

void TernaryExpr::codegen(Instance& instance, std::ostream& stream) const noexcept {
    // add brackets and shit to make sure meaning 
    // doesn't change (C++ presedence might differ 
    // from Instance)
    stream << "((";
    condition->codegen(instance, stream);
    stream << ")";

    instance.getCompiler().getFormatter().newline(stream);

    stream << " ? (";
    truthy->codegen(instance, stream);
    stream << ")";

    instance.getCompiler().getFormatter().newline(stream);

    stream << " : (";
    falsy->codegen(instance, stream);
    stream << "))";
}

// PointerExpr

TypeCheckResult PointerExpr::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILD(to);

    evalType = QualifiedType(
        instance.getCompiler().makeType<PointerType>(
            to->evalType,
            type
        )
    );

    DEBUG_LOG_TYPE();
    return Ok();
}

void PointerExpr::codegen(Instance& instance, std::ostream& stream) const noexcept {
    stream << evalType.codegenName();
}

// NameExpr

TypeCheckResult NamedEntityExpr::compile(Instance& instance) noexcept {
    auto var = instance.getCompiler().getVariable(name->fullName());
    if (!var) {
        THROW_COMPILE_ERR(
           "Identifier \"" + name->fullName() + "\" is undefined",
            "",
            ""
        );
    }

    evalType = var->type;

    DEBUG_LOG_TYPE();
    
    return Ok();
}

Value* NamedEntityExpr::eval(Instance& instance) {
    return instance.getCompiler().getVariable(name->fullName())->value;
}

void NamedEntityExpr::codegen(Instance& instance, std::ostream& stream) const noexcept {
    name->codegen(instance, stream);
}

// ScopeExpr

TypeCheckResult ScopeExpr::compile(Instance& instance) noexcept {
    PUSH_NAMESPACE(name);
    GDML_TYPECHECK_CHILD(item);
    POP_NAMESPACE(name);

    return Ok();
}

// TypeNameExpr

TypeCheckResult TypeNameExpr::compile(Instance& instance) noexcept {
    if (!instance.getCompiler().typeExists(name->fullName())) {
        THROW_TYPE_ERR(
            "Unknown type \"" + name->fullName() + "\"",
            "Not all C++ types are supported yet, sorry!",
            ""
        );
    }

    evalType = QualifiedType {
        instance.getCompiler().getType(name->fullName()),
        qualifiers
    };
    DEBUG_LOG_TYPE();

    return Ok();
}

void TypeNameExpr::codegen(Instance& instance, std::ostream& stream) const noexcept {
    stream << evalType.codegenName();
}

// NameSpaceStmt

TypeCheckResult NameSpaceStmt::compile(Instance& instance) noexcept {
    PUSH_NAMESPACE(name);
    GDML_TYPECHECK_CHILD(contents);
    POP_NAMESPACE(name);
    
    return Ok();
}

void NameSpaceStmt::codegen(Instance& instance, std::ostream& stream) const noexcept {
    stream << "namespace " << name << " {";
    PUSH_INDENT();
    contents->codegen(instance, stream);
    POP_INDENT();
    stream << "}";
}

// VariableDeclExpr

TypeCheckResult VariableDeclExpr::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILD_O(type);
    GDML_TYPECHECK_CHILD_O(value);

    // does variable have an explicit type
    if (type.has_value()) {
        evalType = type.value()->evalType;

        // does value match
        if (value.has_value()) {
            if (!evalType.convertibleTo(value.value()->evalType)) {
                THROW_TYPE_ERR(
                    "Declared type `" + evalType.toString() + 
                    "` does not match inferred type `" + 
                    value.value()->evalType.toString() + 
                    "` of value",
                    "",
                    ""
                );
            }
        }
    }
    // otherwise type is just inferred from value
    else {
        if (value.has_value()) {
            evalType = value.value()->evalType;
        }
    }

    if (instance.getCompiler().getScope().hasVariable(name)) {
        THROW_COMPILE_ERR(
            "Entity named \"" + name + "\" already exists "
            "in this scope",
            "",
            ""
        );
    }
    variable = instance.getCompiler().getScope().pushVariable(
        name, NamedEntity(evalType, nullptr, this)
    );
    DEBUG_LOG_TYPE();
    
    return Ok();
}

Value* VariableDeclExpr::eval(Instance& instance) {    
    if (value.value()) {
        variable->value = value.value()->eval(instance);
    }
    return variable->value;
}

void VariableDeclExpr::codegen(Instance& instance, std::ostream& stream) const noexcept {
    if (evalType.type) {
        stream << evalType.codegenName() << " ";
    } else {
        stream << "auto ";
    }
    stream << name;
    if (value.has_value()) {
        stream << " = ";
        value.value()->codegen(instance, stream);
    }
}

// CastTypeExpr

TypeCheckResult CastTypeExpr::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILD(target);
    GDML_TYPECHECK_CHILD(intoType);

    if (!target->evalType.castableTo(intoType->evalType)) {
        THROW_TYPE_ERR(
            "Type `" + target->evalType.toString() + "` does "
            "not have an implemented cast operator to type `" + 
            intoType->evalType.toString() + "`",

            "Implement a cast operator: `impl " + 
            target->evalType.type->toString() + " as " +
            intoType->evalType.type->toString() + " { /* ... */ }`",

            "Const-qualified values can not be casted to non-const-"
            "qualified values"
        );
    }

    evalType = intoType->evalType;

    return Ok();
}

void CastTypeExpr::codegen(Instance& instance, std::ostream& stream) const noexcept {
    stream
        << target->evalType.type->getCastOperatorFor(intoType->evalType.type)
        << "(";
    target->codegen(instance, stream);
    stream << ")";
}

// FunctionTypeExpr

TypeCheckResult FunctionTypeExpr::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILDREN(parameters);
    GDML_TYPECHECK_CHILD_O(returnType);

    QualifiedType evalRetType {};
    if (returnType.has_value()) {
        evalRetType = returnType.value()->evalType;
    }

    std::vector<QualifiedType> evalParamTypes {};
    for (auto& param : parameters) {
        evalParamTypes.push_back(param->evalType);
    }

    evalType = QualifiedType {
        instance.getCompiler().makeType<FunctionType>(
            evalRetType, evalParamTypes
        ),
        qualifiers
    };

    DEBUG_LOG_TYPE();

    return Ok();
}

// FunctionDeclStmt

TypeCheckResult FunctionDeclStmt::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILD(type);
    GDML_TYPECHECK_CHILD(name);

    if (instance.getCompiler().getScope().hasVariable(name->fullName())) {
        THROW_COMPILE_ERR(
            "Entity named \"" + name->fullName() + "\" already exists "
            "in this scope",
            "",
            ""
        );
    }

    entity = instance.getCompiler().getScope().pushVariable(
        name->fullName(), NamedEntity(type->evalType, nullptr, this)
    );

    PUSH_SCOPE();
    GDML_TYPECHECK_CHILD_O(body);
    POP_SCOPE();
    
    auto funType = static_cast<FunctionType*>(type->evalType.type.get());

    // return type inference
    if (!funType->getReturnType().type.get() && body.has_value()) {
        auto infer = body.value()->inferBranchReturnType(instance);
        if (!infer) return infer.unwrapErr();
        
        auto inferredType = infer.unwrap();
        if (inferredType.has_value()) {
            funType->setReturnType(inferredType.value());
        } else {
            funType->setReturnType(QualifiedType(
                instance.getCompiler().getBuiltInType(types::DataType::Void)
            ));
        }
    }

    return Ok();
}

void FunctionDeclStmt::codegen(Instance& instance, std::ostream& stream) const noexcept {
    // get evaluated funtion type
    auto funType = static_cast<FunctionType*>(type->evalType.type.get());

    // default static for file functions
    if (
        instance.getShared().getRule(LanguageRule::DefaultStaticFunctions) &&
        !name->isScoped()
    ) {
        stream << "static ";
    }

    if (funType->getReturnType().type) {
        stream << funType->getReturnType().codegenName() << " ";
    } else {
        stream << "auto ";
    }
    name->codegen(instance, stream);
    stream << "(";
    bool firstArg = true;
    for (auto& param : type->parameters) {
        if (!firstArg) {
            stream << ", ";
        }
        firstArg = false;
        param->codegen(instance, stream);
    }
    stream << ")";
    if (body.has_value()) {
        stream << " {";
        PUSH_INDENT();
        body.value()->codegen(instance, stream);
        POP_INDENT();
        stream << "}";
    }
}

// CallExpr

TypeCheckResult CallExpr::compile(Instance& instance) noexcept {
    GDML_TYPECHECK_CHILD(target);
    GDML_TYPECHECK_CHILDREN(args);

    // todo: support operator()
    if (target->evalType.type->getType() != types::DataType::Function) {
        THROW_TYPE_ERR(
            "Attempted to call an expression that did not "
            "evaluate to a function type",
            "",
            ""
        );
    }

    auto targetType = static_cast<FunctionType*>(target->evalType.type.get());

    size_t i = 0;
    for (auto& arg : args) {
        auto targetArg = targetType->getParameters().at(i);
        if (!arg->evalType.convertibleTo(targetArg)) {
            THROW_TYPE_ERR_AT(
                "Argument of type `" + arg->evalType.toString() + "` cannot "
                "be passed to parameter of type `" + targetArg.toString() + "`",

                "Add an explicit type conversion on the argument: "
                "`as " + targetArg.toString() + "`",

                "There are no implicit conversions in GDML. All types "
                "must match exactly!",

                arg->start, arg->end
            );
        }
        i++;
    }

    evalType = targetType->getReturnType();

    DEBUG_LOG_TYPE();

    return Ok();
}

// BlockStmt

BranchInferResult BlockStmt::inferBranchReturnType(Instance& instance) {
    return body->inferBranchReturnType(instance);
}

TypeCheckResult BlockStmt::compile(Instance& instance) noexcept {
    PUSH_SCOPE();
    GDML_TYPECHECK_CHILD(body);
    POP_SCOPE();

    return Ok();
}

void BlockStmt::codegen(Instance& instance, std::ostream& stream) const noexcept {
    stream << "{";
    PUSH_INDENT();
    body->codegen(instance, stream);
    POP_INDENT();
    stream << "}";
}

// ImportStmt

void ImportStmt::codegen(Instance& instance, std::ostream& stream) const noexcept {
    if (path.extension() != ".gdml") {
        if (!isRelative) {
            stream << "#include <" << path.string() << ">";
            NEW_LINE();
        }
        else if (path.is_absolute()) {
            stream << "#include \"" << path.string() << "\"";
            NEW_LINE();
        }
        else {
            stream << "#include \""
                << instance.getSource()->path.parent_path().string()
                << "/" << path.string() << "\"";
            NEW_LINE();
        }
    }
}

// IfStmt

BranchInferResult IfStmt::inferBranchReturnType(Instance& instance) {
    Option<QualifiedType> ifInfer = None;
    PROPAGATE_ASSIGN(ifInfer, branch->inferBranchReturnType(instance));

    if (elseBranch.has_value()) {
        Option<QualifiedType> elseInfer = None;
        PROPAGATE_ASSIGN(elseInfer, elseBranch.value()->inferBranchReturnType(instance));

        if (!matchBranchTypes(ifInfer, elseInfer)) {
            // matchBranchTypes only fails if both branches 
            // actually have a return type
            THROW_TYPE_ERR(
                "Branches have incompatible return types; "
                "If branch returns `" + ifInfer.value().toString() + 
                "` but else branch returns `" + elseInfer.value().toString() + "`",
                "",
                ""
            );
        }
    }

    return ifInfer;
}

TypeCheckResult IfStmt::compile(Instance& instance) noexcept {
    PUSH_SCOPE();
    GDML_TYPECHECK_CHILD_O(condition);
    POP_SCOPE();

    PUSH_SCOPE();
    GDML_TYPECHECK_CHILD_O(elseBranch);
    POP_SCOPE();

    return Ok();
}

void IfStmt::codegen(Instance& instance, std::ostream& stream) const noexcept {
    if (condition.has_value()) {
        stream << "if (";
        condition.value()->codegen(instance, stream);
        stream << ") ";
    }
    stream << "{";
    PUSH_INDENT();
    branch->codegen(instance, stream);
    POP_INDENT();
    stream << "}";
    if (elseBranch.has_value()) {
        stream << " else ";
        elseBranch.value()->codegen(instance, stream);
    }
}

// StmtList

BranchInferResult StmtList::inferBranchReturnType(Instance& instance) {
    Option<QualifiedType> value = None;
    for (auto& stmt : statements) {
        if (value) {
            THROW_COMPILE_ERR(
                "Found unreachable code",
                "",
                ""
            );
            break;
        }
        PROPAGATE_ASSIGN(value, stmt->inferBranchReturnType(instance));
    }
    return value;
}

void StmtList::codegen(Instance& instance, std::ostream& stream) const noexcept {
    bool first = true;
    for (auto const& stmt : statements) {
        if (!first) {
            NEW_LINE();
            NEW_LINE();
        }
        first = false;

        stmt->codegen(instance, stream);
        stream << ";";
    }
}

// ReturnStmt

BranchInferResult ReturnStmt::inferBranchReturnType(Instance& instance) {
    return Option<QualifiedType>(value->evalType);
}

// EmbedCodeStmt

void EmbedCodeStmt::codegen(Instance& instance, std::ostream& stream) const noexcept {
    stream << data;
}

// AST

void AST::codegen(Instance& instance, std::ostream& stream) const noexcept {
    bool first = true;
    for (auto& stmt : m_tree) {
        if (!first) {
            NEW_LINE();
            NEW_LINE();
        }
        first = false;

        stmt->codegen(instance, stream);
        stream << ";";
    }
    NEW_LINE();
}
