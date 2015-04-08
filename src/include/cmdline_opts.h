#pragma once

#include <cstddef>
#include <cstdlib>
#include <experimental/optional>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

namespace cmdline_opts
{
  namespace detail
  {
    //------------------------------------------------------------------------------
    // Figure out whether a type is optional or not

    template <typename T>
    struct is_optional : public std::false_type {};

    template <typename T>
    struct is_optional<std::experimental::optional<T>> : public std::true_type {};

    //------------------------------------------------------------------------------
    // For function objects (and lambdas), their function_traits are the
    // function_traits of their operator()
    template <typename T>
    struct function_traits
      : public function_traits<decltype(&T::operator())>
    {};

    // For a regular function, we destructure its type appropriately
    // (we only deal with 0 or 1 arg)
    template <typename R>
    struct function_traits<R()>
    {
      static const std::size_t arity = 0;
    };

    template <typename R, typename A>
    struct function_traits<R(A)>
    {
      static const std::size_t arity = 1;
      using argType = std::decay_t<A>;
    };

    // For class member functions, extract the type
    template <typename C, typename R, typename... A>
    struct function_traits<R(C::*)(A...)>
      : public function_traits<R(A...)>
    {};

    template <typename C, typename R, typename... A>
    struct function_traits<R(C::*)(A...) const>
      : public function_traits<R(A...)>
    {};

    // Remove const, volatile, pointers and references
    template <typename T>
    struct function_traits<const T>
      : public function_traits<T>
    {};

    template <typename T>
    struct function_traits<volatile T>
      : public function_traits<T>
    {};

    template <typename T>
    struct function_traits<T*>
      : public function_traits<T>
    {};

    template <typename T>
    struct function_traits<T&>
      : public function_traits<T>
    {};

    //------------------------------------------------------------------------------
    class ArgHandlerBase
    {
    public:
      virtual ~ArgHandlerBase() {}
      virtual bool apply(const std::experimental::optional<std::string>&) = 0;
      virtual std::experimental::optional<std::string> getArg(const char*) const = 0;
    };

    template <typename U, typename E = void>
    class ArgHandler;

    // 0 arguments
    template <typename U>
    class ArgHandler<
      U, std::enable_if_t<function_traits<U>::arity == 0>>
      : public ArgHandlerBase
    {
    public:
      ArgHandler(const U& u)
        : m_u(u)
      {}
      ArgHandler(U&& u)
        : m_u(std::move(u))
      {}

      virtual bool apply(const std::experimental::optional<std::string>&)
      {
        m_u();
        return true;
      }

      virtual std::experimental::optional<std::string> getArg(const char*) const
      {
        return std::experimental::optional<std::string>();
      }

      U m_u;
    };

    // 1 argument, optional
    template <typename U>
    class ArgHandler<
      U, std::enable_if_t<function_traits<U>::arity == 1
                          && is_optional<typename function_traits<U>::argType>::value>>
      : public ArgHandlerBase
    {
    public:
      ArgHandler(const U& u)
        : m_u(u)
      {}
      ArgHandler(U&& u)
        : m_u(std::move(u))
      {}

      virtual bool apply(const std::experimental::optional<std::string>& s)
      {
        m_u(s);
        return true;
      }

      virtual std::experimental::optional<std::string> getArg(const char* s) const
      {
        if (!s || s[0] == '-')
          return std::experimental::optional<std::string>();
        return std::experimental::optional<std::string>(std::string(s));
      }

      U m_u;
    };

    // 1 argument, mandatory
    template <typename U>
    class ArgHandler<
      U, std::enable_if_t<function_traits<U>::arity == 1
                          && !is_optional<typename function_traits<U>::argType>::value>>
      : public ArgHandlerBase
    {
    public:
      ArgHandler(const U& u)
        : m_u(u)
      {}
      ArgHandler(U&& u)
        : m_u(std::move(u))
      {}

      virtual bool apply(const std::experimental::optional<std::string>& s)
      {
        if (!s) return false;
        m_u(*s);
        return true;
      }

      virtual std::experimental::optional<std::string> getArg(const char* s) const
      {
        if (!s)
          return std::experimental::optional<std::string>();
        return std::experimental::optional<std::string>(std::string(s));
      }

      U m_u;
    };
  }

  //------------------------------------------------------------------------------
  class ArgDescr
  {
  public:
    template <typename F>
    ArgDescr(const std::string& s, F&& f)
      : m_desc(s)
      , m_f(std::make_unique<detail::ArgHandler<F>>(std::forward<F>(f)))
    {}

    ArgDescr(ArgDescr&& other) noexcept = default;
    ArgDescr& operator=(ArgDescr&& other) noexcept = default;

    bool apply(const std::experimental::optional<std::string>& s) const
    {
      return m_f->apply(s);
    }

    std::experimental::optional<std::string> getArg(const char* s) const
    {
      return m_f->getArg(s);
    }

    std::string m_desc;
    std::unique_ptr<detail::ArgHandlerBase> m_f;
  };

  //------------------------------------------------------------------------------
  struct OptDescr
  {
    OptDescr(const std::string& shortArgs,
             const std::string& longArg,
             ArgDescr&& argDescr,
             const std::string& explanation)
      : m_shortArgs(shortArgs)
      , m_longArg(longArg)
      , m_argDescr(std::move(argDescr))
      , m_explanation(explanation)
    {}

    OptDescr(std::string&& shortArgs,
             std::string&& longArg,
             ArgDescr&& argDescr,
             std::string&& explanation)
      : m_shortArgs(std::move(shortArgs))
      , m_longArg(std::move(longArg))
      , m_argDescr(std::move(argDescr))
      , m_explanation(std::move(explanation))
    {}

    OptDescr(OptDescr&& other) noexcept = default;
    OptDescr& operator=(OptDescr&& other) noexcept = default;

    std::string m_shortArgs;
    std::string m_longArg;
    ArgDescr m_argDescr;
    std::string m_explanation;
  };

  namespace detail
  {
    //------------------------------------------------------------------------------
    auto findOption(const char* arg, const std::vector<OptDescr>& opts)
    {
      if (arg[0] == '-')
      {
        if (arg[1] == '-')
        {
          return find_if(opts.cbegin(), opts.cend(),
                         [s = &arg[2]] (const OptDescr &o)
                         { return o.m_longArg == s; });
        }
        else
        {
          return find_if(opts.cbegin(), opts.cend(),
                         [s = &arg[1]] (const OptDescr &o)
                         { return o.m_shortArgs == s; });
        }
      }
      return opts.cend();
    }

    //------------------------------------------------------------------------------
    void optsUsage(const std::vector<OptDescr>& opts)
    {
      auto maxlen = std::accumulate(
          opts.cbegin(), opts.cend(), std::string::size_type{0},
          [] (std::string::size_type l, const OptDescr& o)
          {
            return std::max(l, o.m_shortArgs.size() + o.m_longArg.size());
          });

      for (auto& o : opts)
      {
        std::cout << "-" << o.m_shortArgs << ", --" << o.m_longArg;
        auto l = o.m_shortArgs.size() + o.m_longArg.size();
        std::string space(maxlen - l + 4, ' ');
        std::cout << space << o.m_explanation << std::endl;
      }
    }
  }

  //------------------------------------------------------------------------------
  bool processOptions(int argc, char* argv[], const std::vector<OptDescr>& opts)
  {
    for (int i = 1; i < argc; ++i)
    {
      const char* o = argv[i];
      auto it = detail::findOption(o, opts);
      if (it == opts.cend())
        return false;
      const char* arg = i+1 < argc ? argv[i+1] : nullptr;
      std::experimental::optional<std::string> s = it->m_argDescr.getArg(arg);
      if (s) ++i;
      if (!it->m_argDescr.apply(s))
      {
        return false;
      }
    }
    return true;
  }

  //------------------------------------------------------------------------------
  void usage(const char* name, const std::vector<OptDescr>& opts)
  {
    std::cout << "Usage: " << name << " [OPTIONS]" << std::endl << std::endl;
    detail::optsUsage(opts);
  }
}
