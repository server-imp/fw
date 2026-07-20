#ifndef FW_MODULE_HPP
#define FW_MODULE_HPP
#pragma once
#include "range.hpp"

namespace memory
{
    class RefData
    {
    public:
        enum class Type : uint8_t
        {
            Any,
            ReadWrite,
            Read,
            Write,
            Address
        };

    private:
        ZydisDecodedInstruction _instruction {};
        Type                    _type {};

    public:
        RefData() = default;
        explicit RefData(const ZydisDecodedInstruction& instruction, Type type);

        [[nodiscard]] const ZydisDecodedInstruction& instruction() const;
        [[nodiscard]] Type                           type() const;

        static const char* typeToString(Type type);
    };

    class Module : public Range
    {
    private:
        HMODULE               _hModule {};
        std::string           _name {};
        std::filesystem::path _path {};

        bool               _sectionsInitialized {};
        std::vector<Range> _textSections {};
        std::vector<Range> _dataSections {};

        bool                                   _ripRelativeInitialized {};
        std::unordered_map<uintptr_t, RefData> _ripRelativeInstructions {};

        bool                                               _refStringsInitialized {};
        std::unordered_map<uintptr_t, std::vector<Handle>> _refStringsAscii {};
        std::unordered_map<uintptr_t, std::vector<Handle>> _refStringsUtf16 {};

        void initRipRelativeIndex();
        void initSections();
        void initRefStrings();

    public:
        [[nodiscard]] HMODULE handle() const;

        const std::string& name();

        const std::filesystem::path& path();

        bool findPattern(const std::string& pattern, Handle& result);

        bool findString(const std::string& string, Handle& result);

        bool findWstring(const std::wstring& string, Handle& result);

        bool findReference(const Handle& handle, Handle& result, RefData::Type type = RefData::Type::Any);
        bool findReferences(
            const Handle&        handle,
            std::vector<Handle>& results,
            RefData::Type        type = RefData::Type::Any,
            int                  max  = 0
        );
        bool findStringReference(const std::string& string, Handle& result);
        bool findStringReferences(const std::string& string, std::vector<Handle>& results, int max = 0);
        bool findWstringReference(const std::wstring& string, Handle& result);
        bool findWstringReferences(const std::wstring& string, std::vector<Handle>& results, int max = 0);

        [[nodiscard]] const std::vector<Range>& textSections();
        [[nodiscard]] const std::vector<Range>& dataSections();

        bool getDataSection(const Handle& handle, Range& result);

        const std::unordered_map<uintptr_t, RefData>& ripRelativeInstructions();

        const std::unordered_map<uintptr_t, std::vector<Handle>>& refStringsAscii();
        const std::unordered_map<uintptr_t, std::vector<Handle>>& refStringsUtf16();

        bool isInCodeSection(const Handle& handle);
        bool isInDataSection(const Handle& handle);

        ~Module();

        static Module getFromHandle(HMODULE hModule);

        static bool tryGetByName(const std::string& name, Module& result);

        static Module getByName(const std::string& name);

        static bool tryGetByAddr(const Handle& addr, Module& result);

        static Module getMain();

        static Module getThis();
    };
}

#endif //FW_MODULE_HPP
