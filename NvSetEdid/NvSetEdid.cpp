#include <iostream>
#include <nvapi.h>
#include <Windows.h>
#include <fstream>
#include <vector>
#include <algorithm>

// Edid data for 1920x1080
unsigned char edid_data[129] = "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x3A\xC4\x00\x00\x00\x00\x00\x00\x0F\x19\x01\x04\xA5\x3E\x22\x64\x06\x92\xB1\xA3\x54\x4C\x99\x26\x0F\x50\x54\x00\x00\x00\x95\x00\x81\x00\xD1\x00\xD1\xC0\x81\xC0\xB3\x00\x00\x00\x00\x00\x1A\x36\x80\xA0\x70\x38\x1E\x40\x30\x20\x35\x00\x13\x2B\x21\x00\x00\x1A\x39\x1C\x58\xA0\x50\x00\x16\x30\x30\x20\x3A\x00\x6D\x55\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x1E\x46\x1E\x8C\x36\x01\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x4E\x56\x49\x44\x49\x41\x20\x56\x47\x58\x20\x0A\x20\x00\x08";

bool is_number(const std::string& s) 
{
	return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

int main(int argc, char* argv[])
{
	std::vector<std::string> arguments(argv, argv + argc);
	
	enum class command
	{		
		set,
		remove,
		query,
		help
	};

	command command = command::set;
	int target_gpu_index = -1;
	int target_display_index = -1;

	for (auto it = arguments.begin(); it != arguments.end(); it++)
	{
		if (*it == "-q") command = command::query;
		if (*it == "-h") command = command::help;
		if (*it == "-r") command = command::remove;

		if (*it == "-g" && (command == command::set || command == command::remove))
		{
			auto next_it = std::next(it);
				
			if (next_it == arguments.end() || !is_number(*next_it))
			{
				std::cout << "Invalid syntax. Type \"NvSetEdid -h\" to get display command help." << std::endl;
				return 1;
			}

			target_gpu_index = std::atoi(next_it->c_str());
		}

		if (*it == "-d" && (command == command::set || command == command::remove))
		{
			auto next_it = std::next(it);

			if (next_it == arguments.end() || !is_number(*next_it))
			{
				std::cout << "Invalid syntax. Type \"NvSetEdid -h\" to get display command help." << std::endl;
				return 1;
			}

			target_display_index = std::atoi(next_it->c_str());
		}
	}

	switch (command)
	{
		case command::remove:
		case command::set:
		{
			if (target_gpu_index < 0 || target_display_index < 0)
			{
				std::cout << "Target GPU and monitor not specified. Type \"NvSetEdid -h\" to get display command help." << std::endl;
				return 2;
			}

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

			if (command == command::set)
				edid.sizeofEDID = sizeof(edid_data);
			else
				edid.sizeofEDID = 0;

			if (NvAPI_GPU_SetEDID(physical_gpus[target_gpu_index], displays[target_display_index].displayId, &edid))
			{
				std::cout << "Failed to set edid displays." << std::endl;
				NvAPI_Unload();
				return 7;
			}

			if (command == command::set)
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

				for (int i = 0; i < gpu_count; i++)
					std::cout << "\tDISPLAY[" << i << "]" << std::endl;
			}
		}
		break;

		case command::help:
			std::cout << "NvSetEdid by acceleration3" << std::endl << std::endl;
			std::cout << "Example usage:" << std::endl;
			std::cout << "\tQuery GPUs and displays:" << std::endl;
			std::cout << "\t\tNvSetEdid -q" << std::endl;
			std::cout << "\tSet GPU 0, Display 0's EDID:" << std::endl;
			std::cout << "\t\tNvSetEdid -g 0 -d 0" << std::endl;
			std::cout << "\tRemove GPU 0, Display 0's EDID:" << std::endl;
			std::cout << "\t\tNvSetEdid -r -g 0 -d 0" << std::endl;
			break;
	}
	
	return 0;
}