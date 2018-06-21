// check_file_update.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>


boost::filesystem::path GetInputParams(int argc, const char* argv[])
{
	namespace po = boost::program_options;

	boost::filesystem::path file_path{};

	try
	{
		po::options_description cmd_params_description;
		cmd_params_description.add_options()
			("src_file", po::value<std::string>(), "type here input file name");

		po::variables_map input_params;
		po::store(po::parse_command_line(argc, argv, cmd_params_description), input_params);
		po::notify(input_params);
			
		for (const auto& param : input_params)
		{
			if (param.first == "src_file")
			{
				file_path = param.second.as < std::string>();
			}
		}
	
	}
	catch (std::exception const &ex)
	{
		std::cerr << "Invalid cmd params: " << ex.what() << "\r\n";
	}

	return file_path;
}


int main(int argc, const char* argv[])
{
	const auto file_path = GetInputParams(argc, argv);

	std::cout << file_path << std::endl;

    return 0;
}

