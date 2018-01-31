#include "process.hpp"
#include <cassert>
#include <ntstatus.h>
#include <TlHelp32.h>

inline bool is_equal(const PUNICODE_STRING s1, const PUNICODE_STRING s2, const bool case_insensitive)
{
    using RtlEqualUnicodeStringFn = BOOLEAN(WINAPI*)(PUNICODE_STRING, PUNICODE_STRING, BOOLEAN);
    static RtlEqualUnicodeStringFn RtlEqualUnicodeString = nullptr;
    if (RtlEqualUnicodeString == nullptr)
    {
        const auto ntdll = GetModuleHandleA("ntdll");
        assert(ntdll != nullptr);
        RtlEqualUnicodeString = (RtlEqualUnicodeStringFn)GetProcAddress(ntdll, "RtlEqualUnicodeString");
        assert(RtlEqualUnicodeString != nullptr);
    }
    return RtlEqualUnicodeString(s1, s2, case_insensitive);
}


bool process::open()
{
    if (is_open())
        return true;

    if (m_by_name)
    {
        if (!find(m_name, m_pid))
            return false;
        m_by_name = false;
    }

    m_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_pid);
    return m_handle == nullptr;
}

bool process::read(const uintptr_t address, const size_t size, void* buffer) const
{
    return ReadProcessMemory(m_handle, (void*)address, buffer, size, nullptr);
}

bool process::write(const uintptr_t address, const size_t size, void* buffer) const
{
    return WriteProcessMemory(m_handle, (void*)address, buffer, size, nullptr);
}

uintptr_t process::module_base(const std::wstring& name) const
{
    uintptr_t base_address = 0;
    MODULEENTRY32W moduleentry32 = {0};
    moduleentry32.dwSize = sizeof(moduleentry32);

    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, m_pid);

    if (snapshot != nullptr)
    {
        if (Module32FirstW(snapshot, &moduleentry32) == TRUE)
        {
            while (wcsncmp(name.c_str(), moduleentry32.szModule, name.size()) != 0)
            {
                if (Module32NextW(snapshot, &moduleentry32) == FALSE)
                {
                    moduleentry32 = {0};
                    moduleentry32.dwSize = sizeof(moduleentry32);
                    break;
                }
            }

            base_address = uintptr_t(moduleentry32.modBaseAddr);
        }
    }
    else
    {
        return 0;
    }

    CloseHandle(snapshot);
    return base_address;
}

bool process::find(const std::wstring& name, pid_t& pid)
{
    ULONG required_size = 0;
    SYSTEM_PROCESS_INFORMATION* buffer = nullptr;
    UNICODE_STRING process_name = {0};
    auto status = STATUS_SUCCESS;

    pid = 0;
    RtlInitUnicodeString(&process_name, name.c_str());

    status = NtQuerySystemInformation(SystemProcessInformation, nullptr, 0, &required_size);

    while (status == STATUS_INFO_LENGTH_MISMATCH)
    {
        delete[] buffer;
        buffer = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(new uint8_t[required_size]);
        status = NtQuerySystemInformation(SystemProcessInformation, buffer, required_size, &required_size);
    }

    if (!NT_SUCCESS(status))
    {
        delete[] buffer;
        return false;
    }

    auto current = buffer;

    while (current->NextEntryOffset != 0)
    {
        if (is_equal(&process_name, &current->ImageName, true))
        {
            pid = (pid_t)current->UniqueProcessId;
            delete[] buffer;
            return true;
        }

        current = (SYSTEM_PROCESS_INFORMATION*)(uintptr_t(current) + current->NextEntryOffset);
    }

    delete[] buffer;
    return false;
}
