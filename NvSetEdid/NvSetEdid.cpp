#include <iostream>
#include <nvapi.h>
#include <Windows.h>
#include <fstream>
#include <vector>
#include <algorithm>

// Edid data for 1920x1080
unsigned char edid_data[129] = "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x3A\xC4\x00\x00\x00\x00\x00\x00\x0F\x19\x01\x04\xA5\x3E\x22\x64\x06\x92\xB1\xA3\x54\x4C\x99\x26\x0F\x50\x54\x00\x00\x00\x95\x00\x81\x00\xD1\x00\xD1\xC0\x81\xC0\xB3\x00\x00\x00\x00\x00\x1A\x36\x80\xA0\x70\x38\x1E\x40\x30\x20\x35\x00\x13\x2B\x21\x00\x00\x1A\x39\x1C\x58\xA0\x50\x00\x16\x30\x30\x20\x3A\x00\x6D\x55\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x1E\x46\x1E\x8C\x36\x01\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x4E\x56\x49\x44\x49\x41\x20\x56\x47\x58\x20\x0A\x20\x00\x08";

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

bool is_number(const std::string& s) 
{
	return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

int main(int argc, char* argv[])
{
	std::vector<std::string> arguments(argv, argv + argc);
	
	enum class command
	{		
		add,
		remove,
		query,
		help
	};

	command command = command::help;
	int target_gpu_index = -1;
	int target_display_index = -1;

	std::cout << "NvSetEdid by acceleration3" << std::endl << std::endl;

	for (auto it = arguments.begin(); it != arguments.end(); it++)
	{
		if (*it == "-a") command = command::add;
		if (*it == "-r") command = command::remove;
		if (*it == "-q") command = command::query;
		
		if (*it == "-g" && (command == command::add || command == command::remove))
		{
			auto next_it = std::next(it);
				
			if (next_it == arguments.end() || !is_number(*next_it))
			{
				std::cout << "Invalid syntax: Wrong number of arguments." << std::endl;
				command = command::help;
				break;
			}

			target_gpu_index = std::atoi(next_it->c_str());
		}

		if (*it == "-d" && (command == command::add || command == command::remove))
		{
			auto next_it = std::next(it);

			if (next_it == arguments.end() || !is_number(*next_it))
			{
				std::cout << "Invalid syntax: Wrong number of arguments." << std::endl;
				command = command::help;
				break;
			}

			target_display_index = std::atoi(next_it->c_str());
		}
	}

	if ((target_gpu_index < 0 || target_display_index < 0) && (command != command::help && command != command::query))
	{
		std::cout << "Invalid syntax: Target GPU or display not specified." << std::endl;
		command = command::help;
	}

	switch (command)
	{
		case command::remove:
		case command::add:
		{
			

			std::cout << "Target GPU index: " << target_gpu_index << std::endl;
			std::cout << "Target display index: " << target_display_index << std::endl;

			if (NvAPI_Initialize() != NVAPI_OK)
			{
				std::cout << "NvAPI failed to initialize" << std::endl;
				return 3;
			}

			NvU32 gpu_count = 0;
			NvPhysicalGpuHandle physical_gpus[NVAPI_MAX_PHYSICAL_GPUS];

			if (NvAPI_EnumPhysicalGPUs(physical_gpus, &gpu_count) != NVAPI_OK)
			{
				std::cout << "Failed to query GPUs." << std::endl;
				NvAPI_Unload();
				return 4;
			}

			NvU32 display_count = 0;

			if (NvAPI_GPU_GetAllDisplayIds(physical_gpus[0], NULL, &display_count) != NVAPI_OK)
			{
				std::cout << "Failed to get display count." << std::endl;
				NvAPI_Unload();
				return 5;
			}

			std::cout << "Display count: " << display_count << std::endl;

			NV_GPU_DISPLAYIDS displays[NVAPI_MAX_DISPLAYS];
			displays[0].version = NV_GPU_DISPLAYIDS_VER;

			if (NvAPI_GPU_GetAllDisplayIds(physical_gpus[0], displays, &display_count))
			{
				std::cout << "Failed to query displays." << std::endl;
				NvAPI_Unload();
				return 6;
			}

			NV_EDID edid;
			memset(&edid, 0, sizeof(edid));
			edid.version = NV_EDID_VER;
			memcpy(edid.EDID_Data, edid_data, sizeof(edid_data));

			if (command == command::add)
				edid.sizeofEDID = sizeof(edid_data);
			else
				edid.sizeofEDID = 0;

			if (NvAPI_GPU_SetEDID(physical_gpus[target_gpu_index], displays[target_display_index].displayId, &edid))
			{
				std::cout << "Failed to set edid displays." << std::endl;
				NvAPI_Unload();
				return 7;
			}

			if (command == command::add)
				std::cout << "Successfully set EDID." << std::endl;
			else
				std::cout << "Successfully removed EDID." << std::endl;
			
		}
		break;

		case command::query:
		{
			if (NvAPI_Initialize() != NVAPI_OK)
			{
				std::cout << "NvAPI failed to initialize" << std::endl;
				return 2;
			}

			NvU32 gpu_count = 0;
			NvPhysicalGpuHandle physical_gpus[NVAPI_MAX_PHYSICAL_GPUS];

			if (NvAPI_EnumPhysicalGPUs(physical_gpus, &gpu_count) != NVAPI_OK)
			{
				std::cout << "Failed to query GPUs." << std::endl;
				NvAPI_Unload();
				return 3;
			}

			for (int i = 0; i < gpu_count; i++)
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

				for (int j = 0; j < display_count; j++)
				{
					std::string connector = (displays[j].connectorType == -1 ? "Unknown" : connectors[displays[j].connectorType]);

					std::cout << "\tDISPLAY[" << j << "] ";
					std::cout << "type = " << connector.c_str() << ",";
					std::cout << "connected = " << std::boolalpha << displays[j].isConnected << ",";
					std::cout << "active = " << std::boolalpha << displays[j].isActive;
				}
			}
		}
		break;

		case command::help:
			std::cout << std::endl << "Command line:" << std::endl << std::endl;
			std::cout << "-a -g <gpu index> -d <display index>\tAdd an EDID config to the GPU/Display combo." << std::endl;
			std::cout << "-r -g <gpu index> -d <display index>\tRemove an EDID config from the GPU/Display combo." << std::endl;
			std::cout << "-q\t\t\t\t\tQuery the available GPUs and attached displays." << std::endl << std::endl;

			std::cout << "Example usage:" << std::endl << std::endl;
			std::cout << "Query GPUs and displays:" << std::endl;
			std::cout << "  NvSetEdid -q" << std::endl << std::endl;
			std::cout << "Add EDID to GPU 0, Display 0:" << std::endl;
			std::cout << "  NvSetEdid -a -g 0 -d 0" << std::endl << std::endl;
			std::cout << "Remove GPU 0, Display 0's EDID:" << std::endl;
			std::cout << "  NvSetEdid -r -g 0 -d 0" << std::endl;
			break;
	}
	
	return 0;
}