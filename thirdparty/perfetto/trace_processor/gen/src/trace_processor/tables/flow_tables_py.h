#ifndef SRC_TRACE_PROCESSOR_TABLES_FLOW_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_FLOW_TABLES_PY_H_

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

#include "src/trace_processor/tables/slice_tables_py.h"

namespace perfetto::trace_processor::tables {

class FlowTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t slice_out = 2;
    static constexpr uint32_t slice_in = 3;
    static constexpr uint32_t trace_id = 4;
    static constexpr uint32_t arg_set_id = 5;
  };
  struct ColumnType {
    using id = IdColumn<FlowTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using slice_out = TypedColumn<SliceTable::Id>;
    using slice_in = TypedColumn<SliceTable::Id>;
    using trace_id = TypedColumn<std::optional<int64_t>>;
    using arg_set_id = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(SliceTable::Id in_slice_out = {},
        SliceTable::Id in_slice_in = {},
        std::optional<int64_t> in_trace_id = {},
        uint32_t in_arg_set_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          slice_out(in_slice_out),
          slice_in(in_slice_in),
          trace_id(in_trace_id),
          arg_set_id(in_arg_set_id) {
      type_ = "flow";
    }
    SliceTable::Id slice_out;
    SliceTable::Id slice_in;
    std::optional<int64_t> trace_id;
    uint32_t arg_set_id;

    bool operator==(const FlowTable::Row& other) const {
      return type() == other.type() && ColumnType::slice_out::Equals(slice_out, other.slice_out) &&
       ColumnType::slice_in::Equals(slice_in, other.slice_in) &&
       ColumnType::trace_id::Equals(trace_id, other.trace_id) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t slice_out = ColumnType::slice_out::default_flags();
    static constexpr uint32_t slice_in = ColumnType::slice_in::default_flags();
    static constexpr uint32_t trace_id = ColumnType::trace_id::default_flags();
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      FlowTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    FlowTable, RowNumber> {
   public:
    ConstRowReference(const FlowTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::slice_out::type slice_out() const {
      return table_->slice_out()[row_number_];
    }
    ColumnType::slice_in::type slice_in() const {
      return table_->slice_in()[row_number_];
    }
    ColumnType::trace_id::type trace_id() const {
      return table_->trace_id()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const FlowTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_slice_out(
        ColumnType::slice_out::non_optional_type v) {
      return mutable_table()->mutable_slice_out()->Set(row_number_, v);
    }
    void set_slice_in(
        ColumnType::slice_in::non_optional_type v) {
      return mutable_table()->mutable_slice_in()->Set(row_number_, v);
    }
    void set_trace_id(
        ColumnType::trace_id::non_optional_type v) {
      return mutable_table()->mutable_trace_id()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }

   private:
    FlowTable* mutable_table() const {
      return const_cast<FlowTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, FlowTable, RowNumber, ConstRowReference> {
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
    ColumnType::slice_out::type slice_out() const {
      const auto& col = table_->slice_out();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::slice_in::type slice_in() const {
      const auto& col = table_->slice_in();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::trace_id::type trace_id() const {
      const auto& col = table_->trace_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const FlowTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class FlowTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, FlowTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class FlowTable;

     explicit Iterator(FlowTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     FlowTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      FlowTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "slice_out", &self->slice_out_, ColumnFlag::slice_out,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "slice_in", &self->slice_in_, ColumnFlag::slice_in,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "trace_id", &self->trace_id_, ColumnFlag::trace_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit FlowTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        slice_out_(ColumnStorage<ColumnType::slice_out::stored_type>::Create<false>()),
        slice_in_(ColumnStorage<ColumnType::slice_in::stored_type>::Create<false>()),
        trace_id_(ColumnStorage<ColumnType::trace_id::stored_type>::Create<false>()),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        slice_out_storage_layer_(
        new column::NumericStorage<ColumnType::slice_out::non_optional_stored_type>(
          &slice_out_.vector(),
          ColumnTypeHelper<ColumnType::slice_out::stored_type>::ToColumnType(),
          false)),
        slice_in_storage_layer_(
        new column::NumericStorage<ColumnType::slice_in::non_optional_stored_type>(
          &slice_in_.vector(),
          ColumnTypeHelper<ColumnType::slice_in::stored_type>::ToColumnType(),
          false)),
        trace_id_storage_layer_(
          new column::NumericStorage<ColumnType::trace_id::non_optional_stored_type>(
            &trace_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::trace_id::stored_type>::ToColumnType(),
            false)),
        arg_set_id_storage_layer_(
        new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
          &arg_set_id_.vector(),
          ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
          false))
,
        trace_id_null_layer_(new column::NullOverlay(trace_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::slice_out::stored_type>(
          ColumnFlag::slice_out),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::slice_in::stored_type>(
          ColumnFlag::slice_in),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::trace_id::stored_type>(
          ColumnFlag::trace_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,slice_out_storage_layer_,slice_in_storage_layer_,trace_id_storage_layer_,arg_set_id_storage_layer_},
      {{},{},{},{},trace_id_null_layer_,{}});
  }
  ~FlowTable() override;

  static const char* Name() { return "flow"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "slice_out", ColumnType::slice_out::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "slice_in", ColumnType::slice_in::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "trace_id", ColumnType::trace_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "arg_set_id", ColumnType::arg_set_id::SqlValueType(), false,
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
    slice_out_.ShrinkToFit();
    slice_in_.ShrinkToFit();
    trace_id_.ShrinkToFit();
    arg_set_id_.ShrinkToFit();
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
    mutable_slice_out()->Append(row.slice_out);
    mutable_slice_in()->Append(row.slice_in);
    mutable_trace_id()->Append(row.trace_id);
    mutable_arg_set_id()->Append(row.arg_set_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<FlowTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<SliceTable::Id>& slice_out() const {
    return static_cast<const ColumnType::slice_out&>(columns()[ColumnIndex::slice_out]);
  }
  const TypedColumn<SliceTable::Id>& slice_in() const {
    return static_cast<const ColumnType::slice_in&>(columns()[ColumnIndex::slice_in]);
  }
  const TypedColumn<std::optional<int64_t>>& trace_id() const {
    return static_cast<const ColumnType::trace_id&>(columns()[ColumnIndex::trace_id]);
  }
  const TypedColumn<uint32_t>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }

  TypedColumn<SliceTable::Id>* mutable_slice_out() {
    return static_cast<ColumnType::slice_out*>(
        GetColumn(ColumnIndex::slice_out));
  }
  TypedColumn<SliceTable::Id>* mutable_slice_in() {
    return static_cast<ColumnType::slice_in*>(
        GetColumn(ColumnIndex::slice_in));
  }
  TypedColumn<std::optional<int64_t>>* mutable_trace_id() {
    return static_cast<ColumnType::trace_id*>(
        GetColumn(ColumnIndex::trace_id));
  }
  TypedColumn<uint32_t>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::slice_out::stored_type> slice_out_;
  ColumnStorage<ColumnType::slice_in::stored_type> slice_in_;
  ColumnStorage<ColumnType::trace_id::stored_type> trace_id_;
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> slice_out_storage_layer_;
  RefPtr<column::StorageLayer> slice_in_storage_layer_;
  RefPtr<column::StorageLayer> trace_id_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;

  RefPtr<column::OverlayLayer> trace_id_null_layer_;
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_FLOW_TABLES_PY_H_
