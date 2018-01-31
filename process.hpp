#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define WIN32_NO_STATUS
#include <Windows.h>
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")
#undef WIN32_NO_STATUS
#include <cstdint>
#include <string>

using pid_t = uintptr_t;

class process
{
public:
    explicit process(const std::wstring& name) : m_name(name), m_by_name(true)
    {

    }

    explicit process(const pid_t pid) noexcept : m_pid(pid)
    {

    }

    bool is_open() const noexcept
    {
        return m_handle != nullptr;
    }

    pid_t pid() const noexcept
    {
        return m_pid;
    }

    ~process()
    {
        if (m_handle != nullptr)
            CloseHandle(m_handle);
    }

private:
    pid_t m_pid = 0;
    HANDLE m_handle = nullptr;
    std::wstring m_name = L"";
    bool m_by_name = false;
public:
    bool read(const uintptr_t address, const size_t size, void* buffer) const;

    bool write(const uintptr_t address, const size_t size, void* buffer) const;

    template<typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
    bool read(const uintptr_t address, T& data)
    {
        return read(address, sizeof(T), &data);
    }

    template<typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
    bool write(const uintptr_t address, T& data)
    {
        return write(address, sizeof(T), &data);
    }

    template<typename T, typename = std::enable_if_t <std::is_trivially_constructible_v<T >> >
    T read(const uintptr_t address)
    {
        T data;
        if (!read(address, sizeof(T), &data)) {
            const auto error = GetLastError();
            printf("error! %p -> %d\n", (void*)address, error);
            throw std::runtime_error("failed to rpm! e:");
        }
        return data;
    }



    bool open();

    uintptr_t module_base(const std::wstring& name) const;

    static bool find(const std::wstring& name, pid_t& pid);
};
