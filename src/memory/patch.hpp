#ifndef FW_PATCH_HPP
#define FW_PATCH_HPP
#include "handle.hpp"
#include "module.hpp"
#include "toggleable.hpp"

namespace memory
{
    class BytePatch : public Toggleable
    {
    protected:
        Handle _target {};

        std::vector<uint8_t> _original {};
        std::vector<uint8_t> _patched {};

        bool _flushInstructionCache {};

        bool internalEnable() override;
        bool internalDisable() override;

    public:
        explicit BytePatch(
            const std::string&                    name,
            const Handle&                         target,
            bool                                  flushInstructionCache,
            const std::initializer_list<uint8_t>& patchBytes
        );

        ~BytePatch() override;

        static std::shared_ptr<BytePatch> create(
            const std::string&                    name,
            const Handle&                         target,
            bool                                  flushInstructionCache,
            const std::initializer_list<uint8_t>& patchBytes
        );
    };

    class NopPatch : public BytePatch
    {
    public:
        explicit NopPatch(const std::string& name, const Handle& target, size_t size);

        static std::shared_ptr<NopPatch> create(const std::string& name, const Handle& target, size_t size);
    };

    class RefNopPatch : public Toggleable
    {
    protected:
        std::vector<PToggleable> _patches {};

        bool internalEnable() override;
        bool internalDisable() override;

    public:
        explicit RefNopPatch(const std::string& name, Module& module, const Handle& target, RefData::Type refType);

        ~RefNopPatch() override;

        static std::shared_ptr<RefNopPatch> create(
            const std::string& name,
            Module&            module,
            const Handle&      target,
            RefData::Type      refType
        );
    };

    class StringRefPatch : public Toggleable
    {
    protected:
        bool _valid {};

        Handle _lea {};
        Handle _originalString {};
        Handle _allocation {};
        size_t _allocationSize {};

        bool internalEnable() override;
        bool internalDisable() override;

    public:
        explicit StringRefPatch(std::string name, const RefData& ref);

        ~StringRefPatch() override;

        void setString(const std::string& string);
        void setWstring(const std::wstring& string);

        static std::shared_ptr<StringRefPatch> create(const std::string& name, const RefData& lea);
    };
}

#endif //FW_PATCH_HPP
