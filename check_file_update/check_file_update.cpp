// check_file_update.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <csignal>
#include <iostream>
#include <functional>
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

std::function<void(int)> exit_hanlder;
void signal_hanlder(int signal)
{
	exit_hanlder(signal);
}

int main(int argc, const char* argv[])
{
	const auto file_path = GetInputParams(argc, argv);


	boost::optional<time_t> last_file_update_time {};

	std::atomic_bool exit_condition = false;

	// set true exit condition
	exit_hanlder = [&exit_condition](int signal)
	{
		exit_condition = true;
	};

	// set signal ctrl+c handler
	signal(SIGINT, signal_hanlder);


	std::thread check_file_update([&file_path, &last_file_update_time, &exit_condition]()
	{
		while (!exit_condition)
		{
			try
			{
				namespace fs = boost::filesystem;
				auto status = fs::status(file_path);
				if (fs::exists(status) && fs::is_regular_file(status))
				{
					const auto t = fs::last_write_time(file_path);

					if (last_file_update_time.is_initialized())
					{
						const auto time_diff = t - last_file_update_time.get();

						if (time_diff > 0)
						{
							last_file_update_time = t;
							std::cout << time_diff << "\r\n";
							// get file diff
						}
					}
					else
					{
						last_file_update_time = t;
						//  only read file
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

	std::cout << file_path << std::endl;

    return 0;
}

