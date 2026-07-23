#ifndef FW_PATCH_HPP
#define FW_PATCH_HPP
#include "handle.hpp"
#include "module.hpp"

namespace memory
{
    class Patch
    {
    protected:
        std::string _name {};
        bool        _enabled {};

    public:
        Patch() = default;
        explicit Patch(std::string name);
        virtual  ~Patch() = default;

        [[nodiscard]] bool         enabled() const;
        [[nodiscard]] virtual bool valid() const;

        virtual bool enable();
        virtual bool disable();
    };

    using PatchPtr = std::shared_ptr<Patch>;

    class BytePatch : public Patch
    {
    protected:
        Handle _target {};

        std::vector<uint8_t> _original {};
        std::vector<uint8_t> _patched {};

        bool _flushInstructionCache {};

    public:
        explicit BytePatch(const std::string& name,
            const Handle&                         target,
            bool                                  flushInstructionCache,
            const std::initializer_list<uint8_t>& patchBytes
        );

        bool enable() override;
        bool disable() override;

        static std::shared_ptr<BytePatch> create(
            const std::string&                           name,
            const Handle&                         target,
            bool                                  flushInstructionCache,
            const std::initializer_list<uint8_t>& patchBytes
        );
    };

    class NopPatch : public BytePatch
    {
    public:
        explicit NopPatch(const std::string& name, const Handle& target, size_t size);
        bool     enable() override;
        bool     disable() override;

        static std::shared_ptr<NopPatch> create(const std::string& name, const Handle& target, size_t size);
    };

    class RefNopPatch : public Patch
    {
    protected:
        std::vector<PatchPtr> _patches {};

    public:
        explicit RefNopPatch(std::string name, Module& module, const Handle& target, RefData::Type refType);

        bool enable() override;
        bool disable() override;

        static std::shared_ptr<RefNopPatch> create(
            const std::string&   name,
            Module&       module,
            const Handle& target,
            RefData::Type refType
        );
    };

    class StringRefPatch : public Patch
    {
    protected:
        bool _valid {};

        Handle _lea {};
        Handle _originalString {};
        Handle _allocation {};
        size_t _allocationSize {};

    public:
        explicit StringRefPatch(std::string name, const RefData& ref);

        void setString(const std::string& string);
        void setWstring(const std::wstring& string);

        bool enable() override;
        bool disable() override;

        static std::shared_ptr<StringRefPatch> create(const std::string& name, const RefData& lea);
    };
}

#endif //FW_PATCH_HPP
