#include <iostream>
#include <nvapi.h>
#include <Windows.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>

std::vector<std::string> connectors =
{
	"Uninitialized",
	"VGA",
	"Component",
	"S-Video",
	"HDMI",
	"DVI",
	"LVDS",
	"DisplayPort",
	"Composite"
};

enum class command
{
	add,
	remove,
	query,
	help
};

bool is_number(const std::string& s) 
{
	return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

int main(int argc, char* argv[])
{
	std::vector<std::string> arguments(argv, argv + argc);

	command command = command::help;
	int target_gpu_index = -1;
	int target_display_index = -1;
	std::string edid_file = "";

	std::cout << "NvSetEdid by acceleration3" << std::endl << std::endl;

	if (std::find(arguments.begin(), arguments.end(), "-a") != arguments.end())
		command = command::add;
	else if (std::find(arguments.begin(), arguments.end(), "-r") != arguments.end())
		command = command::remove;
	else if (std::find(arguments.begin(), arguments.end(), "-q") != arguments.end())
		command = command::query;
	else
		command = command::help;

	if (command == command::add || command == command::remove)
	{
		
		auto display_it = std::find(arguments.begin(), arguments.end(), "-d");
		auto gpu_it = std::find(arguments.begin(), arguments.end(), "-g");

		if (command == command::add)
		{
			auto file_it = std::find(arguments.begin(), arguments.end(), "-f");
			
			if (file_it != arguments.end())
			{
				auto value_it = std::next(file_it);

				if (value_it == arguments.end())
				{
					std::cout << "Error: No value for argument \"-f\" specified." << std::endl;
					command = command::help;
				}
				else
				{
					edid_file = *value_it;
				}
			}
			else
			{
				std::cout << "Error: Argument \"-f\" not found." << std::endl;
				command = command::help;
			}
		}
		

		if (display_it != arguments.end())
		{
			auto value_it = std::next(display_it);

			if (value_it == arguments.end() || !is_number(*value_it))
			{
				std::cout << "Error: No value for argument \"-d\" specified." << std::endl;
				command = command::help;
			}
			else
			{
				target_display_index = std::atoi(value_it->c_str());
			}
		}
		else
		{
			std::cout << "Error: Argument \"-d\" not found." << std::endl;
			command = command::help;
		}

		if (gpu_it != arguments.end())
		{
			auto value_it = std::next(gpu_it);

			if (value_it == arguments.end() || !is_number(*value_it))
			{
				std::cout << "Error: No value for argument \"-g\" specified." << std::endl;
				command = command::help;
			}
			else
			{
				target_gpu_index = std::atoi(value_it->c_str());
			}
		}
		else
		{
			std::cout << "Error: Argument \"-g\" not found." << std::endl;
			command = command::help;
		}
	}

	if(command != command::help)
	{
		if (NvAPI_Initialize() != NVAPI_OK)
		{
			std::cout << "NvAPI failed to initialize" << std::endl;
			return 1;
		}

		NvU32 gpu_count = 0;
		NvPhysicalGpuHandle physical_gpus[NVAPI_MAX_PHYSICAL_GPUS];

		if (NvAPI_EnumPhysicalGPUs(physical_gpus, &gpu_count) != NVAPI_OK)
		{
			std::cout << "Failed to query GPUs." << std::endl;
			NvAPI_Unload();
			return 2;
		}

		if (command == command::query)
		{
			for (NvU32 i = 0; i < gpu_count; i++)
			{
				NvAPI_String gpu_name;
				NvAPI_GPU_GetFullName(physical_gpus[i], gpu_name);
				std::cout << "GPU[" << i << "] \"" << gpu_name << "\"" << std::endl;

				NvU32 display_count = 0;

				if (NvAPI_GPU_GetAllDisplayIds(physical_gpus[i], NULL, &display_count) != NVAPI_OK)
				{
					std::cout << "Failed to get display count." << std::endl;
					NvAPI_Unload();
					return 4;
				}

				NV_GPU_DISPLAYIDS displays[NVAPI_MAX_DISPLAYS];
				displays[0].version = NV_GPU_DISPLAYIDS_VER;

				if (NvAPI_GPU_GetAllDisplayIds(physical_gpus[i], displays, &display_count))
				{
					std::cout << "Failed to query displays." << std::endl;
					NvAPI_Unload();
					return 5;
				}

				for (NvU32 j = 0; j < display_count; j++)
				{
					std::string connector = (displays[j].connectorType == -1 ? "Unknown" : connectors[displays[j].connectorType]);

					std::cout << "\tDISPLAY[" << j << "] ";
					std::cout << "type = " << connector << ",";
					std::cout << "connected = " << std::boolalpha << (displays[j].isConnected == 1) << ",";
					std::cout << "active = " << std::boolalpha << (displays[j].isActive == 1);
					std::cout << std::endl;
				}
			}
		}
		else
		{
			std::cout << "Target GPU index: " << target_gpu_index << std::endl;
			std::cout << "Target display index: " << target_display_index << std::endl;
			std::cout << "EDID file: " << edid_file << std::endl;

			std::ifstream file_stream(edid_file);

			if (!file_stream) 
			{
				std::cout << "Failed to open EDID file." << std::endl;
				NvAPI_Unload();
				return 3;
			}

			std::stringstream buffer;
			buffer << file_stream.rdbuf();
			
			std::stringstream converter;
			std::vector<uint8_t> edid_data;
			std::string hex;
			while (buffer >> hex)
			{
				uint8_t byte;
				converter << std::hex << hex;
				converter >> byte;
				edid_data.push_back(byte);
			}

			std::cout << "Loaded EDID file with " << edid_data.size() << " bytes." << std::endl;

			NvU32 display_count = 0;

			if (NvAPI_GPU_GetAllDisplayIds(physical_gpus[0], NULL, &display_count) != NVAPI_OK)
			{
				std::cout << "Failed to get display count." << std::endl;
				NvAPI_Unload();
				return 4;
			}

			std::cout << "Display count: " << display_count << std::endl;

			NV_GPU_DISPLAYIDS displays[NVAPI_MAX_DISPLAYS];
			displays[0].version = NV_GPU_DISPLAYIDS_VER;

			if (NvAPI_GPU_GetAllDisplayIds(physical_gpus[0], displays, &display_count))
			{
				std::cout << "Failed to query displays." << std::endl;
				NvAPI_Unload();
				return 5;
			}

			NV_EDID edid;
			memset(&edid, 0, sizeof(edid));
			edid.version = NV_EDID_VER;
			std::copy(edid_data.begin(), edid_data.end(), edid.EDID_Data);
			
			if (command == command::add)
				edid.sizeofEDID = edid_data.size();
			else
				edid.sizeofEDID = 0;

			if (NvAPI_GPU_SetEDID(physical_gpus[target_gpu_index], displays[target_display_index].displayId, &edid))
			{
				std::cout << "Failed to set EDID." << std::endl;
				NvAPI_Unload();
				return 6;
			}

			if (command == command::add)
				std::cout << "Successfully set EDID." << std::endl;
			else
				std::cout << "Successfully removed EDID." << std::endl;
		}
	}
	else
	{
		std::cout << std::endl << "Command line:" << std::endl << std::endl;
		std::cout << "-a -f <edid file> -g <gpu index> -d <display index>\tAdd an EDID config to the GPU/Display combo." << std::endl;
		std::cout << "-r -g <gpu index> -d <display index>\t\t\tRemove an EDID config from the GPU/Display combo." << std::endl;
		std::cout << "-q\t\t\t\t\t\t\tQuery the available GPUs and attached displays." << std::endl << std::endl;

		std::cout << "Example usage:" << std::endl << std::endl;
		std::cout << "Query GPUs and displays:" << std::endl;
		std::cout << "  NvSetEdid -q" << std::endl << std::endl;
		std::cout << "Add EDID from edid.txt to GPU 0, Display 0:" << std::endl;
		std::cout << "  NvSetEdid -a -f edid.txt -g 0 -d 0" << std::endl << std::endl;
		std::cout << "Remove GPU 0, Display 0's EDID:" << std::endl;
		std::cout << "  NvSetEdid -r -g 0 -d 0" << std::endl;
	}
	
	return 0;
}