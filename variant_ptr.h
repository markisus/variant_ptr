#ifndef _LIUS_TOOLS_VARIANT_PTR_H_
#define _LIUS_TOOLS_VARIANT_PTR_H_

#include <type_traits>
#include <typeinfo>
#include <utility>

namespace lius_tools {

namespace {
template <bool B>
using bool_constant = std::integral_constant<bool, B>;

template <typename X, typename... Ts>
struct contains;

template <typename X>
struct contains<X> : public bool_constant<false> {};

template <typename X, typename... Ts>
struct contains<X, X, Ts...> : public bool_constant<true> {};

template <typename X, typename T, typename... Ts>
struct contains<X, T, Ts...> : public contains<X, Ts...> {};


template <typename X, typename... Ts>
struct index_of_type {
  // fallback
  static constexpr size_t value = -1;
};

template <typename X, typename Y, typename... Ts>
struct index_of_type<X, Y, Ts...> {
  static constexpr size_t value =
      std::conditional<std::is_convertible<X,Y>::value,
                       std::integral_constant<size_t, 0>,
                       std::integral_constant<size_t,
                                              1 + index_of_type<X, Ts...>::value>
                       >::type::value;
};

template <typename... Us>
struct TypeList {};

template <typename... Ts>
struct length_of;

template <>
struct length_of<> {
  static constexpr size_t value = 0;
};

template <typename T, typename... Ts>
struct length_of<T, Ts...> {
  static constexpr size_t value = length_of<Ts...>::value + 1;
};

template <typename... Ts>
struct head;

template <typename T, typename... Ts>
struct head<T, Ts...> {
  using type = T;
};

template <typename T, typename U>
struct list_concat;

template <typename... Ts, typename... Us>
struct list_concat<TypeList<Ts...>, TypeList<Us...>> :
      public TypeList<Ts..., Us...> {};

template <typename T, typename U>
struct list_product;

template <typename... Ts, typename... Us>
struct list_product<TypeList<Ts...>, TypeList<Us...>> :
      public TypeList<std::pair<Ts,Us>...> {};
}

template <typename... Ts>
class variant_ptr {
 private:
  using types = TypeList<Ts...>;
  using example_visitee_type = typename head<Ts...>::type;

 public:
  template <typename X>
  variant_ptr(X* ptr) {
    reset(ptr);
  }

  template <typename X>
  void reset(X* ptr) {
    type_index_= index_of_type<X, Ts...>::value;
    ptr_ = (void *)(ptr);
  }

  template <typename X>
  bool has_type() {
    return has_type_impl<X>(TypeList<Ts...>{});
  }

  template <typename TVisitor, typename... TExtras>
  auto visit(
      TVisitor&& visitor, TExtras&&... extras) const {
    return visit_impl(
        visitor, TypeList<Ts...>{}, extras...);
  }

 private:
  template <typename X, typename U, typename... Us>
  bool has_type_impl(TypeList<U, Us...>) const {
    bool u_is_correct_type =
        type_index_ == (length_of<Ts...>::value - length_of<U, Us...>::value);
    if (u_is_correct_type) {
      return std::is_same<X, U>::value;
    }
    else {
      return has_type_impl<X>(TypeList<Us...>{});
    }
  }

  template <typename X, typename U>
  bool has_type_impl(TypeList<U>) const {
    return std::is_same<X, U>::value;
  }

  // Cast the pointer to U* and return TVisitor::visit<U>(*ptr, extras)
  template <typename U, typename TVisitor, typename... TExtras>
  auto cast_and_visit(
      TVisitor&& visitor, TExtras... extras) const {
    U* casted_ptr = (U*)ptr_;
    return visitor.visit(*casted_ptr, extras...);
  }

  // Recursive visit_impl.
  template <typename TVisitor,
            typename U, typename... Us, typename... TExtras>
  auto visit_impl(
      TVisitor&& visitor, TypeList<U, Us...>, TExtras&&... extras) const {
    if (type_index_ == (length_of<Ts...>::value - length_of<U, Us...>::value)) {
      return cast_and_visit<U>(visitor, extras...);
    }
    else {
      // recurse
      return visit_impl(visitor, TypeList<Us...>{}, extras...);
    }
  };

  // Base case visit_impl: should never be reached since the type list
  // is now empty. Any expression that evaluates to a compatible return
  // type can returned.
  template <typename TVisitor,
            typename... TExtras>
  auto visit_impl(
      TVisitor&& visitor, TypeList<>, TExtras&&... extras) const {
    return cast_and_visit<example_visitee_type>(visitor, extras...);
  };

  void* ptr_;
  size_t type_index_;
};

template <typename... Ts>
using const_variant_ptr = variant_ptr<const Ts...>;


// MultiVisitor support:
//
// A multi visitor is class that provides an overloaded
// visit function over more than one type, e.g.
//
// struct BiVisitor {
//    void visit(double a, int b);
//    void visit(double a, long b);
//    void visit(char a, int b);
//    void visit(char a, long b);
// };
//
// This visitor is applicable to a pair of variant_ptrs
// of type variant_ptr<double,char>
// and variant_ptr<int, long> respectively


// BindVisitor takes a MultiVisitor of arity n and binds the
// first parameter to create a MultiVisitor of arity n-1
template <typename TMultiVisitor, typename T>
class BindVisitor {
 public:
  BindVisitor(TMultiVisitor& m, T& e) :
      multi_visitor_(m),
      element_(e) {}

  template <typename... Ts>
  auto visit(Ts&&... ts) {
    return multi_visitor_.visit(element_, ts...);
  }

 private:
  TMultiVisitor& multi_visitor_;
  T& element_;
};

// MultiVisitorToSingleVisitor adapts a MultiVisitor so that
// it passed in as an argument to a variant_ptr's visit function
//
// my_variant_ptr_a.visit(
//     my_multi_visitor_to_single_visitor,
//     my_variant_ptr_b,
//     my_variant_ptr_c);
//
// which will call the wrapped MultiVisitor's visit function
// with the underlying types of the variants.
template <typename TMultiVisitor>
class MultiVisitorToSingleVisitor {
 public:
  MultiVisitorToSingleVisitor(TMultiVisitor& multi_visitor) :
      multi_visitor_(multi_visitor) {}
  template <typename A>
  auto visit(A&& a) {
    return multi_visitor_.visit(a);
  }

  template <typename A, typename TVariant, typename... TVariants>
  auto visit(A&& a, TVariant&& variant, TVariants&&... variants) {
    // What have I done
    BindVisitor<TMultiVisitor, A> bind_visitor { multi_visitor_, a };
    MultiVisitorToSingleVisitor<BindVisitor<TMultiVisitor, A>>
        bind_single_visitor { bind_visitor };
    return variant.visit(bind_single_visitor, variants...);
  }

 private:
  TMultiVisitor& multi_visitor_;
};

template <typename TVisitor, typename TVariant, typename... TVariants>
auto apply_visitor(
    TVisitor visitor, TVariant&& variant, TVariants&&... variants) {
  MultiVisitorToSingleVisitor<TVisitor> single_visitor { visitor };
  return variant.visit(single_visitor, variants...);
}

}

#endif /* _LIUS_TOOLS_VARIANT_PTR_H_ */
