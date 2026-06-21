#include "lua_meta.h"

#include <fstream>
#include <sstream>

#include "engine/core/logging.h"

namespace hob {
    namespace {
        void emit_ctor_overloads(std::ostringstream& out, const LuaUsertypeInfo& ut) {
            for (const auto& c : ut.ctors) {
                const bool use_explicit_names = c.arg_names.size() == c.args.size();
                const bool use_field_names = !use_explicit_names && c.args.size() <= ut.fields.size();
                out << "---@overload fun(";
                for (std::size_t i = 0; i < c.args.size(); ++i) {
                    if (i > 0) {
                        out << ", ";
                    }

                    if (use_explicit_names) {
                        out << c.arg_names[i];
                    }
                    else if (use_field_names) {
                        out << ut.fields[i].name;
                    }
                    else {
                        out << "arg" << (i + 1);
                    }
                    out << ": " << c.args[i];
                }

                out << "): " << ut.name << "\n";
            }
        }

        void emit_method(std::ostringstream& out, const std::string& owner, const LuaMethodInfo& m) {
            // method_sig override: ret holds the entire "(args): ret" tail.
            if (!m.ret.empty() && m.ret.front() == '(') {
                // Parse "(arg1: T, arg2: U): R" into params and return type.
                // Paren-depth aware: handles nested fun(...) in arg types.
                const std::string& sig = m.ret;
                std::size_t close = std::string::npos;
                int depth = 0;
                for (std::size_t i = 0; i < sig.size(); ++i) {
                    if (sig[i] == '(') {
                        ++depth;
                    }
                    else if (sig[i] == ')') {
                        --depth;
                        if (depth == 0) {
                            close = i;
                            break;
                        }
                    }
                }
                const std::string params = (close != std::string::npos) ? sig.substr(1, close - 1) : "";
                std::string ret;
                if (close != std::string::npos) {
                    auto colon = sig.find(':', close);
                    if (colon != std::string::npos) {
                        ret = sig.substr(colon + 1);
                        auto first = ret.find_first_not_of(" \t");
                        if (first != std::string::npos) {
                            ret.erase(0, first);
                        }
                    }
                }

                // Split params on top-level commas (depth 0); preserve commas nested inside
                // fun(a, b) and generics like table<k, v>.
                std::vector<std::string> parts;
                {
                    std::string cur;
                    int d = 0;
                    for (const char c : params) {
                        if (c == '(' || c == '<') {
                            ++d;
                            cur.push_back(c);
                        }
                        else if (c == ')' || c == '>') {
                            --d;
                            cur.push_back(c);
                        }
                        else if (c == ',' && d == 0) {
                            parts.push_back(std::move(cur));
                            cur.clear();
                        }
                        else {
                            cur.push_back(c);
                        }
                    }

                    if (!cur.empty()) {
                        parts.push_back(std::move(cur));
                    }
                }

                std::vector<std::string> arg_names;
                for (auto& part : parts) {
                    auto first = part.find_first_not_of(" \t");
                    if (first == std::string::npos) {
                        continue;
                    }
                    part.erase(0, first);

                    // Trim trailing whitespace.
                    auto last = part.find_last_not_of(" \t");
                    if (last != std::string::npos) {
                        part.erase(last + 1);
                    }

                    // Vararg: `...` (optionally with a `: type` suffix).
                    if (part.rfind("...", 0) == 0) {
                        std::string type = "any";
                        auto sep = part.find(':');
                        if (sep != std::string::npos) {
                            type = part.substr(sep + 1);
                            auto t0 = type.find_first_not_of(" \t");
                            if (t0 != std::string::npos) {
                                type.erase(0, t0);
                            }
                        }

                        out << "---@vararg " << type << "\n";
                        arg_names.push_back("...");
                        continue;
                    }

                    // Find the name/type separator ':' at top-level (depth 0),
                    // skipping any ':' nested inside fun(...) or a generic like table<k, v>.
                    std::size_t sep = std::string::npos;
                    {
                        int d = 0;
                        for (std::size_t i = 0; i < part.size(); ++i) {
                            if (part[i] == '(' || part[i] == '<') {
                                ++d;
                            }
                            else if (part[i] == ')' || part[i] == '>') {
                                --d;
                            }
                            else if (part[i] == ':' && d == 0) {
                                sep = i;
                                break;
                            }
                        }
                    }
                    if (sep == std::string::npos) {
                        continue;
                    }

                    std::string name = part.substr(0, sep);
                    auto nlast = name.find_last_not_of(" \t");
                    if (nlast != std::string::npos) {
                        name.erase(nlast + 1);
                    }

                    std::string type = part.substr(sep + 1);
                    auto t0 = type.find_first_not_of(" \t");
                    if (t0 != std::string::npos) {
                        type.erase(0, t0);
                    }

                    out << "---@param " << name << " " << type << "\n";
                    arg_names.push_back(name);
                }
                if (!ret.empty()) {
                    out << "---@return " << ret << "\n";
                }

                if (owner.empty()) {
                    out << "function " << m.name << "(";
                }
                else {
                    out << "function " << owner << (m.is_static ? "." : ":") << m.name << "(";
                }

                for (std::size_t i = 0; i < arg_names.size(); ++i) {
                    if (i > 0) {
                        out << ", ";
                    }
                    out << arg_names[i];
                }

                out << ") end\n\n";
                return;
            }

            // Deduced from C++ signature.
            const bool have_names = m.arg_names.size() == m.args.size();
            auto name_at = [&](std::size_t i) -> std::string {
                return have_names ? m.arg_names[i] : ("arg" + std::to_string(i + 1));
            };

            for (std::size_t i = 0; i < m.args.size(); ++i) {
                out << "---@param " << name_at(i) << " " << m.args[i] << "\n";
            }

            if (!m.ret.empty()) {
                out << "---@return " << m.ret << "\n";
            }

            if (owner.empty()) {
                out << "function " << m.name << "(";
            }
            else {
                out << "function " << owner << (m.is_static ? "." : ":") << m.name << "(";
            }

            for (std::size_t i = 0; i < m.args.size(); ++i) {
                if (i > 0) {
                    out << ", ";
                }
                out << name_at(i);
            }

            out << ") end\n\n";
        }

        void emit_usertype(std::ostringstream& out, const LuaUsertypeInfo& ut) {
            out << "-- " << ut.name << "\n";
            if (ut.base.empty()) {
                out << "---@class " << ut.name << "\n";
            }
            else {
                out << "---@class " << ut.name << " : " << ut.base << "\n";
            }

            for (const auto& f : ut.fields) {
                out << "---@field " << f.name << " " << f.type << "\n";
            }

            for (const auto& op : ut.operators) {
                if (op.rhs.empty()) {
                    out << "---@operator " << op.op << ": " << op.ret << "\n";
                }
                else {
                    out << "---@operator " << op.op << "(" << op.rhs << "): " << op.ret << "\n";
                }
            }
            emit_ctor_overloads(out, ut);
            out << "local " << ut.name << " = {}\n\n";

            for (const auto& m : ut.methods) {
                emit_method(out, ut.name, m);
            }

            out << "_G." << ut.name << " = " << ut.name << "\n\n";
        }

        void emit_enum(std::ostringstream& out, const LuaEnumInfo& e) {
            out << "-- " << e.name << "\n";
            out << "---@enum " << e.name << "\n";
            out << e.name << " = {\n";
            for (const auto& v : e.values) {
                out << "    " << v.name << " = " << v.value << ",\n";
            }

            out << "}\n\n";
        }

        void emit_table(std::ostringstream& out, const LuaTableInfo& t) {
            out << "-- " << t.name << "\n";
            out << "---@class " << t.name << "\n";
            for (const auto& f : t.fields) {
                out << "---@field " << f.name << " " << f.type << "\n";
            }
            out << t.name << " = {}\n\n";

            for (const auto& m : t.methods) {
                emit_method(out, t.name, m);
            }
        }
    } // namespace

    bool LuaMetaRegistry::write_to_file(const std::filesystem::path& path) const {
        std::ostringstream out;
        out << "---@meta\n";
        out << "-- AUTO-GENERATED by LuaMetaRegistry. Do not edit by hand.\n";
        out << "-- Source of truth: hob::LuaScriptSystem::register_bindings().\n\n";

        for (const auto& t : m_tables) {
            emit_table(out, t);
        }

        for (const auto& e : m_enums) {
            emit_enum(out, e);
        }

        for (const auto& ut : m_usertypes) {
            emit_usertype(out, ut);
        }

        if (!m_global_fields.empty() || !m_global_funcs.empty()) {
            out << "-- Globals\n";
            for (const auto& g : m_global_fields) {
                out << "---@type " << g.type << "\n";
                out << g.name << " = " << (g.value.empty() ? "nil" : g.value) << "\n\n";
            }

            for (const auto& m : m_global_funcs) {
                emit_method(out, "", m);
            }
        }

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) {
            log::lua.error("LuaMetaRegistry: failed to open '{}' for writing", path.string());
            return false;
        }

        const std::string str = out.str();
        f.write(str.data(), static_cast<std::streamsize>(str.size()));

        return f.good();
    }
} // namespace hob
