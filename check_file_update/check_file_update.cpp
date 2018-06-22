// check_file_update.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <csignal>
#include <iostream>
#include <functional>
#include <fstream>
#include <unordered_map>
#include <thread>

#include <boost/optional.hpp>
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
			("src_file", po::value<std::string>()->required(), "type here input file name");

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

// exit handler
std::function<void(int)> exit_hanlder;

void signal_hanlder(int signal)
{
	exit_hanlder(signal);
}

int main(int argc, const char* argv[])
{
	const auto file_path = GetInputParams(argc, argv);

	if (file_path.empty())
		return -1;

	boost::optional<time_t> last_file_time_update {};

	std::atomic_bool exit_condition = false;

	// set true exit condition
	exit_hanlder = [&exit_condition](int signal)
	{
		exit_condition = true;
	};

	// set signal ctrl+c handler
	signal(SIGINT, signal_hanlder);

	std::thread check_file_update([&file_path, &last_file_time_update, &exit_condition]()
	{
		std::unordered_multimap <size_t, std::string> file_map;
		while (!exit_condition)
		{
			try
			{
				namespace fs = boost::filesystem;
				auto status = fs::status(file_path);
				if (fs::exists(status) && fs::is_regular_file(status))
				{
					const auto file_time = fs::last_write_time(file_path);

					if (last_file_time_update.is_initialized())
					{
						const auto time_diff = file_time - last_file_time_update.get();

						if (time_diff > 0)
						{
							// update file time
							last_file_time_update = file_time;
							// get file diff
							std::ifstream my_file(file_path.c_str(), std::ios::in);
							std::string current_line;
							decltype(file_map) updated_file_map;
							while (getline(my_file, current_line))
							{
								auto line_hash = std::hash<std::string>()(current_line);

								const auto same_line = file_map.find(line_hash);
								if (same_line != file_map.end())
								{
									file_map.erase(same_line);
								}
								else
								{
									std::cout << "+\t" << current_line << "\r\n";
								}
								updated_file_map.emplace(line_hash, std::move(current_line));
							}

							for (const auto& line : file_map)
							{
								std::cout << "-\t" << line.second << "\r\n";
							}

							file_map.swap(updated_file_map);
						}
					}
					else
					{
						last_file_time_update = file_time;
						//  only read file
						std::ifstream my_file(file_path.c_str(), std::ios::in);
						std::string current_line;
						while (getline(my_file, current_line))
						{
							file_map.emplace(std::hash<std::string>()(current_line), std::move(current_line));
						}
					}
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << "\r\n";
				return -1;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		return 0;
	});

	if (check_file_update.joinable())
		check_file_update.join();

    return 0;
}

