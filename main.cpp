#include <cassert>
#include <thread>
#include <chrono>
#include "process.hpp"

#ifdef _WIN64 
#error "x86 only!"
#endif

namespace offsets
{
    std::size_t LocalPlayer = 0x1ED7794;
    std::size_t Client = 0x1ED73D8;
    std::size_t Render = 0x1E82B28;
    std::size_t SceneContext = 0x1CD54D0;
    std::size_t m_ppObjects = 0xE9C;
    std::size_t m_entities = 0xE98;
}

struct vec3_t
{
    float x, y, z;
};

class PtrTable
{
public:
    char pad_0x0000[0x4]; //0x0000
    uint32_t name; //0x0004
    uint32_t data; //0x0004
}; //Size=0x0044

int main(const int argc, char** argv)
{
    SetConsoleTitle("RoS netvar dumper by wlan");
    printf("RoS netvar dumper by wlan\n");
    process ros_process(L"ros.exe");
    printf("waiting for rules of survial.");

    while(!ros_process.is_open())
    {
        using namespace std::chrono_literals;

        if (ros_process.open())
            break;

        printf(".");
        std::this_thread::sleep_for(500ms);
    }

    const auto module_base = ros_process.module_base(L"ros.exe");

    printf("\nfound! [pid: 0x%I32X]\n", ros_process.pid());
    printf("base: 0x%I32X\n", module_base);

    const auto client = ros_process.read<uint32_t>(module_base + offsets::Client);
    printf("client: 0x%I32X\n", client);
    
    const auto entities = ros_process.read<uint32_t>(client + offsets::m_entities);
    printf("entities: %u\n", entities);

    if(entities == 0)
    {
        printf("please join a game first!\n");
        getchar();
        return 0;
    }

    const auto entity_list = ros_process.read<uint32_t>(client + offsets::m_ppObjects);
    

    const auto end = ros_process.read<uint32_t>(entity_list + 4);
    printf("entity list end: 0x%I32X\n", end);

    auto start = ros_process.read<uint32_t>(entity_list);
    start = ros_process.read<uint32_t>(start);
    start = ros_process.read<uint32_t>(start);

    printf("entity list start: 0x%I32X\n", start);

    printf("entity list: 0x%I32X\n", entity_list);

    if(argc > 1)
    {
        printf("ignoring anything but %s\n", argv[1]);
    }

    while(start != end)
    {
        const auto entity = ros_process.read<uint32_t>(start + 0xC);
        const auto position = ros_process.read<vec3_t>(entity + 0x10);
        const auto meta = ros_process.read<uint32_t>(entity + 0x4);
        const auto name = ros_process.read<uint32_t>(meta + 0xC);
        const auto table = ros_process.read<uint32_t>(entity + 0x100);
        const auto entries = ros_process.read<uint32_t>(table + 0xC);
        const auto first_entry = ros_process.read<uint32_t>(table + 0x14);

        char name_buffer[MAX_PATH];
        ros_process.read(name, sizeof(name_buffer), name_buffer);
        name_buffer[MAX_PATH - 1] = 0;

        if(argc > 1)
        {
            if (strcmp(argv[1], name_buffer) != 0) {
                start = ros_process.read<uint32_t>(start);
                printf("ignored %s\n", name_buffer);
                continue;
            }
        }

        printf("entity: 0x%I32X\n", start);
        printf("position: [x: %f y: %f z: %f]\n", position.x, position.y, position.z);
        printf("entity type: %s\n", name_buffer);
        printf("table: 0x%I32X, entries: %d\n", table, entries);
        
        for(unsigned i = 0; i < entries; i++)
        {
            const auto entry = first_entry + (i * sizeof(uint32_t) * 3);
            const auto name_entry = ros_process.read<uint32_t>(entry + 0x4);
            const auto data_entry = ros_process.read<uint32_t>(entry + 0x8);
          
            //printf("%I32X -> %I32X\n",entry, name_entry);          
            //printf("size:%d\n", entry_size);

            const auto entry_size = ros_process.read<uint32_t>(name_entry + 0x8);
            const auto buffer = new char[entry_size+1];

            ros_process.read(name_entry + 0x14, entry_size, buffer);
            buffer[entry_size] = 0;

            printf("\t%s\n", buffer);

            delete[] buffer;

            const auto type = ros_process.read<uint32_t>(data_entry + 0x4);
            const auto type_name = ros_process.read<uint32_t>(type + 0xC);

            char typename_buffer[MAX_PATH];
            ros_process.read(type_name, sizeof(typename_buffer), typename_buffer);
            typename_buffer[MAX_PATH - 1] = 0;

            printf("\t\t-> type: %s\n", typename_buffer);

            if(strncmp("str", typename_buffer, 4) == 0)
            {
                const auto string_length = ros_process.read<uint32_t>(data_entry + 0x8);
                printf("\t\t-> size: %d\n", string_length);
                if(string_length != 0)
                {
                    const auto string_buffer = new char[string_length + 1];
                    ros_process.read(data_entry + 0x14, string_length, string_buffer);
                    printf("\t\t-> value: \"%s\"\n", string_buffer);
                    delete[] string_buffer;
                }
            } else if (strncmp("int", typename_buffer, 4) == 0)
            {
                printf("\t\t-> value: %d\n", ros_process.read<int32_t>(data_entry + 0x14));
            } else if(strncmp("long", typename_buffer, 5) == 0)
            {
                printf("\t\t-> value: %d\n", ros_process.read<long>(data_entry + 0x14));
            }
            else if (strncmp("float", typename_buffer, 5) == 0)
            {
                printf("\t\t-> value: %f\n", ros_process.read<float>(data_entry + 0x14));
            } else if(strncmp("bool", typename_buffer, 5) == 0)
            {
                const auto value = ros_process.read<bool>(data_entry+ 0x14);

                printf("\t\t-> value: %s\n", value ? "true" : "false");
            }

            printf("\t\t-> pointer: 0x%08X + 0x14\n", data_entry);

        }

        start = ros_process.read<uint32_t>(start);
    }

}