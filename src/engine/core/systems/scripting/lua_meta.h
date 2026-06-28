#pragma once

#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include <sol/sol.hpp>

namespace hob {
    // ---------------------------------------------------------------------
    // LuaTypeName trait — maps C++ types to Lua annotation type names.
    // Specialize via HOB_LUA_TYPE for every usertype/enum exposed to Lua.
    // ---------------------------------------------------------------------

    template<typename T>
    struct LuaTypeName {
        static constexpr const char* value = nullptr;
    };

#define HOB_LUA_TYPE(CppType, LuaName)                                                                                 \
    template<>                                                                                                         \
    struct LuaTypeName<CppType> {                                                                                      \
        static constexpr const char* value = LuaName;                                                                  \
    };

    // clang-format off
    HOB_LUA_TYPE(void, "")
    HOB_LUA_TYPE(bool, "boolean")
    HOB_LUA_TYPE(short, "integer")
    HOB_LUA_TYPE(unsigned short, "integer")
    HOB_LUA_TYPE(int, "integer")
    HOB_LUA_TYPE(unsigned int, "integer")
    HOB_LUA_TYPE(long, "integer")
    HOB_LUA_TYPE(unsigned long, "integer")
    HOB_LUA_TYPE(long long, "integer")
    HOB_LUA_TYPE(unsigned long long, "integer")
    HOB_LUA_TYPE(float, "number")
    HOB_LUA_TYPE(double, "number")
    HOB_LUA_TYPE(std::string, "string")
    HOB_LUA_TYPE(const char*, "string")
    HOB_LUA_TYPE(sol::table, "table")
    // clang-format on

    namespace meta_detail {
        template<typename T>
        using strip_t = std::remove_cv_t<std::remove_reference_t<std::remove_pointer_t<std::remove_cv_t<T>>>>;

        template<typename T>
        struct is_sol_optional : std::false_type {};

        template<typename T>
        struct is_sol_optional<sol::optional<T>> : std::true_type {};

        template<typename T>
        struct is_std_vector : std::false_type {};

        template<typename T, typename A>
        struct is_std_vector<std::vector<T, A>> : std::true_type {};

        template<typename T>
        struct is_shared_ptr : std::false_type {};

        template<typename T>
        struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

        template<typename T>
        const char* lua_name() {
            using no_cv_ref = std::remove_cv_t<std::remove_reference_t<T>>;

            // Direct hit on the (cv/ref-stripped) type first. Catches `const char*`
            // → "string" before the pointer branch below would turn it into "string?".
            if constexpr (LuaTypeName<no_cv_ref>::value != nullptr) {
                return LuaTypeName<no_cv_ref>::value;
            }
            // Raw pointer to a usertype → nullable in Lua (sol2 converts nullptr to nil).
            else if constexpr (std::is_pointer_v<no_cv_ref>) {
                using pointee = strip_t<no_cv_ref>;
                static const std::string s =
                    std::string(LuaTypeName<pointee>::value ? LuaTypeName<pointee>::value : "any") + "?";
                return s.c_str();
            }
            else if constexpr (is_sol_optional<no_cv_ref>::value) {
                using inner = strip_t<typename no_cv_ref::value_type>;
                static const std::string s =
                    std::string(LuaTypeName<inner>::value ? LuaTypeName<inner>::value : "any") + "?";
                return s.c_str();
            }
            else if constexpr (is_std_vector<no_cv_ref>::value) {
                using inner = strip_t<typename no_cv_ref::value_type>;
                static const std::string s =
                    std::string(LuaTypeName<inner>::value ? LuaTypeName<inner>::value : "any") + "[]";
                return s.c_str();
            }
            // std::shared_ptr<T> → nullable in Lua (sol2 converts empty shared_ptr to nil).
            else if constexpr (is_shared_ptr<no_cv_ref>::value) {
                using inner = strip_t<typename no_cv_ref::element_type>;
                static const std::string s =
                    std::string(LuaTypeName<inner>::value ? LuaTypeName<inner>::value : "any") + "?";
                return s.c_str();
            }
            else {
                return "any";
            }
        }

        // Function traits — extracts return type and arg pack from function
        // pointers, member func pointers, and lambdas (via operator() const).
        template<typename F, typename = void>
        struct func_traits;

        template<typename R, typename... A>
        struct func_traits<R (*)(A...)> {
            using ret = R;
            using args = std::tuple<A...>;
        };

        template<typename R, typename C, typename... A>
        struct func_traits<R (C::*)(A...)> {
            using ret = R;
            using args = std::tuple<A...>;
        };

        template<typename R, typename C, typename... A>
        struct func_traits<R (C::*)(A...) const> {
            using ret = R;
            using args = std::tuple<A...>;
        };

        // Lambda / functor: delegate to its const call operator.
        template<typename F>
        struct func_traits<F, std::void_t<decltype(&F::operator())>> : func_traits<decltype(&F::operator())> {};

        // Tail of a tuple (drop the first element). Undefined for empty tuples;
        // guard with first_arg_is_self<T, Tuple>() before using.
        template<typename Tuple>
        struct tail_impl;

        template<typename Head, typename... Tail>
        struct tail_impl<std::tuple<Head, Tail...>> {
            using type = std::tuple<Tail...>;
        };

        template<typename Tuple>
        using tail_t = tail_impl<Tuple>::type;

        template<typename T, typename Tuple>
        constexpr bool first_arg_is_self() {
            if constexpr (std::tuple_size_v<Tuple> == 0) {
                return false;
            }
            else {
                using first = std::tuple_element_t<0, Tuple>;
                return std::is_same_v<strip_t<first>, T>;
            }
        }

        template<typename Tuple, std::size_t... I>
        void fill_arg_names(std::vector<std::string>& out, std::index_sequence<I...>) {
            (out.emplace_back(lua_name<std::tuple_element_t<I, Tuple>>()), ...);
        }

        template<typename Tuple>
        std::vector<std::string> arg_names() {
            std::vector<std::string> out;
            fill_arg_names<Tuple>(out, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
            return out;
        }

        template<typename V>
        std::string to_lua_literal(const V& value) {
            if constexpr (std::is_same_v<V, bool>) {
                return value ? "true" : "false";
            }
            else if constexpr (std::is_arithmetic_v<V>) {
                return std::format("{}", value);
            }
            else if constexpr (std::is_convertible_v<V, std::string_view>) {
                // No escaping; assumes engine-provided literals don't contain quotes/backslashes.
                return std::format("\"{}\"", std::string_view(value));
            }
            else {
                return "nil";
            }
        }
    } // namespace meta_detail

    // ---------------------------------------------------------------------
    // Registry data model.
    // ---------------------------------------------------------------------

    struct LuaMethodInfo {
        std::string name;
        std::string ret;
        std::vector<std::string> args; // Lua type names, one per arg
        std::vector<std::string> arg_names; // optional, parallel to args; empty -> emit arg1, arg2, ...
        bool is_static = false;
    };

    struct LuaFieldInfo {
        std::string name;
        std::string type;
        std::string value; // only used for global fields; empty for usertype/table fields
    };

    struct LuaOperatorInfo {
        std::string op; // Operator name: add, sub, unm, mul, div
        std::string rhs; // empty for unary
        std::string ret;
    };

    struct LuaCtorInfo {
        std::vector<std::string> args;
        std::vector<std::string> arg_names; // optional, parallel to args; empty -> emits arg1, arg2, ...
    };

    struct LuaUsertypeInfo {
        std::string name;
        std::string base; // optional base class for ---@class inheritance
        std::vector<LuaFieldInfo> fields;
        std::vector<LuaMethodInfo> methods;
        std::vector<LuaOperatorInfo> operators;
        std::vector<LuaCtorInfo> ctors;

        // sol2 doesn't propagate metamethods (__tostring, __concat, etc.) through
        // sol::bases — those are looked up directly on the metatable, not via __index.
        // We cache them here so derived usertypes can re-install them at bind time.
        sol::function metamethod_tostring;
        sol::function metamethod_concat;
    };

    struct LuaEnumEntry {
        std::string name;
        int value;
    };

    struct LuaEnumInfo {
        std::string name;
        std::vector<LuaEnumEntry> values;
    };

    struct LuaTableInfo {
        std::string name;
        std::vector<LuaFieldInfo> fields; // constants
        std::vector<LuaMethodInfo> methods; // free functions (emitted as Name.func(...))
    };

    class LuaMetaRegistry {
        std::vector<LuaUsertypeInfo> m_usertypes;
        std::vector<LuaEnumInfo> m_enums;
        std::vector<LuaTableInfo> m_tables;
        std::vector<LuaMethodInfo> m_global_funcs; // free functions in _G
        std::vector<LuaFieldInfo> m_global_fields; // fields in _G

    public:
        std::vector<LuaMethodInfo>& global_funcs() {
            return m_global_funcs;
        }

        const std::vector<LuaMethodInfo>& global_funcs() const {
            return m_global_funcs;
        }

        std::vector<LuaFieldInfo>& global_fields() {
            return m_global_fields;
        }

        const std::vector<LuaFieldInfo>& global_fields() const {
            return m_global_fields;
        }

        LuaUsertypeInfo& add_usertype(std::string name, std::string base = {}) {
            LuaUsertypeInfo info;
            info.name = std::move(name);
            info.base = std::move(base);
            m_usertypes.push_back(std::move(info));
            return m_usertypes.back();
        }

        LuaEnumInfo& add_enum(std::string name) {
            LuaEnumInfo info;
            info.name = std::move(name);
            m_enums.push_back(std::move(info));
            return m_enums.back();
        }

        LuaTableInfo& add_table(std::string name) {
            m_tables.push_back({std::move(name), {}});
            return m_tables.back();
        }

        LuaUsertypeInfo* find_usertype(std::string_view name) {
            for (auto& ut : m_usertypes) {
                if (ut.name == name) {
                    return &ut;
                }
            }

            return nullptr;
        }

        bool write_to_file(const std::filesystem::path& full_path) const;
    };

    // ---------------------------------------------------------------------
    // UsertypeBuilder — thin wrapper around sol::usertype<T> that also
    // records meta for the annotation generator.
    // ---------------------------------------------------------------------

    // Bases<...> tag used to declare a usertype's C++ base classes for the
    // builder. Maps to sol::base_classes and the LuaLS ---@class : Base list.
    template<typename... B>
    struct Bases {};

    template<typename T>
    class UsertypeBuilder {
        sol::usertype<T> m_usertype;
        LuaUsertypeInfo* m_info;

    public:
        template<typename... B>
        UsertypeBuilder(sol::state& lua, LuaMetaRegistry& reg, Bases<B...>)
            : m_usertype(make_usertype<B...>(lua, LuaTypeName<T>::value))
            , m_info(&reg.add_usertype(LuaTypeName<T>::value, base_name<B...>())) {
            static_assert(LuaTypeName<T>::value != nullptr,
                          "Missing HOB_LUA_TYPE specialization for this usertype; declare it in lua_type_names.h");

            // Pull metamethods from each base (they don't propagate through sol::bases).
            (inherit_metamethods_from<B>(reg), ...);
        }

        // ----- Constructors. Pass each ctor as sol::types<Args...>. -----
        template<typename... Sigs>
        UsertypeBuilder& ctors() {
            m_usertype[sol::call_constructor] = sol::factories(make_factory(Sigs{})...);
            (record_ctor(Sigs{}), ...);
            return *this;
        }

        // Custom factory constructor — useful when construction isn't a simple
        // positional ctor (e.g. takes a config table and resolves paths through a
        // subsystem). Arg types are derived from F via func_traits; `arg_names`
        // is for autocompletion only (same pattern as `method`).
        // Replaces any previously registered call-constructor on this usertype.
        template<typename F>
        UsertypeBuilder& factory_ctor(F func, std::initializer_list<const char*> arg_names = {}) {
            m_usertype[sol::call_constructor] = sol::factories(func);
            using traits = meta_detail::func_traits<F>;
            using all_args = traits::args;

            LuaCtorInfo c;
            c.args = meta_detail::arg_names<all_args>();
            for (const char* n : arg_names) {
                c.arg_names.emplace_back(n);
            }
            m_info->ctors.push_back(std::move(c));
            return *this;
        }

        // ----- Fields (public member data) -----
        template<typename M, typename C>
        UsertypeBuilder& field(const char* name, M C::* ptr) {
            m_usertype[name] = ptr;
            m_info->fields.push_back({name, meta_detail::lua_name<M>()});
            return *this;
        }

        // ----- Computed property (read-only). Exposed to Lua as `obj.name`. -----
        template<typename G>
        UsertypeBuilder& property(const char* name, G getter) {
            m_usertype[name] = sol::property(getter);
            using traits = meta_detail::func_traits<G>;
            m_info->fields.push_back({name, meta_detail::lua_name<typename traits::ret>()});
            return *this;
        }

        // Explicit type override for cases the trait can't deduce
        // (getters returning sol::object, etc.). `type` is the LuaCATS type
        // name to emit, e.g. "Entity?" or "RaycastHit[]".
        template<typename G>
        UsertypeBuilder& property_sig(const char* name, G getter, const char* type) {
            m_usertype[name] = sol::property(getter);
            m_info->fields.push_back({name, type});
            return *this;
        }

        // ----- Methods. Member func -> :method(), free/static func -> .method() -----
        // Optional `arg_names` provides parameter names for the generated
        // annotations; if empty, falls back to arg1, arg2, ...
        template<typename F>
        UsertypeBuilder& method(const char* name, F func, std::initializer_list<const char*> arg_names = {}) {
            m_usertype[name] = func;
            record_method(name, func, arg_names);
            return *this;
        }

        // Explicit signature override for cases the trait can't deduce
        // (lambdas, sol::variadic_args, overloads). The signature uses the
        // Annotation tail-shape: "(arg1: type, arg2: type): ret"
        template<typename F>
        UsertypeBuilder& method_sig(const char* name, F func, const char* sig, bool is_static = false) {
            m_usertype[name] = func;
            LuaMethodInfo info;
            info.name = name;
            info.is_static = is_static;
            info.ret = sig; // emitter recognizes leading '('
            m_info->methods.push_back(std::move(info));
            return *this;
        }

        // ----- Operators that emit ---@operator lines -----
        template<typename F>
        UsertypeBuilder& op_add(F func) {
            return binary_op(sol::meta_function::addition, "add", func);
        }

        template<typename F>
        UsertypeBuilder& op_sub(F func) {
            return binary_op(sol::meta_function::subtraction, "sub", func);
        }

        template<typename F>
        UsertypeBuilder& op_mul(F func) {
            return binary_op(sol::meta_function::multiplication, "mul", func);
        }

        template<typename F>
        UsertypeBuilder& op_div(F func) {
            return binary_op(sol::meta_function::division, "div", func);
        }

        template<typename F>
        UsertypeBuilder& op_unm(F func) {
            return unary_op(sol::meta_function::unary_minus, "unm", func);
        }

        // Metamethods that have no ---@operator counterpart.
        template<typename F>
        UsertypeBuilder& op_eq(F func) {
            m_usertype[sol::meta_function::equal_to] = func;
            return *this;
        }

        template<typename F>
        UsertypeBuilder& op_tostring(F func) {
            sol::function fn = wrap_self_callable(func);
            m_usertype[sol::meta_function::to_string] = fn;
            m_info->metamethod_tostring = std::move(fn);
            return *this;
        }

        template<typename F>
        UsertypeBuilder& op_concat(F tostring_func) {
            auto concat_lambda = [tostring_func](const sol::object& a, const sol::object& b) -> std::string {
                auto to_str = [&](const sol::object& obj) -> std::string {
                    if (obj.is<T>()) {
                        T& self = obj.as<T&>();
                        if constexpr (std::is_member_function_pointer_v<F>) {
                            return (self.*tostring_func)();
                        }
                        else {
                            return tostring_func(self);
                        }
                    }
                    return obj.as<std::string>();
                };
                return to_str(a) + to_str(b);
            };

            sol::function fn = make_function(concat_lambda);
            m_usertype[sol::meta_function::concatenation] = fn;
            m_info->metamethod_concat = std::move(fn);
            return *this;
        }

        // Direct sol2 access for things outside this wrapper's API.
        sol::usertype<T>& sol() {
            return m_usertype;
        }

    private:
        // sol::function can't be constructed directly from a raw lambda; sol2 needs the
        // value to be pushed into the Lua state first. Stash through a temporary global,
        // read back as sol::function, then clear the global.
        template<typename F>
        sol::function make_function(F func) {
            sol::state_view lua(m_usertype.lua_state());
            lua["__hob_meta_tmp__"] = std::move(func);
            sol::function fn = lua["__hob_meta_tmp__"];
            lua["__hob_meta_tmp__"] = sol::lua_nil;
            return fn;
        }

        // Wraps `func` into a sol::function so it can be stored on LuaUsertypeInfo and
        // re-installed onto derived metatables. Member-function pointers need to go through
        // a lambda first since make_function can't push a member ptr alone.
        template<typename F>
        sol::function wrap_self_callable(F func) {
            if constexpr (std::is_member_function_pointer_v<F>) {
                return make_function([func](T& self) {
                    return (self.*func)();
                });
            }
            else {
                return make_function(func);
            }
        }

        template<typename B>
        void inherit_metamethods_from(LuaMetaRegistry& reg) {
            const char* base_type_name = LuaTypeName<B>::value;
            if (!base_type_name) {
                return;
            }

            LuaUsertypeInfo* base_info = reg.find_usertype(base_type_name);
            if (!base_info) {
                return;
            }

            // Forward-chain so a grandchild type binding also sees these.
            if (base_info->metamethod_tostring.valid()) {
                m_usertype[sol::meta_function::to_string] = base_info->metamethod_tostring;
                m_info->metamethod_tostring = base_info->metamethod_tostring;
            }

            if (base_info->metamethod_concat.valid()) {
                m_usertype[sol::meta_function::concatenation] = base_info->metamethod_concat;
                m_info->metamethod_concat = base_info->metamethod_concat;
            }
        }

        template<typename F>
        UsertypeBuilder& binary_op(sol::meta_function mf, const char* op_name, F func) {
            m_usertype[mf] = func;
            using traits = meta_detail::func_traits<F>;
            using args_t = traits::args;
            static_assert(std::tuple_size_v<args_t> >= 1, "binary_op expects at least one rhs arg");
            // Member operator+ has args=(rhs); free op+(lhs, rhs) has args=(lhs, rhs).
            constexpr std::size_t rhs_idx = std::tuple_size_v<args_t> - 1;
            LuaOperatorInfo info;
            info.op = op_name;
            info.rhs = meta_detail::lua_name<std::tuple_element_t<rhs_idx, args_t>>();
            info.ret = meta_detail::lua_name<typename traits::ret>();
            m_info->operators.push_back(std::move(info));
            return *this;
        }

        template<typename F>
        UsertypeBuilder& unary_op(sol::meta_function mf, const char* op_name, F func) {
            m_usertype[mf] = func;
            using traits = meta_detail::func_traits<F>;
            LuaOperatorInfo info;
            info.op = op_name;
            info.ret = meta_detail::lua_name<typename traits::ret>();
            m_info->operators.push_back(std::move(info));
            return *this;
        }

        template<typename... B>
        static sol::usertype<T> make_usertype(sol::state& lua, const char* name) {
            if constexpr (sizeof...(B) > 0) {
                return lua.new_usertype<T>(name, sol::no_constructor, sol::base_classes, sol::bases<B...>());
            }
            else {
                return lua.new_usertype<T>(name, sol::no_constructor);
            }
        }

        template<typename... B>
        static std::string base_name() {
            if constexpr (sizeof...(B) > 0) {
                using first = std::tuple_element_t<0, std::tuple<B...>>;
                const char* n = LuaTypeName<first>::value;
                return n ? n : "";
            }
            else {
                return {};
            }
        }

        template<typename... A>
        static auto make_factory(sol::types<A...>) {
            return [](A... a) {
                return T(a...);
            };
        }

        template<typename... A>
        void record_ctor(sol::types<A...>) {
            LuaCtorInfo c;
            (c.args.emplace_back(meta_detail::lua_name<A>()), ...);
            m_info->ctors.push_back(std::move(c));
        }

        template<typename F>
        void record_method(const char* name, F /*func*/, std::initializer_list<const char*> arg_names) {
            using traits = meta_detail::func_traits<F>;
            using all_args = traits::args;

            LuaMethodInfo info;
            info.name = name;
            info.ret = meta_detail::lua_name<typename traits::ret>();

            if constexpr (std::is_member_function_pointer_v<F>) {
                // Real member func pointer: always an instance method, no self in args.
                info.is_static = false;
                info.args = meta_detail::arg_names<all_args>();
            }
            else if constexpr (std::is_pointer_v<F> && std::is_function_v<std::remove_pointer_t<F>>) {
                // Plain function pointer (free func or static member func): always static.
                info.is_static = true;
                info.args = meta_detail::arg_names<all_args>();
            }
            else if constexpr (meta_detail::first_arg_is_self<T, all_args>()) {
                // Lambda / functor whose first arg is T -> instance method.
                info.is_static = false;
                info.args = meta_detail::arg_names<meta_detail::tail_t<all_args>>();
            }
            else {
                // Lambda / functor with no self -> static.
                info.is_static = true;
                info.args = meta_detail::arg_names<all_args>();
            }

            for (const char* n : arg_names) {
                info.arg_names.emplace_back(n);
            }
            m_info->methods.push_back(std::move(info));
        }
    };

    template<typename T, typename... B>
    UsertypeBuilder<T> bind_usertype(sol::state& lua, LuaMetaRegistry& reg, Bases<B...> bases = {}) {
        return UsertypeBuilder<T>(lua, reg, bases);
    }

    // ---------------------------------------------------------------------
    // TableBuilder — wraps a global table of constants.
    // ---------------------------------------------------------------------

    class TableBuilder {
        sol::table m_table;
        LuaTableInfo* m_info;

    public:
        TableBuilder(sol::state& lua, LuaMetaRegistry& reg, const char* name)
            : m_table(lua.create_named_table(name))
            , m_info(&reg.add_table(name)) {}

        template<typename V>
        TableBuilder& constant(const char* name, V value) {
            m_table[name] = value;
            m_info->fields.push_back({name, meta_detail::lua_name<V>()});
            return *this;
        }

        // Free function on a table. Always emitted as Name.func(...).
        template<typename F>
        TableBuilder& func(const char* name, F func, std::initializer_list<const char*> arg_names = {}) {
            m_table.set_function(name, func);
            using traits = meta_detail::func_traits<F>;
            LuaMethodInfo info;
            info.name = name;
            info.is_static = true;
            info.ret = meta_detail::lua_name<typename traits::ret>();
            info.args = meta_detail::arg_names<typename traits::args>();
            for (const char* n : arg_names) {
                info.arg_names.emplace_back(n);
            }
            m_info->methods.push_back(std::move(info));
            return *this;
        }

        // Explicit signature override (variadics, complex types). The signature
        // uses the tail-shape: "(name1: type, name2: type): ret".
        template<typename F>
        TableBuilder& func_sig(const char* name, F func, const char* sig) {
            m_table.set_function(name, func);
            LuaMethodInfo info;
            info.name = name;
            info.is_static = true;
            info.ret = sig; // emitter recognizes leading '('
            m_info->methods.push_back(std::move(info));
            return *this;
        }
    };

    inline TableBuilder bind_table(sol::state& lua, LuaMetaRegistry& reg, const char* name) {
        return TableBuilder(lua, reg, name);
    }

    // ---------------------------------------------------------------------
    // Enum binding.
    // ---------------------------------------------------------------------

    // Free global function with explicit signature.
    template<typename F>
    void bind_global_func_sig(sol::state& lua, LuaMetaRegistry& reg, const char* name, F func, const char* sig) {
        lua.set_function(name, func);
        LuaMethodInfo info;
        info.name = name;
        info.is_static = true;
        info.ret = sig;
        reg.global_funcs().push_back(std::move(info));
    }

    // Global field in _G. The annotation generator emits a `---@type <T>` for it along with the literal value.
    template<typename V>
    void bind_global_field(sol::state& lua, LuaMetaRegistry& reg, const char* name, V value) {
        lua[name] = value;
        reg.global_fields().push_back({name, meta_detail::lua_name<V>(), meta_detail::to_lua_literal(value)});
    }

    template<typename E>
    void bind_enum(sol::state& lua, LuaMetaRegistry& reg, std::initializer_list<std::pair<const char*, E>> values) {
        static_assert(LuaTypeName<E>::value != nullptr,
                      "Missing HOB_LUA_TYPE specialization for this enum; declare it in lua_type_names.h");

        const char* name = LuaTypeName<E>::value;
        sol::table t = lua.create_named_table(name);
        for (const auto& v : values) {
            t[v.first] = v.second;
        }

        auto& info = reg.add_enum(name);
        for (const auto& v : values) {
            info.values.push_back({v.first, static_cast<int>(v.second)});
        }
    }
} // namespace hob
