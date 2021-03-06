#include "chig/LangModule.hpp"
#include "chig/Context.hpp"

#include <llvm/AsmParser/Parser.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/Support/SourceMgr.h>

using namespace chig;

LangModule::LangModule(Context& contextArg) : ChigModule(contextArg)
{
	using namespace std::string_literals;

	name = "lang";

	// populate them
	nodes = {
		{"if"s, [this](const nlohmann::json&) { return std::make_unique<IfNodeType>(*context); }},
		{"entry"s,
			[this](const nlohmann::json& data) {

				// transform the JSON data into this data structure
				std::vector<std::pair<llvm::Type*, std::string>> inputs;

				for (auto iter = data.begin(); iter != data.end(); ++iter) {
					std::string qualifiedType = iter.value();
					std::string module = qualifiedType.substr(0, qualifiedType.find(':'));
					std::string type = qualifiedType.substr(qualifiedType.find(':') + 1);

					llvm::Type* llty;
					// TODO: maybe not discard res
					context->getType(module.c_str(), type.c_str(), &llty);

					inputs.emplace_back(llty, iter.key());
				}

				return std::make_unique<EntryNodeType>(*context, inputs);
			}
		},
		{"exit"s,
			[this](const nlohmann::json& data) {
				// transform the JSON data into this data structure
				std::vector<std::pair<llvm::Type*, std::string>> outputs;

				for (auto iter = data.begin(); iter != data.end(); ++iter) {
					std::string qualifiedType = iter.value();
					std::string module = qualifiedType.substr(0, qualifiedType.find(':'));
					std::string type = qualifiedType.substr(qualifiedType.find(':') + 1);

					llvm::Type* llty;
					// TODO: don't discard res
					context->getType(module.c_str(), type.c_str(), &llty);
					outputs.emplace_back(llty, iter.key());
				}

				return std::make_unique<ExitNodeType>(*context, outputs);

			}
		},
		{"const-int"s, [this](const nlohmann::json& data) {
			
				int num = data.is_number_integer() ? (int)data : 0;

				return std::make_unique<ConstIntNodeType>(*context, num);
			}
		},
		{"const-bool"s, [this](const nlohmann::json& data) {
			
				bool num = data.is_boolean() ? (bool)data : false;

				return std::make_unique<ConstBoolNodeType>(*context, num);
			}
		},
		{"strliteral"s, [this](const nlohmann::json& data) {
			std::string str;
			if(data.is_string()) {
				str = data;
			} else {
				str = "";
			}
			
			return std::make_unique<StringLiteralNodeType>(*context, str);
		}
		}
		};
}

std::unique_ptr<NodeType> LangModule::createNodeType(
	const char* name, const nlohmann::json& json_data) const
{
	auto iter = nodes.find(name);
	if (iter != nodes.end()) {
		return iter->second(json_data);
	}
	return nullptr;
}

// the lang module just has the basic llvm types.
llvm::Type* LangModule::getType(const char* name) const
{
	using namespace std::string_literals;

	// just parse the type
	auto IR = "@G = external global "s + name;
	auto err = llvm::SMDiagnostic();
	auto tmpModule = llvm::parseAssemblyString(IR, err, context->llcontext);
	if (!tmpModule) return nullptr;

	// returns the pointer type, so get the contained type
	return tmpModule->getNamedValue("G")->getType()->getContainedType(0);
}

Result IfNodeType::codegen(size_t, llvm::Module* mod, llvm::Function*, const std::vector< llvm::Value*>& io, llvm::BasicBlock* codegenInto, const std::vector< llvm::BasicBlock*>& outputBlocks) const
{
    llvm::IRBuilder<> builder(codegenInto);
    builder.CreateCondBr(io[0], outputBlocks[0], outputBlocks[1]);

    return {};
}

IfNodeType::IfNodeType(Context& con) : NodeType(con)
{
    module = "lang";
    name = "if";
    description = "branch on a bool";

    execInputs = {""};
    execOutputs = {"True", "False"};

    dataInputs = {{llvm::Type::getInt1Ty(context->llcontext), "condition"}};
}


std::unique_ptr< chig::NodeType, std::default_delete< chig::NodeType > > IfNodeType::clone() const
{
    return std::make_unique<IfNodeType>(*this);
}


EntryNodeType::EntryNodeType(Context& con, const std::vector< std::pair< llvm::Type*,std::string>>& funInputs) 
		: NodeType{con}
{
    module = "lang";
    name = "entry";
    description = "entry to a function";

    execOutputs = {""};

    dataOutputs = funInputs;
}


Result EntryNodeType::codegen(size_t, llvm::Module* mod, llvm::Function* f, const std::vector< llvm::Value*, std::allocator< llvm::Value* > >& io, llvm::BasicBlock* codegenInto, const std::vector< llvm::BasicBlock*, std::allocator< llvm::BasicBlock* > >& outputBlocks) const
{
    llvm::IRBuilder<> builder(codegenInto);
    // just go to the block
    builder.CreateBr(outputBlocks[0]);

    return {};
}


std::unique_ptr< NodeType > EntryNodeType::clone() const
{
    return std::make_unique<EntryNodeType>(*this);
}


nlohmann::json EntryNodeType::toJSON() const
{
    nlohmann::json ret = nlohmann::json::object();

    for (auto& pair : dataOutputs) {
        // TODO: user made types
        ret[pair.second] = "lang:" + context->stringifyType(pair.first);
    }

    return ret;
}


ConstIntNodeType::ConstIntNodeType(Context& con, int num) : NodeType{con}, number{num}
{
    module = "lang";
    name = "const-int";
    description = "constant int value";

    execInputs = {""};
    execOutputs = {""};

    dataOutputs = {{llvm::IntegerType::getInt32Ty(con.llcontext), "out"}};
}


Result ConstIntNodeType::codegen(size_t, llvm::Module* mod, llvm::Function* f, const std::vector< llvm::Value*>& io, llvm::BasicBlock* codegenInto, const std::vector< llvm::BasicBlock*, std::allocator< llvm::BasicBlock* > >& outputBlocks) const
{
    llvm::IRBuilder<> builder(codegenInto);
    // just go to the block
    assert(io.size() == 1);

    builder.CreateStore(
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context->llcontext), number), io[0],
        false);
    builder.CreateBr(outputBlocks[0]);

    return {};
}


std::unique_ptr< chig::NodeType> ConstIntNodeType::clone() const
{
    return std::make_unique<ConstIntNodeType>(*this);
}


nlohmann::json ConstIntNodeType::toJSON() const
{
    return number;
}


ConstBoolNodeType::ConstBoolNodeType(Context& con, bool num) : NodeType{con}, value{num}
{
    module = "lang";
    name = "const-bool";
    description = "constant boolean value";

    execInputs = {""};
    execOutputs = {""};

    dataOutputs = {{llvm::IntegerType::getInt1Ty(con.llcontext), "out"}};
}


Result ConstBoolNodeType::codegen(size_t, llvm::Module* mod, llvm::Function* f, const std::vector< llvm::Value*>& io, llvm::BasicBlock* codegenInto, const std::vector< llvm::BasicBlock*, std::allocator< llvm::BasicBlock* > >& outputBlocks) const
{
    llvm::IRBuilder<> builder(codegenInto);
    // just go to the block
    assert(io.size() == 1);

    builder.CreateStore(
        llvm::ConstantInt::get(llvm::IntegerType::getInt1Ty(context->llcontext), value), io[0],
        false);
    builder.CreateBr(outputBlocks[0]);

    return {};
}


std::unique_ptr< chig::NodeType> ConstBoolNodeType::clone() const
{
    return std::make_unique<ConstBoolNodeType>(*this);
}


nlohmann::json ConstBoolNodeType::toJSON() const
{
    return value;
}


ExitNodeType::ExitNodeType(Context& con, const std::vector< std::pair< llvm::Type*, std::string>>& funOutputs) : NodeType{con}
{
    execInputs = {""};

    module = "lang";
    name = "exit";
    description = "exit from a function; think return";

    dataInputs = funOutputs;
}


Result ExitNodeType::codegen(size_t execInputID, llvm::Module* mod, llvm::Function* f, const std::vector< llvm::Value*>& io, llvm::BasicBlock* codegenInto, const std::vector< llvm::BasicBlock* >&) const
{
    // assign the return types
    llvm::IRBuilder<> builder(codegenInto);
    size_t ret_start =
        f->arg_size() - io.size();  // returns are after args, find where returns start
    auto arg_iter = f->arg_begin();
    std::advance(arg_iter, ret_start);
    for (int idx = 0; idx < io.size(); ++idx) {
        builder.CreateStore(io[idx], &*arg_iter, false);  // TODO: volitility?
        ++arg_iter;
    }

    builder.CreateRet(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context->llcontext), execInputID));

    return {};
}


std::unique_ptr< NodeType> ExitNodeType::clone() const
{
    return std::make_unique<ExitNodeType>(*this);
}


nlohmann::json ExitNodeType::toJSON() const
{
	nlohmann::json ret = nlohmann::json::object();

    for (auto& pair : dataOutputs) {
        // TODO: user made types
        ret[pair.second] = "lang:" + context->stringifyType(pair.first);
    }

    return ret;
}




StringLiteralNodeType::StringLiteralNodeType(Context& con, std::string str) : NodeType{con}, literalString(std::move(str))
{
	
    execInputs = {""};
	execOutputs = {""};

    module = "lang";
    name = "strliteral";
    description = "exit from a function; think return";

	// TODO: research address types
	dataOutputs = {{llvm::PointerType::getInt8PtrTy(con.llcontext, 0), "string"}};
}


Result StringLiteralNodeType::codegen(size_t execInputID, llvm::Module* mod, llvm::Function* f, const std::vector< llvm::Value*>& io, llvm::BasicBlock* codegenInto, const std::vector< llvm::BasicBlock* >& outputBlocks) const
{
	llvm::IRBuilder<> builder(codegenInto);
	
	auto global = builder.CreateGlobalString(literalString);
	
	auto const0ID = llvm::ConstantInt::get(context->llcontext, llvm::APInt(32, 0, false));
	auto gep = builder.CreateGEP(global, {const0ID, const0ID});
	builder.CreateStore(gep, io[0], false);
	
	builder.CreateBr(outputBlocks[0]);
	
	return {};
	
}


std::unique_ptr< NodeType> StringLiteralNodeType::clone() const
{
    return std::make_unique<StringLiteralNodeType>(*this);
}


nlohmann::json StringLiteralNodeType::toJSON() const
{
    return literalString;
	
}

