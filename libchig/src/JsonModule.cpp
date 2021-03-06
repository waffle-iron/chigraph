#include "chig/JsonModule.hpp"

#include "chig/Result.hpp"
#include "chig/GraphFunction.hpp"
#include "chig/NodeType.hpp"
#include <chig/NodeInstance.hpp>

using namespace chig;

JsonModule::JsonModule(const nlohmann::json& json_data, Context& cont, Result* r) : ChigModule(cont)
{
	// load name
	{
		auto iter = json_data.find("name");
		if(iter == json_data.end()) {
			r->add_entry("E34", "No name element in module", {});
			return;
		}
		if(!iter->is_string()) {
			r->add_entry("E35", "name element in module isn't a string", {});
			return;
		}
		name = *iter;
	}
	// load dependencies
	{
		auto iter = json_data.find("dependencies");
		if(iter == json_data.end()) {
			r->add_entry("E34", "No dependencies element in module", {});
			return;
		}
		if(!iter->is_array()) {
			r->add_entry("E36", "dependencies element isn't an array", {});
			return;
		}
		dependencies.reserve(iter->size());
		for(auto dep : *iter) {
			if(!dep.is_string()) {
				r->add_entry("E37", "dependency isn't a string", {{"Actual Data", dep}});
				continue;
			}
			dependencies.push_back(dep);
		}
	}
	// load graphs
	{
		auto iter = json_data.find("graphs");
		if(iter == json_data.end()) {
			r->add_entry("E38", "no graphs element in module", {});
			return;
		}
		if(!iter->is_array()) {
			r->add_entry("E39", "graph element isn't an array", {{"Actual Data", *iter}});
			return;
		}
		functions.reserve(iter->size());
		for(auto graph : *iter) {
			std::unique_ptr<GraphFunction> newf;
			*r += GraphFunction::fromJSON(*context, graph, &newf);
			functions.push_back(std::move(newf));
		}
		
	}
}


Result JsonModule::compile(std::unique_ptr<llvm::Module>* mod) const {
	// create llvm module
	*mod = std::make_unique<llvm::Module>(name, context->llcontext);
	
	Result res;
	
	for(auto& graph : functions) {
		llvm::Function* f;
		res += graph->compile(mod->get(), &f);
	}
	
	return res;
}

Result JsonModule::toJSON(nlohmann::json* to_fill) const {
	auto& ret = *to_fill;
	ret = nlohmann::json::object();

	Result res;

	ret["name"] = name;
	ret["dependencies"] = dependencies;

	auto& graphsjson = ret["graphs"];
	graphsjson = nlohmann::json::array();
	for(auto& graph : functions) {

		nlohmann::json to_fill;
		res += graph->toJSON(&to_fill);
		graphsjson.push_back(to_fill);
	}

	return  res;
}

GraphFunction* JsonModule::graphFuncFromName(const char* str) const {
    auto iter = std::find_if(functions.begin(), functions.end(), [&](auto& ptr)  {
        return ptr->graphName == str;
    });

    if(iter != functions.end()) return iter->get();
    return nullptr;
}

std::unique_ptr<NodeType> JsonModule::createNodeType(
	const char* name, const nlohmann::json& json_data) const {
	
	auto graph = graphFuncFromName(name);
	
	if(!graph) return nullptr;
	
	return std::make_unique<JsonFuncCallNodeType>(context, this, name);
}


std::vector<std::string> JsonModule::getNodeTypeNames() const {
    
	std::vector<std::string> ret;
	std::transform(functions.begin(), functions.end(), std::back_inserter(ret), [](auto& gPtr) {
		return gPtr->graphName;
	});
	
	return ret;
}


JsonFuncCallNodeType::JsonFuncCallNodeType(Context* c, const JsonModule* JModule_, const char* funcname) : NodeType(*c), JModule{JModule_}
{
	auto* mygraph = JModule->graphFuncFromName(funcname);
	
	assert(mygraph); // TODO: actual error reporting
	
	auto inNode = mygraph->getEntryNode();
	auto outtys = mygraph->getReturnTypes();
	
	std::transform(outtys->begin(), outtys->end(), std::back_inserter(dataOutputs), [](auto ty) {
		return std::make_pair(ty, ""); // TODO: output names
	});
	
	// the inputs to the function are the types the entry yields
	dataInputs = inNode->type->dataOutputs;
	
	name = funcname;
	module = JModule->name;
	// TODO: description
	
	execInputs = {""};
	execOutputs = {""};
}

Result JsonFuncCallNodeType::codegen(size_t execInputID, llvm::Module* mod, llvm::Function* f, const std::vector<llvm::Value *>& io, llvm::BasicBlock* codegenInto, const std::vector<llvm::BasicBlock *>& outputBlocks) const
{
	llvm::IRBuilder<> builder(codegenInto);
	
	// TODO: intermodule calls
	
	builder.CreateCall(mod->getFunction(name), io);
	
	builder.CreateBr(outputBlocks[0]);
	
	return {};
}


nlohmann::json JsonFuncCallNodeType::toJSON() const
{
	return {};
}


std::unique_ptr<NodeType> JsonFuncCallNodeType::clone() const
{
	return std::make_unique<JsonFuncCallNodeType>(context, JModule, name.c_str());
	
}

