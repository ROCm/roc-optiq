#ifndef SRC_TRACE_PROCESSOR_TABLES_V8_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_V8_TABLES_PY_H_

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

#include "src/trace_processor/tables/jit_tables_py.h"

namespace perfetto::trace_processor::tables {

class V8IsolateTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 10;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t upid = 2;
    static constexpr uint32_t internal_isolate_id = 3;
    static constexpr uint32_t embedded_blob_code_start_address = 4;
    static constexpr uint32_t embedded_blob_code_size = 5;
    static constexpr uint32_t code_range_base_address = 6;
    static constexpr uint32_t code_range_size = 7;
    static constexpr uint32_t shared_code_range = 8;
    static constexpr uint32_t embedded_blob_code_copy_start_address = 9;
  };
  struct ColumnType {
    using id = IdColumn<V8IsolateTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using upid = TypedColumn<uint32_t>;
    using internal_isolate_id = TypedColumn<int32_t>;
    using embedded_blob_code_start_address = TypedColumn<int64_t>;
    using embedded_blob_code_size = TypedColumn<int64_t>;
    using code_range_base_address = TypedColumn<std::optional<int64_t>>;
    using code_range_size = TypedColumn<std::optional<int64_t>>;
    using shared_code_range = TypedColumn<std::optional<uint32_t>>;
    using embedded_blob_code_copy_start_address = TypedColumn<std::optional<int64_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_upid = {},
        int32_t in_internal_isolate_id = {},
        int64_t in_embedded_blob_code_start_address = {},
        int64_t in_embedded_blob_code_size = {},
        std::optional<int64_t> in_code_range_base_address = {},
        std::optional<int64_t> in_code_range_size = {},
        std::optional<uint32_t> in_shared_code_range = {},
        std::optional<int64_t> in_embedded_blob_code_copy_start_address = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          upid(in_upid),
          internal_isolate_id(in_internal_isolate_id),
          embedded_blob_code_start_address(in_embedded_blob_code_start_address),
          embedded_blob_code_size(in_embedded_blob_code_size),
          code_range_base_address(in_code_range_base_address),
          code_range_size(in_code_range_size),
          shared_code_range(in_shared_code_range),
          embedded_blob_code_copy_start_address(in_embedded_blob_code_copy_start_address) {
      type_ = "__intrinsic_v8_isolate";
    }
    uint32_t upid;
    int32_t internal_isolate_id;
    int64_t embedded_blob_code_start_address;
    int64_t embedded_blob_code_size;
    std::optional<int64_t> code_range_base_address;
    std::optional<int64_t> code_range_size;
    std::optional<uint32_t> shared_code_range;
    std::optional<int64_t> embedded_blob_code_copy_start_address;

    bool operator==(const V8IsolateTable::Row& other) const {
      return type() == other.type() && ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::internal_isolate_id::Equals(internal_isolate_id, other.internal_isolate_id) &&
       ColumnType::embedded_blob_code_start_address::Equals(embedded_blob_code_start_address, other.embedded_blob_code_start_address) &&
       ColumnType::embedded_blob_code_size::Equals(embedded_blob_code_size, other.embedded_blob_code_size) &&
       ColumnType::code_range_base_address::Equals(code_range_base_address, other.code_range_base_address) &&
       ColumnType::code_range_size::Equals(code_range_size, other.code_range_size) &&
       ColumnType::shared_code_range::Equals(shared_code_range, other.shared_code_range) &&
       ColumnType::embedded_blob_code_copy_start_address::Equals(embedded_blob_code_copy_start_address, other.embedded_blob_code_copy_start_address);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
    static constexpr uint32_t internal_isolate_id = ColumnType::internal_isolate_id::default_flags();
    static constexpr uint32_t embedded_blob_code_start_address = ColumnType::embedded_blob_code_start_address::default_flags();
    static constexpr uint32_t embedded_blob_code_size = ColumnType::embedded_blob_code_size::default_flags();
    static constexpr uint32_t code_range_base_address = ColumnType::code_range_base_address::default_flags();
    static constexpr uint32_t code_range_size = ColumnType::code_range_size::default_flags();
    static constexpr uint32_t shared_code_range = ColumnType::shared_code_range::default_flags();
    static constexpr uint32_t embedded_blob_code_copy_start_address = ColumnType::embedded_blob_code_copy_start_address::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      V8IsolateTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    V8IsolateTable, RowNumber> {
   public:
    ConstRowReference(const V8IsolateTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
    ColumnType::internal_isolate_id::type internal_isolate_id() const {
      return table_->internal_isolate_id()[row_number_];
    }
    ColumnType::embedded_blob_code_start_address::type embedded_blob_code_start_address() const {
      return table_->embedded_blob_code_start_address()[row_number_];
    }
    ColumnType::embedded_blob_code_size::type embedded_blob_code_size() const {
      return table_->embedded_blob_code_size()[row_number_];
    }
    ColumnType::code_range_base_address::type code_range_base_address() const {
      return table_->code_range_base_address()[row_number_];
    }
    ColumnType::code_range_size::type code_range_size() const {
      return table_->code_range_size()[row_number_];
    }
    ColumnType::shared_code_range::type shared_code_range() const {
      return table_->shared_code_range()[row_number_];
    }
    ColumnType::embedded_blob_code_copy_start_address::type embedded_blob_code_copy_start_address() const {
      return table_->embedded_blob_code_copy_start_address()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const V8IsolateTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_internal_isolate_id(
        ColumnType::internal_isolate_id::non_optional_type v) {
      return mutable_table()->mutable_internal_isolate_id()->Set(row_number_, v);
    }
    void set_embedded_blob_code_start_address(
        ColumnType::embedded_blob_code_start_address::non_optional_type v) {
      return mutable_table()->mutable_embedded_blob_code_start_address()->Set(row_number_, v);
    }
    void set_embedded_blob_code_size(
        ColumnType::embedded_blob_code_size::non_optional_type v) {
      return mutable_table()->mutable_embedded_blob_code_size()->Set(row_number_, v);
    }
    void set_code_range_base_address(
        ColumnType::code_range_base_address::non_optional_type v) {
      return mutable_table()->mutable_code_range_base_address()->Set(row_number_, v);
    }
    void set_code_range_size(
        ColumnType::code_range_size::non_optional_type v) {
      return mutable_table()->mutable_code_range_size()->Set(row_number_, v);
    }
    void set_shared_code_range(
        ColumnType::shared_code_range::non_optional_type v) {
      return mutable_table()->mutable_shared_code_range()->Set(row_number_, v);
    }
    void set_embedded_blob_code_copy_start_address(
        ColumnType::embedded_blob_code_copy_start_address::non_optional_type v) {
      return mutable_table()->mutable_embedded_blob_code_copy_start_address()->Set(row_number_, v);
    }

   private:
    V8IsolateTable* mutable_table() const {
      return const_cast<V8IsolateTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, V8IsolateTable, RowNumber, ConstRowReference> {
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
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::internal_isolate_id::type internal_isolate_id() const {
      const auto& col = table_->internal_isolate_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::embedded_blob_code_start_address::type embedded_blob_code_start_address() const {
      const auto& col = table_->embedded_blob_code_start_address();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::embedded_blob_code_size::type embedded_blob_code_size() const {
      const auto& col = table_->embedded_blob_code_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::code_range_base_address::type code_range_base_address() const {
      const auto& col = table_->code_range_base_address();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::code_range_size::type code_range_size() const {
      const auto& col = table_->code_range_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::shared_code_range::type shared_code_range() const {
      const auto& col = table_->shared_code_range();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::embedded_blob_code_copy_start_address::type embedded_blob_code_copy_start_address() const {
      const auto& col = table_->embedded_blob_code_copy_start_address();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const V8IsolateTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class V8IsolateTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, V8IsolateTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class V8IsolateTable;

     explicit Iterator(V8IsolateTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     V8IsolateTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      V8IsolateTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "internal_isolate_id", &self->internal_isolate_id_, ColumnFlag::internal_isolate_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "embedded_blob_code_start_address", &self->embedded_blob_code_start_address_, ColumnFlag::embedded_blob_code_start_address,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "embedded_blob_code_size", &self->embedded_blob_code_size_, ColumnFlag::embedded_blob_code_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "code_range_base_address", &self->code_range_base_address_, ColumnFlag::code_range_base_address,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "code_range_size", &self->code_range_size_, ColumnFlag::code_range_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "shared_code_range", &self->shared_code_range_, ColumnFlag::shared_code_range,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "embedded_blob_code_copy_start_address", &self->embedded_blob_code_copy_start_address_, ColumnFlag::embedded_blob_code_copy_start_address,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit V8IsolateTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        internal_isolate_id_(ColumnStorage<ColumnType::internal_isolate_id::stored_type>::Create<false>()),
        embedded_blob_code_start_address_(ColumnStorage<ColumnType::embedded_blob_code_start_address::stored_type>::Create<false>()),
        embedded_blob_code_size_(ColumnStorage<ColumnType::embedded_blob_code_size::stored_type>::Create<false>()),
        code_range_base_address_(ColumnStorage<ColumnType::code_range_base_address::stored_type>::Create<false>()),
        code_range_size_(ColumnStorage<ColumnType::code_range_size::stored_type>::Create<false>()),
        shared_code_range_(ColumnStorage<ColumnType::shared_code_range::stored_type>::Create<false>()),
        embedded_blob_code_copy_start_address_(ColumnStorage<ColumnType::embedded_blob_code_copy_start_address::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false)),
        internal_isolate_id_storage_layer_(
        new column::NumericStorage<ColumnType::internal_isolate_id::non_optional_stored_type>(
          &internal_isolate_id_.vector(),
          ColumnTypeHelper<ColumnType::internal_isolate_id::stored_type>::ToColumnType(),
          false)),
        embedded_blob_code_start_address_storage_layer_(
        new column::NumericStorage<ColumnType::embedded_blob_code_start_address::non_optional_stored_type>(
          &embedded_blob_code_start_address_.vector(),
          ColumnTypeHelper<ColumnType::embedded_blob_code_start_address::stored_type>::ToColumnType(),
          false)),
        embedded_blob_code_size_storage_layer_(
        new column::NumericStorage<ColumnType::embedded_blob_code_size::non_optional_stored_type>(
          &embedded_blob_code_size_.vector(),
          ColumnTypeHelper<ColumnType::embedded_blob_code_size::stored_type>::ToColumnType(),
          false)),
        code_range_base_address_storage_layer_(
          new column::NumericStorage<ColumnType::code_range_base_address::non_optional_stored_type>(
            &code_range_base_address_.non_null_vector(),
            ColumnTypeHelper<ColumnType::code_range_base_address::stored_type>::ToColumnType(),
            false)),
        code_range_size_storage_layer_(
          new column::NumericStorage<ColumnType::code_range_size::non_optional_stored_type>(
            &code_range_size_.non_null_vector(),
            ColumnTypeHelper<ColumnType::code_range_size::stored_type>::ToColumnType(),
            false)),
        shared_code_range_storage_layer_(
          new column::NumericStorage<ColumnType::shared_code_range::non_optional_stored_type>(
            &shared_code_range_.non_null_vector(),
            ColumnTypeHelper<ColumnType::shared_code_range::stored_type>::ToColumnType(),
            false)),
        embedded_blob_code_copy_start_address_storage_layer_(
          new column::NumericStorage<ColumnType::embedded_blob_code_copy_start_address::non_optional_stored_type>(
            &embedded_blob_code_copy_start_address_.non_null_vector(),
            ColumnTypeHelper<ColumnType::embedded_blob_code_copy_start_address::stored_type>::ToColumnType(),
            false))
,
        code_range_base_address_null_layer_(new column::NullOverlay(code_range_base_address_.bv())),
        code_range_size_null_layer_(new column::NullOverlay(code_range_size_.bv())),
        shared_code_range_null_layer_(new column::NullOverlay(shared_code_range_.bv())),
        embedded_blob_code_copy_start_address_null_layer_(new column::NullOverlay(embedded_blob_code_copy_start_address_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::internal_isolate_id::stored_type>(
          ColumnFlag::internal_isolate_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::embedded_blob_code_start_address::stored_type>(
          ColumnFlag::embedded_blob_code_start_address),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::embedded_blob_code_size::stored_type>(
          ColumnFlag::embedded_blob_code_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::code_range_base_address::stored_type>(
          ColumnFlag::code_range_base_address),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::code_range_size::stored_type>(
          ColumnFlag::code_range_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::shared_code_range::stored_type>(
          ColumnFlag::shared_code_range),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::embedded_blob_code_copy_start_address::stored_type>(
          ColumnFlag::embedded_blob_code_copy_start_address),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,upid_storage_layer_,internal_isolate_id_storage_layer_,embedded_blob_code_start_address_storage_layer_,embedded_blob_code_size_storage_layer_,code_range_base_address_storage_layer_,code_range_size_storage_layer_,shared_code_range_storage_layer_,embedded_blob_code_copy_start_address_storage_layer_},
      {{},{},{},{},{},{},code_range_base_address_null_layer_,code_range_size_null_layer_,shared_code_range_null_layer_,embedded_blob_code_copy_start_address_null_layer_});
  }
  ~V8IsolateTable() override;

  static const char* Name() { return "__intrinsic_v8_isolate"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "upid", ColumnType::upid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "internal_isolate_id", ColumnType::internal_isolate_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "embedded_blob_code_start_address", ColumnType::embedded_blob_code_start_address::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "embedded_blob_code_size", ColumnType::embedded_blob_code_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "code_range_base_address", ColumnType::code_range_base_address::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "code_range_size", ColumnType::code_range_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "shared_code_range", ColumnType::shared_code_range::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "embedded_blob_code_copy_start_address", ColumnType::embedded_blob_code_copy_start_address::SqlValueType(), false,
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
    upid_.ShrinkToFit();
    internal_isolate_id_.ShrinkToFit();
    embedded_blob_code_start_address_.ShrinkToFit();
    embedded_blob_code_size_.ShrinkToFit();
    code_range_base_address_.ShrinkToFit();
    code_range_size_.ShrinkToFit();
    shared_code_range_.ShrinkToFit();
    embedded_blob_code_copy_start_address_.ShrinkToFit();
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
    mutable_upid()->Append(row.upid);
    mutable_internal_isolate_id()->Append(row.internal_isolate_id);
    mutable_embedded_blob_code_start_address()->Append(row.embedded_blob_code_start_address);
    mutable_embedded_blob_code_size()->Append(row.embedded_blob_code_size);
    mutable_code_range_base_address()->Append(row.code_range_base_address);
    mutable_code_range_size()->Append(row.code_range_size);
    mutable_shared_code_range()->Append(row.shared_code_range);
    mutable_embedded_blob_code_copy_start_address()->Append(row.embedded_blob_code_copy_start_address);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<V8IsolateTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<int32_t>& internal_isolate_id() const {
    return static_cast<const ColumnType::internal_isolate_id&>(columns()[ColumnIndex::internal_isolate_id]);
  }
  const TypedColumn<int64_t>& embedded_blob_code_start_address() const {
    return static_cast<const ColumnType::embedded_blob_code_start_address&>(columns()[ColumnIndex::embedded_blob_code_start_address]);
  }
  const TypedColumn<int64_t>& embedded_blob_code_size() const {
    return static_cast<const ColumnType::embedded_blob_code_size&>(columns()[ColumnIndex::embedded_blob_code_size]);
  }
  const TypedColumn<std::optional<int64_t>>& code_range_base_address() const {
    return static_cast<const ColumnType::code_range_base_address&>(columns()[ColumnIndex::code_range_base_address]);
  }
  const TypedColumn<std::optional<int64_t>>& code_range_size() const {
    return static_cast<const ColumnType::code_range_size&>(columns()[ColumnIndex::code_range_size]);
  }
  const TypedColumn<std::optional<uint32_t>>& shared_code_range() const {
    return static_cast<const ColumnType::shared_code_range&>(columns()[ColumnIndex::shared_code_range]);
  }
  const TypedColumn<std::optional<int64_t>>& embedded_blob_code_copy_start_address() const {
    return static_cast<const ColumnType::embedded_blob_code_copy_start_address&>(columns()[ColumnIndex::embedded_blob_code_copy_start_address]);
  }

  TypedColumn<uint32_t>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<int32_t>* mutable_internal_isolate_id() {
    return static_cast<ColumnType::internal_isolate_id*>(
        GetColumn(ColumnIndex::internal_isolate_id));
  }
  TypedColumn<int64_t>* mutable_embedded_blob_code_start_address() {
    return static_cast<ColumnType::embedded_blob_code_start_address*>(
        GetColumn(ColumnIndex::embedded_blob_code_start_address));
  }
  TypedColumn<int64_t>* mutable_embedded_blob_code_size() {
    return static_cast<ColumnType::embedded_blob_code_size*>(
        GetColumn(ColumnIndex::embedded_blob_code_size));
  }
  TypedColumn<std::optional<int64_t>>* mutable_code_range_base_address() {
    return static_cast<ColumnType::code_range_base_address*>(
        GetColumn(ColumnIndex::code_range_base_address));
  }
  TypedColumn<std::optional<int64_t>>* mutable_code_range_size() {
    return static_cast<ColumnType::code_range_size*>(
        GetColumn(ColumnIndex::code_range_size));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_shared_code_range() {
    return static_cast<ColumnType::shared_code_range*>(
        GetColumn(ColumnIndex::shared_code_range));
  }
  TypedColumn<std::optional<int64_t>>* mutable_embedded_blob_code_copy_start_address() {
    return static_cast<ColumnType::embedded_blob_code_copy_start_address*>(
        GetColumn(ColumnIndex::embedded_blob_code_copy_start_address));
  }

 private:
  
  
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::internal_isolate_id::stored_type> internal_isolate_id_;
  ColumnStorage<ColumnType::embedded_blob_code_start_address::stored_type> embedded_blob_code_start_address_;
  ColumnStorage<ColumnType::embedded_blob_code_size::stored_type> embedded_blob_code_size_;
  ColumnStorage<ColumnType::code_range_base_address::stored_type> code_range_base_address_;
  ColumnStorage<ColumnType::code_range_size::stored_type> code_range_size_;
  ColumnStorage<ColumnType::shared_code_range::stored_type> shared_code_range_;
  ColumnStorage<ColumnType::embedded_blob_code_copy_start_address::stored_type> embedded_blob_code_copy_start_address_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> internal_isolate_id_storage_layer_;
  RefPtr<column::StorageLayer> embedded_blob_code_start_address_storage_layer_;
  RefPtr<column::StorageLayer> embedded_blob_code_size_storage_layer_;
  RefPtr<column::StorageLayer> code_range_base_address_storage_layer_;
  RefPtr<column::StorageLayer> code_range_size_storage_layer_;
  RefPtr<column::StorageLayer> shared_code_range_storage_layer_;
  RefPtr<column::StorageLayer> embedded_blob_code_copy_start_address_storage_layer_;

  RefPtr<column::OverlayLayer> code_range_base_address_null_layer_;
  RefPtr<column::OverlayLayer> code_range_size_null_layer_;
  RefPtr<column::OverlayLayer> shared_code_range_null_layer_;
  RefPtr<column::OverlayLayer> embedded_blob_code_copy_start_address_null_layer_;
};
  

class V8JsScriptTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 7;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t v8_isolate_id = 2;
    static constexpr uint32_t internal_script_id = 3;
    static constexpr uint32_t script_type = 4;
    static constexpr uint32_t name = 5;
    static constexpr uint32_t source = 6;
  };
  struct ColumnType {
    using id = IdColumn<V8JsScriptTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using v8_isolate_id = TypedColumn<V8IsolateTable::Id>;
    using internal_script_id = TypedColumn<int32_t>;
    using script_type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using source = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(V8IsolateTable::Id in_v8_isolate_id = {},
        int32_t in_internal_script_id = {},
        StringPool::Id in_script_type = {},
        StringPool::Id in_name = {},
        std::optional<StringPool::Id> in_source = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          v8_isolate_id(in_v8_isolate_id),
          internal_script_id(in_internal_script_id),
          script_type(in_script_type),
          name(in_name),
          source(in_source) {
      type_ = "__intrinsic_v8_js_script";
    }
    V8IsolateTable::Id v8_isolate_id;
    int32_t internal_script_id;
    StringPool::Id script_type;
    StringPool::Id name;
    std::optional<StringPool::Id> source;

    bool operator==(const V8JsScriptTable::Row& other) const {
      return type() == other.type() && ColumnType::v8_isolate_id::Equals(v8_isolate_id, other.v8_isolate_id) &&
       ColumnType::internal_script_id::Equals(internal_script_id, other.internal_script_id) &&
       ColumnType::script_type::Equals(script_type, other.script_type) &&
       ColumnType::name::Equals(name, other.name) &&
       ColumnType::source::Equals(source, other.source);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t v8_isolate_id = ColumnType::v8_isolate_id::default_flags();
    static constexpr uint32_t internal_script_id = ColumnType::internal_script_id::default_flags();
    static constexpr uint32_t script_type = ColumnType::script_type::default_flags();
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t source = ColumnType::source::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      V8JsScriptTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    V8JsScriptTable, RowNumber> {
   public:
    ConstRowReference(const V8JsScriptTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      return table_->v8_isolate_id()[row_number_];
    }
    ColumnType::internal_script_id::type internal_script_id() const {
      return table_->internal_script_id()[row_number_];
    }
    ColumnType::script_type::type script_type() const {
      return table_->script_type()[row_number_];
    }
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::source::type source() const {
      return table_->source()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const V8JsScriptTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_v8_isolate_id(
        ColumnType::v8_isolate_id::non_optional_type v) {
      return mutable_table()->mutable_v8_isolate_id()->Set(row_number_, v);
    }
    void set_internal_script_id(
        ColumnType::internal_script_id::non_optional_type v) {
      return mutable_table()->mutable_internal_script_id()->Set(row_number_, v);
    }
    void set_script_type(
        ColumnType::script_type::non_optional_type v) {
      return mutable_table()->mutable_script_type()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_source(
        ColumnType::source::non_optional_type v) {
      return mutable_table()->mutable_source()->Set(row_number_, v);
    }

   private:
    V8JsScriptTable* mutable_table() const {
      return const_cast<V8JsScriptTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, V8JsScriptTable, RowNumber, ConstRowReference> {
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
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      const auto& col = table_->v8_isolate_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::internal_script_id::type internal_script_id() const {
      const auto& col = table_->internal_script_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::script_type::type script_type() const {
      const auto& col = table_->script_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source::type source() const {
      const auto& col = table_->source();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const V8JsScriptTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class V8JsScriptTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, V8JsScriptTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class V8JsScriptTable;

     explicit Iterator(V8JsScriptTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     V8JsScriptTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      V8JsScriptTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "v8_isolate_id", &self->v8_isolate_id_, ColumnFlag::v8_isolate_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "internal_script_id", &self->internal_script_id_, ColumnFlag::internal_script_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "script_type", &self->script_type_, ColumnFlag::script_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "source", &self->source_, ColumnFlag::source,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit V8JsScriptTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        v8_isolate_id_(ColumnStorage<ColumnType::v8_isolate_id::stored_type>::Create<false>()),
        internal_script_id_(ColumnStorage<ColumnType::internal_script_id::stored_type>::Create<false>()),
        script_type_(ColumnStorage<ColumnType::script_type::stored_type>::Create<false>()),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        source_(ColumnStorage<ColumnType::source::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        v8_isolate_id_storage_layer_(
        new column::NumericStorage<ColumnType::v8_isolate_id::non_optional_stored_type>(
          &v8_isolate_id_.vector(),
          ColumnTypeHelper<ColumnType::v8_isolate_id::stored_type>::ToColumnType(),
          false)),
        internal_script_id_storage_layer_(
        new column::NumericStorage<ColumnType::internal_script_id::non_optional_stored_type>(
          &internal_script_id_.vector(),
          ColumnTypeHelper<ColumnType::internal_script_id::stored_type>::ToColumnType(),
          false)),
        script_type_storage_layer_(
          new column::StringStorage(string_pool(), &script_type_.vector())),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        source_storage_layer_(
          new column::StringStorage(string_pool(), &source_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::v8_isolate_id::stored_type>(
          ColumnFlag::v8_isolate_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::internal_script_id::stored_type>(
          ColumnFlag::internal_script_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::script_type::stored_type>(
          ColumnFlag::script_type),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::source::stored_type>(
          ColumnFlag::source),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,v8_isolate_id_storage_layer_,internal_script_id_storage_layer_,script_type_storage_layer_,name_storage_layer_,source_storage_layer_},
      {{},{},{},{},{},{},{}});
  }
  ~V8JsScriptTable() override;

  static const char* Name() { return "__intrinsic_v8_js_script"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "v8_isolate_id", ColumnType::v8_isolate_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "internal_script_id", ColumnType::internal_script_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "script_type", ColumnType::script_type::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source", ColumnType::source::SqlValueType(), false,
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
    v8_isolate_id_.ShrinkToFit();
    internal_script_id_.ShrinkToFit();
    script_type_.ShrinkToFit();
    name_.ShrinkToFit();
    source_.ShrinkToFit();
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
    mutable_v8_isolate_id()->Append(row.v8_isolate_id);
    mutable_internal_script_id()->Append(row.internal_script_id);
    mutable_script_type()->Append(row.script_type);
    mutable_name()->Append(row.name);
    mutable_source()->Append(row.source);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<V8JsScriptTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<V8IsolateTable::Id>& v8_isolate_id() const {
    return static_cast<const ColumnType::v8_isolate_id&>(columns()[ColumnIndex::v8_isolate_id]);
  }
  const TypedColumn<int32_t>& internal_script_id() const {
    return static_cast<const ColumnType::internal_script_id&>(columns()[ColumnIndex::internal_script_id]);
  }
  const TypedColumn<StringPool::Id>& script_type() const {
    return static_cast<const ColumnType::script_type&>(columns()[ColumnIndex::script_type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& source() const {
    return static_cast<const ColumnType::source&>(columns()[ColumnIndex::source]);
  }

  TypedColumn<V8IsolateTable::Id>* mutable_v8_isolate_id() {
    return static_cast<ColumnType::v8_isolate_id*>(
        GetColumn(ColumnIndex::v8_isolate_id));
  }
  TypedColumn<int32_t>* mutable_internal_script_id() {
    return static_cast<ColumnType::internal_script_id*>(
        GetColumn(ColumnIndex::internal_script_id));
  }
  TypedColumn<StringPool::Id>* mutable_script_type() {
    return static_cast<ColumnType::script_type*>(
        GetColumn(ColumnIndex::script_type));
  }
  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_source() {
    return static_cast<ColumnType::source*>(
        GetColumn(ColumnIndex::source));
  }

 private:
  
  
  ColumnStorage<ColumnType::v8_isolate_id::stored_type> v8_isolate_id_;
  ColumnStorage<ColumnType::internal_script_id::stored_type> internal_script_id_;
  ColumnStorage<ColumnType::script_type::stored_type> script_type_;
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::source::stored_type> source_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> v8_isolate_id_storage_layer_;
  RefPtr<column::StorageLayer> internal_script_id_storage_layer_;
  RefPtr<column::StorageLayer> script_type_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> source_storage_layer_;

  
};
  

class V8WasmScriptTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 6;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t v8_isolate_id = 2;
    static constexpr uint32_t internal_script_id = 3;
    static constexpr uint32_t url = 4;
    static constexpr uint32_t source = 5;
  };
  struct ColumnType {
    using id = IdColumn<V8WasmScriptTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using v8_isolate_id = TypedColumn<V8IsolateTable::Id>;
    using internal_script_id = TypedColumn<int32_t>;
    using url = TypedColumn<StringPool::Id>;
    using source = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(V8IsolateTable::Id in_v8_isolate_id = {},
        int32_t in_internal_script_id = {},
        StringPool::Id in_url = {},
        std::optional<StringPool::Id> in_source = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          v8_isolate_id(in_v8_isolate_id),
          internal_script_id(in_internal_script_id),
          url(in_url),
          source(in_source) {
      type_ = "__intrinsic_v8_wasm_script";
    }
    V8IsolateTable::Id v8_isolate_id;
    int32_t internal_script_id;
    StringPool::Id url;
    std::optional<StringPool::Id> source;

    bool operator==(const V8WasmScriptTable::Row& other) const {
      return type() == other.type() && ColumnType::v8_isolate_id::Equals(v8_isolate_id, other.v8_isolate_id) &&
       ColumnType::internal_script_id::Equals(internal_script_id, other.internal_script_id) &&
       ColumnType::url::Equals(url, other.url) &&
       ColumnType::source::Equals(source, other.source);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t v8_isolate_id = ColumnType::v8_isolate_id::default_flags();
    static constexpr uint32_t internal_script_id = ColumnType::internal_script_id::default_flags();
    static constexpr uint32_t url = ColumnType::url::default_flags();
    static constexpr uint32_t source = ColumnType::source::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      V8WasmScriptTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    V8WasmScriptTable, RowNumber> {
   public:
    ConstRowReference(const V8WasmScriptTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      return table_->v8_isolate_id()[row_number_];
    }
    ColumnType::internal_script_id::type internal_script_id() const {
      return table_->internal_script_id()[row_number_];
    }
    ColumnType::url::type url() const {
      return table_->url()[row_number_];
    }
    ColumnType::source::type source() const {
      return table_->source()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const V8WasmScriptTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_v8_isolate_id(
        ColumnType::v8_isolate_id::non_optional_type v) {
      return mutable_table()->mutable_v8_isolate_id()->Set(row_number_, v);
    }
    void set_internal_script_id(
        ColumnType::internal_script_id::non_optional_type v) {
      return mutable_table()->mutable_internal_script_id()->Set(row_number_, v);
    }
    void set_url(
        ColumnType::url::non_optional_type v) {
      return mutable_table()->mutable_url()->Set(row_number_, v);
    }
    void set_source(
        ColumnType::source::non_optional_type v) {
      return mutable_table()->mutable_source()->Set(row_number_, v);
    }

   private:
    V8WasmScriptTable* mutable_table() const {
      return const_cast<V8WasmScriptTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, V8WasmScriptTable, RowNumber, ConstRowReference> {
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
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      const auto& col = table_->v8_isolate_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::internal_script_id::type internal_script_id() const {
      const auto& col = table_->internal_script_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::url::type url() const {
      const auto& col = table_->url();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source::type source() const {
      const auto& col = table_->source();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const V8WasmScriptTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class V8WasmScriptTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, V8WasmScriptTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class V8WasmScriptTable;

     explicit Iterator(V8WasmScriptTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     V8WasmScriptTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      V8WasmScriptTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "v8_isolate_id", &self->v8_isolate_id_, ColumnFlag::v8_isolate_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "internal_script_id", &self->internal_script_id_, ColumnFlag::internal_script_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "url", &self->url_, ColumnFlag::url,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "source", &self->source_, ColumnFlag::source,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit V8WasmScriptTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        v8_isolate_id_(ColumnStorage<ColumnType::v8_isolate_id::stored_type>::Create<false>()),
        internal_script_id_(ColumnStorage<ColumnType::internal_script_id::stored_type>::Create<false>()),
        url_(ColumnStorage<ColumnType::url::stored_type>::Create<false>()),
        source_(ColumnStorage<ColumnType::source::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        v8_isolate_id_storage_layer_(
        new column::NumericStorage<ColumnType::v8_isolate_id::non_optional_stored_type>(
          &v8_isolate_id_.vector(),
          ColumnTypeHelper<ColumnType::v8_isolate_id::stored_type>::ToColumnType(),
          false)),
        internal_script_id_storage_layer_(
        new column::NumericStorage<ColumnType::internal_script_id::non_optional_stored_type>(
          &internal_script_id_.vector(),
          ColumnTypeHelper<ColumnType::internal_script_id::stored_type>::ToColumnType(),
          false)),
        url_storage_layer_(
          new column::StringStorage(string_pool(), &url_.vector())),
        source_storage_layer_(
          new column::StringStorage(string_pool(), &source_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::v8_isolate_id::stored_type>(
          ColumnFlag::v8_isolate_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::internal_script_id::stored_type>(
          ColumnFlag::internal_script_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::url::stored_type>(
          ColumnFlag::url),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::source::stored_type>(
          ColumnFlag::source),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,v8_isolate_id_storage_layer_,internal_script_id_storage_layer_,url_storage_layer_,source_storage_layer_},
      {{},{},{},{},{},{}});
  }
  ~V8WasmScriptTable() override;

  static const char* Name() { return "__intrinsic_v8_wasm_script"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "v8_isolate_id", ColumnType::v8_isolate_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "internal_script_id", ColumnType::internal_script_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "url", ColumnType::url::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source", ColumnType::source::SqlValueType(), false,
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
    v8_isolate_id_.ShrinkToFit();
    internal_script_id_.ShrinkToFit();
    url_.ShrinkToFit();
    source_.ShrinkToFit();
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
    mutable_v8_isolate_id()->Append(row.v8_isolate_id);
    mutable_internal_script_id()->Append(row.internal_script_id);
    mutable_url()->Append(row.url);
    mutable_source()->Append(row.source);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<V8WasmScriptTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<V8IsolateTable::Id>& v8_isolate_id() const {
    return static_cast<const ColumnType::v8_isolate_id&>(columns()[ColumnIndex::v8_isolate_id]);
  }
  const TypedColumn<int32_t>& internal_script_id() const {
    return static_cast<const ColumnType::internal_script_id&>(columns()[ColumnIndex::internal_script_id]);
  }
  const TypedColumn<StringPool::Id>& url() const {
    return static_cast<const ColumnType::url&>(columns()[ColumnIndex::url]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& source() const {
    return static_cast<const ColumnType::source&>(columns()[ColumnIndex::source]);
  }

  TypedColumn<V8IsolateTable::Id>* mutable_v8_isolate_id() {
    return static_cast<ColumnType::v8_isolate_id*>(
        GetColumn(ColumnIndex::v8_isolate_id));
  }
  TypedColumn<int32_t>* mutable_internal_script_id() {
    return static_cast<ColumnType::internal_script_id*>(
        GetColumn(ColumnIndex::internal_script_id));
  }
  TypedColumn<StringPool::Id>* mutable_url() {
    return static_cast<ColumnType::url*>(
        GetColumn(ColumnIndex::url));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_source() {
    return static_cast<ColumnType::source*>(
        GetColumn(ColumnIndex::source));
  }

 private:
  
  
  ColumnStorage<ColumnType::v8_isolate_id::stored_type> v8_isolate_id_;
  ColumnStorage<ColumnType::internal_script_id::stored_type> internal_script_id_;
  ColumnStorage<ColumnType::url::stored_type> url_;
  ColumnStorage<ColumnType::source::stored_type> source_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> v8_isolate_id_storage_layer_;
  RefPtr<column::StorageLayer> internal_script_id_storage_layer_;
  RefPtr<column::StorageLayer> url_storage_layer_;
  RefPtr<column::StorageLayer> source_storage_layer_;

  
};
  

class V8JsFunctionTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 8;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t v8_js_script_id = 3;
    static constexpr uint32_t is_toplevel = 4;
    static constexpr uint32_t kind = 5;
    static constexpr uint32_t line = 6;
    static constexpr uint32_t col = 7;
  };
  struct ColumnType {
    using id = IdColumn<V8JsFunctionTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using v8_js_script_id = TypedColumn<V8JsScriptTable::Id>;
    using is_toplevel = TypedColumn<uint32_t>;
    using kind = TypedColumn<StringPool::Id>;
    using line = TypedColumn<std::optional<uint32_t>>;
    using col = TypedColumn<std::optional<uint32_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_name = {},
        V8JsScriptTable::Id in_v8_js_script_id = {},
        uint32_t in_is_toplevel = {},
        StringPool::Id in_kind = {},
        std::optional<uint32_t> in_line = {},
        std::optional<uint32_t> in_col = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          name(in_name),
          v8_js_script_id(in_v8_js_script_id),
          is_toplevel(in_is_toplevel),
          kind(in_kind),
          line(in_line),
          col(in_col) {
      type_ = "__intrinsic_v8_js_function";
    }
    StringPool::Id name;
    V8JsScriptTable::Id v8_js_script_id;
    uint32_t is_toplevel;
    StringPool::Id kind;
    std::optional<uint32_t> line;
    std::optional<uint32_t> col;

    bool operator==(const V8JsFunctionTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::v8_js_script_id::Equals(v8_js_script_id, other.v8_js_script_id) &&
       ColumnType::is_toplevel::Equals(is_toplevel, other.is_toplevel) &&
       ColumnType::kind::Equals(kind, other.kind) &&
       ColumnType::line::Equals(line, other.line) &&
       ColumnType::col::Equals(col, other.col);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t v8_js_script_id = ColumnType::v8_js_script_id::default_flags();
    static constexpr uint32_t is_toplevel = ColumnType::is_toplevel::default_flags();
    static constexpr uint32_t kind = ColumnType::kind::default_flags();
    static constexpr uint32_t line = ColumnType::line::default_flags();
    static constexpr uint32_t col = ColumnType::col::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      V8JsFunctionTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    V8JsFunctionTable, RowNumber> {
   public:
    ConstRowReference(const V8JsFunctionTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::v8_js_script_id::type v8_js_script_id() const {
      return table_->v8_js_script_id()[row_number_];
    }
    ColumnType::is_toplevel::type is_toplevel() const {
      return table_->is_toplevel()[row_number_];
    }
    ColumnType::kind::type kind() const {
      return table_->kind()[row_number_];
    }
    ColumnType::line::type line() const {
      return table_->line()[row_number_];
    }
    ColumnType::col::type col() const {
      return table_->col()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const V8JsFunctionTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_v8_js_script_id(
        ColumnType::v8_js_script_id::non_optional_type v) {
      return mutable_table()->mutable_v8_js_script_id()->Set(row_number_, v);
    }
    void set_is_toplevel(
        ColumnType::is_toplevel::non_optional_type v) {
      return mutable_table()->mutable_is_toplevel()->Set(row_number_, v);
    }
    void set_kind(
        ColumnType::kind::non_optional_type v) {
      return mutable_table()->mutable_kind()->Set(row_number_, v);
    }
    void set_line(
        ColumnType::line::non_optional_type v) {
      return mutable_table()->mutable_line()->Set(row_number_, v);
    }
    void set_col(
        ColumnType::col::non_optional_type v) {
      return mutable_table()->mutable_col()->Set(row_number_, v);
    }

   private:
    V8JsFunctionTable* mutable_table() const {
      return const_cast<V8JsFunctionTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, V8JsFunctionTable, RowNumber, ConstRowReference> {
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
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::v8_js_script_id::type v8_js_script_id() const {
      const auto& col = table_->v8_js_script_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::is_toplevel::type is_toplevel() const {
      const auto& col = table_->is_toplevel();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::kind::type kind() const {
      const auto& col = table_->kind();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::line::type line() const {
      const auto& col = table_->line();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::col::type col() const {
      const auto& col = table_->col();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const V8JsFunctionTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class V8JsFunctionTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, V8JsFunctionTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class V8JsFunctionTable;

     explicit Iterator(V8JsFunctionTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     V8JsFunctionTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      V8JsFunctionTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "v8_js_script_id", &self->v8_js_script_id_, ColumnFlag::v8_js_script_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "is_toplevel", &self->is_toplevel_, ColumnFlag::is_toplevel,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "kind", &self->kind_, ColumnFlag::kind,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "line", &self->line_, ColumnFlag::line,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "col", &self->col_, ColumnFlag::col,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit V8JsFunctionTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        v8_js_script_id_(ColumnStorage<ColumnType::v8_js_script_id::stored_type>::Create<false>()),
        is_toplevel_(ColumnStorage<ColumnType::is_toplevel::stored_type>::Create<false>()),
        kind_(ColumnStorage<ColumnType::kind::stored_type>::Create<false>()),
        line_(ColumnStorage<ColumnType::line::stored_type>::Create<false>()),
        col_(ColumnStorage<ColumnType::col::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        v8_js_script_id_storage_layer_(
        new column::NumericStorage<ColumnType::v8_js_script_id::non_optional_stored_type>(
          &v8_js_script_id_.vector(),
          ColumnTypeHelper<ColumnType::v8_js_script_id::stored_type>::ToColumnType(),
          false)),
        is_toplevel_storage_layer_(
        new column::NumericStorage<ColumnType::is_toplevel::non_optional_stored_type>(
          &is_toplevel_.vector(),
          ColumnTypeHelper<ColumnType::is_toplevel::stored_type>::ToColumnType(),
          false)),
        kind_storage_layer_(
          new column::StringStorage(string_pool(), &kind_.vector())),
        line_storage_layer_(
          new column::NumericStorage<ColumnType::line::non_optional_stored_type>(
            &line_.non_null_vector(),
            ColumnTypeHelper<ColumnType::line::stored_type>::ToColumnType(),
            false)),
        col_storage_layer_(
          new column::NumericStorage<ColumnType::col::non_optional_stored_type>(
            &col_.non_null_vector(),
            ColumnTypeHelper<ColumnType::col::stored_type>::ToColumnType(),
            false))
,
        line_null_layer_(new column::NullOverlay(line_.bv())),
        col_null_layer_(new column::NullOverlay(col_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::v8_js_script_id::stored_type>(
          ColumnFlag::v8_js_script_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::is_toplevel::stored_type>(
          ColumnFlag::is_toplevel),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::kind::stored_type>(
          ColumnFlag::kind),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::line::stored_type>(
          ColumnFlag::line),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::col::stored_type>(
          ColumnFlag::col),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,name_storage_layer_,v8_js_script_id_storage_layer_,is_toplevel_storage_layer_,kind_storage_layer_,line_storage_layer_,col_storage_layer_},
      {{},{},{},{},{},{},line_null_layer_,col_null_layer_});
  }
  ~V8JsFunctionTable() override;

  static const char* Name() { return "__intrinsic_v8_js_function"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "v8_js_script_id", ColumnType::v8_js_script_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "is_toplevel", ColumnType::is_toplevel::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "kind", ColumnType::kind::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "line", ColumnType::line::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "col", ColumnType::col::SqlValueType(), false,
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
    name_.ShrinkToFit();
    v8_js_script_id_.ShrinkToFit();
    is_toplevel_.ShrinkToFit();
    kind_.ShrinkToFit();
    line_.ShrinkToFit();
    col_.ShrinkToFit();
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
    mutable_name()->Append(row.name);
    mutable_v8_js_script_id()->Append(row.v8_js_script_id);
    mutable_is_toplevel()->Append(row.is_toplevel);
    mutable_kind()->Append(row.kind);
    mutable_line()->Append(row.line);
    mutable_col()->Append(row.col);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<V8JsFunctionTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<V8JsScriptTable::Id>& v8_js_script_id() const {
    return static_cast<const ColumnType::v8_js_script_id&>(columns()[ColumnIndex::v8_js_script_id]);
  }
  const TypedColumn<uint32_t>& is_toplevel() const {
    return static_cast<const ColumnType::is_toplevel&>(columns()[ColumnIndex::is_toplevel]);
  }
  const TypedColumn<StringPool::Id>& kind() const {
    return static_cast<const ColumnType::kind&>(columns()[ColumnIndex::kind]);
  }
  const TypedColumn<std::optional<uint32_t>>& line() const {
    return static_cast<const ColumnType::line&>(columns()[ColumnIndex::line]);
  }
  const TypedColumn<std::optional<uint32_t>>& col() const {
    return static_cast<const ColumnType::col&>(columns()[ColumnIndex::col]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<V8JsScriptTable::Id>* mutable_v8_js_script_id() {
    return static_cast<ColumnType::v8_js_script_id*>(
        GetColumn(ColumnIndex::v8_js_script_id));
  }
  TypedColumn<uint32_t>* mutable_is_toplevel() {
    return static_cast<ColumnType::is_toplevel*>(
        GetColumn(ColumnIndex::is_toplevel));
  }
  TypedColumn<StringPool::Id>* mutable_kind() {
    return static_cast<ColumnType::kind*>(
        GetColumn(ColumnIndex::kind));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_line() {
    return static_cast<ColumnType::line*>(
        GetColumn(ColumnIndex::line));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_col() {
    return static_cast<ColumnType::col*>(
        GetColumn(ColumnIndex::col));
  }

 private:
  
  
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::v8_js_script_id::stored_type> v8_js_script_id_;
  ColumnStorage<ColumnType::is_toplevel::stored_type> is_toplevel_;
  ColumnStorage<ColumnType::kind::stored_type> kind_;
  ColumnStorage<ColumnType::line::stored_type> line_;
  ColumnStorage<ColumnType::col::stored_type> col_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> v8_js_script_id_storage_layer_;
  RefPtr<column::StorageLayer> is_toplevel_storage_layer_;
  RefPtr<column::StorageLayer> kind_storage_layer_;
  RefPtr<column::StorageLayer> line_storage_layer_;
  RefPtr<column::StorageLayer> col_storage_layer_;

  RefPtr<column::OverlayLayer> line_null_layer_;
  RefPtr<column::OverlayLayer> col_null_layer_;
};
  

class V8JsCodeTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 6;

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
    static constexpr uint32_t v8_js_function_id = 3;
    static constexpr uint32_t tier = 4;
    static constexpr uint32_t bytecode_base64 = 5;
  };
  struct ColumnType {
    using id = IdColumn<V8JsCodeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using jit_code_id = TypedColumn<std::optional<JitCodeTable::Id>>;
    using v8_js_function_id = TypedColumn<V8JsFunctionTable::Id>;
    using tier = TypedColumn<StringPool::Id>;
    using bytecode_base64 = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(std::optional<JitCodeTable::Id> in_jit_code_id = {},
        V8JsFunctionTable::Id in_v8_js_function_id = {},
        StringPool::Id in_tier = {},
        std::optional<StringPool::Id> in_bytecode_base64 = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          jit_code_id(in_jit_code_id),
          v8_js_function_id(in_v8_js_function_id),
          tier(in_tier),
          bytecode_base64(in_bytecode_base64) {
      type_ = "__intrinsic_v8_js_code";
    }
    std::optional<JitCodeTable::Id> jit_code_id;
    V8JsFunctionTable::Id v8_js_function_id;
    StringPool::Id tier;
    std::optional<StringPool::Id> bytecode_base64;

    bool operator==(const V8JsCodeTable::Row& other) const {
      return type() == other.type() && ColumnType::jit_code_id::Equals(jit_code_id, other.jit_code_id) &&
       ColumnType::v8_js_function_id::Equals(v8_js_function_id, other.v8_js_function_id) &&
       ColumnType::tier::Equals(tier, other.tier) &&
       ColumnType::bytecode_base64::Equals(bytecode_base64, other.bytecode_base64);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t jit_code_id = ColumnType::jit_code_id::default_flags();
    static constexpr uint32_t v8_js_function_id = ColumnType::v8_js_function_id::default_flags();
    static constexpr uint32_t tier = ColumnType::tier::default_flags();
    static constexpr uint32_t bytecode_base64 = ColumnType::bytecode_base64::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      V8JsCodeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    V8JsCodeTable, RowNumber> {
   public:
    ConstRowReference(const V8JsCodeTable* table, uint32_t row_number)
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
    ColumnType::v8_js_function_id::type v8_js_function_id() const {
      return table_->v8_js_function_id()[row_number_];
    }
    ColumnType::tier::type tier() const {
      return table_->tier()[row_number_];
    }
    ColumnType::bytecode_base64::type bytecode_base64() const {
      return table_->bytecode_base64()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const V8JsCodeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_jit_code_id(
        ColumnType::jit_code_id::non_optional_type v) {
      return mutable_table()->mutable_jit_code_id()->Set(row_number_, v);
    }
    void set_v8_js_function_id(
        ColumnType::v8_js_function_id::non_optional_type v) {
      return mutable_table()->mutable_v8_js_function_id()->Set(row_number_, v);
    }
    void set_tier(
        ColumnType::tier::non_optional_type v) {
      return mutable_table()->mutable_tier()->Set(row_number_, v);
    }
    void set_bytecode_base64(
        ColumnType::bytecode_base64::non_optional_type v) {
      return mutable_table()->mutable_bytecode_base64()->Set(row_number_, v);
    }

   private:
    V8JsCodeTable* mutable_table() const {
      return const_cast<V8JsCodeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, V8JsCodeTable, RowNumber, ConstRowReference> {
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
    ColumnType::v8_js_function_id::type v8_js_function_id() const {
      const auto& col = table_->v8_js_function_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::tier::type tier() const {
      const auto& col = table_->tier();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::bytecode_base64::type bytecode_base64() const {
      const auto& col = table_->bytecode_base64();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const V8JsCodeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class V8JsCodeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, V8JsCodeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class V8JsCodeTable;

     explicit Iterator(V8JsCodeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     V8JsCodeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      V8JsCodeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "jit_code_id", &self->jit_code_id_, ColumnFlag::jit_code_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "v8_js_function_id", &self->v8_js_function_id_, ColumnFlag::v8_js_function_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "tier", &self->tier_, ColumnFlag::tier,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "bytecode_base64", &self->bytecode_base64_, ColumnFlag::bytecode_base64,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit V8JsCodeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        jit_code_id_(ColumnStorage<ColumnType::jit_code_id::stored_type>::Create<false>()),
        v8_js_function_id_(ColumnStorage<ColumnType::v8_js_function_id::stored_type>::Create<false>()),
        tier_(ColumnStorage<ColumnType::tier::stored_type>::Create<false>()),
        bytecode_base64_(ColumnStorage<ColumnType::bytecode_base64::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        jit_code_id_storage_layer_(
          new column::NumericStorage<ColumnType::jit_code_id::non_optional_stored_type>(
            &jit_code_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::jit_code_id::stored_type>::ToColumnType(),
            false)),
        v8_js_function_id_storage_layer_(
        new column::NumericStorage<ColumnType::v8_js_function_id::non_optional_stored_type>(
          &v8_js_function_id_.vector(),
          ColumnTypeHelper<ColumnType::v8_js_function_id::stored_type>::ToColumnType(),
          false)),
        tier_storage_layer_(
          new column::StringStorage(string_pool(), &tier_.vector())),
        bytecode_base64_storage_layer_(
          new column::StringStorage(string_pool(), &bytecode_base64_.vector()))
,
        jit_code_id_null_layer_(new column::NullOverlay(jit_code_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::jit_code_id::stored_type>(
          ColumnFlag::jit_code_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::v8_js_function_id::stored_type>(
          ColumnFlag::v8_js_function_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::tier::stored_type>(
          ColumnFlag::tier),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::bytecode_base64::stored_type>(
          ColumnFlag::bytecode_base64),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,jit_code_id_storage_layer_,v8_js_function_id_storage_layer_,tier_storage_layer_,bytecode_base64_storage_layer_},
      {{},{},jit_code_id_null_layer_,{},{},{}});
  }
  ~V8JsCodeTable() override;

  static const char* Name() { return "__intrinsic_v8_js_code"; }

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
        "v8_js_function_id", ColumnType::v8_js_function_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "tier", ColumnType::tier::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "bytecode_base64", ColumnType::bytecode_base64::SqlValueType(), false,
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
    v8_js_function_id_.ShrinkToFit();
    tier_.ShrinkToFit();
    bytecode_base64_.ShrinkToFit();
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
    mutable_v8_js_function_id()->Append(row.v8_js_function_id);
    mutable_tier()->Append(row.tier);
    mutable_bytecode_base64()->Append(row.bytecode_base64);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<V8JsCodeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<std::optional<JitCodeTable::Id>>& jit_code_id() const {
    return static_cast<const ColumnType::jit_code_id&>(columns()[ColumnIndex::jit_code_id]);
  }
  const TypedColumn<V8JsFunctionTable::Id>& v8_js_function_id() const {
    return static_cast<const ColumnType::v8_js_function_id&>(columns()[ColumnIndex::v8_js_function_id]);
  }
  const TypedColumn<StringPool::Id>& tier() const {
    return static_cast<const ColumnType::tier&>(columns()[ColumnIndex::tier]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& bytecode_base64() const {
    return static_cast<const ColumnType::bytecode_base64&>(columns()[ColumnIndex::bytecode_base64]);
  }

  TypedColumn<std::optional<JitCodeTable::Id>>* mutable_jit_code_id() {
    return static_cast<ColumnType::jit_code_id*>(
        GetColumn(ColumnIndex::jit_code_id));
  }
  TypedColumn<V8JsFunctionTable::Id>* mutable_v8_js_function_id() {
    return static_cast<ColumnType::v8_js_function_id*>(
        GetColumn(ColumnIndex::v8_js_function_id));
  }
  TypedColumn<StringPool::Id>* mutable_tier() {
    return static_cast<ColumnType::tier*>(
        GetColumn(ColumnIndex::tier));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_bytecode_base64() {
    return static_cast<ColumnType::bytecode_base64*>(
        GetColumn(ColumnIndex::bytecode_base64));
  }

 private:
  
  
  ColumnStorage<ColumnType::jit_code_id::stored_type> jit_code_id_;
  ColumnStorage<ColumnType::v8_js_function_id::stored_type> v8_js_function_id_;
  ColumnStorage<ColumnType::tier::stored_type> tier_;
  ColumnStorage<ColumnType::bytecode_base64::stored_type> bytecode_base64_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> jit_code_id_storage_layer_;
  RefPtr<column::StorageLayer> v8_js_function_id_storage_layer_;
  RefPtr<column::StorageLayer> tier_storage_layer_;
  RefPtr<column::StorageLayer> bytecode_base64_storage_layer_;

  RefPtr<column::OverlayLayer> jit_code_id_null_layer_;
};
  

class V8InternalCodeTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 6;

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
    static constexpr uint32_t v8_isolate_id = 3;
    static constexpr uint32_t function_name = 4;
    static constexpr uint32_t code_type = 5;
  };
  struct ColumnType {
    using id = IdColumn<V8InternalCodeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using jit_code_id = TypedColumn<JitCodeTable::Id>;
    using v8_isolate_id = TypedColumn<V8IsolateTable::Id>;
    using function_name = TypedColumn<StringPool::Id>;
    using code_type = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(JitCodeTable::Id in_jit_code_id = {},
        V8IsolateTable::Id in_v8_isolate_id = {},
        StringPool::Id in_function_name = {},
        StringPool::Id in_code_type = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          jit_code_id(in_jit_code_id),
          v8_isolate_id(in_v8_isolate_id),
          function_name(in_function_name),
          code_type(in_code_type) {
      type_ = "__intrinsic_v8_internal_code";
    }
    JitCodeTable::Id jit_code_id;
    V8IsolateTable::Id v8_isolate_id;
    StringPool::Id function_name;
    StringPool::Id code_type;

    bool operator==(const V8InternalCodeTable::Row& other) const {
      return type() == other.type() && ColumnType::jit_code_id::Equals(jit_code_id, other.jit_code_id) &&
       ColumnType::v8_isolate_id::Equals(v8_isolate_id, other.v8_isolate_id) &&
       ColumnType::function_name::Equals(function_name, other.function_name) &&
       ColumnType::code_type::Equals(code_type, other.code_type);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t jit_code_id = ColumnType::jit_code_id::default_flags();
    static constexpr uint32_t v8_isolate_id = ColumnType::v8_isolate_id::default_flags();
    static constexpr uint32_t function_name = ColumnType::function_name::default_flags();
    static constexpr uint32_t code_type = ColumnType::code_type::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      V8InternalCodeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    V8InternalCodeTable, RowNumber> {
   public:
    ConstRowReference(const V8InternalCodeTable* table, uint32_t row_number)
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
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      return table_->v8_isolate_id()[row_number_];
    }
    ColumnType::function_name::type function_name() const {
      return table_->function_name()[row_number_];
    }
    ColumnType::code_type::type code_type() const {
      return table_->code_type()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const V8InternalCodeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_jit_code_id(
        ColumnType::jit_code_id::non_optional_type v) {
      return mutable_table()->mutable_jit_code_id()->Set(row_number_, v);
    }
    void set_v8_isolate_id(
        ColumnType::v8_isolate_id::non_optional_type v) {
      return mutable_table()->mutable_v8_isolate_id()->Set(row_number_, v);
    }
    void set_function_name(
        ColumnType::function_name::non_optional_type v) {
      return mutable_table()->mutable_function_name()->Set(row_number_, v);
    }
    void set_code_type(
        ColumnType::code_type::non_optional_type v) {
      return mutable_table()->mutable_code_type()->Set(row_number_, v);
    }

   private:
    V8InternalCodeTable* mutable_table() const {
      return const_cast<V8InternalCodeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, V8InternalCodeTable, RowNumber, ConstRowReference> {
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
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      const auto& col = table_->v8_isolate_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::function_name::type function_name() const {
      const auto& col = table_->function_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::code_type::type code_type() const {
      const auto& col = table_->code_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const V8InternalCodeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class V8InternalCodeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, V8InternalCodeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class V8InternalCodeTable;

     explicit Iterator(V8InternalCodeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     V8InternalCodeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      V8InternalCodeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "jit_code_id", &self->jit_code_id_, ColumnFlag::jit_code_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "v8_isolate_id", &self->v8_isolate_id_, ColumnFlag::v8_isolate_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "function_name", &self->function_name_, ColumnFlag::function_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "code_type", &self->code_type_, ColumnFlag::code_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit V8InternalCodeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        jit_code_id_(ColumnStorage<ColumnType::jit_code_id::stored_type>::Create<false>()),
        v8_isolate_id_(ColumnStorage<ColumnType::v8_isolate_id::stored_type>::Create<false>()),
        function_name_(ColumnStorage<ColumnType::function_name::stored_type>::Create<false>()),
        code_type_(ColumnStorage<ColumnType::code_type::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        jit_code_id_storage_layer_(
        new column::NumericStorage<ColumnType::jit_code_id::non_optional_stored_type>(
          &jit_code_id_.vector(),
          ColumnTypeHelper<ColumnType::jit_code_id::stored_type>::ToColumnType(),
          false)),
        v8_isolate_id_storage_layer_(
        new column::NumericStorage<ColumnType::v8_isolate_id::non_optional_stored_type>(
          &v8_isolate_id_.vector(),
          ColumnTypeHelper<ColumnType::v8_isolate_id::stored_type>::ToColumnType(),
          false)),
        function_name_storage_layer_(
          new column::StringStorage(string_pool(), &function_name_.vector())),
        code_type_storage_layer_(
          new column::StringStorage(string_pool(), &code_type_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::jit_code_id::stored_type>(
          ColumnFlag::jit_code_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::v8_isolate_id::stored_type>(
          ColumnFlag::v8_isolate_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::function_name::stored_type>(
          ColumnFlag::function_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::code_type::stored_type>(
          ColumnFlag::code_type),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,jit_code_id_storage_layer_,v8_isolate_id_storage_layer_,function_name_storage_layer_,code_type_storage_layer_},
      {{},{},{},{},{},{}});
  }
  ~V8InternalCodeTable() override;

  static const char* Name() { return "__intrinsic_v8_internal_code"; }

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
        "v8_isolate_id", ColumnType::v8_isolate_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "function_name", ColumnType::function_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "code_type", ColumnType::code_type::SqlValueType(), false,
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
    v8_isolate_id_.ShrinkToFit();
    function_name_.ShrinkToFit();
    code_type_.ShrinkToFit();
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
    mutable_v8_isolate_id()->Append(row.v8_isolate_id);
    mutable_function_name()->Append(row.function_name);
    mutable_code_type()->Append(row.code_type);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<V8InternalCodeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<JitCodeTable::Id>& jit_code_id() const {
    return static_cast<const ColumnType::jit_code_id&>(columns()[ColumnIndex::jit_code_id]);
  }
  const TypedColumn<V8IsolateTable::Id>& v8_isolate_id() const {
    return static_cast<const ColumnType::v8_isolate_id&>(columns()[ColumnIndex::v8_isolate_id]);
  }
  const TypedColumn<StringPool::Id>& function_name() const {
    return static_cast<const ColumnType::function_name&>(columns()[ColumnIndex::function_name]);
  }
  const TypedColumn<StringPool::Id>& code_type() const {
    return static_cast<const ColumnType::code_type&>(columns()[ColumnIndex::code_type]);
  }

  TypedColumn<JitCodeTable::Id>* mutable_jit_code_id() {
    return static_cast<ColumnType::jit_code_id*>(
        GetColumn(ColumnIndex::jit_code_id));
  }
  TypedColumn<V8IsolateTable::Id>* mutable_v8_isolate_id() {
    return static_cast<ColumnType::v8_isolate_id*>(
        GetColumn(ColumnIndex::v8_isolate_id));
  }
  TypedColumn<StringPool::Id>* mutable_function_name() {
    return static_cast<ColumnType::function_name*>(
        GetColumn(ColumnIndex::function_name));
  }
  TypedColumn<StringPool::Id>* mutable_code_type() {
    return static_cast<ColumnType::code_type*>(
        GetColumn(ColumnIndex::code_type));
  }

 private:
  
  
  ColumnStorage<ColumnType::jit_code_id::stored_type> jit_code_id_;
  ColumnStorage<ColumnType::v8_isolate_id::stored_type> v8_isolate_id_;
  ColumnStorage<ColumnType::function_name::stored_type> function_name_;
  ColumnStorage<ColumnType::code_type::stored_type> code_type_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> jit_code_id_storage_layer_;
  RefPtr<column::StorageLayer> v8_isolate_id_storage_layer_;
  RefPtr<column::StorageLayer> function_name_storage_layer_;
  RefPtr<column::StorageLayer> code_type_storage_layer_;

  
};
  

class V8WasmCodeTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 8;

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
    static constexpr uint32_t v8_isolate_id = 3;
    static constexpr uint32_t v8_wasm_script_id = 4;
    static constexpr uint32_t function_name = 5;
    static constexpr uint32_t tier = 6;
    static constexpr uint32_t code_offset_in_module = 7;
  };
  struct ColumnType {
    using id = IdColumn<V8WasmCodeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using jit_code_id = TypedColumn<JitCodeTable::Id>;
    using v8_isolate_id = TypedColumn<V8IsolateTable::Id>;
    using v8_wasm_script_id = TypedColumn<V8WasmScriptTable::Id>;
    using function_name = TypedColumn<StringPool::Id>;
    using tier = TypedColumn<StringPool::Id>;
    using code_offset_in_module = TypedColumn<int32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(JitCodeTable::Id in_jit_code_id = {},
        V8IsolateTable::Id in_v8_isolate_id = {},
        V8WasmScriptTable::Id in_v8_wasm_script_id = {},
        StringPool::Id in_function_name = {},
        StringPool::Id in_tier = {},
        int32_t in_code_offset_in_module = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          jit_code_id(in_jit_code_id),
          v8_isolate_id(in_v8_isolate_id),
          v8_wasm_script_id(in_v8_wasm_script_id),
          function_name(in_function_name),
          tier(in_tier),
          code_offset_in_module(in_code_offset_in_module) {
      type_ = "__intrinsic_v8_wasm_code";
    }
    JitCodeTable::Id jit_code_id;
    V8IsolateTable::Id v8_isolate_id;
    V8WasmScriptTable::Id v8_wasm_script_id;
    StringPool::Id function_name;
    StringPool::Id tier;
    int32_t code_offset_in_module;

    bool operator==(const V8WasmCodeTable::Row& other) const {
      return type() == other.type() && ColumnType::jit_code_id::Equals(jit_code_id, other.jit_code_id) &&
       ColumnType::v8_isolate_id::Equals(v8_isolate_id, other.v8_isolate_id) &&
       ColumnType::v8_wasm_script_id::Equals(v8_wasm_script_id, other.v8_wasm_script_id) &&
       ColumnType::function_name::Equals(function_name, other.function_name) &&
       ColumnType::tier::Equals(tier, other.tier) &&
       ColumnType::code_offset_in_module::Equals(code_offset_in_module, other.code_offset_in_module);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t jit_code_id = ColumnType::jit_code_id::default_flags();
    static constexpr uint32_t v8_isolate_id = ColumnType::v8_isolate_id::default_flags();
    static constexpr uint32_t v8_wasm_script_id = ColumnType::v8_wasm_script_id::default_flags();
    static constexpr uint32_t function_name = ColumnType::function_name::default_flags();
    static constexpr uint32_t tier = ColumnType::tier::default_flags();
    static constexpr uint32_t code_offset_in_module = ColumnType::code_offset_in_module::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      V8WasmCodeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    V8WasmCodeTable, RowNumber> {
   public:
    ConstRowReference(const V8WasmCodeTable* table, uint32_t row_number)
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
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      return table_->v8_isolate_id()[row_number_];
    }
    ColumnType::v8_wasm_script_id::type v8_wasm_script_id() const {
      return table_->v8_wasm_script_id()[row_number_];
    }
    ColumnType::function_name::type function_name() const {
      return table_->function_name()[row_number_];
    }
    ColumnType::tier::type tier() const {
      return table_->tier()[row_number_];
    }
    ColumnType::code_offset_in_module::type code_offset_in_module() const {
      return table_->code_offset_in_module()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const V8WasmCodeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_jit_code_id(
        ColumnType::jit_code_id::non_optional_type v) {
      return mutable_table()->mutable_jit_code_id()->Set(row_number_, v);
    }
    void set_v8_isolate_id(
        ColumnType::v8_isolate_id::non_optional_type v) {
      return mutable_table()->mutable_v8_isolate_id()->Set(row_number_, v);
    }
    void set_v8_wasm_script_id(
        ColumnType::v8_wasm_script_id::non_optional_type v) {
      return mutable_table()->mutable_v8_wasm_script_id()->Set(row_number_, v);
    }
    void set_function_name(
        ColumnType::function_name::non_optional_type v) {
      return mutable_table()->mutable_function_name()->Set(row_number_, v);
    }
    void set_tier(
        ColumnType::tier::non_optional_type v) {
      return mutable_table()->mutable_tier()->Set(row_number_, v);
    }
    void set_code_offset_in_module(
        ColumnType::code_offset_in_module::non_optional_type v) {
      return mutable_table()->mutable_code_offset_in_module()->Set(row_number_, v);
    }

   private:
    V8WasmCodeTable* mutable_table() const {
      return const_cast<V8WasmCodeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, V8WasmCodeTable, RowNumber, ConstRowReference> {
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
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      const auto& col = table_->v8_isolate_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::v8_wasm_script_id::type v8_wasm_script_id() const {
      const auto& col = table_->v8_wasm_script_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::function_name::type function_name() const {
      const auto& col = table_->function_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::tier::type tier() const {
      const auto& col = table_->tier();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::code_offset_in_module::type code_offset_in_module() const {
      const auto& col = table_->code_offset_in_module();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const V8WasmCodeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class V8WasmCodeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, V8WasmCodeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class V8WasmCodeTable;

     explicit Iterator(V8WasmCodeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     V8WasmCodeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      V8WasmCodeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "jit_code_id", &self->jit_code_id_, ColumnFlag::jit_code_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "v8_isolate_id", &self->v8_isolate_id_, ColumnFlag::v8_isolate_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "v8_wasm_script_id", &self->v8_wasm_script_id_, ColumnFlag::v8_wasm_script_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "function_name", &self->function_name_, ColumnFlag::function_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "tier", &self->tier_, ColumnFlag::tier,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "code_offset_in_module", &self->code_offset_in_module_, ColumnFlag::code_offset_in_module,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit V8WasmCodeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        jit_code_id_(ColumnStorage<ColumnType::jit_code_id::stored_type>::Create<false>()),
        v8_isolate_id_(ColumnStorage<ColumnType::v8_isolate_id::stored_type>::Create<false>()),
        v8_wasm_script_id_(ColumnStorage<ColumnType::v8_wasm_script_id::stored_type>::Create<false>()),
        function_name_(ColumnStorage<ColumnType::function_name::stored_type>::Create<false>()),
        tier_(ColumnStorage<ColumnType::tier::stored_type>::Create<false>()),
        code_offset_in_module_(ColumnStorage<ColumnType::code_offset_in_module::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        jit_code_id_storage_layer_(
        new column::NumericStorage<ColumnType::jit_code_id::non_optional_stored_type>(
          &jit_code_id_.vector(),
          ColumnTypeHelper<ColumnType::jit_code_id::stored_type>::ToColumnType(),
          false)),
        v8_isolate_id_storage_layer_(
        new column::NumericStorage<ColumnType::v8_isolate_id::non_optional_stored_type>(
          &v8_isolate_id_.vector(),
          ColumnTypeHelper<ColumnType::v8_isolate_id::stored_type>::ToColumnType(),
          false)),
        v8_wasm_script_id_storage_layer_(
        new column::NumericStorage<ColumnType::v8_wasm_script_id::non_optional_stored_type>(
          &v8_wasm_script_id_.vector(),
          ColumnTypeHelper<ColumnType::v8_wasm_script_id::stored_type>::ToColumnType(),
          false)),
        function_name_storage_layer_(
          new column::StringStorage(string_pool(), &function_name_.vector())),
        tier_storage_layer_(
          new column::StringStorage(string_pool(), &tier_.vector())),
        code_offset_in_module_storage_layer_(
        new column::NumericStorage<ColumnType::code_offset_in_module::non_optional_stored_type>(
          &code_offset_in_module_.vector(),
          ColumnTypeHelper<ColumnType::code_offset_in_module::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::jit_code_id::stored_type>(
          ColumnFlag::jit_code_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::v8_isolate_id::stored_type>(
          ColumnFlag::v8_isolate_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::v8_wasm_script_id::stored_type>(
          ColumnFlag::v8_wasm_script_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::function_name::stored_type>(
          ColumnFlag::function_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::tier::stored_type>(
          ColumnFlag::tier),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::code_offset_in_module::stored_type>(
          ColumnFlag::code_offset_in_module),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,jit_code_id_storage_layer_,v8_isolate_id_storage_layer_,v8_wasm_script_id_storage_layer_,function_name_storage_layer_,tier_storage_layer_,code_offset_in_module_storage_layer_},
      {{},{},{},{},{},{},{},{}});
  }
  ~V8WasmCodeTable() override;

  static const char* Name() { return "__intrinsic_v8_wasm_code"; }

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
        "v8_isolate_id", ColumnType::v8_isolate_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "v8_wasm_script_id", ColumnType::v8_wasm_script_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "function_name", ColumnType::function_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "tier", ColumnType::tier::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "code_offset_in_module", ColumnType::code_offset_in_module::SqlValueType(), false,
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
    v8_isolate_id_.ShrinkToFit();
    v8_wasm_script_id_.ShrinkToFit();
    function_name_.ShrinkToFit();
    tier_.ShrinkToFit();
    code_offset_in_module_.ShrinkToFit();
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
    mutable_v8_isolate_id()->Append(row.v8_isolate_id);
    mutable_v8_wasm_script_id()->Append(row.v8_wasm_script_id);
    mutable_function_name()->Append(row.function_name);
    mutable_tier()->Append(row.tier);
    mutable_code_offset_in_module()->Append(row.code_offset_in_module);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<V8WasmCodeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<JitCodeTable::Id>& jit_code_id() const {
    return static_cast<const ColumnType::jit_code_id&>(columns()[ColumnIndex::jit_code_id]);
  }
  const TypedColumn<V8IsolateTable::Id>& v8_isolate_id() const {
    return static_cast<const ColumnType::v8_isolate_id&>(columns()[ColumnIndex::v8_isolate_id]);
  }
  const TypedColumn<V8WasmScriptTable::Id>& v8_wasm_script_id() const {
    return static_cast<const ColumnType::v8_wasm_script_id&>(columns()[ColumnIndex::v8_wasm_script_id]);
  }
  const TypedColumn<StringPool::Id>& function_name() const {
    return static_cast<const ColumnType::function_name&>(columns()[ColumnIndex::function_name]);
  }
  const TypedColumn<StringPool::Id>& tier() const {
    return static_cast<const ColumnType::tier&>(columns()[ColumnIndex::tier]);
  }
  const TypedColumn<int32_t>& code_offset_in_module() const {
    return static_cast<const ColumnType::code_offset_in_module&>(columns()[ColumnIndex::code_offset_in_module]);
  }

  TypedColumn<JitCodeTable::Id>* mutable_jit_code_id() {
    return static_cast<ColumnType::jit_code_id*>(
        GetColumn(ColumnIndex::jit_code_id));
  }
  TypedColumn<V8IsolateTable::Id>* mutable_v8_isolate_id() {
    return static_cast<ColumnType::v8_isolate_id*>(
        GetColumn(ColumnIndex::v8_isolate_id));
  }
  TypedColumn<V8WasmScriptTable::Id>* mutable_v8_wasm_script_id() {
    return static_cast<ColumnType::v8_wasm_script_id*>(
        GetColumn(ColumnIndex::v8_wasm_script_id));
  }
  TypedColumn<StringPool::Id>* mutable_function_name() {
    return static_cast<ColumnType::function_name*>(
        GetColumn(ColumnIndex::function_name));
  }
  TypedColumn<StringPool::Id>* mutable_tier() {
    return static_cast<ColumnType::tier*>(
        GetColumn(ColumnIndex::tier));
  }
  TypedColumn<int32_t>* mutable_code_offset_in_module() {
    return static_cast<ColumnType::code_offset_in_module*>(
        GetColumn(ColumnIndex::code_offset_in_module));
  }

 private:
  
  
  ColumnStorage<ColumnType::jit_code_id::stored_type> jit_code_id_;
  ColumnStorage<ColumnType::v8_isolate_id::stored_type> v8_isolate_id_;
  ColumnStorage<ColumnType::v8_wasm_script_id::stored_type> v8_wasm_script_id_;
  ColumnStorage<ColumnType::function_name::stored_type> function_name_;
  ColumnStorage<ColumnType::tier::stored_type> tier_;
  ColumnStorage<ColumnType::code_offset_in_module::stored_type> code_offset_in_module_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> jit_code_id_storage_layer_;
  RefPtr<column::StorageLayer> v8_isolate_id_storage_layer_;
  RefPtr<column::StorageLayer> v8_wasm_script_id_storage_layer_;
  RefPtr<column::StorageLayer> function_name_storage_layer_;
  RefPtr<column::StorageLayer> tier_storage_layer_;
  RefPtr<column::StorageLayer> code_offset_in_module_storage_layer_;

  
};
  

class V8RegexpCodeTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 5;

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
    static constexpr uint32_t v8_isolate_id = 3;
    static constexpr uint32_t pattern = 4;
  };
  struct ColumnType {
    using id = IdColumn<V8RegexpCodeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using jit_code_id = TypedColumn<JitCodeTable::Id>;
    using v8_isolate_id = TypedColumn<V8IsolateTable::Id>;
    using pattern = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(JitCodeTable::Id in_jit_code_id = {},
        V8IsolateTable::Id in_v8_isolate_id = {},
        StringPool::Id in_pattern = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          jit_code_id(in_jit_code_id),
          v8_isolate_id(in_v8_isolate_id),
          pattern(in_pattern) {
      type_ = "__intrinsic_v8_regexp_code";
    }
    JitCodeTable::Id jit_code_id;
    V8IsolateTable::Id v8_isolate_id;
    StringPool::Id pattern;

    bool operator==(const V8RegexpCodeTable::Row& other) const {
      return type() == other.type() && ColumnType::jit_code_id::Equals(jit_code_id, other.jit_code_id) &&
       ColumnType::v8_isolate_id::Equals(v8_isolate_id, other.v8_isolate_id) &&
       ColumnType::pattern::Equals(pattern, other.pattern);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t jit_code_id = ColumnType::jit_code_id::default_flags();
    static constexpr uint32_t v8_isolate_id = ColumnType::v8_isolate_id::default_flags();
    static constexpr uint32_t pattern = ColumnType::pattern::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      V8RegexpCodeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    V8RegexpCodeTable, RowNumber> {
   public:
    ConstRowReference(const V8RegexpCodeTable* table, uint32_t row_number)
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
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      return table_->v8_isolate_id()[row_number_];
    }
    ColumnType::pattern::type pattern() const {
      return table_->pattern()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const V8RegexpCodeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_jit_code_id(
        ColumnType::jit_code_id::non_optional_type v) {
      return mutable_table()->mutable_jit_code_id()->Set(row_number_, v);
    }
    void set_v8_isolate_id(
        ColumnType::v8_isolate_id::non_optional_type v) {
      return mutable_table()->mutable_v8_isolate_id()->Set(row_number_, v);
    }
    void set_pattern(
        ColumnType::pattern::non_optional_type v) {
      return mutable_table()->mutable_pattern()->Set(row_number_, v);
    }

   private:
    V8RegexpCodeTable* mutable_table() const {
      return const_cast<V8RegexpCodeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, V8RegexpCodeTable, RowNumber, ConstRowReference> {
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
    ColumnType::v8_isolate_id::type v8_isolate_id() const {
      const auto& col = table_->v8_isolate_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::pattern::type pattern() const {
      const auto& col = table_->pattern();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const V8RegexpCodeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class V8RegexpCodeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, V8RegexpCodeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class V8RegexpCodeTable;

     explicit Iterator(V8RegexpCodeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     V8RegexpCodeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      V8RegexpCodeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "jit_code_id", &self->jit_code_id_, ColumnFlag::jit_code_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "v8_isolate_id", &self->v8_isolate_id_, ColumnFlag::v8_isolate_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "pattern", &self->pattern_, ColumnFlag::pattern,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit V8RegexpCodeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        jit_code_id_(ColumnStorage<ColumnType::jit_code_id::stored_type>::Create<false>()),
        v8_isolate_id_(ColumnStorage<ColumnType::v8_isolate_id::stored_type>::Create<false>()),
        pattern_(ColumnStorage<ColumnType::pattern::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        jit_code_id_storage_layer_(
        new column::NumericStorage<ColumnType::jit_code_id::non_optional_stored_type>(
          &jit_code_id_.vector(),
          ColumnTypeHelper<ColumnType::jit_code_id::stored_type>::ToColumnType(),
          false)),
        v8_isolate_id_storage_layer_(
        new column::NumericStorage<ColumnType::v8_isolate_id::non_optional_stored_type>(
          &v8_isolate_id_.vector(),
          ColumnTypeHelper<ColumnType::v8_isolate_id::stored_type>::ToColumnType(),
          false)),
        pattern_storage_layer_(
          new column::StringStorage(string_pool(), &pattern_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::jit_code_id::stored_type>(
          ColumnFlag::jit_code_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::v8_isolate_id::stored_type>(
          ColumnFlag::v8_isolate_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::pattern::stored_type>(
          ColumnFlag::pattern),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,jit_code_id_storage_layer_,v8_isolate_id_storage_layer_,pattern_storage_layer_},
      {{},{},{},{},{}});
  }
  ~V8RegexpCodeTable() override;

  static const char* Name() { return "__intrinsic_v8_regexp_code"; }

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
        "v8_isolate_id", ColumnType::v8_isolate_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "pattern", ColumnType::pattern::SqlValueType(), false,
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
    v8_isolate_id_.ShrinkToFit();
    pattern_.ShrinkToFit();
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
    mutable_v8_isolate_id()->Append(row.v8_isolate_id);
    mutable_pattern()->Append(row.pattern);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<V8RegexpCodeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<JitCodeTable::Id>& jit_code_id() const {
    return static_cast<const ColumnType::jit_code_id&>(columns()[ColumnIndex::jit_code_id]);
  }
  const TypedColumn<V8IsolateTable::Id>& v8_isolate_id() const {
    return static_cast<const ColumnType::v8_isolate_id&>(columns()[ColumnIndex::v8_isolate_id]);
  }
  const TypedColumn<StringPool::Id>& pattern() const {
    return static_cast<const ColumnType::pattern&>(columns()[ColumnIndex::pattern]);
  }

  TypedColumn<JitCodeTable::Id>* mutable_jit_code_id() {
    return static_cast<ColumnType::jit_code_id*>(
        GetColumn(ColumnIndex::jit_code_id));
  }
  TypedColumn<V8IsolateTable::Id>* mutable_v8_isolate_id() {
    return static_cast<ColumnType::v8_isolate_id*>(
        GetColumn(ColumnIndex::v8_isolate_id));
  }
  TypedColumn<StringPool::Id>* mutable_pattern() {
    return static_cast<ColumnType::pattern*>(
        GetColumn(ColumnIndex::pattern));
  }

 private:
  
  
  ColumnStorage<ColumnType::jit_code_id::stored_type> jit_code_id_;
  ColumnStorage<ColumnType::v8_isolate_id::stored_type> v8_isolate_id_;
  ColumnStorage<ColumnType::pattern::stored_type> pattern_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> jit_code_id_storage_layer_;
  RefPtr<column::StorageLayer> v8_isolate_id_storage_layer_;
  RefPtr<column::StorageLayer> pattern_storage_layer_;

  
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_V8_TABLES_PY_H_
