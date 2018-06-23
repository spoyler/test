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

#include <zlib.h>

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


#define CHUNK 16384
/* Decompress from file source to file dest until stream ends or EOF.
inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
allocated for processing, Z_DATA_ERROR if the deflate data is
invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
the version of the library linked do not match, or Z_ERRNO if there
is an error reading or writing the files. */
//int inf(FILE *source, FILE *dest)
int unpack_data(std::ifstream& source, std::stringstream& dest)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	//
	ret = inflateInit2_(&strm, MAX_WBITS + 16, ZLIB_VERSION, sizeof(z_stream));
	if (ret != Z_OK)
		return ret;

	/* decompress until deflate stream ends or end of file */
	do {
		source.read(reinterpret_cast<char*>(in), CHUNK);
		strm.avail_in = source.gcount();
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			have = CHUNK - strm.avail_out;
			dest.write(reinterpret_cast<char*>(out), have);

		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif


int main(int argc, const char* argv[])
{
	/* avoid end-of-line conversions */
	SET_BINARY_MODE(stdin);
	SET_BINARY_MODE(stdout);

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
							std::stringstream unpacked_data;
							std::ifstream my_file(file_path.c_str(), std::ios::in | std::ios::binary);
							if (unpack_data(my_file, unpacked_data) != Z_OK)
							{
								file_map.clear();
								std::cerr << "File was updated, but ZLib can't unpack it" << "\r\n";
								continue;
							}
							std::string current_line;
							decltype(file_map) updated_file_map;
							while (getline(unpacked_data, current_line))
							{
								auto line_hash = std::hash<std::string>()(current_line);

								const auto same_line = file_map.find(line_hash);
								if (same_line != file_map.end())
								{
									file_map.erase(same_line);
								}
								else
								{
									std::cout << "+" << current_line << "\r\n";
								}
								updated_file_map.emplace(line_hash, std::move(current_line));
							}

							for (const auto& line : file_map)
								std::cout << "-" << line.second << "\r\n";

							file_map.swap(updated_file_map);
						}
					}
					else
					{
						last_file_time_update = file_time;
						//  only read file
						std::ifstream my_file(file_path.c_str(), std::ios::in | std::ios::binary);
						std::stringstream unpacked_data;
						if (unpack_data(my_file, unpacked_data) != Z_OK)
						{
							std::cerr << "Zlib unpack error" << "\r\n";
							continue;
						}

						// read unpacked data
						std::string current_line;
						while (getline(unpacked_data, current_line))
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

