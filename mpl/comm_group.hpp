#if !(defined MPL_COMM_GROUP_HPP)

#define MPL_COMM_GROUP_HPP

#include <mpi.h>
#include <type_traits>
#include <tuple>
#include <thread>
#include <mpl/layout.hpp>
#include <mpl/vector.hpp>

namespace mpl {

  class group;

  class communicator;

  class cart_communicator;

  class graph_communicator;

  namespace environment {

    namespace detail {

      class env;

    }

  }  // namespace environment

  //--------------------------------------------------------------------

  /// \brief Represents a group of processes.
  class group {
    MPI_Group gr_{MPI_GROUP_EMPTY};

  public:
    /// \brief Group equality types.
    enum class equality_type {
      /// groups are identical, i.e., groups have same the members in same rank order
      identical = MPI_IDENT,
      /// groups are similar, i.e., groups have same tha members in different rank order
      similar = MPI_SIMILAR,
      /// groups are unequal, i.e., groups have different sets of members
      unequal = MPI_UNEQUAL
    };

    /// indicates that groups are identical, i.e., groups have same the members in same rank
    /// order
    static constexpr equality_type identical = equality_type::identical;
    /// indicates that groups are similar, i.e., groups have same tha members in different rank
    /// order
    static constexpr equality_type similar = equality_type::similar;
    /// indicates that groups are unequal, i.e., groups have different sets of members
    static constexpr equality_type unequal = equality_type::unequal;

    /// \brief Indicates the creation of a union of two groups.
    class Union_tag {};
    /// \brief Indicates the creation of a union of two groups.
    static constexpr Union_tag Union{};

    /// \brief Indicates the creation of an intersection of two groups.
    class intersection_tag {};
    /// \brief Indicates the creation of an intersection of two groups.
    static constexpr intersection_tag intersection{};

    /// \brief Indicates the creation of a difference of two groups.
    class difference_tag {};
    /// \brief Indicates the creation of a difference of two groups.
    static constexpr difference_tag difference{};

    /// \brief Indicates the creation of a subgroup by including members of an existing group.
    class include_tag {};
    /// \brief Indicates the creation of a subgroup by including members of an existing group.
    static constexpr include_tag include{};

    /// \brief Indicates the creation of a subgroup by excluding members of an existing group.
    class exclude_tag {};
    /// \brief Indicates the creation of a subgroup by excluding members of an existing group.
    static constexpr exclude_tag exclude{};

    /// \brief Creates an empty process group.
    group() = default;

    /// \brief Creates a new process group by copying an existing one.
    /// \param other the other group to copy from
    /// \note Process groups should not be copied unless a new independent group is wanted.
    /// Process groups should be passed via references to functions to avoid unnecessary
    /// copying.
    group(const group &other);

    /// \brief Move-constructs a process group.
    /// \param other the other group to move from
    group(group &&other) noexcept : gr_{other.gr_} { other.gr_ = MPI_GROUP_EMPTY; }

    /// \brief Creates a new group that consists of all processes of the given communicator.
    /// \param comm the communicator
    explicit group(const communicator &comm);

    /// \brief Creates a new group that consists of the union of two existing process groups.
    /// \param tag indicates the unification of two existing process groups
    /// \param other_1 first existing process group
    /// \param other_2 second existing process group
    explicit group(Union_tag tag, const group &other_1, const group &other_2);

    /// \brief Creates a new group that consists of the intersection of two existing process
    /// groups.
    /// \param tag indicates the intersection of two existing process groups
    /// \param other_1 first existing process group
    /// \param other_2 second existing process group
    explicit group(intersection_tag tag, const group &other_1, const group &other_2);

    /// \brief Creates a new group that consists of the difference of two existing process
    /// groups.
    /// \param tag indicates the difference of two existing process groups
    /// \param other_1 first existing process group
    /// \param other_2 second existing process group
    explicit group(difference_tag tag, const group &other_1, const group &other_2);

    /// \brief Creates a new group by including members of an existing process group.
    /// \param tag indicates inclusion from an existing process group
    /// \param other existing process group
    /// \param rank set of ranks to include
    explicit group(include_tag tag, const group &other, const ranks &rank);

    /// \brief Creates a new group by excluding members of an existing process group.
    /// \param tag indicates exclusion from an existing process group
    /// \param other existing process group
    /// \param rank set of ranks to exclude
    explicit group(exclude_tag tag, const group &other, const ranks &rank);

    /// \brief Destructs a process group.
    ~group() {
      int result;
      MPI_Group_compare(gr_, MPI_GROUP_EMPTY, &result);
      if (result != MPI_IDENT)
        MPI_Group_free(&gr_);
    }

    /// \brief Copy-assigns a process group.
    /// \param other the other group to move from
    /// \return this group
    /// \note Process groups should not be copied unless a new independent group is wanted.
    /// Process groups should be passed via references to functions to avoid unnecessary
    /// copying.
    group &operator=(const group &other) {
      if (this != &other) {
        int result;
        MPI_Group_compare(gr_, MPI_GROUP_EMPTY, &result);
        if (result != MPI_IDENT)
          MPI_Group_free(&gr_);
        MPI_Group_excl(other.gr_, 0, nullptr, &gr_);
      }
      return *this;
    }

    /// \brief Move-assigns a process group.
    /// \param other the other group to move from
    /// \return this group
    group &operator=(group &&other) noexcept {
      if (this != &other) {
        int result;
        MPI_Group_compare(gr_, MPI_GROUP_EMPTY, &result);
        if (result != MPI_IDENT)
          MPI_Group_free(&gr_);
        gr_ = other.gr_;
        other.gr_ = MPI_GROUP_EMPTY;
      }
      return *this;
    }

    /// \brief Determines the total number of processes in a process group.
    /// \return number of processes
    [[nodiscard]] int size() const {
      int result;
      MPI_Group_size(gr_, &result);
      return result;
    }

    /// \brief Determines the rank within a process group.
    /// \return the rank of the calling process in the group
    [[nodiscard]] int rank() const {
      int result;
      MPI_Group_rank(gr_, &result);
      return result;
    }

    /// \brief Determines the relative numbering of the same process in two different groups.
    /// \param rank a valid rank in the given process group
    /// \param other process group
    /// \return corresponding rank in this process group
    [[nodiscard]] int translate(int rank, const group &other) const {
      int other_rank;
      MPI_Group_translate_ranks(gr_, 1, &rank, other.gr_, &other_rank);
      return other_rank;
    }

    /// \brief Determines the relative numbering of the same process in two different groups.
    /// \param rank a set valid ranks in the given process group
    /// \param other process group
    /// \return corresponding ranks in this process group
    [[nodiscard]] ranks translate(const ranks &rank, const group &other) const {
      ranks other_rank(rank.size());
      MPI_Group_translate_ranks(gr_, static_cast<int>(rank.size()), rank(), other.gr_,
                                other_rank());
      return other_rank;
    }

    /// \brief Tests for identity of process groups.
    /// \return true if identical
    bool operator==(const group &other) const {
      int result;
      MPI_Group_compare(gr_, other.gr_, &result);
      return result == MPI_IDENT;
    }

    /// \brief Tests for identity of process groups.
    /// \return true if not identical
    bool operator!=(const group &other) const {
      int result;
      MPI_Group_compare(gr_, other.gr_, &result);
      return result != MPI_IDENT;
    }

    /// \brief Compares to another process group.
    /// \return equality type
    [[nodiscard]] equality_type compare(const group &other) const {
      int result;
      MPI_Group_compare(gr_, other.gr_, &result);
      return static_cast<equality_type>(result);
    }

    friend class communicator;
  };

  //--------------------------------------------------------------------

  /// \brief Specifies the communication context for a communication operation.
  class communicator {
    struct isend_irecv_state {
      MPI_Request req{};
      int source{MPI_ANY_SOURCE};
      int tag{MPI_ANY_TAG};
      MPI_Datatype datatype{MPI_DATATYPE_NULL};
      int count{MPI_UNDEFINED};
    };

    static int isend_irecv_query(void *state, MPI_Status *s) {
      isend_irecv_state *sendrecv_state{reinterpret_cast<isend_irecv_state *>(state)};
      MPI_Status_set_elements(s, sendrecv_state->datatype, sendrecv_state->count);
      MPI_Status_set_cancelled(s, 0);
      s->MPI_SOURCE = sendrecv_state->source;
      s->MPI_TAG = sendrecv_state->tag;
      return MPI_SUCCESS;
    }

    static int isend_irecv_free(void *state) {
      isend_irecv_state *sendrecv_state{reinterpret_cast<isend_irecv_state *>(state)};
      delete sendrecv_state;
      return MPI_SUCCESS;
    }

    static int isend_irecv_cancel(void *state, int complete) { return MPI_SUCCESS; }

  protected:
    MPI_Comm comm_{MPI_COMM_NULL};

  public:
    /// \brief Equality types for communicator comparison.
    enum class equality_type {
      /// communicators are identical, i.e., communicators represent the same communication
      /// context
      identical = MPI_IDENT,
      /// communicators are identical, i.e., communicators have same the members in same rank
      /// order but different context
      congruent = MPI_CONGRUENT,
      /// communicators are similar, i.e., communicators have same tha members in different rank
      /// order
      similar = MPI_SIMILAR,
      /// communicators are unequal, i.e., communicators have different sets of members
      unequal = MPI_UNEQUAL
    };

    /// indicates that communicators are identical, i.e., communicators represent the same
    /// communication context
    static constexpr equality_type identical = equality_type::identical;
    /// indicates that communicators are identical, i.e., communicators have same the members in
    /// same rank order but different context
    static constexpr equality_type congruent = equality_type::congruent;
    /// indicates that communicators are similar, i.e., communicators have same tha members in
    /// different rank order
    static constexpr equality_type similar = equality_type::similar;
    /// indicates that communicators are unequal, i.e., communicators have different sets of
    /// members
    static constexpr equality_type unequal = equality_type::unequal;

    /// \brief Indicates the creation of a new communicator by a call that in collective for all
    /// processes in the given communicator.
    class comm_collective_tag {};
    /// \brief Indicates the creation of a new communicator by a call that in collective for all
    /// processes in the given communicator.
    static constexpr comm_collective_tag comm_collective{};

    /// \brief Indicates the creation of a new communicator by a call that in collective for all
    /// processes in the given group.
    class group_collective_tag {};
    /// \brief Indicates the creation of a new communicator by a call that in collective for all
    /// processes in the given group.
    static constexpr group_collective_tag group_collective{};

    /// \brief Indicates the creation of a new communicator by spitting an existing communicator
    /// into disjoint subgroups.
    class split_tag {};
    /// \brief Indicates the creation of a new communicator by spitting an existing communicator
    /// into disjoint subgroups.
    static constexpr split_tag split{};

    /// \brief Indicates the creation of a new communicator by spitting an existing communicator
    /// into disjoint subgroups each of which can create a shared memory region.
    class split_shared_memory_tag {};
    /// \brief Indicates the creation of a new communicator by spitting an existing communicator
    /// into disjoint subgroups each of which can create a shared memory region.
    static constexpr split_shared_memory_tag split_shared_memory{};

  private:
    void check_dest(int dest) const {
#if defined MPL_DEBUG
      if (dest != proc_null and (dest < 0 or dest >= size()))
        throw invalid_rank();
#endif
    }

    void check_source(int source) const {
#if defined MPL_DEBUG
      if (source != proc_null and source != any_source and (source < 0 or source >= size()))
        throw invalid_rank();
#endif
    }

    void check_send_tag(tag t) const {
#if defined MPL_DEBUG
      if (static_cast<int>(t) < 0 or static_cast<int>(t) > static_cast<int>(tag::up()))
        throw invalid_tag();
#endif
    }

    void check_recv_tag(tag t) const {
#if defined MPL_DEBUG
      if (static_cast<int>(t) != static_cast<int>(tag::any()) and
          (static_cast<int>(t) < 0 or static_cast<int>(t) > static_cast<int>(tag::up())))
        throw invalid_tag();
#endif
    }

    void check_root(int root_rank) const {
#if defined MPL_DEBUG
      if (root_rank < 0 or root_rank >= size())
        throw invalid_rank();
#endif
    }

    void check_nonroot(int root_rank) const {
#if defined MPL_DEBUG
      if (root_rank < 0 or root_rank >= size() or root_rank == rank())
        throw invalid_rank();
#endif
    }

    template<typename T>
    void check_size(const layouts<T> &l) const {
#if defined MPL_DEBUG
      if (static_cast<int>(l.size()) > size())
        throw invalid_size();
#endif
    }

    void check_size(const displacements &d) const {
#if defined MPL_DEBUG
      if (static_cast<int>(d.size()) > size())
        throw invalid_size();
#endif
    }

    void check_count(int count) const {
#if defined MPL_DEBUG
      if (count == MPI_UNDEFINED)
        throw invalid_count();
#endif
    }

    template<typename T>
    void check_container_size(const T &container, detail::basic_or_fixed_size_type) const {}

    template<typename T>
    void check_container_size(const T &container, detail::stl_container) const {
#if defined MPL_DEBUG
      if (container.size() > std::numeric_limits<int>::max())
        throw invalid_count();
#endif
    }

    template<typename T>
    void check_container_size(const T &container) const {
      check_container_size(container,
                           typename detail::datatype_traits<T>::data_type_category{});
    }

  protected:
    explicit communicator(MPI_Comm comm) : comm_(comm) {}

  public:
    /// \brief Creates an empty communicator with no associated process.
    communicator() = default;

    /// \brief Creates a new communicator which is equivalent to an existing one.
    /// \param other the other communicator to copy from
    /// \note Communicators should not be copied unless a new independent communicator is
    /// wanted. Communicators should be passed via references to functions to avoid unnecessary
    /// copying.
    communicator(const communicator &other) { MPI_Comm_dup(other.comm_, &comm_); }

    /// \brief Move-constructs a communicator.
    /// \param other the other communicator to move from
    communicator(communicator &&other) noexcept : comm_{other.comm_} {
      other.comm_ = MPI_COMM_NULL;
    }

    /// \brief Constructs a new communicator from an existing one with a specified communication group.
    /// \param comm_collective tag to indicate the mode of construction
    /// \param other the communicator
    /// \param gr the group that determines the new communicator's structure
    /// \note This is a collective operation that needs to be carried out by all processes of the communicator other.
    explicit communicator(comm_collective_tag comm_collective, const communicator &other, const group &gr) {
      MPI_Comm_create(other.comm_, gr.gr_, &comm_);
    }

    /// \brief Constructs a new communicator from an existing one with a specified communication group.
    /// \param group_collective tag to indicate the mode of construction
    /// \param other the communicator
    /// \param gr the group that determines the new communicator's structure
    /// \param t tag to distinguish between different parallel operations in different threads
    /// \note This is a collective operation that needs to be carried out by all processes of the given group.
    explicit communicator(group_collective_tag group_collective, const communicator &other, const group &gr,
                          tag t = tag(0)) {
      MPI_Comm_create_group(other.comm_, gr.gr_, static_cast<int>(t), &comm_);
    }

    /// \brief Constructs a new communicator from an existing one with a specified communication group.
    /// \param split tag to indicate the mode of construction
    /// \param other the communicator
    /// \param color control of subset assignment
    /// \param key  control of rank assignment
    /// \tparam color_type color type, must be integral type
    /// \tparam key_type key type, must be integral type
    template<typename color_type, typename key_type = int>
    explicit communicator(split_tag split, const communicator &other, color_type color,
                          key_type key = 0) {
      static_assert(detail::is_valid_color_v<color_type>,
                    "not an enumeration type or underlying enumeration type too large");
      static_assert(detail::is_valid_key_v<key_type>,
                    "not an enumeration type or underlying enumeration type too large");
      MPI_Comm_split(other.comm_, detail::underlying_type<color_type>::value(color),
                     detail::underlying_type<key_type>::value(key), &comm_);
    }

    /// \brief Constructs a new communicator from an existing one by spitting the communicator into disjoint subgroups each of which can create a shared memory region.
    /// \param split_shared_memory tag to indicate the mode of construction
    /// \param other the communicator
    /// \param key  control of rank assignment
    /// \tparam color_type color type, must be integral type
    template<typename key_type = int>
    explicit communicator(split_shared_memory_tag split_shared_memory, const communicator &other,
                          key_type key = 0) {
      static_assert(detail::is_valid_tag_v<key_type>,
                    "not an enumeration type or underlying enumeration type too large");
      MPI_Comm_split_type(other.comm_, MPI_COMM_TYPE_SHARED,
                          detail::underlying_type<key_type>::value(key), MPI_INFO_NULL, &comm_);
    }

    /// \brief Destructs a communicator.
    ~communicator() {
      if (is_valid()) {
        int result1;
        MPI_Comm_compare(comm_, MPI_COMM_WORLD, &result1);
        int result2;
        MPI_Comm_compare(comm_, MPI_COMM_SELF, &result2);
        if (result1 != MPI_IDENT and result2 != MPI_IDENT)
          MPI_Comm_free(&comm_);
      }
    }

    /// \brief Copy-assigns and creates a new communicator which is equivalent to an existing
    /// one.
    /// \param other the other communicator to copy from
    /// \note Communicators should not be copied unless a new independent communicator is
    /// wanted. Communicators should be passed via references to functions to avoid unnecessary
    /// copying.
    communicator &operator=(const communicator &other)  noexcept {
      if (this != &other) {
        if (is_valid()) {
          int result1;
          MPI_Comm_compare(comm_, MPI_COMM_WORLD, &result1);
          int result2;
          MPI_Comm_compare(comm_, MPI_COMM_SELF, &result2);
          if (result1 != MPI_IDENT and result2 != MPI_IDENT)
            MPI_Comm_free(&comm_);
        }
        MPI_Comm_dup(other.comm_, &comm_);
      }
      return *this;
    }

    /// \brief Move-assigns a communicator.
    /// \param other the other communicator to move from
    communicator &operator=(communicator &&other) noexcept {
      if (this != &other) {
        if (is_valid()) {
          int result1;
          MPI_Comm_compare(comm_, MPI_COMM_WORLD, &result1);
          int result2;
          MPI_Comm_compare(comm_, MPI_COMM_SELF, &result2);
          if (result1 != MPI_IDENT and result2 != MPI_IDENT)
            MPI_Comm_free(&comm_);
        }
        comm_ = other.comm_;
        other.comm_ = MPI_COMM_NULL;
      }
      return *this;
    }

    /// \brief Determines the total number of processes in a communicator.
    /// \return number of processes
    [[nodiscard]] int size() const {
      int result;
      MPI_Comm_size(comm_, &result);
      return result;
    }

    /// \brief Determines the rank within a communicator.
    /// \return the rank of the calling process in the communicator
    [[nodiscard]] int rank() const {
      int result;
      MPI_Comm_rank(comm_, &result);
      return result;
    }

    /// \brief Tests for identity of communicators.
    /// \return true if identical
    bool operator==(const communicator &other) const {
      int result;
      MPI_Comm_compare(comm_, other.comm_, &result);
      return result == MPI_IDENT;
    }

    /// \brief Tests for identity of communicators.
    /// \return true if not identical
    bool operator!=(const communicator &other) const {
      int result;
      MPI_Comm_compare(comm_, other.comm_, &result);
      return result != MPI_IDENT;
    }

    /// \brief Compares to another communicator.
    /// \return equality type
    [[nodiscard]] equality_type compare(const communicator &other) const {
      int result;
      MPI_Comm_compare(comm_, other.comm_, &result);
      return static_cast<equality_type>(result);
    }

   /// \brief Checks if a communicator is valid, i.e., is not an empty communicator with no
   /// associated process.
   /// \return true if communicator is valid
   /// \note A default constructed communicator is a non valid communicator.
    [[nodiscard]] bool is_valid() const { return comm_ != MPI_COMM_NULL; }

    /// \brief Aborts all processes associated to the communicator.
    /// \param err error code, becomes the return code of the main program
    /// \note Method provides just a "best attempt" to abort processes.
    void abort(int err) const { MPI_Abort(comm_, err); }

    friend class group;

    friend class cart_communicator;

    friend class graph_communicator;

    friend class dist_graph_communicator;

    friend class environment::detail::env;

    // === point to point ==============================================

    // === standard send ===
    // --- blocking standard send ---
  private:
    template<typename T>
    void send(const T &data, int destination, tag t, detail::basic_or_fixed_size_type) const {
      MPI_Send(&data, 1, detail::datatype_traits<T>::get_datatype(), destination,
               static_cast<int>(t), comm_);
    }

    template<typename T>
    void send(const T &data, int destination, tag t,
              detail::contigous_const_stl_container) const {
      using value_type = typename T::value_type;
      vector_layout<value_type> l(data.size());
      send(data.size() > 0 ? &data[0] : nullptr, l, destination, t);
    }

    template<typename T>
    void send(const T &data, int destination, tag t, detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      detail::vector<value_type> serial_data(data.size(), std::begin(data));
      vector_layout<value_type> l(serial_data.size());
      send(serial_data.data(), l, destination, t);
    }

  public:
    /// \brief Sends a message with a single value via a blocking standard send operation.
    /// \tparam T type of the data to send, must meet the requirements as described in the \ref
    /// data_types "data types" section or an STL container that holds elements that comply with
    /// the mentioned requirements
    /// \param data value to send
    /// \param destination rank of the receiving process
    /// \param t tag associated to this message
    /// \note Sending STL containers is a convenience feature, which may have non-optimal
    /// performance characteristics. Use alternative overloads in performance critical code sections.
    template<typename T>
    void send(const T &data, int destination, tag t = tag(0)) const {
      check_dest(destination);
      check_send_tag(t);
      check_container_size(data);
      send(data, destination, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    /// \brief Sends a message with a several values having a specific memory layout via a
    /// blocking standard send operation.
    /// \tparam T type of the data to send, must meet the requirements as described in the \ref
    /// data_types "data types" section
    /// \param data pointer to the data to send
    /// \param l memory layout of the data to send
    /// \param destination rank of the receiving process
    /// \param t tag associated to this message
    template<typename T>
    void send(const T *data, const layout<T> &l, int destination, tag t = tag(0)) const {
      check_dest(destination);
      check_send_tag(t);
      MPI_Send(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), destination,
               static_cast<int>(t), comm_);
    }

    /// \brief Sends a message with a several values given by a pair of iterators via a
    /// blocking standard send operation.
    /// \tparam iterT iterator type, must fulfill the requirements of a
    /// <a href="https://en.cppreference.com/w/cpp/named_req/ForwardIterator">LegacyForwardIterator</a>,
    /// the iterator's value-type must meet the requirements as described in the
    /// \ref data_types "data types" section
    /// \param begin iterator pointing to the first data value to send
    /// \param end iterator pointing one element beyond the last data value to send
    /// \param destination rank of the receiving process
    /// \param t tag associated to this message
    /// \note This is a convenience method, which may have non-optimal performance
    /// characteristics. Use alternative overloads in performance critical code sections.
    template<typename iterT>
    void send(iterT begin, iterT end, int destination, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        send(&(*begin), l, destination, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        send(&(*begin), l, destination, t);
      }
    }

    // --- nonblocking standard send ---
  private:
    template<typename T>
    impl::irequest isend(const T &data, int dest, tag t,
                         detail::basic_or_fixed_size_type) const {
      MPI_Request req;
      MPI_Isend(&data, 1, detail::datatype_traits<T>::get_datatype(), dest, static_cast<int>(t),
                comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    void isend(const T &data, int dest, tag t, isend_irecv_state *isend_state,
               detail::contigous_const_stl_container) const {
      using value_type = typename T::value_type;
      const int count(data.size());
      MPI_Datatype datatype{detail::datatype_traits<value_type>::get_datatype()};
      MPI_Request req;
      MPI_Isend(data.size() > 0 ? &data[0] : nullptr, count, datatype, dest,
                static_cast<int>(t), comm_, &req);
      MPI_Status s;
      MPI_Wait(&req, &s);
      isend_state->source = s.MPI_SOURCE;
      isend_state->tag = s.MPI_TAG;
      isend_state->datatype = datatype;
      isend_state->count = 0;
      MPI_Grequest_complete(isend_state->req);
    }

    template<typename T>
    void isend(const T &data, int dest, tag t, isend_irecv_state *isend_state,
               detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      detail::vector<value_type> serial_data(data.size(), std::begin(data));
      isend(serial_data, dest, t, isend_state, detail::contigous_const_stl_container{});
    }

    template<typename T, typename C>
    impl::irequest isend(const T &data, int dest, tag t, C) const {
      isend_irecv_state *send_state{new isend_irecv_state()};
      MPI_Request req;
      MPI_Grequest_start(isend_irecv_query, isend_irecv_free, isend_irecv_cancel, send_state,
                         &req);
      send_state->req = req;
      std::thread thread(
          [this, &data, dest, t, send_state]() { isend(data, dest, t, send_state, C{}); });
      thread.detach();
      return impl::irequest(req);
    }

  public:
    template<typename T>
    irequest isend(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      check_container_size(data);
      return isend(data, dest, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    irequest isend(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Isend(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename iterT>
    irequest isend(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return isend(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return isend(&(*begin), l, dest, t);
      }
    }

    // --- persistent standard send ---
    template<typename T>
    prequest send_init(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Send_init(&data, 1, detail::datatype_traits<T>::get_datatype(), dest,
                    static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename T>
    prequest send_init(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Send_init(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                    static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename iterT>
    prequest send_init(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return send_init(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return send_init(&(*begin), l, dest, t);
      }
    }

    // === buffered send ===
    // --- determine buffer size ---
    template<typename T>
    [[nodiscard]] int bsend_size() const {
      int pack_size{0};
      MPI_Pack_size(1, detail::datatype_traits<T>::get_datatype(), comm_, &pack_size);
      return pack_size + MPI_BSEND_OVERHEAD;
    }

    template<typename T>
    int bsend_size(const layout<T> &l) const {
      int pack_size{0};
      MPI_Pack_size(1, detail::datatype_traits<layout<T>>::get_datatype(l), comm_, &pack_size);
      return pack_size + MPI_BSEND_OVERHEAD;
    }

    // --- blocking buffered send ---
  private:
    template<typename T>
    void bsend(const T &data, int dest, tag t, detail::basic_or_fixed_size_type) const {
      MPI_Bsend(&data, 1, detail::datatype_traits<T>::get_datatype(), dest, static_cast<int>(t),
                comm_);
    }

    template<typename T>
    void bsend(const T &data, int dest, tag t, detail::contigous_const_stl_container) const {
      using value_type = typename T::value_type;
      vector_layout<value_type> l(data.size());
      bsend(data.size() > 0 ? &data[0] : nullptr, l, dest, t);
    }

    template<typename T>
    void bsend(const T &data, int dest, tag t, detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      detail::vector<value_type> serial_data(data.size(), std::begin(data));
      vector_layout<value_type> l(serial_data.size());
      bsend(serial_data.data(), l, dest, t);
    }

  public:
    /// \anchor communicator_bsend
    template<typename T>
    void bsend(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      check_container_size(data);
      bsend(data, dest, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    void bsend(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Bsend(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                static_cast<int>(t), comm_);
    }

    template<typename iterT>
    void bsend(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        bsend(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        bsend(&(*begin), l, dest, t);
      }
    }

    // --- nonblocking buffered send ---
  private:
    template<typename T>
    irequest ibsend(const T &data, int dest, tag t, detail::basic_or_fixed_size_type) const {
      MPI_Request req;
      MPI_Ibsend(&data, 1, detail::datatype_traits<T>::get_datatype(), dest,
                 static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    void ibsend(const T &data, int dest, tag t, isend_irecv_state *isend_state,
                detail::contigous_const_stl_container) const {
      using value_type = typename T::value_type;
      const int count(data.size());
      MPI_Datatype datatype{detail::datatype_traits<value_type>::get_datatype()};
      MPI_Request req;
      MPI_Ibsend(data.size() > 0 ? &data[0] : nullptr, count, datatype, dest,
                 static_cast<int>(t), comm_, &req);
      MPI_Status s;
      MPI_Wait(&req, &s);
      isend_state->source = s.MPI_SOURCE;
      isend_state->tag = s.MPI_TAG;
      isend_state->datatype = datatype;
      isend_state->count = 0;
      MPI_Grequest_complete(isend_state->req);
    }

    template<typename T>
    void ibsend(const T &data, int dest, tag t, isend_irecv_state *isend_state,
                detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      detail::vector<value_type> serial_data(data.size(), std::begin(data));
      ibsend(serial_data, dest, t, isend_state, detail::contigous_const_stl_container{});
    }

    template<typename T, typename C>
    irequest ibsend(const T &data, int dest, tag t, C) const {
      isend_irecv_state *send_state{new isend_irecv_state()};
      MPI_Request req;
      MPI_Grequest_start(isend_irecv_query, isend_irecv_free, isend_irecv_cancel, send_state,
                         &req);
      send_state->req = req;
      std::thread thread(
          [this, &data, dest, t, send_state]() { ibsend(data, dest, t, send_state, C{}); });
      thread.detach();
      return impl::irequest(req);
    }

  public:
    template<typename T>
    irequest ibsend(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      check_container_size(data);
      return ibsend(data, dest, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    irequest ibsend(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Ibsend(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                 static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename iterT>
    irequest ibsend(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return ibsend(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return ibsend(&(*begin), l, dest, t);
      }
    }

    // --- persistent buffered send ---
    template<typename T>
    prequest bsend_init(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Bsend_init(&data, 1, detail::datatype_traits<T>::get_datatype(), dest,
                     static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename T>
    prequest bsend_init(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Bsend_init(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                     static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename iterT>
    prequest bsend_init(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return bsend_init(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return bsend_init(&(*begin), l, dest, t);
      }
    }

    // === synchronous send ===
    // --- blocking synchronous send ---
  private:
    template<typename T>
    void ssend(const T &data, int dest, tag t, detail::basic_or_fixed_size_type) const {
      MPI_Ssend(&data, 1, detail::datatype_traits<T>::get_datatype(), dest, static_cast<int>(t),
                comm_);
    }

    template<typename T>
    void ssend(const T &data, int dest, tag t, detail::contigous_const_stl_container) const {
      using value_type = typename T::value_type;
      vector_layout<value_type> l(data.size());
      ssend(data.size() > 0 ? &data[0] : nullptr, l, dest, t);
    }

    template<typename T>
    void ssend(const T &data, int dest, tag t, detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      detail::vector<value_type> serial_data(data.size(), std::begin(data));
      vector_layout<value_type> l(serial_data.size());
      ssend(serial_data.data(), l, dest, t);
    }

  public:
    template<typename T>
    void ssend(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      ssend(data, dest, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    void ssend(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Ssend(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                static_cast<int>(t), comm_);
    }

    template<typename iterT>
    void ssend(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        ssend(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        ssend(&(*begin), l, dest, t);
      }
    }

    // --- nonblocking synchronous send ---
  private:
    template<typename T>
    irequest issend(const T &data, int dest, tag t, detail::basic_or_fixed_size_type) const {
      MPI_Request req;
      MPI_Issend(&data, 1, detail::datatype_traits<T>::get_datatype(), dest,
                 static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    void issend(const T &data, int dest, tag t, isend_irecv_state *isend_state,
                detail::contigous_const_stl_container) const {
      using value_type = typename T::value_type;
      const int count(data.size());
      MPI_Datatype datatype{detail::datatype_traits<value_type>::get_datatype()};
      MPI_Request req;
      MPI_Issend(data.size() > 0 ? &data[0] : nullptr, count, datatype, dest,
                 static_cast<int>(t), comm_, &req);
      MPI_Status s;
      MPI_Wait(&req, &s);
      isend_state->source = s.MPI_SOURCE;
      isend_state->tag = s.MPI_TAG;
      isend_state->datatype = datatype;
      isend_state->count = 0;
      MPI_Grequest_complete(isend_state->req);
    }

    template<typename T>
    void issend(const T &data, int dest, tag t, isend_irecv_state *isend_state,
                detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      detail::vector<value_type> serial_data(data.size(), std::begin(data));
      issend(serial_data, dest, t, isend_state, detail::contigous_const_stl_container{});
    }

    template<typename T, typename C>
    irequest issend(const T &data, int dest, tag t, C) const {
      isend_irecv_state *send_state{new isend_irecv_state()};
      MPI_Request req;
      MPI_Grequest_start(isend_irecv_query, isend_irecv_free, isend_irecv_cancel, send_state,
                         &req);
      send_state->req = req;
      std::thread thread(
          [this, &data, dest, t, send_state]() { issend(data, dest, t, send_state, C{}); });
      thread.detach();
      return impl::irequest(req);
    }

  public:
    template<typename T>
    irequest issend(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      check_container_size(data);
      return issend(data, dest, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    irequest issend(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Issend(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                 static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename iterT>
    irequest issend(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return issend(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return issend(&(*begin), l, dest, t);
      }
    }

    // --- persistent synchronous send ---
    template<typename T>
    prequest ssend_init(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Ssend_init(&data, 1, detail::datatype_traits<T>::get_datatype(), dest,
                     static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename T>
    prequest ssend_init(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Ssend_init(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                     static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename iterT>
    prequest ssend_init(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return ssend_init(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return ssend_init(&(*begin), l, dest, t);
      }
    }

    // === ready send ===
    // --- blocking ready send ---
  private:
    template<typename T>
    void rsend(const T &data, int dest, tag t, detail::basic_or_fixed_size_type) const {
      MPI_Rsend(&data, 1, detail::datatype_traits<T>::get_datatype(), dest, static_cast<int>(t),
                comm_);
    }

    template<typename T>
    void rsend(const T &data, int dest, tag t, detail::contigous_const_stl_container) const {
      using value_type = typename T::value_type;
      vector_layout<value_type> l(data.size());
      rsend(data.size() > 0 ? &data[0] : nullptr, l, dest, t);
    }

    template<typename T>
    void rsend(const T &data, int dest, tag t, detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      detail::vector<value_type> serial_data(data.size(), std::begin(data));
      vector_layout<value_type> l(serial_data.size());
      rsend(serial_data.data(), l, dest, t);
    }

  public:
    template<typename T>
    void rsend(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      check_container_size(data);
      rsend(data, dest, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    void rsend(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Rsend(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                static_cast<int>(t), comm_);
    }

    template<typename iterT>
    void rsend(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        rsend(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        rsend(&(*begin), l, dest, t);
      }
    }

    // --- nonblocking ready send ---
  private:
    template<typename T>
    irequest irsend(const T &data, int dest, tag t, detail::basic_or_fixed_size_type) const {
      MPI_Request req;
      MPI_Irsend(&data, 1, detail::datatype_traits<T>::get_datatype(), dest,
                 static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    void irsend(const T &data, int dest, tag t, isend_irecv_state *isend_state,
                detail::contigous_const_stl_container) const {
      using value_type = typename T::value_type;
      const int count(data.size());
      MPI_Datatype datatype{detail::datatype_traits<value_type>::get_datatype()};
      MPI_Request req;
      MPI_Irsend(data.size() > 0 ? &data[0] : nullptr, count, datatype, dest,
                 static_cast<int>(t), comm_, &req);
      MPI_Status s;
      MPI_Wait(&req, &s);
      isend_state->source = s.MPI_SOURCE;
      isend_state->tag = s.MPI_TAG;
      isend_state->datatype = datatype;
      isend_state->count = 0;
      MPI_Grequest_complete(isend_state->req);
    }

    template<typename T>
    void irsend(const T &data, int dest, tag t, isend_irecv_state *isend_state,
                detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      detail::vector<value_type> serial_data(data.size(), std::begin(data));
      irsend(serial_data, dest, t, isend_state, detail::contigous_const_stl_container{});
    }

    template<typename T, typename C>
    irequest irsend(const T &data, int dest, tag t, C) const {
      isend_irecv_state *send_state{new isend_irecv_state()};
      MPI_Request req;
      MPI_Grequest_start(isend_irecv_query, isend_irecv_free, isend_irecv_cancel, send_state,
                         &req);
      send_state->req = req;
      std::thread thread(
          [this, &data, dest, t, send_state]() { irsend(data, dest, t, send_state, C{}); });
      thread.detach();
      return impl::irequest(req);
    }

  public:
    template<typename T>
    irequest irsend(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      check_container_size(data);
      return irsend(data, dest, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    irequest irsend(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Irsend(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                 static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename iterT>
    irequest irsend(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return irsend(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return irsend(&(*begin), l, dest, t);
      }
    }

    // --- persistent ready send ---
    template<typename T>
    prequest rsend_init(const T &data, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Rsend_init(&data, 1, detail::datatype_traits<T>::get_datatype(), dest,
                     static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename T>
    prequest rsend_init(const T *data, const layout<T> &l, int dest, tag t = tag(0)) const {
      check_dest(dest);
      check_send_tag(t);
      MPI_Request req;
      MPI_Rsend_init(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                     static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename iterT>
    prequest rsend_init(iterT begin, iterT end, int dest, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return rsend_init(&(*begin), l, dest, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return rsend_init(&(*begin), l, dest, t);
      }
    }

    // === receive ===
    // --- blocking receive ---
  private:
    template<typename T>
    status recv(T &data, int source, tag t, detail::basic_or_fixed_size_type) const {
      status s;
      MPI_Recv(&data, 1, detail::datatype_traits<T>::get_datatype(), source,
               static_cast<int>(t), comm_, reinterpret_cast<MPI_Status *>(&s));
      return s;
    }

    template<typename T>
    status recv(T &data, int source, tag t, detail::contigous_stl_container) const {
      using value_type = typename T::value_type;
      status s;
      auto *ps{reinterpret_cast<MPI_Status *>(&s)};
      MPI_Message message;
      MPI_Mprobe(source, static_cast<int>(t), comm_, &message, ps);
      int count{0};
      MPI_Get_count(ps, detail::datatype_traits<value_type>::get_datatype(), &count);
      check_count(count);
      data.resize(count);
      MPI_Mrecv(data.size() > 0 ? &data[0] : nullptr, count,
                detail::datatype_traits<value_type>::get_datatype(), &message, ps);
      return s;
    }

    template<typename T>
    status recv(T &data, int source, tag t, detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      status s;
      auto *ps{reinterpret_cast<MPI_Status *>(&s)};
      MPI_Message message;
      MPI_Mprobe(source, static_cast<int>(t), comm_, &message, ps);
      int count{0};
      MPI_Get_count(ps, detail::datatype_traits<value_type>::get_datatype(), &count);
      check_count(count);
      detail::vector<value_type> serial_data(count, detail::uninitialized{});
      MPI_Mrecv(serial_data.data(), count, detail::datatype_traits<value_type>::get_datatype(),
                &message, ps);
      T new_data(serial_data.begin(), serial_data.end());
      data.swap(new_data);
      return s;
    }

  public:
    /// \anchor communicator_recv
    template<typename T>
    status recv(T &data, int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      return recv(data, source, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    status recv(T *data, const layout<T> &l, int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      status s;
      MPI_Recv(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), source,
               static_cast<int>(t), comm_, reinterpret_cast<MPI_Status *>(&s));
      return s;
    }

    template<typename iterT>
    status recv(iterT begin, iterT end, int source, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return recv(&(*begin), l, source, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return recv(&(*begin), l, source, t);
      }
    }

    // --- nonblocking receive ---
  private:
    template<typename T>
    irequest irecv(T &data, int source, tag t, detail::basic_or_fixed_size_type) const {
      MPI_Request req;
      MPI_Irecv(&data, 1, detail::datatype_traits<T>::get_datatype(), source,
                static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    void irecv(T &data, int source, tag t, isend_irecv_state *irecv_state,
               detail::stl_container) const {
      using value_type = detail::remove_const_from_members_t<typename T::value_type>;
      const status s{recv(data, source, t)};
      irecv_state->source = s.source();
      irecv_state->tag = static_cast<int>(s.tag());
      irecv_state->datatype = detail::datatype_traits<value_type>::get_datatype();
      irecv_state->count = s.get_count<value_type>();
      MPI_Grequest_complete(irecv_state->req);
    }

    template<typename T, typename C>
    irequest irecv(T &data, int source, tag t, C) const {
      isend_irecv_state *recv_state{new isend_irecv_state()};
      MPI_Request req;
      MPI_Grequest_start(isend_irecv_query, isend_irecv_free, isend_irecv_cancel, recv_state,
                         &req);
      recv_state->req = req;
      std::thread thread(
          [this, &data, source, t, recv_state]() { irecv(data, source, t, recv_state, C{}); });
      thread.detach();
      return impl::irequest(req);
    }

  public:
    template<typename T>
    irequest irecv(T &data, int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      return irecv(data, source, t, typename detail::datatype_traits<T>::data_type_category{});
    }

    template<typename T>
    irequest irecv(T *data, const layout<T> &l, int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      MPI_Request req;
      MPI_Irecv(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), source,
                static_cast<int>(t), comm_, &req);
      return impl::irequest(req);
    }

    template<typename iterT>
    irequest irecv(iterT begin, iterT end, int source, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return irecv(&(*begin), l, source, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return irecv(&(*begin), l, source, t);
      }
    }

    // --- persistent receive ---
    template<typename T>
    prequest recv_init(T &data, int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      MPI_Request req;
      MPI_Recv_init(&data, 1, detail::datatype_traits<T>::get_datatype(), source,
                    static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename T>
    prequest recv_init(T *data, const layout<T> &l, int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      MPI_Request req;
      MPI_Recv_init(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), source,
                    static_cast<int>(t), comm_, &req);
      return impl::prequest(req);
    }

    template<typename iterT>
    prequest recv_init(iterT begin, iterT end, int source, tag t = tag(0)) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return recv_init(&(*begin), l, source, t);
      } else {
        iterator_layout<value_type> l(begin, end);
        return recv_init(&(*begin), l, source, t);
      }
    }

    // === probe ===
    // --- blocking probe ---
    [[nodiscard]] status probe(int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      status s;
      MPI_Probe(source, static_cast<int>(t), comm_, reinterpret_cast<MPI_Status *>(&s));
      return s;
    }

    // --- nonblocking probe ---
    [[nodiscard]] std::pair<bool, status> iprobe(int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      int result;
      status s;
      MPI_Iprobe(source, static_cast<int>(t), comm_, &result,
                 reinterpret_cast<MPI_Status *>(&s));
      return std::make_pair(static_cast<bool>(result), s);
    }

    // === matching probe ===
    // --- blocking matching probe ---
    [[nodiscard]] std::pair<message, status> mprobe(int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      status s;
      message m;
      MPI_Mprobe(source, static_cast<int>(t), comm_, &m, reinterpret_cast<MPI_Status *>(&s));
      return std::make_pair(m, s);
    }

    // --- nonblocking matching probe ---
    [[nodiscard]] std::tuple<bool, message, status> improbe(int source, tag t = tag(0)) const {
      check_source(source);
      check_recv_tag(t);
      int result;
      status s;
      message m;
      MPI_Improbe(source, static_cast<int>(t), comm_, &result, &m,
                  reinterpret_cast<MPI_Status *>(&s));
      return std::make_tuple(static_cast<bool>(result), m, s);
    }

    // === matching receive ===
    // --- blocking matching receive ---

    // --- nonblocking matching receive ---

    // === send and receive ===
    // --- send and receive ---
    template<typename T>
    status sendrecv(const T &senddata, int dest, tag sendtag, T &recvdata, int source,
                    tag recvtag) const {
      check_dest(dest);
      check_source(source);
      check_send_tag(sendtag);
      check_recv_tag(recvtag);
      status s;
      MPI_Sendrecv(&senddata, 1, detail::datatype_traits<T>::get_datatype(), dest,
                   static_cast<int>(sendtag), &recvdata, 1,
                   detail::datatype_traits<T>::get_datatype(), source,
                   static_cast<int>(recvtag), comm_, reinterpret_cast<MPI_Status *>(&s));
      return s;
    }

    template<typename T>
    status sendrecv(const T *senddata, const layout<T> &sendl, int dest, tag sendtag,
                    T *recvdata, const layout<T> &recvl, int source, tag recvtag) const {
      check_dest(dest);
      check_source(source);
      check_send_tag(sendtag);
      check_recv_tag(recvtag);
      status s;
      MPI_Sendrecv(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl), dest,
                   static_cast<int>(sendtag), recvdata, 1,
                   detail::datatype_traits<layout<T>>::get_datatype(recvl), source,
                   static_cast<int>(recvtag), comm_, reinterpret_cast<MPI_Status *>(&s));
      return s;
    }

    template<typename iterT1, typename iterT2>
    status sendrecv(iterT1 begin1, iterT1 end1, int dest, tag sendtag, iterT2 begin2,
                    iterT2 end2, int source, tag recvtag) const {
      using value_type1 = typename std::iterator_traits<iterT1>::value_type;
      using value_type2 = typename std::iterator_traits<iterT2>::value_type;
      if constexpr (detail::is_contiguous_iterator_v<iterT1> and
                    detail::is_contiguous_iterator_v<iterT2>) {
        vector_layout<value_type1> l1(std::distance(begin1, end1));
        vector_layout<value_type2> l2(std::distance(begin2, end2));
        return sendrecv(&(*begin1), l1, dest, sendtag, &(*begin2), l2, source, recvtag);
      } else if constexpr (detail::is_contiguous_iterator_v<iterT1>) {
        vector_layout<value_type1> l1(std::distance(begin1, end1));
        iterator_layout<value_type2> l2(begin2, end2);
        return sendrecv(&(*begin1), l1, dest, sendtag, &(*begin2), l2, source, recvtag);
      } else if constexpr (detail::is_contiguous_iterator_v<iterT2>) {
        iterator_layout<value_type2> l1(begin1, end1);
        vector_layout<value_type2> l2(std::distance(begin2, end2));
        return sendrecv(&(*begin1), l1, dest, sendtag, &(*begin2), l2, source, recvtag);
      } else {
        iterator_layout<value_type1> l1(begin1, end1);
        iterator_layout<value_type2> l2(begin2, end2);
        return sendrecv(&(*begin1), l1, dest, sendtag, &(*begin2), l2, source, recvtag);
      }
    }

    // --- send, receive and replace ---
    template<typename T>
    status sendrecv_replace(T &data, int dest, tag sendtag, int source, tag recvtag) const {
      check_dest(dest);
      check_source(source);
      check_send_tag(sendtag);
      check_recv_tag(recvtag);
      status s;
      MPI_Sendrecv_replace(&data, 1, detail::datatype_traits<T>::get_datatype(), dest,
                           static_cast<int>(sendtag), source, static_cast<int>(recvtag), comm_,
                           reinterpret_cast<MPI_Status *>(&s));
      return s;
    }

    template<typename T>
    status sendrecv_replace(T *data, const layout<T> &l, int dest, tag sendtag, int source,
                            tag recvtag) const {
      check_dest(dest);
      check_source(source);
      check_send_tag(sendtag);
      check_recv_tag(recvtag);
      status s;
      MPI_Sendrecv_replace(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), dest,
                           static_cast<int>(sendtag), source, static_cast<int>(recvtag), comm_,
                           reinterpret_cast<MPI_Status *>(&s));
      return s;
    }

    template<typename iterT>
    status sendrecv_replace(iterT begin, iterT end, int dest, tag sendtag, int source,
                            tag recvtag) const {
      using value_type = typename std::iterator_traits<iterT>::value_type;
      if (detail::is_contiguous_iterator_v<iterT>) {
        vector_layout<value_type> l(std::distance(begin, end));
        return sendrecv_replace(&(*begin), l, dest, sendtag, source, recvtag);
      } else {
        iterator_layout<value_type> l(begin, end);
        return sendrecv_replace(&(*begin), l, dest, sendtag, source, recvtag);
      }
    }

    // === collective ==================================================
    // === barrier ===
    // --- blocking barrier ---
    void barrier() const { MPI_Barrier(comm_); }

    // --- nonblocking barrier ---
    [[nodiscard]] irequest ibarrier() const {
      MPI_Request req;
      MPI_Ibarrier(comm_, &req);
      return impl::irequest(req);
    }

    // === broadcast ===
    // --- blocking broadcast ---
    template<typename T>
    void bcast(int root_rank, T &data) const {
      check_root(root_rank);
      MPI_Bcast(&data, 1, detail::datatype_traits<T>::get_datatype(), root_rank, comm_);
    }

    template<typename T>
    void bcast(int root_rank, T *data, const layout<T> &l) const {
      check_root(root_rank);
      MPI_Bcast(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), root_rank, comm_);
    }

    // --- nonblocking broadcast ---
    template<typename T>
    irequest ibcast(int root_rank, T &data) const {
      check_root(root_rank);
      MPI_Request req;
      MPI_Ibcast(&data, 1, detail::datatype_traits<T>::get_datatype(), root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    irequest ibcast(int root_rank, T *data, const layout<T> &l) const {
      check_root(root_rank);
      MPI_Request req;
      MPI_Ibcast(data, 1, detail::datatype_traits<layout<T>>::get_datatype(l), root_rank, comm_,
                 &req);
      return impl::irequest(req);
    }

    // === gather ===
    // === root gets a single value from each rank and stores in contiguous memory
    // --- blocking gather ---
    template<typename T>
    void gather(int root_rank, const T &senddata, T *recvdata) const {
      check_root(root_rank);
      MPI_Gather(&senddata, 1, detail::datatype_traits<T>::get_datatype(), recvdata, 1,
                 detail::datatype_traits<T>::get_datatype(), root_rank, comm_);
    }

    template<typename T>
    void gather(int root_rank, const T *senddata, const layout<T> &sendl, T *recvdata,
                const layout<T> &recvl) const {
      check_root(root_rank);
      MPI_Gather(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl), recvdata,
                 1, detail::datatype_traits<layout<T>>::get_datatype(recvl), root_rank, comm_);
    }

    // --- nonblocking gather ---
    template<typename T>
    irequest igather(int root_rank, const T &senddata, T *recvdata) const {
      check_root(root_rank);
      MPI_Request req;
      MPI_Igather(&senddata, 1, detail::datatype_traits<T>::get_datatype(), recvdata, 1,
                  detail::datatype_traits<T>::get_datatype(), root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    irequest igather(int root_rank, const T *senddata, const layout<T> &sendl, T *recvdata,
                     const layout<T> &recvl) const {
      check_root(root_rank);
      MPI_Request req;
      MPI_Igather(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl),
                  recvdata, 1, detail::datatype_traits<layout<T>>::get_datatype(recvl),
                  root_rank, comm_, &req);
      return impl::irequest(req);
    }

    // --- blocking gather, non-root variant ---
    template<typename T>
    void gather(int root_rank, const T &senddata) const {
      check_nonroot(root_rank);
      MPI_Gather(&senddata, 1, detail::datatype_traits<T>::get_datatype(), 0, 0,
                 MPI_DATATYPE_NULL, root_rank, comm_);
    }

    template<typename T>
    void gather(int root_rank, const T *senddata, const layout<T> &sendl) const {
      check_nonroot(root_rank);
      MPI_Gather(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl), 0, 0,
                 MPI_DATATYPE_NULL, root_rank, comm_);
    }

    // --- nonblocking gather, non-root variant ---
    template<typename T>
    irequest igather(int root_rank, const T &senddata) const {
      check_nonroot(root_rank);
      MPI_Request req;
      MPI_Igather(&senddata, 1, detail::datatype_traits<T>::get_datatype(), 0, 0,
                  MPI_DATATYPE_NULL, root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    irequest igather(int root_rank, const T *senddata, const layout<T> &sendl) const {
      check_nonroot(root_rank);
      MPI_Request req;
      MPI_Igather(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl), 0, 0,
                  MPI_DATATYPE_NULL, root_rank, comm_, &req);
      return impl::irequest(req);
    }

    // === root gets varying amount of data from each rank and stores in non-contiguous memory
    // --- blocking gather ---
    template<typename T>
    void gatherv(int root_rank, const T *senddata, const layout<T> &sendl, T *recvdata,
                 const layouts<T> &recvls, const displacements &recvdispls) const {
      check_root(root_rank);
      check_size(recvls);
      check_size(recvdispls);
      int N(size());
      displacements senddispls(N);
      layouts<T> sendls(N);
      sendls[root_rank] = sendl;
      if (rank() == root_rank)
        alltoallv(senddata, sendls, senddispls, recvdata, recvls, recvdispls);
      else
        alltoallv(senddata, sendls, senddispls, recvdata, mpl::layouts<T>(N), recvdispls);
    }

    template<typename T>
    void gatherv(int root_rank, const T *senddata, const layout<T> &sendl, T *recvdata,
                 const layouts<T> &recvls) const {
      gatherv(root_rank, senddata, sendl, recvdata, recvls, displacements(size()));
    }

    // --- nonblocking gather ---
    template<typename T>
    irequest igatherv(int root_rank, const T *senddata, const layout<T> &sendl, T *recvdata,
                      const layouts<T> &recvls, const displacements &recvdispls) const {
      check_root(root_rank);
      check_size(recvls);
      check_size(recvdispls);
      int N(size());
      displacements senddispls(N);
      layouts<T> sendls(N);
      sendls[root_rank] = sendl;
      if (rank() == root_rank)
        return ialltoallv(senddata, sendls, senddispls, recvdata, recvls, recvdispls);
      else
        return ialltoallv(senddata, sendls, senddispls, recvdata, mpl::layouts<T>(N),
                          recvdispls);
    }

    template<typename T>
    irequest igatherv(int root_rank, const T *senddata, const layout<T> &sendl, T *recvdata,
                      const layouts<T> &recvls) const {
      return igatherv(root_rank, senddata, sendl, recvdata, recvls, displacements(size()));
    }

    // --- blocking gather, non-root variant ---
    template<typename T>
    void gatherv(int root_rank, const T *senddata, const layout<T> &sendl) const {
      check_nonroot(root_rank);
      int N(size());
      displacements sendrecvdispls(N);
      layouts<T> sendls(N);
      sendls[root_rank] = sendl;
      alltoallv(senddata, sendls, sendrecvdispls, static_cast<T *>(nullptr), mpl::layouts<T>(N),
                sendrecvdispls);
    }

    // --- nonblocking gather, non-root variant ---
    template<typename T>
    irequest igatherv(int root_rank, const T *senddata, const layout<T> &sendl) const {
      check_nonroot(root_rank);
      int N(size());
      displacements sendrecvdispls(N);
      layouts<T> sendls(N);
      sendls[root_rank] = sendl;
      return ialltoallv(senddata, sendls, sendrecvdispls, static_cast<T *>(nullptr),
                        mpl::layouts<T>(N), sendrecvdispls);
    }

    // === allgather ===
    // === get a single value from each rank and stores in contiguous memory
    // --- blocking allgather ---
    template<typename T>
    void allgather(const T &senddata, T *recvdata) const {
      MPI_Allgather(&senddata, 1, detail::datatype_traits<T>::get_datatype(), recvdata, 1,
                    detail::datatype_traits<T>::get_datatype(), comm_);
    }

    template<typename T>
    void allgather(const T *senddata, const layout<T> &sendl, T *recvdata,
                   const layout<T> &recvl) const {
      MPI_Allgather(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl),
                    recvdata, 1, detail::datatype_traits<layout<T>>::get_datatype(recvl),
                    comm_);
    }

    // --- nonblocking allgather ---
    template<typename T>
    irequest iallgather(const T &senddata, T *recvdata) const {
      MPI_Request req;
      MPI_Iallgather(&senddata, 1, detail::datatype_traits<T>::get_datatype(), recvdata, 1,
                     detail::datatype_traits<T>::get_datatype(), comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    irequest iallgather(const T *senddata, const layout<T> &sendl, T *recvdata,
                        const layout<T> &recvl) const {
      MPI_Request req;
      MPI_Iallgather(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl),
                     recvdata, 1, detail::datatype_traits<layout<T>>::get_datatype(recvl),
                     comm_,
                     &req);
      return impl::irequest(req);
    }

    // === get varying amount of data from each rank and stores in non-contiguous memory
    // --- blocking allgather ---
    template<typename T>
    void allgatherv(const T *senddata, const layout<T> &sendl, T *recvdata,
                    const layouts<T> &recvls, const displacements &recvdispls) const {
      check_size(recvls);
      check_size(recvdispls);
      int N(size());
      displacements senddispls(N);
      layouts<T> sendls(N, sendl);
      alltoallv(senddata, sendls, senddispls, recvdata, recvls, recvdispls);
    }

    template<typename T>
    void allgatherv(const T *senddata, const layout<T> &sendl, T *recvdata,
                    const layouts<T> &recvls) const {
      allgatherv(senddata, sendl, recvdata, recvls, displacements(size()));
    }

    // --- nonblocking allgather ---
    template<typename T>
    irequest iallgatherv(const T *senddata, const layout<T> &sendl, T *recvdata,
                         const layouts<T> &recvls, const displacements &recvdispls) const {
      check_size(recvls);
      check_size(recvdispls);
      int N(size());
      displacements senddispls(N);
      layouts<T> sendls(N, sendl);
      return ialltoallv(senddata, sendls, senddispls, recvdata, recvls, recvdispls);
    }

    template<typename T>
    irequest iallgatherv(const T *senddata, const layout<T> &sendl, T *recvdata,
                         const layouts<T> &recvls) const {
      return iallgatherv(senddata, sendl, recvdata, recvls, displacements(size()));
    }

    // === scatter ===
    // === root sends a single value from contiguous memory to each rank
    // --- blocking scatter ---
    template<typename T>
    void scatter(int root_rank, const T *senddata, T &recvdata) const {
      check_root(root_rank);
      MPI_Scatter(senddata, 1, detail::datatype_traits<T>::get_datatype(), &recvdata, 1,
                  detail::datatype_traits<T>::get_datatype(), root_rank, comm_);
    }

    template<typename T>
    void scatter(int root_rank, const T *senddata, const layout<T> &sendl, T *recvdata,
                 const layout<T> &recvl) const {
      check_root(root_rank);
      MPI_Scatter(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl),
                  recvdata, 1, detail::datatype_traits<layout<T>>::get_datatype(recvl),
                  root_rank, comm_);
    }

    // --- nonblocking scatter ---
    template<typename T>
    irequest iscatter(int root_rank, const T *senddata, T &recvdata) const {
      check_root(root_rank);
      MPI_Request req;
      MPI_Iscatter(senddata, 1, detail::datatype_traits<T>::get_datatype(), &recvdata, 1,
                   detail::datatype_traits<T>::get_datatype(), root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    irequest iscatter(int root_rank, const T *senddata, const layout<T> &sendl, T *recvdata,
                      const layout<T> &recvl) const {
      check_root(root_rank);
      MPI_Request req;
      MPI_Iscatter(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(sendl),
                   recvdata, 1, detail::datatype_traits<layout<T>>::get_datatype(recvl),
                   root_rank, comm_, &req);
      return impl::irequest(req);
    }

    // --- blocking scatter, non-root variant ---
    template<typename T>
    void scatter(int root_rank, T &recvdata) const {
      check_nonroot(root_rank);
      MPI_Scatter(0, 0, MPI_DATATYPE_NULL, &recvdata, 1,
                  detail::datatype_traits<T>::get_datatype(), root_rank, comm_);
    }

    template<typename T>
    void scatter(int root_rank, T *recvdata, const layout<T> &recvl) const {
      check_root(root_rank);
      MPI_Scatter(0, 0, MPI_DATATYPE_NULL, recvdata, 1,
                  detail::datatype_traits<layout<T>>::get_datatype(recvl), root_rank, comm_);
    }

    // --- nonblocking scatter, non-root variant ---
    template<typename T>
    irequest iscatter(int root_rank, T &recvdata) const {
      check_nonroot(root_rank);
      MPI_Request req;
      MPI_Iscatter(0, 0, MPI_DATATYPE_NULL, &recvdata, 1,
                   detail::datatype_traits<T>::get_datatype(), root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    irequest iscatter(int root_rank, T *recvdata, const layout<T> &recvl) const {
      check_nonroot(root_rank);
      MPI_Request req;
      MPI_Iscatter(0, 0, MPI_DATATYPE_NULL, recvdata, 1,
                   detail::datatype_traits<layout<T>>::get_datatype(recvl), root_rank, comm_,
                   &req);
      return impl::irequest(req);
    }

    // === root sends varying amount of data from non-contiguous memory to each rank
    // --- blocking scatter ---
    template<typename T>
    void scatterv(int root_rank, const T *senddata, const layouts<T> &sendls,
                  const displacements &senddispls, T *recvdata, const layout<T> &recvl) const {
      check_root(root_rank);
      check_size(sendls);
      check_size(senddispls);
      const int N{size()};
      displacements recvdispls(N);
      layouts<T> recvls(N);
      recvls[root_rank] = recvl;
      if (rank() == root_rank)
        alltoallv(senddata, sendls, senddispls, recvdata, recvls, recvdispls);
      else
        alltoallv(senddata, sendls, senddispls, recvdata, mpl::layouts<T>(N), recvdispls);
    }

    template<typename T>
    void scatterv(int root_rank, const T *senddata, const layouts<T> &sendls, T *recvdata,
                  const layout<T> &recvl) const {
      scatterv(root_rank, senddata, sendls, displacements(size()), recvdata, recvl);
    }

    // --- nonblocking scatter ---
    template<typename T>
    irequest iscatterv(int root_rank, const T *senddata, const layouts<T> &sendls,
                       const displacements &senddispls, T *recvdata,
                       const layout<T> &recvl) const {
      check_root(root_rank);
      check_size(sendls);
      check_size(senddispls);
      const int N{size()};
      displacements recvdispls(N);
      layouts<T> recvls(N);
      recvls[root_rank] = recvl;
      if (rank() == root_rank)
        return ialltoallv(senddata, sendls, senddispls, recvdata, recvls, recvdispls);
      else
        return ialltoallv(senddata, sendls, senddispls, recvdata, mpl::layouts<T>(N),
                          recvdispls);
    }

    template<typename T>
    irequest iscatterv(int rootRank, const T *senddata, const layouts<T> &sendls, T *recvdata,
                       const layout<T> &recvl) const {
      return iscatterv(rootRank, senddata, sendls, displacements(size()), recvdata, recvl);
    }

    // --- blocking scatter, non-root variant ---
    template<typename T>
    void scatterv(int root_rank, T *recvdata, const layout<T> &recvl) const {
      check_root(root_rank);
      const int N{size()};
      displacements sendrecvdispls(N);
      layouts<T> recvls(N);
      recvls[root_rank] = recvl;
      alltoallv(static_cast<const T *>(nullptr), mpl::layouts<T>(N), sendrecvdispls, recvdata,
                recvls, sendrecvdispls);
    }

    // --- nonblocking scatter, non-root variant ---
    template<typename T>
    irequest iscatterv(int root_rank, T *recvdata, const layout<T> &recvl) const {
      check_root(root_rank);
      const int N{size()};
      displacements sendrecvdispls(N);
      layouts<T> recvls(N);
      recvls[root_rank] = recvl;
      return ialltoallv(static_cast<const T *>(nullptr), mpl::layouts<T>(N), sendrecvdispls,
                        recvdata, recvls, sendrecvdispls);
    }

    // === all-to-all ===
    // === each rank sends a single value to each rank
    // --- blocking all-to-all ---
    template<typename T>
    void alltoall(const T *senddata, T *recvdata) const {
      MPI_Alltoall(senddata, 1, detail::datatype_traits<T>::get_datatype(), recvdata, 1,
                   detail::datatype_traits<T>::get_datatype(), comm_);
    }

    template<typename T>
    void alltoall(const T *senddata, const layout<T> &sendl, T *recvdata,
                  const layout<T> &recvl) const {
      MPI_Alltoall(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(), recvdata, 1,
                   detail::datatype_traits<layout<T>>::get_datatype(), comm_);
    }

    // --- nonblocking all-to-all ---
    template<typename T>
    irequest ialltoall(const T *senddata, T *recvdata) const {
      MPI_Request req;
      MPI_Ialltoall(senddata, 1, detail::datatype_traits<T>::get_datatype(), recvdata, 1,
                    detail::datatype_traits<T>::get_datatype(), comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    irequest ialltoall(const T *senddata, const layout<T> &sendl, T *recvdata,
                       const layout<T> &recvl) const {
      MPI_Request req;
      MPI_Ialltoall(senddata, 1, detail::datatype_traits<layout<T>>::get_datatype(), recvdata,
                    1, detail::datatype_traits<layout<T>>::get_datatype(), comm_, &req);
      return impl::irequest(req);
    }

    // --- blocking all-to-all, in place ---
    template<typename T>
    void alltoall(T *recvdata) const {
      MPI_Alltoall(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, recvdata, 1,
                   detail::datatype_traits<T>::get_datatype(), comm_);
    }

    template<typename T>
    void alltoall(T *recvdata, const layout<T> &recvl) const {
      MPI_Alltoall(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, recvdata, 1,
                   detail::datatype_traits<layout<T>>::get_datatype(), comm_);
    }

    // --- nonblocking all-to-all, in place ---
    template<typename T>
    irequest ialltoall(T *recvdata) const {
      MPI_Request req;
      MPI_Ialltoall(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, recvdata, 1,
                    detail::datatype_traits<T>::get_datatype(), comm_, &req);
      return impl::irequest(req);
    }

    template<typename T>
    irequest ialltoall(T *recvdata, const layout<T> &recvl) const {
      MPI_Request req;
      MPI_Ialltoall(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, recvdata, 1,
                    detail::datatype_traits<layout<T>>::get_datatype(), comm_, &req);
      return impl::irequest(req);
    }

    // === each rank sends a varying number of values to each rank with possibly different
    // layouts
    // --- blocking all-to-all ---
    template<typename T>
    void alltoallv(const T *senddata, const layouts<T> &sendl, const displacements &senddispls,
                   T *recvdata, const layouts<T> &recvl,
                   const displacements &recvdispls) const {
      check_size(senddispls);
      check_size(sendl);
      check_size(recvdispls);
      check_size(recvl);
      std::vector<int> counts(recvl.size(), 1);
      std::vector<int> senddispls_int(senddispls.begin(), senddispls.end());
      std::vector<int> recvdispls_int(recvdispls.begin(), recvdispls.end());
      MPI_Alltoallw(senddata, counts.data(), senddispls_int.data(),
                    reinterpret_cast<const MPI_Datatype *>(sendl()), recvdata, counts.data(),
                    recvdispls_int.data(), reinterpret_cast<const MPI_Datatype *>(recvl()),
                    comm_);
    }

    template<typename T>
    void alltoallv(const T *senddata, const layouts<T> &sendl, T *recvdata,
                   const layouts<T> &recvl) const {
      displacements sendrecvdispls(size());
      alltoallv(senddata, sendl, sendrecvdispls, recvdata, recvl, sendrecvdispls);
    }

    // --- non-blocking all-to-all ---
  private:
    template<typename T>
    struct ialltoallv_state {
      MPI_Request req{};
      layouts<T> sendl;
      layouts<T> recvl;
      std::vector<int> counts;
      std::vector<int> senddispls_int;
      std::vector<int> recvdispls_int;
      MPI_Status status{};
      ialltoallv_state(const layouts<T> &sendl, const layouts<T> &recvl,
                       std::vector<int> &&counts, std::vector<int> &&senddispls_int,
                       std::vector<int> &&recvdispls_int)
          : sendl{sendl},
            recvl{recvl},
            counts{std::move(counts)},
            senddispls_int{std::move(senddispls_int)},
            recvdispls_int{std::move(recvdispls_int)} {}
      ialltoallv_state(const layouts<T> &recvl, std::vector<int> &&counts,
                       std::vector<int> &&recvdispls_int)
          : sendl{},
            recvl{recvl},
            counts{std::move(counts)},
            senddispls_int{},
            recvdispls_int{std::move(recvdispls_int)} {}
    };

    template<typename T>
    static int ialltoallv_query(void *state, MPI_Status *s) {
      ialltoallv_state<T> *sendrecv_state{reinterpret_cast<ialltoallv_state<T> *>(state)};
      const int error_backup{s->MPI_ERROR};
      *s = sendrecv_state->status;
      s->MPI_ERROR = error_backup;
      return MPI_SUCCESS;
    }

    template<typename T>
    static int ialltoallv_free(void *state) {
      ialltoallv_state<T> *sendrecv_state{reinterpret_cast<ialltoallv_state<T> *>(state)};
      delete sendrecv_state;
      return MPI_SUCCESS;
    }

    static int ialltoallv_cancel(void *state, int complete) { return MPI_SUCCESS; }

    template<typename T>
    void ialltoallv(const T *senddata, T *recvdata, ialltoallv_state<T> *state) const {
      MPI_Request req;
      if (senddata != nullptr)
        MPI_Ialltoallw(senddata, state->counts.data(), state->senddispls_int.data(),
                       reinterpret_cast<const MPI_Datatype *>(state->sendl()), recvdata,
                       state->counts.data(), state->recvdispls_int.data(),
                       reinterpret_cast<const MPI_Datatype *>(state->recvl()), comm_, &req);
      else
        MPI_Ialltoallw(MPI_IN_PLACE, 0, 0, 0, recvdata, state->counts.data(),
                       state->recvdispls_int.data(),
                       reinterpret_cast<const MPI_Datatype *>(state->recvl()), comm_, &req);
      MPI_Status s;
      MPI_Wait(&req, &s);
      state->status = s;
      MPI_Grequest_complete(state->req);
    }

  public:
    template<typename T>
    irequest ialltoallv(const T *senddata, const layouts<T> &sendl,
                        const displacements &senddispls, T *recvdata, const layouts<T> &recvl,
                        const displacements &recvdispls) const {
      check_size(senddispls);
      check_size(sendl);
      check_size(recvdispls);
      check_size(recvl);
      ialltoallv_state<T> *state{
          new ialltoallv_state<T>(sendl, recvl, std::vector<int>(recvl.size(), 1),
                                  std::vector<int>(senddispls.begin(), senddispls.end()),
                                  std::vector<int>(recvdispls.begin(), recvdispls.end()))};
      MPI_Request req;
      MPI_Grequest_start(ialltoallv_query<T>, ialltoallv_free<T>, ialltoallv_cancel, state,
                         &req);
      state->req = req;
      std::thread thread(
          [this, senddata, recvdata, state]() { ialltoallv(senddata, recvdata, state); });
      thread.detach();
      return impl::irequest(req);
    }

    template<typename T>
    irequest ialltoallv(const T *senddata, const layouts<T> &sendl, T *recvdata,
                        const layouts<T> &recvl) const {
      displacements sendrecvdispls(size());
      return ialltoallv(senddata, sendl, sendrecvdispls, recvdata, recvl, sendrecvdispls);
    }

    // --- blocking all-to-all, in place ---
    template<typename T>
    void alltoallv(T *recvdata, const layouts<T> &recvl,
                   const displacements &recvdispls) const {
      check_size(recvdispls);
      check_size(recvl);
      std::vector<int> counts(recvl.size(), 1);
      std::vector<int> recvdispls_int(recvdispls.begin(), recvdispls.end());
      MPI_Alltoallw(MPI_IN_PLACE, 0, 0, 0, recvdata, counts.data(), recvdispls_int.data(),
                    reinterpret_cast<const MPI_Datatype *>(recvl()), comm_);
    }

    template<typename T>
    void alltoallv(T *recvdata, const layouts<T> &recvl) const {
      alltoallv(recvdata, recvl, displacements(size()));
    }

    // --- non-blocking all-to-all, in place ---
    template<typename T>
    irequest ialltoallv(T *recvdata, const layouts<T> &recvl,
                        const displacements &recvdispls) const {
      check_size(recvdispls);
      check_size(recvl);
      ialltoallv_state<T> *state{
          new ialltoallv_state<T>(recvl, std::vector<int>(recvl.size(), 1),
                                  std::vector<int>(recvdispls.begin(), recvdispls.end()))};
      MPI_Request req;
      MPI_Grequest_start(ialltoallv_query<T>, ialltoallv_free<T>, ialltoallv_cancel, state,
                         &req);
      state->req = req;
      std::thread thread([this, recvdata, state]() { ialltoallv(nullptr, recvdata, state); });
      thread.detach();
      return impl::irequest(req);
    }

    template<typename T>
    irequest ialltoallv(T *recvdata, const layouts<T> &recvl) const {
      return ialltoallv(recvdata, recvl, displacements(size()));
    }

    // === reduce ===
    // --- blocking reduce ---
    template<typename T, typename F>
    void reduce(F f, int root_rank, const T &senddata, T &recvdata) const {
      check_root(root_rank);
      MPI_Reduce(&senddata, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                 detail::get_op<T, F>(f).mpi_op, root_rank, comm_);
    }

    /// \anchor communicator_reduce_contiguous_layout
    template<typename T, typename F>
    void reduce(F f, int root_rank, const T *senddata, T *recvdata,
                const contiguous_layout<T> &l) const {
      check_root(root_rank);
      MPI_Reduce(senddata, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                 detail::get_op<T, F>(f).mpi_op, root_rank, comm_);
    }

    // --- non-blocking reduce ---
    template<typename T, typename F>
    irequest ireduce(F f, int root_rank, const T &senddata, T &recvdata) const {
      check_root(root_rank);
      MPI_Request req;
      MPI_Ireduce(&senddata, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                  detail::get_op<T, F>(f).mpi_op, root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest ireduce(F f, int root_rank, const T *senddata, T *recvdata,
                     const contiguous_layout<T> &l) const {
      check_root(root_rank);
      MPI_Request req;
      MPI_Ireduce(senddata, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                  detail::get_op<T, F>(f).mpi_op, root_rank, comm_, &req);
      return impl::irequest(req);
    }

    // --- blocking reduce, in place ---
    template<typename T, typename F>
    void reduce(F f, int root_rank, T &sendrecvdata) const {
      check_root(root_rank);
      if (rank() == root_rank)
        MPI_Reduce(MPI_IN_PLACE, &sendrecvdata, 1, detail::datatype_traits<T>::get_datatype(),
                   detail::get_op<T, F>(f).mpi_op, root_rank, comm_);
      else
        MPI_Reduce(&sendrecvdata, nullptr, 1, detail::datatype_traits<T>::get_datatype(),
                   detail::get_op<T, F>(f).mpi_op, root_rank, comm_);
    }

    template<typename T, typename F>
    void reduce(F f, int root_rank, const T &senddata) const {
      check_nonroot(root_rank);
      MPI_Reduce(&senddata, nullptr, 1, detail::datatype_traits<T>::get_datatype(),
                 detail::get_op<T, F>(f).mpi_op, root_rank, comm_);
    }

    template<typename T, typename F>
    void reduce(F f, int root_rank, T *sendrecvdata, const contiguous_layout<T> &l) const {
      if (rank() == root_rank)
        MPI_Reduce(MPI_IN_PLACE, sendrecvdata, l.size(),
                   detail::datatype_traits<T>::get_datatype(), detail::get_op<T, F>(f).mpi_op,
                   root_rank, comm_);
      else
        MPI_Reduce(sendrecvdata, nullptr, l.size(), detail::datatype_traits<T>::get_datatype(),
                   detail::get_op<T, F>(f).mpi_op, root_rank, comm_);
    }

    template<typename T, typename F>
    void reduce(F f, int root_rank, const T *sendrecvdata,
                const contiguous_layout<T> &l) const {
      check_nonroot(root_rank);
      MPI_Reduce(sendrecvdata, nullptr, l.size(), detail::datatype_traits<T>::get_datatype(),
                 detail::get_op<T, F>(f).mpi_op, root_rank, comm_);
    }

    // --- non-blocking reduce, in place ---
    template<typename T, typename F>
    irequest ireduce(F f, int root_rank, T &sendrecvdata) const {
      check_root(root_rank);
      MPI_Request req;
      if (rank() == root_rank)
        MPI_Ireduce(MPI_IN_PLACE, &sendrecvdata, 1, detail::datatype_traits<T>::get_datatype(),
                    detail::get_op<T, F>(f).mpi_op, root_rank, comm_, &req);
      else
        MPI_Ireduce(&sendrecvdata, nullptr, 1, detail::datatype_traits<T>::get_datatype(),
                    detail::get_op<T, F>(f).mpi_op, root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest ireduce(F f, int root_rank, const T &sendrecvdata) const {
      check_nonroot(root_rank);
      MPI_Request req;
      MPI_Ireduce(&sendrecvdata, nullptr, 1, detail::datatype_traits<T>::get_datatype(),
                  detail::get_op<T, F>(f).mpi_op, root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest ireduce(F f, int root_rank, T *sendrecvdata, const contiguous_layout<T> &l) const {
      check_root(root_rank);
      MPI_Request req;
      if (rank() == root_rank)
        MPI_Ireduce(MPI_IN_PLACE, sendrecvdata, l.size(),
                    detail::datatype_traits<T>::get_datatype(), detail::get_op<T, F>(f).mpi_op,
                    root_rank, comm_, &req);
      else
        MPI_Ireduce(sendrecvdata, nullptr, l.size(), detail::datatype_traits<T>::get_datatype(),
                    detail::get_op<T, F>(f).mpi_op, root_rank, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest ireduce(F f, int root_rank, const T *sendrecvdata,
                     const contiguous_layout<T> &l) const {
      check_nonroot(root_rank);
      MPI_Request req;
      MPI_Ireduce(sendrecvdata, nullptr, l.size(), detail::datatype_traits<T>::get_datatype(),
                  detail::get_op<T, F>(f).mpi_op, root_rank, comm_, &req);
      return impl::irequest(req);
    }

    // === all-reduce ===
    // --- blocking all-reduce ---
    template<typename T, typename F>
    void allreduce(F f, const T &senddata, T &recvdata) const {
      MPI_Allreduce(&senddata, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                    detail::get_op<T, F>(f).mpi_op, comm_);
    }

    template<typename T, typename F>
    void allreduce(F f, const T *senddata, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Allreduce(senddata, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                    detail::get_op<T, F>(f).mpi_op, comm_);
    }

    // --- non-blocking all-reduce ---
    template<typename T, typename F>
    irequest iallreduce(F f, const T &senddata, T &recvdata) const {
      MPI_Request req;
      MPI_Iallreduce(&senddata, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                     detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest iallreduce(F f, const T *senddata, T *recvdata,
                        const contiguous_layout<T> &l) const {
      MPI_Request req;
      MPI_Iallreduce(senddata, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                     detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    // --- blocking all-reduce, in place ---
    template<typename T, typename F>
    void allreduce(F f, T &sendrecvdata) const {
      MPI_Allreduce(MPI_IN_PLACE, &sendrecvdata, 1, detail::datatype_traits<T>::get_datatype(),
                    detail::get_op<T, F>(f).mpi_op, comm_);
    }

    template<typename T, typename F>
    void allreduce(F f, T *sendrecvdata, const contiguous_layout<T> &l) const {
      MPI_Allreduce(MPI_IN_PLACE, sendrecvdata, l.size(),
                    detail::datatype_traits<T>::get_datatype(), detail::get_op<T, F>(f).mpi_op,
                    comm_);
    }

    // --- non-blocking all-reduce, in place ---
    template<typename T, typename F>
    irequest iallreduce(F f, T &sendrecvdata) const {
      MPI_Request req;
      MPI_Iallreduce(MPI_IN_PLACE, &sendrecvdata, 1, detail::datatype_traits<T>::get_datatype(),
                     detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest iallreduce(F f, T *sendrecvdata, const contiguous_layout<T> &l) const {
      MPI_Request req;
      MPI_Iallreduce(MPI_IN_PLACE, sendrecvdata, l.size(),
                     detail::datatype_traits<T>::get_datatype(), detail::get_op<T, F>(f).mpi_op,
                     comm_, &req);
      return impl::irequest(req);
    }

    // === reduce-scatter-block ===
    // --- blocking reduce-scatter-block ---
    template<typename T, typename F>
    void reduce_scatter_block(F f, const T *senddata, T &recvdata) const {
      MPI_Reduce_scatter_block(senddata, &recvdata, 1,
                               detail::datatype_traits<T>::get_datatype(),
                               detail::get_op<T, F>(f).mpi_op, comm_);
    }

    template<typename T, typename F>
    void reduce_scatter_block(F f, const T *senddata, T *recvdata,
                              const contiguous_layout<T> &l) const {
      MPI_Reduce_scatter_block(senddata, recvdata, l.size(),
                               detail::datatype_traits<T>::get_datatype(),
                               detail::get_op<T, F>(f).mpi_op, comm_);
    }

    // --- non-blocking reduce-scatter-block ---
    template<typename T, typename F>
    irequest ireduce_scatter_block(F f, const T *senddata, T &recvdata) const {
      MPI_Request req;
      MPI_Ireduce_scatter_block(senddata, &recvdata, 1,
                                detail::datatype_traits<T>::get_datatype(),
                                detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest ireduce_scatter_block(F f, const T *senddata, T *recvdata,
                                   const contiguous_layout<T> &l) const {
      MPI_Request req;
      MPI_Ireduce_scatter_block(senddata, recvdata, l.size(),
                                detail::datatype_traits<T>::get_datatype(),
                                detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    // === reduce-scatter ===
    // --- blocking reduce-scatter ---
    template<typename T, typename F>
    void reduce_scatter(F f, const T *senddata, T *recvdata,
                        const contiguous_layouts<T> &recvcounts) const {
      MPI_Reduce_scatter(senddata, recvdata, recvcounts.sizes(),
                         detail::datatype_traits<T>::get_datatype(),
                         detail::get_op<T, F>(f).mpi_op, comm_);
    }

    // --- non-blocking reduce-scatter ---
    template<typename T, typename F>
    irequest ireduce_scatter(F f, const T *senddata, T *recvdata,
                             contiguous_layouts<T> &recvcounts) const {
      MPI_Request req;
      MPI_Ireduce_scatter(senddata, recvdata, recvcounts.sizes(),
                          detail::datatype_traits<T>::get_datatype(),
                          detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    // === scan ===
    // --- blocking scan ---
    template<typename T, typename F>
    void scan(F f, const T &senddata, T &recvdata) const {
      MPI_Scan(&senddata, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
               detail::get_op<T, F>(f).mpi_op, comm_);
    }

    template<typename T, typename F>
    void scan(F f, const T *senddata, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Scan(senddata, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
               detail::get_op<T, F>(f).mpi_op, comm_);
    }

    // --- non-blocking scan ---
    template<typename T, typename F>
    irequest iscan(F f, const T &senddata, T &recvdata) const {
      MPI_Request req;
      MPI_Iscan(&senddata, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest iscan(F f, const T *senddata, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Request req;
      MPI_Iscan(senddata, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    // --- blocking scan, in place ---
    template<typename T, typename F>
    void scan(F f, T &recvdata) const {
      MPI_Scan(MPI_IN_PLACE, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
               detail::get_op<T, F>(f).mpi_op, comm_);
    }

    template<typename T, typename F>
    void scan(F f, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Scan(MPI_IN_PLACE, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
               detail::get_op<T, F>(f).mpi_op, comm_);
    }

    // --- non-blocking scan, in place ---
    template<typename T, typename F>
    irequest iscan(F f, T &recvdata) const {
      MPI_Request req;
      MPI_Iscan(MPI_IN_PLACE, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest iscan(F f, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Request req;
      MPI_Iscan(MPI_IN_PLACE, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    // === exscan ===
    // --- blocking exscan ---
    template<typename T, typename F>
    void exscan(F f, const T &senddata, T &recvdata) const {
      MPI_Exscan(&senddata, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                 detail::get_op<T, F>(f).mpi_op, comm_);
    }

    template<typename T, typename F>
    void exscan(F f, const T *senddata, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Exscan(senddata, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                 detail::get_op<T, F>(f).mpi_op, comm_);
    }

    // --- non-blocking exscan ---
    template<typename T, typename F>
    irequest iexscan(F f, const T &senddata, T &recvdata) const {
      MPI_Request req;
      MPI_Iexscan(&senddata, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                  detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest iexscan(F f, const T *senddata, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Request req;
      MPI_Iexscan(senddata, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                  detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    // --- blocking exscan, in place ---
    template<typename T, typename F>
    void exscan(F f, T &recvdata) const {
      MPI_Exscan(MPI_IN_PLACE, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                 detail::get_op<T, F>(f).mpi_op, comm_);
    }

    template<typename T, typename F>
    void exscan(F f, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Exscan(MPI_IN_PLACE, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                 detail::get_op<T, F>(f).mpi_op, comm_);
    }

    // --- non-blocking exscan, in place ---
    template<typename T, typename F>
    irequest iexscan(F f, T &recvdata) const {
      MPI_Request req;
      MPI_Iexscan(MPI_IN_PLACE, &recvdata, 1, detail::datatype_traits<T>::get_datatype(),
                  detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }

    template<typename T, typename F>
    irequest iexscan(F f, T *recvdata, const contiguous_layout<T> &l) const {
      MPI_Request req;
      MPI_Iexscan(MPI_IN_PLACE, recvdata, l.size(), detail::datatype_traits<T>::get_datatype(),
                  detail::get_op<T, F>(f).mpi_op, comm_, &req);
      return impl::irequest(req);
    }
  };  // namespace mpl

  //--------------------------------------------------------------------

  inline group::group(const group &other) { MPI_Group_excl(other.gr_, 0, nullptr, &gr_); }

  inline group::group(const communicator &comm) { MPI_Comm_group(comm.comm_, &gr_); }

  inline group::group(group::Union_tag, const group &other_1, const group &other_2) {
    MPI_Group_union(other_1.gr_, other_2.gr_, &gr_);
  }

  inline group::group(group::intersection_tag, const group &other_1, const group &other_2) {
    MPI_Group_intersection(other_1.gr_, other_2.gr_, &gr_);
  }

  inline group::group(group::difference_tag, const group &other_1, const group &other_2) {
    MPI_Group_difference(other_1.gr_, other_2.gr_, &gr_);
  }

  inline group::group(group::include_tag, const group &other, const ranks &rank) {
    MPI_Group_incl(other.gr_, rank.size(), rank(), &gr_);
  }

  inline group::group(group::exclude_tag, const group &other, const ranks &rank) {
    MPI_Group_excl(other.gr_, rank.size(), rank(), &gr_);
  }

}  // namespace mpl

#endif
