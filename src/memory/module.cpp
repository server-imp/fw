#include "module.hpp"

#include "bitset.hpp"
#include "logger.hpp"
#include "memory.hpp"
#include "scanner.hpp"
#include "util.hpp"

#ifndef FW_MIN_STRING_LENGTH
#define FW_MIN_STRING_LENGTH 5
#endif
#ifndef FW_MAX_STRING_LENGTH
#define FW_MAX_STRING_LENGTH 128
#endif

memory::RefData::RefData(const uintptr_t instruction,
                         const uint8_t   instructionLength,
                         const Type      type,
                         const uintptr_t referenced)
    : _instruction(instruction), _instructionLength(instructionLength), _type(type), _reference(referenced)
{
}

memory::RefData::RefData(const Handle& instruction,
                         const uint8_t instructionLength,
                         const Type    type,
                         const Handle& referenced)
    : _instruction(instruction), _instructionLength(instructionLength), _type(type), _reference(referenced)
{
}

const memory::Handle& memory::RefData::instruction() const
{
    return _instruction;
}

uint8_t memory::RefData::instructionLength() const
{
    return _instructionLength;
}

memory::RefData::Type memory::RefData::type() const
{
    return _type;
}

const memory::Handle& memory::RefData::reference() const
{
    return _reference;
}

bool memory::RefData::nop() const
{
    return _instruction.nop(_instructionLength);
}

const char* memory::RefData::typeToString(const Type type)
{
    switch (type)
    {
    case Type::Any:
        return "Any";
    case Type::Address:
        return "Address";
    case Type::Read:
        return "Read";
    case Type::Write:
        return "Write";
    case Type::ReadWrite:
        return "ReadWrite";
    }

    return "Unknown";
}

std::size_t memory::RefDataHash::operator()(const RefData& obj) const noexcept
{
    return std::hash<uintptr_t> {}(obj.instruction().raw());
}

void memory::Module::initSections()
{
    if (_sectionsInitialized)
    {
        return;
    }

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
            LOG_DBG("Text section at {} [{:X}]", _textSections.back().start().formatted(), _textSections.back().size());
        }
        else if (memcmp(section->Name, ".rdata", 6) == 0 || memcmp(section->Name, ".data", 5) == 0)
        {
            _dataSections.emplace_back(Handle(secBase), secSize);
            LOG_DBG("Data section at {} [{:X}]", _dataSections.back().start().formatted(), _dataSections.back().size());
        }
    }

    if (_textSections.empty())
    {
        LOG_DBG("No text sections found");
    }

    if (_dataSections.empty())
    {
        LOG_DBG("No data sections found");
    }

    _sectionsInitialized = true;
}

void memory::Module::initEntryPoints()
{
    if (_entryPointsInitialized)
    {
        return;
    }

    LOG_DBG("Initializing entry points of module \"{}\"", _name);

    const auto  base = _start.raw();
    const auto* dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        LOG_DBG("Invalid DOS header");
        return;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
    {
        LOG_DBG("Invalid NT header");
        return;
    }

    std::vector<Range> entries {};
    const auto&        exportDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDir.VirtualAddress && exportDir.Size)
    {
        const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + exportDir.VirtualAddress);

        const auto* functions = reinterpret_cast<const DWORD*>(base + exports->AddressOfFunctions);

        entries.reserve(exports->NumberOfFunctions + 1);

        for (DWORD i = 0; i < exports->NumberOfFunctions; ++i)
        {
            const DWORD rva = functions[i];

            if (rva == 0)
            {
                continue;
            }

            if (rva >= exportDir.VirtualAddress && rva < exportDir.VirtualAddress + exportDir.Size)
            {
                continue;
            }

            entries.emplace_back(base + rva, 0);
        }

        LOG_DBG("{} export entries", entries.size());
    }

    if (nt->OptionalHeader.AddressOfEntryPoint)
    {
        entries.emplace_back(base + nt->OptionalHeader.AddressOfEntryPoint, 0);
        LOG_DBG("1 entry point");
    }

    std::sort(entries.begin(), entries.end());
    entries.erase(std::ranges::unique(entries,
                                      [](const auto& a, const auto& b)
                                      {
                                          return a.start() == b.start();
                                      })
                      .begin(),
                  entries.end());

    const auto& pdataDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (pdataDir.VirtualAddress && pdataDir.Size)
    {
        const auto* functions = reinterpret_cast<const IMAGE_RUNTIME_FUNCTION_ENTRY*>(base + pdataDir.VirtualAddress);

        const std::size_t count = pdataDir.Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);

        _entryPoints.reserve(entries.size() + count);

        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& fn = functions[i];

            if (fn.EndAddress <= fn.BeginAddress)
            {
                continue;
            }

            const auto address = base + fn.BeginAddress;
            const auto size    = fn.EndAddress - fn.BeginAddress;

            _entryPoints.emplace_back(address, size);
        }

        LOG_DBG("{} .pdata entries", _entryPoints.size());
    }

    std::sort(_entryPoints.begin(), _entryPoints.end());
    _entryPoints.erase(std::ranges::unique(_entryPoints,
                                           [](const auto& a, const auto& b)
                                           {
                                               return a.start() == b.start();
                                           })
                           .begin(),
                       _entryPoints.end());

    for (auto& entry : entries)
    {
        auto duplicate = false;

        for (const auto& existing : _entryPoints)
        {
            if (existing.contains(entry.start()))
            {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
        {
            _entryPoints.emplace_back(entry);
        }
    }

    LOG_DBG("Collected {} entry points", _entryPoints.size());
    _entryPointsInitialized = true;
}

void memory::Module::initRipRelativeIndex()
{
    if (_ripRelativeInitialized)
    {
        return;
    }

    LOG_DBG("Initializing RIP-relative index for module \"{}\"", _name);

    const auto& entries = entryPoints();

    auto start = std::chrono::high_resolution_clock::now();

    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    std::deque work(entries.begin(), entries.end());
    BitSet     visited(this->size());

    while (!work.empty())
    {
        const auto range = work.front();
        work.pop_front();

        if (!this->contains(range.start()))
        {
            continue;
        }

        auto address   = range.start();
        auto rva       = address.sub(_start);
        auto remaining = range.size() == 0 ? this->end().sub(address).raw() : range.end().sub(address).raw();

        while (remaining > 0)
        {
            if (!visited.claim(rva.raw()))
            {
                if (range.size() > 0)
                {
                    if (!visited.claim(rva.raw()))
                    {
                        address = address.add(1);
                        rva     = rva.add(1);
                        remaining--;
                        continue;
                    }
                }
                break;
            }

            ZydisDecodedInstruction instruction;
            ZydisDecoderContext     context;
            size_t                  length = range.size() == 0
                                               ? ZYDIS_MAX_INSTRUCTION_LENGTH
                                               : std::min(static_cast<uintptr_t>(ZYDIS_MAX_INSTRUCTION_LENGTH), remaining);

            if (!ZYAN_SUCCESS(
                    ZydisDecoderDecodeInstruction(&decoder, &context, address.to_ptr<void*>(), length, &instruction)))
            {
                break;
            }

            if (instruction.length == 0)
            {
                break;
            }

            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

            bool operandsDecoded = ZYAN_SUCCESS(
                ZydisDecoderDecodeOperands(&decoder, &context, &instruction, operands, instruction.operand_count));

            if (operandsDecoded)
            {
                for (size_t i = 0; i < instruction.operand_count; ++i)
                {
                    const auto& op = operands[i];

                    if (op.type != ZYDIS_OPERAND_TYPE_MEMORY || op.mem.base != ZYDIS_REGISTER_RIP)
                    {
                        continue;
                    }

                    bool read  = op.actions & ZYDIS_OPERAND_ACTION_MASK_READ;
                    bool write = op.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE;

                    RefData::Type type {};
                    if (read && write)
                    {
                        type = RefData::Type::ReadWrite;
                    }
                    else if (read)
                    {
                        type = RefData::Type::Read;
                    }
                    else if (write)
                    {
                        type = RefData::Type::Write;
                    }
                    else
                    {
                        type = RefData::Type::Address;
                    }

                    _ripRelativeInstructions.emplace(
                        address,
                        instruction.length,
                        type,
                        address.add(static_cast<ptrdiff_t>(instruction.length) + op.mem.disp.value));
                }
            }

            const bool isLoop =
                instruction.mnemonic == ZYDIS_MNEMONIC_LOOP || instruction.mnemonic == ZYDIS_MNEMONIC_LOOPE
                || instruction.mnemonic == ZYDIS_MNEMONIC_LOOPNE || instruction.mnemonic == ZYDIS_MNEMONIC_JCXZ
                || instruction.mnemonic == ZYDIS_MNEMONIC_JECXZ || instruction.mnemonic == ZYDIS_MNEMONIC_JRCXZ;

            const bool isControlFlow = instruction.meta.category == ZYDIS_CATEGORY_CALL
                                    || instruction.meta.category == ZYDIS_CATEGORY_UNCOND_BR
                                    || instruction.meta.category == ZYDIS_CATEGORY_COND_BR || isLoop;

            if (isControlFlow)
            {
                for (size_t i = 0; i < instruction.operand_count; i++)
                {
                    const auto& op = operands[i];

                    if (op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE && op.imm.is_relative)
                    {
                        auto target = address.add(static_cast<int64_t>(instruction.length) + op.imm.value.s);

                        if (this->contains(target))
                        {
                            work.emplace_back(target, 0);
                        }

                        continue;
                    }

                    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY && op.mem.base == ZYDIS_REGISTER_RIP
                        && (instruction.meta.category == ZYDIS_CATEGORY_CALL
                            || instruction.meta.category == ZYDIS_CATEGORY_UNCOND_BR))
                    {
                        auto pointerAddress = address.add(static_cast<int64_t>(instruction.length) + op.mem.disp.value);

                        if (!this->contains(pointerAddress))
                        {
                            continue;
                        }

                        auto target = pointerAddress.deref<uintptr_t>();

                        if (this->contains(target))
                        {
                            work.emplace_back(target, 0);
                        }
                    }
                }
            }

            address = address.add(instruction.length);
            rva     = rva.add(instruction.length);
            remaining -= instruction.length;
        }
    }

    _ripRelativeInitialized = true;
    auto end                = std::chrono::high_resolution_clock::now();
    auto duration           = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    LOG_DBG("Found {} instructions in {}ms", _ripRelativeInstructions.size(), duration);
}

void memory::Module::initRefStrings()
{
    if (_refStringsInitialized)
    {
        return;
    }

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

    for (const auto& data : instructions)
    {
        auto key = data.reference().raw();

        if (auto it = refState.find(key); it != refState.end())
        {
            switch (it->second)
            {
            case RefType::Reject:
                continue;
            case RefType::Ascii:
                _refStringsAscii[key].emplace_back(data);
                continue;
            case RefType::Utf16:
                _refStringsUtf16[key].emplace_back(data);
                continue;
            }
        }

        const auto& ref = data.reference();
        Range       dataSection {};

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
            _refStringsAscii.try_emplace(key).first->second.emplace_back(data);
            refState.emplace(key, RefType::Ascii);
            continue;
        }

        if (utf16MaxLen >= FW_MIN_STRING_LENGTH && util::looksLikeUtf16Ascii(ref, FW_MIN_STRING_LENGTH, utf16MaxLen))
        {
            _refStringsUtf16.try_emplace(key).first->second.emplace_back(data);
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

bool memory::Module::findFunctionStart(const Handle& instruction, Handle& functionStart)
{
    LOG_DBG("Attempting to find function start for {}", instruction.formatted());

    // try to find the function in collected entrypoints first
    for (const auto& entry : entryPoints())
    {
        if (entry.contains(instruction))
        {
            LOG_DBG("Found function in collected entrypoints: {}", entry.start().formatted());
            functionStart = entry.start();
            return true;
        }
    }

    // try janky solution
    LOG_DBG("Could not find function in collected entrypoints, attempting \"brute force\"");
    const auto baseVA = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    uintptr_t  va     = instruction.raw();
    size_t     ccSeq  = 0;

    while (va > baseVA)
    {
        --va;
        const uint8_t b = *reinterpret_cast<uint8_t*>(va);

        if (b == 0xCC && *reinterpret_cast<uint8_t*>(va - 1))
        {
            functionStart = Handle(va + 1);
            return true;
        }

        if (b == 0xCC)
        {
            if (++ccSeq >= 2)
            {
                functionStart = Handle(va + ccSeq);
                return true;
            }
        }
        else
        {
            ccSeq = 0;
        }
    }

    return false;
}

bool
memory::Module::findPattern(const std::vector<uint8_t>& data, const std::vector<uint8_t>& mask, Handle& result) const
{
    return Scanner::findPattern(*this, data, mask, result);
}

bool memory::Module::findString(const std::string& string, Handle& result)
{
    for (const auto& section : dataSections())
    {
        if (Scanner::findString(string, section, result))
        {
            return true;
        }
    }

    return false;
}

bool memory::Module::findWstring(const std::wstring& string, Handle& result)
{
    for (const auto& section : dataSections())
    {
        if (Scanner::findWstring(string, section, result))
        {
            return true;
        }
    }

    return false;
}

bool memory::Module::findReference(const Handle& handle, RefData& result, const RefData::Type type)
{
    std::vector<RefData> results {};

    if (!findReferences(handle, results, type, 1))
    {
        return false;
    }

    result = results.front();
    return true;
}

bool memory::Module::findReferences(const Handle&         handle,
                                    std::vector<RefData>& results,
                                    const RefData::Type   type,
                                    const int             max)
{
    LOG_DBG("Looking for references to {} [{}]", handle.formatted(), RefData::typeToString(type));

    results.clear();

    const uintptr_t target = handle.raw();

    for (const auto& data : ripRelativeInstructions())
    {
        if (type != RefData::Type::Any && data.type() != type)
        {
            continue;
        }

        if (data.reference() == target)
        {
            results.emplace_back(data);
            if (max > 0 && results.size() >= max)
            {
                return true;
            }
        }
    }

    LOG_DBG("Found {} references", results.size());
    return !results.empty();
}

bool memory::Module::findStringReference(const std::string& string, RefData& result)
{
    std::vector<RefData> results {};

    if (!findStringReferences(string, results, 1))
    {
        return false;
    }

    result = results.front();
    return true;
}

bool memory::Module::findStringReferences(const std::string& string, std::vector<RefData>& results, const int max)
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
        {
            continue;
        }

        if (mem[string.size()] != '\0')
        {
            continue;
        }

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

bool memory::Module::findWstringReference(const std::wstring& string, RefData& result)
{
    std::vector<RefData> results {};

    if (!findWstringReferences(string, results, 1))
    {
        return false;
    }

    result = results.front();
    return true;
}

bool memory::Module::findWstringReferences(const std::wstring& string, std::vector<RefData>& results, const int max)
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
        {
            continue;
        }

        if (mem[string.size()] != u'\0')
        {
            continue;
        }

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

const std::vector<memory::Range>& memory::Module::entryPoints()
{
    if (!_entryPointsInitialized)
    {
        initEntryPoints();
    }

    return _entryPoints;
}

const std::unordered_set<memory::RefData, memory::RefDataHash>& memory::Module::ripRelativeInstructions()
{
    if (!_ripRelativeInitialized)
    {
        initRipRelativeIndex();
    }

    return _ripRelativeInstructions;
}

const std::unordered_map<uintptr_t, std::vector<memory::RefData>>& memory::Module::refStringsAscii()
{
    if (!_refStringsInitialized)
    {
        initRefStrings();
    }

    return _refStringsAscii;
}

const std::unordered_map<uintptr_t, std::vector<memory::RefData>>& memory::Module::refStringsUtf16()
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

bool memory::Module::getDataSection(const Handle& handle, Range& result)
{
    for (const auto& section : dataSections())
    {
        if (!section.contains(handle))
        {
            continue;
        }

        result = section;
        return true;
    }

    return false;
}

void memory::Module::clear()
{
    _sectionsInitialized    = false;
    _entryPointsInitialized = false;
    _ripRelativeInitialized = false;
    _refStringsInitialized  = false;

    _dataSections.clear();
    _textSections.clear();
    _refStringsAscii.clear();
    _refStringsUtf16.clear();
    _entryPoints.clear();
    _ripRelativeInstructions.clear();
}

memory::Module::~Module()
{
    clear();
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
    if (addr.null())
    {
        return false;
    }

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

        if (addr >= result.start() && addr <= result.end())
        {
            return true;
        }
    }

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
