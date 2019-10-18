#pragma once

namespace cswinrt
{
    using namespace std::literals;
    using namespace winmd::reader;

    template <typename...T> struct visit_overload : T... { using T::operator()...; };

    template <typename V, typename...C>
    auto call(V&& variant, C&& ...call)
    {
        return std::visit(visit_overload<C...>{ std::forward<C>(call)... }, std::forward<V>(variant));
    }

    struct writer : indented_writer_base<writer>
    {
        using indented_writer_base::indented_writer_base;

        std::string_view _current_namespace{};

        writer(std::string_view current_namespace) :
            _current_namespace(current_namespace)
        {
        }

        using indented_writer_base<writer>::write;

        struct generic_params
        {
            std::vector<std::vector<GenericParam>> stack;

            struct guard
            {
                explicit guard(writer* arg = nullptr)
                    : owner(arg)
                {}

                ~guard()
                {
                    if (owner)
                    {
                        owner->generic_params.pop();
                    }
                }

                guard(guard&& other)
                    : owner(other.owner)
                {
                    owner = nullptr;
                }

                guard& operator=(guard&& other)
                {
                    owner = std::exchange(other.owner, nullptr);
                    return *this;
                }

                guard& operator=(guard&) = delete;
                writer* owner;
            };

            [[nodiscard]] auto push(std::pair<GenericParam, GenericParam> const& range, writer& owner)
            {
                if (empty(range))
                {
                    return guard{ nullptr };
                }
                std::vector<GenericParam> params;
                for (auto&& param : range)
                {
                    params.push_back(param);
                }
                stack.push_back(std::move(params));
                return guard{ &owner };
            }

            auto get(uint32_t index)
            {
                return stack.back()[index];
            }

            void pop()
            {
                stack.pop_back();
            }
        };

        struct generic_args
        {
            std::vector<std::vector<type_semantics>> stack;

            struct guard
            {
                explicit guard(writer* arg = nullptr)
                    : owner(arg)
                {
                    if (owner)
                    {
                        in_generic_instance = std::exchange(owner->in_generic_instance, true);
                    }
                }

                ~guard()
                {
                    if (owner)
                    {
                        owner->generic_args.pop();
                        std::exchange(owner->in_generic_instance, in_generic_instance);
                    }
                }

                guard(guard&& other)
                    : owner(other.owner)
                {
                    owner = nullptr;
                }

                guard& operator=(guard&& other)
                {
                    owner = std::exchange(other.owner, nullptr);
                    return *this;
                }

                guard& operator=(guard const&) = delete;
                writer* owner;
                bool in_generic_instance;
            };

            [[nodiscard]] auto push(generic_type_instance const& type, writer& owner)
            {
                XLANG_ASSERT(!type.generic_args.empty());
                stack.push_back(type.generic_args);
                return guard{ &owner };
            }

            auto get(uint32_t index)
            {
                type_semantics semantics{};
                for (auto&& args = rbegin(stack); args != rend(stack); ++args)
                {
                    semantics = (*args)[index];
                    auto gti = std::get_if<generic_type_index>(&semantics);
                    if(!gti)
                    {
                        return semantics;
                    }
                    index = gti->index;
                }
                return semantics;
            }

            void pop()
            {
                stack.pop_back();
            }
        };

        generic_params generic_params;

        [[nodiscard]] auto push_generic_params(std::pair<GenericParam, GenericParam> const& range)
        {
            return generic_params.push(range, *this);
        }
        
        auto get_generic_param(uint32_t index)
        {
            return generic_params.get(index);
        }

        generic_args generic_args;

        [[nodiscard]] auto push_generic_args(generic_type_instance const& type)
        {
            return generic_args.push(type, *this);
        }

        auto get_generic_arg(uint32_t index)
        {
            return generic_args.get(index);
        }

        bool in_generic_instance{ false };

        void write_code(std::string_view const& value)
        {
            for (auto&& c : value)
            {
                if (c == '`')
                {
                    return;
                }
                else
                {
                    write(c);
                }
            }
        }
    };

    struct separator
    {
        writer& w;
        std::string_view _separator{ ", " };
        bool first{ true };

        void operator()()
        {
            if (first)
            {
                first = false;
            }
            else
            {
                w.write(_separator);
            }
        }
    };
}