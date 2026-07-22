#ifndef SRC_TRACE_PROCESSOR_TABLES_JIT_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_JIT_TABLES_PY_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/ref_counted.h"
#include "src/trace_processor/containers/bit_vector.h"
#include "src/trace_processor/containers/row_map.h"
#include "src/trace_processor/containers/string_pool.h"
#include "src/trace_processor/db/column/arrangement_overlay.h"
#include "src/trace_processor/db/column/data_layer.h"
#include "src/trace_processor/db/column/dense_null_overlay.h"
#include "src/trace_processor/db/column/numeric_storage.h"
#include "src/trace_processor/db/column/id_storage.h"
#include "src/trace_processor/db/column/null_overlay.h"
#include "src/trace_processor/db/column/range_overlay.h"
#include "src/trace_processor/db/column/selector_overlay.h"
#include "src/trace_processor/db/column/set_id_storage.h"
#include "src/trace_processor/db/column/string_storage.h"
#include "src/trace_processor/db/column/types.h"
#include "src/trace_processor/db/column_storage.h"
#include "src/trace_processor/db/column.h"
#include "src/trace_processor/db/table.h"
#include "src/trace_processor/db/typed_column.h"
#include "src/trace_processor/db/typed_column_internal.h"
#include "src/trace_processor/tables/macros_internal.h"

#include "src/trace_processor/tables/profiler_tables_py.h"

namespace perfetto::trace_processor::tables {

class JitCodeTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t create_ts = 2;
    static constexpr uint32_t estimated_delete_ts = 3;
    static constexpr uint32_t utid = 4;
    static constexpr uint32_t start_address = 5;
    static constexpr uint32_t size = 6;
    static constexpr uint32_t function_name = 7;
    static constexpr uint32_t native_code_base64 = 8;
  };
  struct ColumnType {
    using id = IdColumn<JitCodeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using create_ts = TypedColumn<int64_t>;
    using estimated_delete_ts = TypedColumn<std::optional<int64_t>>;
    using utid = TypedColumn<uint32_t>;
    using start_address = TypedColumn<int64_t>;
    using size = TypedColumn<int64_t>;
    using function_name = TypedColumn<StringPool::Id>;
    using native_code_base64 = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_create_ts = {},
        std::optional<int64_t> in_estimated_delete_ts = {},
        uint32_t in_utid = {},
        int64_t in_start_address = {},
        int64_t in_size = {},
        StringPool::Id in_function_name = {},
        std::optional<StringPool::Id> in_native_code_base64 = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          create_ts(in_create_ts),
          estimated_delete_ts(in_estimated_delete_ts),
          utid(in_utid),
          start_address(in_start_address),
          size(in_size),
          function_name(in_function_name),
          native_code_base64(in_native_code_base64) {
      type_ = "__intrinsic_jit_code";
    }
    int64_t create_ts;
    std::optional<int64_t> estimated_delete_ts;
    uint32_t utid;
    int64_t start_address;
    int64_t size;
    StringPool::Id function_name;
    std::optional<StringPool::Id> native_code_base64;

    bool operator==(const JitCodeTable::Row& other) const {
      return type() == other.type() && ColumnType::create_ts::Equals(create_ts, other.create_ts) &&
       ColumnType::estimated_delete_ts::Equals(estimated_delete_ts, other.estimated_delete_ts) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::start_address::Equals(start_address, other.start_address) &&
       ColumnType::size::Equals(size, other.size) &&
       ColumnType::function_name::Equals(function_name, other.function_name) &&
       ColumnType::native_code_base64::Equals(native_code_base64, other.native_code_base64);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t create_ts = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::create_ts::default_flags();
    static constexpr uint32_t estimated_delete_ts = ColumnType::estimated_delete_ts::default_flags();
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
    static constexpr uint32_t start_address = ColumnType::start_address::default_flags();
    static constexpr uint32_t size = ColumnType::size::default_flags();
    static constexpr uint32_t function_name = ColumnType::function_name::default_flags();
    static constexpr uint32_t native_code_base64 = ColumnType::native_code_base64::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      JitCodeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    JitCodeTable, RowNumber> {
   public:
    ConstRowReference(const JitCodeTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::create_ts::type create_ts() const {
      return table_->create_ts()[row_number_];
    }
    ColumnType::estimated_delete_ts::type estimated_delete_ts() const {
      return table_->estimated_delete_ts()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
    ColumnType::start_address::type start_address() const {
      return table_->start_address()[row_number_];
    }
    ColumnType::size::type size() const {
      return table_->size()[row_number_];
    }
    ColumnType::function_name::type function_name() const {
      return table_->function_name()[row_number_];
    }
    ColumnType::native_code_base64::type native_code_base64() const {
      return table_->native_code_base64()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const JitCodeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_create_ts(
        ColumnType::create_ts::non_optional_type v) {
      return mutable_table()->mutable_create_ts()->Set(row_number_, v);
    }
    void set_estimated_delete_ts(
        ColumnType::estimated_delete_ts::non_optional_type v) {
      return mutable_table()->mutable_estimated_delete_ts()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_start_address(
        ColumnType::start_address::non_optional_type v) {
      return mutable_table()->mutable_start_address()->Set(row_number_, v);
    }
    void set_size(
        ColumnType::size::non_optional_type v) {
      return mutable_table()->mutable_size()->Set(row_number_, v);
    }
    void set_function_name(
        ColumnType::function_name::non_optional_type v) {
      return mutable_table()->mutable_function_name()->Set(row_number_, v);
    }
    void set_native_code_base64(
        ColumnType::native_code_base64::non_optional_type v) {
      return mutable_table()->mutable_native_code_base64()->Set(row_number_, v);
    }

   private:
    JitCodeTable* mutable_table() const {
      return const_cast<JitCodeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, JitCodeTable, RowNumber, ConstRowReference> {
   public:
    ColumnType::id::type id() const {
      const auto& col = table_->id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::type::type type() const {
      const auto& col = table_->type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::create_ts::type create_ts() const {
      const auto& col = table_->create_ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::estimated_delete_ts::type estimated_delete_ts() const {
      const auto& col = table_->estimated_delete_ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::start_address::type start_address() const {
      const auto& col = table_->start_address();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::size::type size() const {
      const auto& col = table_->size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::function_name::type function_name() const {
      const auto& col = table_->function_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::native_code_base64::type native_code_base64() const {
      const auto& col = table_->native_code_base64();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const JitCodeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class JitCodeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, JitCodeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class JitCodeTable;

     explicit Iterator(JitCodeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     JitCodeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      JitCodeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "create_ts", &self->create_ts_, ColumnFlag::create_ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "estimated_delete_ts", &self->estimated_delete_ts_, ColumnFlag::estimated_delete_ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "start_address", &self->start_address_, ColumnFlag::start_address,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "size", &self->size_, ColumnFlag::size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "function_name", &self->function_name_, ColumnFlag::function_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "native_code_base64", &self->native_code_base64_, ColumnFlag::native_code_base64,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit JitCodeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        create_ts_(ColumnStorage<ColumnType::create_ts::stored_type>::Create<false>()),
        estimated_delete_ts_(ColumnStorage<ColumnType::estimated_delete_ts::stored_type>::Create<false>()),
        utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>()),
        start_address_(ColumnStorage<ColumnType::start_address::stored_type>::Create<false>()),
        size_(ColumnStorage<ColumnType::size::stored_type>::Create<false>()),
        function_name_(ColumnStorage<ColumnType::function_name::stored_type>::Create<false>()),
        native_code_base64_(ColumnStorage<ColumnType::native_code_base64::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        create_ts_storage_layer_(
        new column::NumericStorage<ColumnType::create_ts::non_optional_stored_type>(
          &create_ts_.vector(),
          ColumnTypeHelper<ColumnType::create_ts::stored_type>::ToColumnType(),
          true)),
        estimated_delete_ts_storage_layer_(
          new column::NumericStorage<ColumnType::estimated_delete_ts::non_optional_stored_type>(
            &estimated_delete_ts_.non_null_vector(),
            ColumnTypeHelper<ColumnType::estimated_delete_ts::stored_type>::ToColumnType(),
            false)),
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false)),
        start_address_storage_layer_(
        new column::NumericStorage<ColumnType::start_address::non_optional_stored_type>(
          &start_address_.vector(),
          ColumnTypeHelper<ColumnType::start_address::stored_type>::ToColumnType(),
          false)),
        size_storage_layer_(
        new column::NumericStorage<ColumnType::size::non_optional_stored_type>(
          &size_.vector(),
          ColumnTypeHelper<ColumnType::size::stored_type>::ToColumnType(),
          false)),
        function_name_storage_layer_(
          new column::StringStorage(string_pool(), &function_name_.vector())),
        native_code_base64_storage_layer_(
          new column::StringStorage(string_pool(), &native_code_base64_.vector()))
,
        estimated_delete_ts_null_layer_(new column::NullOverlay(estimated_delete_ts_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::create_ts::stored_type>(
          ColumnFlag::create_ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::estimated_delete_ts::stored_type>(
          ColumnFlag::estimated_delete_ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::start_address::stored_type>(
          ColumnFlag::start_address),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::size::stored_type>(
          ColumnFlag::size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::function_name::stored_type>(
          ColumnFlag::function_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::native_code_base64::stored_type>(
          ColumnFlag::native_code_base64),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,create_ts_storage_layer_,estimated_delete_ts_storage_layer_,utid_storage_layer_,start_address_storage_layer_,size_storage_layer_,function_name_storage_layer_,native_code_base64_storage_layer_},
      {{},{},{},estimated_delete_ts_null_layer_,{},{},{},{},{}});
  }
  ~JitCodeTable() override;

  static const char* Name() { return "__intrinsic_jit_code"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "create_ts", ColumnType::create_ts::SqlValueType(), false,
        true,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "estimated_delete_ts", ColumnType::estimated_delete_ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "start_address", ColumnType::start_address::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "size", ColumnType::size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "function_name", ColumnType::function_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "native_code_base64", ColumnType::native_code_base64::SqlValueType(), false,
        false,
        false,
        false});
    return schema;
  }

  ConstIterator IterateRows() const {
    return ConstIterator(this, Table::IterateRows());
  }

  Iterator IterateRows() { return Iterator(this, Table::IterateRows()); }

  ConstIterator FilterToIterator(const Query& q) const {
    return ConstIterator(
      this, ApplyAndIterateRows(QueryToRowMap(q)));
  }

  Iterator FilterToIterator(const Query& q) {
    return Iterator(this, ApplyAndIterateRows(QueryToRowMap(q)));
  }

  void ShrinkToFit() {
    type_.ShrinkToFit();
    create_ts_.ShrinkToFit();
    estimated_delete_ts_.ShrinkToFit();
    utid_.ShrinkToFit();
    start_address_.ShrinkToFit();
    size_.ShrinkToFit();
    function_name_.ShrinkToFit();
    native_code_base64_.ShrinkToFit();
  }

  std::optional<ConstRowReference> FindById(Id find_id) const {
    std::optional<uint32_t> row = id().IndexOf(find_id);
    return row ? std::make_optional(ConstRowReference(this, *row))
               : std::nullopt;
  }

  std::optional<RowReference> FindById(Id find_id) {
    std::optional<uint32_t> row = id().IndexOf(find_id);
    return row ? std::make_optional(RowReference(this, *row)) : std::nullopt;
  }

  IdAndRow Insert(const Row& row) {
    uint32_t row_number = row_count();
    Id id = Id{row_number};
    type_.Append(string_pool()->InternString(row.type()));
    mutable_create_ts()->Append(row.create_ts);
    mutable_estimated_delete_ts()->Append(row.estimated_delete_ts);
    mutable_utid()->Append(row.utid);
    mutable_start_address()->Append(row.start_address);
    mutable_size()->Append(row.size);
    mutable_function_name()->Append(row.function_name);
    mutable_native_code_base64()->Append(row.native_code_base64);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<JitCodeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& create_ts() const {
    return static_cast<const ColumnType::create_ts&>(columns()[ColumnIndex::create_ts]);
  }
  const TypedColumn<std::optional<int64_t>>& estimated_delete_ts() const {
    return static_cast<const ColumnType::estimated_delete_ts&>(columns()[ColumnIndex::estimated_delete_ts]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }
  const TypedColumn<int64_t>& start_address() const {
    return static_cast<const ColumnType::start_address&>(columns()[ColumnIndex::start_address]);
  }
  const TypedColumn<int64_t>& size() const {
    return static_cast<const ColumnType::size&>(columns()[ColumnIndex::size]);
  }
  const TypedColumn<StringPool::Id>& function_name() const {
    return static_cast<const ColumnType::function_name&>(columns()[ColumnIndex::function_name]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& native_code_base64() const {
    return static_cast<const ColumnType::native_code_base64&>(columns()[ColumnIndex::native_code_base64]);
  }

  TypedColumn<int64_t>* mutable_create_ts() {
    return static_cast<ColumnType::create_ts*>(
        GetColumn(ColumnIndex::create_ts));
  }
  TypedColumn<std::optional<int64_t>>* mutable_estimated_delete_ts() {
    return static_cast<ColumnType::estimated_delete_ts*>(
        GetColumn(ColumnIndex::estimated_delete_ts));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<int64_t>* mutable_start_address() {
    return static_cast<ColumnType::start_address*>(
        GetColumn(ColumnIndex::start_address));
  }
  TypedColumn<int64_t>* mutable_size() {
    return static_cast<ColumnType::size*>(
        GetColumn(ColumnIndex::size));
  }
  TypedColumn<StringPool::Id>* mutable_function_name() {
    return static_cast<ColumnType::function_name*>(
        GetColumn(ColumnIndex::function_name));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_native_code_base64() {
    return static_cast<ColumnType::native_code_base64*>(
        GetColumn(ColumnIndex::native_code_base64));
  }

 private:
  
  
  ColumnStorage<ColumnType::create_ts::stored_type> create_ts_;
  ColumnStorage<ColumnType::estimated_delete_ts::stored_type> estimated_delete_ts_;
  ColumnStorage<ColumnType::utid::stored_type> utid_;
  ColumnStorage<ColumnType::start_address::stored_type> start_address_;
  ColumnStorage<ColumnType::size::stored_type> size_;
  ColumnStorage<ColumnType::function_name::stored_type> function_name_;
  ColumnStorage<ColumnType::native_code_base64::stored_type> native_code_base64_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> create_ts_storage_layer_;
  RefPtr<column::StorageLayer> estimated_delete_ts_storage_layer_;
  RefPtr<column::StorageLayer> utid_storage_layer_;
  RefPtr<column::StorageLayer> start_address_storage_layer_;
  RefPtr<column::StorageLayer> size_storage_layer_;
  RefPtr<column::StorageLayer> function_name_storage_layer_;
  RefPtr<column::StorageLayer> native_code_base64_storage_layer_;

  RefPtr<column::OverlayLayer> estimated_delete_ts_null_layer_;
};
  

class JitFrameTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 4;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t jit_code_id = 2;
    static constexpr uint32_t frame_id = 3;
  };
  struct ColumnType {
    using id = IdColumn<JitFrameTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using jit_code_id = TypedColumn<JitCodeTable::Id>;
    using frame_id = TypedColumn<StackProfileFrameTable::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(JitCodeTable::Id in_jit_code_id = {},
        StackProfileFrameTable::Id in_frame_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          jit_code_id(in_jit_code_id),
          frame_id(in_frame_id) {
      type_ = "__intrinsic_jit_frame";
    }
    JitCodeTable::Id jit_code_id;
    StackProfileFrameTable::Id frame_id;

    bool operator==(const JitFrameTable::Row& other) const {
      return type() == other.type() && ColumnType::jit_code_id::Equals(jit_code_id, other.jit_code_id) &&
       ColumnType::frame_id::Equals(frame_id, other.frame_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t jit_code_id = ColumnType::jit_code_id::default_flags();
    static constexpr uint32_t frame_id = ColumnType::frame_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      JitFrameTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    JitFrameTable, RowNumber> {
   public:
    ConstRowReference(const JitFrameTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::jit_code_id::type jit_code_id() const {
      return table_->jit_code_id()[row_number_];
    }
    ColumnType::frame_id::type frame_id() const {
      return table_->frame_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const JitFrameTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_jit_code_id(
        ColumnType::jit_code_id::non_optional_type v) {
      return mutable_table()->mutable_jit_code_id()->Set(row_number_, v);
    }
    void set_frame_id(
        ColumnType::frame_id::non_optional_type v) {
      return mutable_table()->mutable_frame_id()->Set(row_number_, v);
    }

   private:
    JitFrameTable* mutable_table() const {
      return const_cast<JitFrameTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, JitFrameTable, RowNumber, ConstRowReference> {
   public:
    ColumnType::id::type id() const {
      const auto& col = table_->id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::type::type type() const {
      const auto& col = table_->type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::jit_code_id::type jit_code_id() const {
      const auto& col = table_->jit_code_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::frame_id::type frame_id() const {
      const auto& col = table_->frame_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const JitFrameTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class JitFrameTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, JitFrameTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class JitFrameTable;

     explicit Iterator(JitFrameTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     JitFrameTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      JitFrameTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "jit_code_id", &self->jit_code_id_, ColumnFlag::jit_code_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "frame_id", &self->frame_id_, ColumnFlag::frame_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit JitFrameTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        jit_code_id_(ColumnStorage<ColumnType::jit_code_id::stored_type>::Create<false>()),
        frame_id_(ColumnStorage<ColumnType::frame_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        jit_code_id_storage_layer_(
        new column::NumericStorage<ColumnType::jit_code_id::non_optional_stored_type>(
          &jit_code_id_.vector(),
          ColumnTypeHelper<ColumnType::jit_code_id::stored_type>::ToColumnType(),
          false)),
        frame_id_storage_layer_(
        new column::NumericStorage<ColumnType::frame_id::non_optional_stored_type>(
          &frame_id_.vector(),
          ColumnTypeHelper<ColumnType::frame_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::jit_code_id::stored_type>(
          ColumnFlag::jit_code_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::frame_id::stored_type>(
          ColumnFlag::frame_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,jit_code_id_storage_layer_,frame_id_storage_layer_},
      {{},{},{},{}});
  }
  ~JitFrameTable() override;

  static const char* Name() { return "__intrinsic_jit_frame"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "jit_code_id", ColumnType::jit_code_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "frame_id", ColumnType::frame_id::SqlValueType(), false,
        false,
        false,
        false});
    return schema;
  }

  ConstIterator IterateRows() const {
    return ConstIterator(this, Table::IterateRows());
  }

  Iterator IterateRows() { return Iterator(this, Table::IterateRows()); }

  ConstIterator FilterToIterator(const Query& q) const {
    return ConstIterator(
      this, ApplyAndIterateRows(QueryToRowMap(q)));
  }

  Iterator FilterToIterator(const Query& q) {
    return Iterator(this, ApplyAndIterateRows(QueryToRowMap(q)));
  }

  void ShrinkToFit() {
    type_.ShrinkToFit();
    jit_code_id_.ShrinkToFit();
    frame_id_.ShrinkToFit();
  }

  std::optional<ConstRowReference> FindById(Id find_id) const {
    std::optional<uint32_t> row = id().IndexOf(find_id);
    return row ? std::make_optional(ConstRowReference(this, *row))
               : std::nullopt;
  }

  std::optional<RowReference> FindById(Id find_id) {
    std::optional<uint32_t> row = id().IndexOf(find_id);
    return row ? std::make_optional(RowReference(this, *row)) : std::nullopt;
  }

  IdAndRow Insert(const Row& row) {
    uint32_t row_number = row_count();
    Id id = Id{row_number};
    type_.Append(string_pool()->InternString(row.type()));
    mutable_jit_code_id()->Append(row.jit_code_id);
    mutable_frame_id()->Append(row.frame_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<JitFrameTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<JitCodeTable::Id>& jit_code_id() const {
    return static_cast<const ColumnType::jit_code_id&>(columns()[ColumnIndex::jit_code_id]);
  }
  const TypedColumn<StackProfileFrameTable::Id>& frame_id() const {
    return static_cast<const ColumnType::frame_id&>(columns()[ColumnIndex::frame_id]);
  }

  TypedColumn<JitCodeTable::Id>* mutable_jit_code_id() {
    return static_cast<ColumnType::jit_code_id*>(
        GetColumn(ColumnIndex::jit_code_id));
  }
  TypedColumn<StackProfileFrameTable::Id>* mutable_frame_id() {
    return static_cast<ColumnType::frame_id*>(
        GetColumn(ColumnIndex::frame_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::jit_code_id::stored_type> jit_code_id_;
  ColumnStorage<ColumnType::frame_id::stored_type> frame_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> jit_code_id_storage_layer_;
  RefPtr<column::StorageLayer> frame_id_storage_layer_;

  
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_JIT_TABLES_PY_H_
