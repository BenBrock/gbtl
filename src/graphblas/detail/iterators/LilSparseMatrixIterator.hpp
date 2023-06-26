#pragma once

#include <ranges>

#include <interfaces/spec/grb/util/util.hpp>
#include <interfaces/spec/grb/detail/iterator_adaptor.hpp>

namespace GRB_SPEC_NAMESPACE {

namespace __detail {

template <std::ranges::viewable_range M>
class adjacency_list_view_accessor {
public:

  using index_type = std::size_t;
  using key_type = GRB_SPEC_NAMESPACE::index<index_type>;
  using size_type = std::ranges::range_size_t<M>;
  using difference_type = std::ranges::range_difference_t<M>;

  using scalar_reference = decltype(std::get<1>(
                           std::declval<
                           std::ranges::range_reference_t<
                            std::ranges::range_reference_t<M>
                                                          >
                                        >()
                                     ));

  using scalar_type = std::remove_cvref_t<scalar_reference>;

  using value_type = GRB_SPEC_NAMESPACE::matrix_entry<scalar_type, index_type>;
  using reference = GRB_SPEC_NAMESPACE::matrix_ref<scalar_type, index_type, scalar_reference>;

  using iterator_category = std::forward_iterator_tag;

  using iterator_accessor = adjacency_list_view_accessor;
  using const_iterator_accessor = adjacency_list_view_accessor;
  using nonconst_iterator_accessor = adjacency_list_view_accessor;

  constexpr adjacency_list_view_accessor() noexcept = default;
  constexpr ~adjacency_list_view_accessor() noexcept = default;
  constexpr adjacency_list_view_accessor(
      const adjacency_list_view_accessor &) noexcept = default;
  constexpr adjacency_list_view_accessor &
  operator=(const adjacency_list_view_accessor &) noexcept = default;

  using inner_iterator = std::ranges::iterator_t<std::ranges::range_value_t<M>>;

  constexpr adjacency_list_view_accessor(M& matrix, std::size_t row, inner_iterator col) noexcept
      : matrix_ptr_(&matrix), row_(row), col_(col) {
        fast_forward();
      }

  constexpr bool operator==(const iterator_accessor &other) const noexcept {
    return row_ == other.row_ && col_ == other.col_;
  }

  constexpr iterator_accessor& operator++() {
    ++col_;
    fast_forward();
    return *this;
  }

  void fast_forward() {
    while (col_ == matrix_()[row_].end() && row_ < matrix_().size()-1) {
      ++row_;
      if (row_ != matrix_().size()-1) {
        col_ = matrix_()[row_].begin();
      }
    }
  }

  constexpr reference operator*() const noexcept {
    auto&& [j, v] = *col_;
    return reference(key_type(row_, j), v);
  }

private:

  M& matrix_() {
    return *matrix_ptr_;
  }

  M* matrix_ptr_;
  size_type row_;
  inner_iterator col_;
};


template <typename M>
using adjacency_list_view_iterator = GRB_SPEC_NAMESPACE::detail::iterator_adaptor<adjacency_list_view_accessor<M>>;

} // end __detail

} // end GRB_SPEC_NAMESPACE