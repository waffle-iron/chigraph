#include <chig/Context.hpp>
#include <chig/GraphFunction.hpp>
#include <chig/LangModule.hpp>
#include <chig/NodeType.hpp>
#include <chig/JsonModule.hpp>
#include <chig/Result.hpp>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <llvm/Support/raw_os_ostream.h>

#include <iostream>
#include <string>
#include <vector>

extern int compile(const std::vector<std::string> opts);
extern int run(const std::vector<std::string> opts);

using namespace chig;

int main(int argc, char** argv)
{
	namespace po = boost::program_options;
	namespace fs = boost::filesystem;

	po::options_description general("chig: Chigraph command line. Usage: chig <command> <arguments>", 50);

	general.add_options()
		("help,h", "Produce Help Message")
		("command", po::value<std::string>(), "which command")
		("subargs", po::value<std::vector<std::string>>(), "arguments for command")
		;

	po::positional_options_description pos;
	pos.add("command", 1)
		.add("subargs", -1);

	po::variables_map vm;
	
	po::parsed_options parsed = po::command_line_parser(argc, argv)
		.options(general)
		.positional(pos)
		.allow_unregistered()
		.run();
	
	po::store(parsed, vm);

	po::notify(vm);

	// see help
	if (vm.count("help")) {
		std::cout << general << std::endl;
		return 0;
	}
	
	if(vm.count("command") != 1) {
		std::cout << general << std::endl;
		// TODO: cout commands
		return 1;
	}
	
	std::string cmd = vm["command"].as<std::string>();
	
	std::vector<std::string> opts = po::collect_unrecognized(parsed.options, po::include_positional);
	opts.erase(opts.begin()); // remove the command
	
	if(cmd == "compile") {
		return compile(opts);
	} else if (cmd == "run") {
		return run(opts);
	}
	// TODO: write other ones

	std::cerr << "Unrecognized command: " << cmd << std::endl;
	return 1;
	
}
