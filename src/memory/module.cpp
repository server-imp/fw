#include "module.hpp"

#include "scanner.hpp"
#include "logger.hpp"
#include "util.hpp"

#ifndef FW_MIN_STRING_LENGTH
#define FW_MIN_STRING_LENGTH 5
#endif
#ifndef FW_MAX_STRING_LENGTH
#define FW_MAX_STRING_LENGTH 128
#endif

memory::RefData::RefData(const ZydisDecodedInstruction& instruction, const Type type) : _instruction(instruction),
    _type(type) {}

const ZydisDecodedInstruction& memory::RefData::instruction() const
{
    return _instruction;
}

memory::RefData::Type memory::RefData::type() const
{
    return _type;
}

const char* memory::RefData::typeToString(const Type type)
{
    switch (type)
    {
    case Type::Any: return "Any";
    case Type::Address: return "Address";
    case Type::Read: return "Read";
    case Type::Write: return "Write";
    case Type::ReadWrite: return "ReadWrite";
    }

    return "Unknown";
}

void memory::Module::initRipRelativeIndex()
{
    if (_ripRelativeInitialized)
        return;

    LOG_DBG("Initializing RIP-relative index for module \"{}\"", _name);

    const auto sections = textSections();

    auto start = std::chrono::high_resolution_clock::now();

    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    //ZydisDecoderEnableMode(&decoder, ZYDIS_DECODER_MODE_MINIMAL, ZYAN_TRUE);

    for (const auto& section : sections)
    {
        auto      address = section.start().raw();
        ptrdiff_t offset  = 0;

        auto*      ptr  = section.start().to_ptr<uint8_t*>();
        const auto size = section.size();

        while (offset < size)
        {
            if (const auto byte = ptr[offset]; byte == 0x00 || byte == 0xCC)
            {
                ptrdiff_t skip {};
                while (offset + skip < size && ptr[offset + skip] == byte)
                    ++skip;

                if (skip >= 4)
                {
                    offset  += skip;
                    address += skip;
                    continue;
                }
            }

            ZydisDecodedInstruction instruction;
            ZydisDecoderContext     context;

            const ZyanStatus status = ZydisDecoderDecodeInstruction(
                &decoder,
                &context,
                ptr + offset,
                size - offset,
                &instruction
            );

            if (!ZYAN_SUCCESS(status))
            {
                ++offset;
                ++address;
                continue;
            }

            const auto& modrm = instruction.raw.modrm;
            const auto& disp  = instruction.raw.disp;

            if (instruction.attributes & ZYDIS_ATTRIB_HAS_MODRM && modrm.mod == 0 && modrm.rm == 5 && disp.size == 32)
            {
                ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
                if (!ZYAN_SUCCESS(
                    ZydisDecoderDecodeOperands( &decoder, &context, &instruction, operands, instruction.operand_count)
                ))
                    goto advance;

                bool read {}, write {};
                for (size_t i = 0; i < instruction.operand_count; ++i)
                {
                    const auto& op = operands[i];
                    if (op.type != ZYDIS_OPERAND_TYPE_MEMORY)
                        continue;

                    if (op.actions & ZYDIS_OPERAND_ACTION_MASK_READ)
                        read = true;
                    if (op.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE)
                        write = true;
                }

                RefData::Type type {};
                if (read && write)
                    type = RefData::Type::ReadWrite;
                else if (read)
                    type = RefData::Type::Read;
                else if (write)
                    type = RefData::Type::Write;
                else
                    type = RefData::Type::Address;

                _ripRelativeInstructions.emplace(address, RefData(instruction, type));
            }

        advance:
            offset += instruction.length;
            address += instruction.length;
        }
    }

    _ripRelativeInitialized = true;
    auto end                = std::chrono::high_resolution_clock::now();
    auto duration           = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    LOG_DBG("Found {} instructions in {}ms", _ripRelativeInstructions.size(), duration);
}

void memory::Module::initSections()
{
    if (_sectionsInitialized)
        return;

    LOG_DBG("Initializing sections of module \"{}\"", _name);

    const auto* dos = _start.to_ptr<PIMAGE_DOS_HEADER>();
    auto*       nt  = _start.add(dos->e_lfanew).to_ptr<PIMAGE_NT_HEADERS>();

    auto* section = IMAGE_FIRST_SECTION(nt);

    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
    {
        auto*  secBase = _start.to_ptr<uint8_t*>() + section->VirtualAddress;
        size_t secSize = section->Misc.VirtualSize;

        if (memcmp(section->Name, ".text", 5) == 0)
        {
            _textSections.emplace_back(Handle(secBase), secSize);
            LOG_DBG(
                "Found text section at {:08X}-{:08X}",
                _textSections.back().start().raw(),
                _textSections.back().end().raw()
            );
        }
        else if (memcmp(section->Name, ".rdata", 6) == 0 || memcmp(section->Name, ".data", 5) == 0)
        {
            _dataSections.emplace_back(Handle(secBase), secSize);
            LOG_DBG(
                "Found data section at {:08X}-{:08X}",
                _dataSections.back().start().raw(),
                _dataSections.back().end().raw()
            );
        }
    }

    if (_textSections.empty())
        LOG_DBG("No text sections found");

    if (_dataSections.empty())
        LOG_DBG("No data sections found");

    _sectionsInitialized = true;
}

void memory::Module::initRefStrings()
{
    if (_refStringsInitialized)
        return;

    LOG_DBG("Collecting referenced strings in module \"{}\"", _name);

    enum class RefType : uint8_t
    {
        Reject,
        Ascii,
        Utf16
    };
    std::unordered_map<uintptr_t, RefType> refState {};

    const auto instructions = ripRelativeInstructions();
    const auto start        = std::chrono::high_resolution_clock::now();

    for (const auto& [address, data] : instructions)
    {
        const auto& ins = data.instruction();
        auto        key = address + ins.length + ins.raw.disp.value;

        if (auto it = refState.find(key); it != refState.end())
        {
            switch (it->second)
            {
            case RefType::Reject: continue;
            case RefType::Ascii: _refStringsAscii[key].emplace_back(address);
                continue;
            case RefType::Utf16: _refStringsUtf16[key].emplace_back(address);
                continue;
            }
        }

        auto  ref = Handle(key);
        Range dataSection {};

        if (!getDataSection(ref, dataSection))
        {
            refState.emplace(key, RefType::Reject);
            continue;
        }

        const size_t remainingBytes = dataSection.end().raw() - ref.raw();

        const size_t asciiMaxLen = std::min<size_t>(FW_MAX_STRING_LENGTH, remainingBytes);
        const size_t utf16MaxLen = std::min<size_t>(FW_MAX_STRING_LENGTH, remainingBytes / sizeof(char16_t));

        if (asciiMaxLen >= FW_MIN_STRING_LENGTH && util::looksLikeAscii(ref, FW_MIN_STRING_LENGTH, asciiMaxLen))
        {
            _refStringsAscii.try_emplace(key).first->second.emplace_back(address);
            refState.emplace(key, RefType::Ascii);
            continue;
        }

        if (utf16MaxLen >= FW_MIN_STRING_LENGTH && util::looksLikeUtf16Ascii(ref, FW_MIN_STRING_LENGTH, utf16MaxLen))
        {
            _refStringsUtf16.try_emplace(key).first->second.emplace_back(address);
            refState.emplace(key, RefType::Utf16);
            continue;
        }

        refState.emplace(key, RefType::Reject);
    }

    _refStringsInitialized = true;
    const auto end         = std::chrono::high_resolution_clock::now();
    auto       duration    = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    LOG_DBG("Found {} referenced strings in {}ms", _refStringsAscii.size() + _refStringsUtf16.size(), duration);
}

HMODULE memory::Module::handle() const
{
    return _hModule;
}

const std::string& memory::Module::name()
{
    return _name;
}

const std::filesystem::path& memory::Module::path()
{
    return _path;
}

bool memory::Module::findPattern(const std::string& pattern, Handle& result)
{
    for (const auto& section : textSections())
    {
        if (Scanner::findPattern(pattern, section, result))
            return true;
    }

    return false;
}

bool memory::Module::findString(const std::string& string, Handle& result)
{
    for (const auto& section : dataSections())
    {
        if (Scanner::findString(string, section, result))
            return true;
    }

    return false;
}

bool memory::Module::findWstring(const std::wstring& string, Handle& result)
{
    for (const auto& section : dataSections())
    {
        if (Scanner::findWstring(string, section, result))
            return true;
    }

    return false;
}

bool memory::Module::findReference(const Handle& handle, Handle& result, const RefData::Type type)
{
    std::vector<Handle> results {};

    if (!findReferences(handle, results, type, 1))
    {
        return false;
    }

    result = results.front();
    return true;
}

bool memory::Module::findReferences(
    const Handle&        handle,
    std::vector<Handle>& results,
    const RefData::Type  type,
    const int            max
)
{
    LOG_DBG("Looking for references to {:08X} [{}]", handle.raw(), RefData::typeToString(type));

    results.clear();

    const uintptr_t target = handle.raw();

    for (const auto& [address, data] : ripRelativeInstructions())
    {
        if (type != RefData::Type::Any && data.type() != type)
            continue;

        const auto& ins = data.instruction();

        if (address + ins.length + ins.raw.disp.value == target)
        {
            results.emplace_back(address);
            if (max > 0 && results.size() >= max)
            {
                return true;
            }
        }
    }

    LOG_DBG("Found {} references", results.size());
    return !results.empty();
}

bool memory::Module::findStringReference(const std::string& string, Handle& result)
{
    std::vector<Handle> results {};

    if (!findStringReferences(string, results, 1))
    {
        return false;
    }

    result = results.front();
    return true;
}

bool memory::Module::findStringReferences(const std::string& string, std::vector<Handle>& results, const int max)
{
    LOG_DBG("Looking for references to \"{}\"", string);

    results.clear();

    if (string.empty())
    {
        LOG_DBG("Empty string");
        return false;
    }

    if (string.size() > FW_MAX_STRING_LENGTH - 1)
    {
        LOG_DBG("String exceeds FW_MAX_STRING_LENGTH");
        return false;
    }

    for (const auto& [ptr, accessors] : refStringsAscii())
    {
        const auto mem = reinterpret_cast<const char*>(ptr);
        if (std::memcmp(mem, string.data(), string.size()) != 0)
            continue;

        if (mem[string.size()] != '\0')
            continue;

        results.insert(results.end(), accessors.begin(), accessors.end());

        if (max > 0 && results.size() >= max)
        {
            results.resize(max);

            LOG_DBG("Found {} references", results.size());
            return true;
        }
    }

    if (results.empty())
    {
        LOG_DBG("Could not find any reference");
        return false;
    }

    LOG_DBG("Found {} references", results.size());
    return true;
}

bool memory::Module::findWstringReference(const std::wstring& string, Handle& result)
{
    std::vector<Handle> results {};

    if (!findWstringReferences(string, results, 1))
    {
        return false;
    }

    result = results.front();
    return true;
}

bool memory::Module::findWstringReferences(const std::wstring& string, std::vector<Handle>& results, const int max)
{
    LOG_DBG("Looking for references to \"{}\"", util::wstringToString(string));

    results.clear();

    if (string.empty())
    {
        LOG_DBG("Empty string");
        return false;
    }

    if (string.size() > FW_MAX_STRING_LENGTH - 1)
    {
        LOG_DBG("String exceeds FW_MAX_STRING_LENGTH");
        return false;
    }

    for (const auto& [ptr, accessors] : refStringsUtf16())
    {
        const auto* mem = reinterpret_cast<const char16_t*>(ptr);
        if (std::memcmp(mem, string.data(), string.size() * sizeof(char16_t)) != 0)
            continue;

        if (mem[string.size()] != u'\0')
            continue;

        results.insert(results.end(), accessors.begin(), accessors.end());

        if (max > 0 && results.size() >= max)
        {
            results.resize(max);

            LOG_DBG("Found {} references", results.size());
            return true;
        }
    }

    if (results.empty())
    {
        LOG_DBG("Could not find any reference");
        return false;
    }

    LOG_DBG("Found {} references", results.size());
    return true;
}

const std::vector<memory::Range>& memory::Module::textSections()
{
    if (!_sectionsInitialized)
    {
        initSections();
    }

    return _textSections;
}

const std::vector<memory::Range>& memory::Module::dataSections()
{
    if (!_sectionsInitialized)
    {
        initSections();
    }

    return _dataSections;
}

bool memory::Module::getDataSection(const Handle& handle, Range& result)
{
    for (const auto& section : dataSections())
    {
        if (!section.contains(handle))
            continue;

        result = section;
        return true;
    }

    return false;
}

const std::unordered_map<uintptr_t, memory::RefData>& memory::Module::ripRelativeInstructions()
{
    if (!_ripRelativeInitialized)
    {
        initRipRelativeIndex();
    }

    return _ripRelativeInstructions;
}

const std::unordered_map<uintptr_t, std::vector<memory::Handle>>& memory::Module::refStringsAscii()
{
    if (!_refStringsInitialized)
    {
        initRefStrings();
    }

    return _refStringsAscii;
}

const std::unordered_map<uintptr_t, std::vector<memory::Handle>>& memory::Module::refStringsUtf16()
{
    if (!_refStringsInitialized)
    {
        initRefStrings();
    }

    return _refStringsUtf16;
}

bool memory::Module::isInCodeSection(const Handle& handle)
{
    return contains(textSections(), handle);
}

bool memory::Module::isInDataSection(const Handle& handle)
{
    return contains(dataSections(), handle);
}

memory::Module::~Module()
{
    _dataSections.clear();
    _textSections.clear();
    _refStringsAscii.clear();
    _refStringsUtf16.clear();
    _ripRelativeInstructions.clear();
}

memory::Module memory::Module::getFromHandle(const HMODULE hModule)
{
    if (!hModule)
    {
        LOG_DBG("Invalid module handle");
        return {};
    }

    MODULEINFO moduleInfo {};
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo)))
    {
        LOG_DBG("GetModuleInformation failed: {}", GetLastError());
        return {};
    }
    char dllPath[MAX_PATH] {};
    if (!GetModuleFileName(hModule, dllPath, MAX_PATH))
    {
        LOG_DBG("GetModuleFileName failed: {}", GetLastError());
        return {};
    }

    Module result {};
    result._hModule = hModule;
    result._path    = dllPath;
    result._name    = result._path.filename().string();

    result._start = Handle(moduleInfo.lpBaseOfDll);
    result._end   = result._start.add(moduleInfo.SizeOfImage);
    result._size  = moduleInfo.SizeOfImage;

    return result;
}

bool memory::Module::tryGetByName(const std::string& name, Module& result)
{
    if (name.empty())
    {
        LOG_DBG("Getting main module");
    }
    else
    {
        LOG_DBG("Getting module by name \"{}\"", name);
    }

    const auto hModule = GetModuleHandleA(name.empty() ? nullptr : name.c_str());
    if (!hModule)
    {
        LOG_DBG("GetModuleHandle failed: {}", GetLastError());
        return false;
    }

    result = getFromHandle(hModule);
    if (!result.size())
    {
        return false;
    }

    LOG_DBG("Found module at {:08X}", result._start.raw());
    return true;
}

memory::Module memory::Module::getByName(const std::string& name)
{
    Module result {};
    if (!tryGetByName(name, result))
    {
        LOG_DBG("tryGetModuleByName failed");
    }
    return result;
}

bool memory::Module::tryGetByAddr(const Handle& addr, Module& result)
{
    if (!addr.raw())
    {
        return false;
    }

    LOG_DBG("Attempting to find module that holds address {:08X}", addr.raw());
    DWORD needed = 0;

    // Get the required size
    if (!EnumProcessModules(GetCurrentProcess(), nullptr, 0, &needed) || needed == 0)
    {
        LOG_DBG("EnumProcessModules[1] failed: {}", GetLastError());
        return false;
    }

    std::vector<HMODULE> mods(needed / sizeof(HMODULE));
    if (!EnumProcessModules(GetCurrentProcess(), mods.data(), needed, &needed))
    {
        LOG_DBG("EnumProcessModules[2] failed: {}", GetLastError());
        return false;
    }

    for (const HMODULE mod : mods)
    {
        result = getFromHandle(mod);
        if (!result.size())
        {
            continue;
        }

        if (addr >= result.start() && addr < result.end())
        {
            LOG_DBG("Found module: {}", result.name());
            return true;
        }
    }

    LOG_DBG("Not found");
    return false;
}

memory::Module memory::Module::getMain()
{
    return getByName("");
}

memory::Module memory::Module::getThis()
{
    HMODULE hModule = nullptr;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(&getThis), &hModule);

    return getFromHandle(hModule);
}
