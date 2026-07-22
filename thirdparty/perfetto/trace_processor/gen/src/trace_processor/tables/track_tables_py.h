#ifndef SRC_TRACE_PROCESSOR_TABLES_TRACK_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_TRACK_TABLES_PY_H_

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

#include "src/trace_processor/tables/metadata_tables_py.h"

namespace perfetto::trace_processor::tables {

class TrackTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
  };
  struct ColumnType {
    using id = IdColumn<TrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<TrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<TrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          name(in_name),
          parent_id(in_parent_id),
          source_arg_set_id(in_source_arg_set_id),
          machine_id(in_machine_id) {
      type_ = "track";
    }
    StringPool::Id name;
    std::optional<TrackTable::Id> parent_id;
    std::optional<uint32_t> source_arg_set_id;
    std::optional<MachineTable::Id> machine_id;

    bool operator==(const TrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t parent_id = ColumnType::parent_id::default_flags();
    static constexpr uint32_t source_arg_set_id = ColumnType::source_arg_set_id::default_flags();
    static constexpr uint32_t machine_id = ColumnType::machine_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      TrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    TrackTable, RowNumber> {
   public:
    ConstRowReference(const TrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const TrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }

   private:
    TrackTable* mutable_table() const {
      return const_cast<TrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, TrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const TrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class TrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, TrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class TrackTable;

     explicit Iterator(TrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     TrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      TrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "parent_id", &self->parent_id_, ColumnFlag::parent_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "source_arg_set_id", &self->source_arg_set_id_, ColumnFlag::source_arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "machine_id", &self->machine_id_, ColumnFlag::machine_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit TrackTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        parent_id_(ColumnStorage<ColumnType::parent_id::stored_type>::Create<false>()),
        source_arg_set_id_(ColumnStorage<ColumnType::source_arg_set_id::stored_type>::Create<false>()),
        machine_id_(ColumnStorage<ColumnType::machine_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        parent_id_storage_layer_(
          new column::NumericStorage<ColumnType::parent_id::non_optional_stored_type>(
            &parent_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_id::stored_type>::ToColumnType(),
            false)),
        source_arg_set_id_storage_layer_(
          new column::NumericStorage<ColumnType::source_arg_set_id::non_optional_stored_type>(
            &source_arg_set_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::source_arg_set_id::stored_type>::ToColumnType(),
            false)),
        machine_id_storage_layer_(
          new column::NumericStorage<ColumnType::machine_id::non_optional_stored_type>(
            &machine_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::machine_id::stored_type>::ToColumnType(),
            false))
,
        parent_id_null_layer_(new column::NullOverlay(parent_id_.bv())),
        source_arg_set_id_null_layer_(new column::NullOverlay(source_arg_set_id_.bv())),
        machine_id_null_layer_(new column::NullOverlay(machine_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_id::stored_type>(
          ColumnFlag::parent_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::source_arg_set_id::stored_type>(
          ColumnFlag::source_arg_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::machine_id::stored_type>(
          ColumnFlag::machine_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,name_storage_layer_,parent_id_storage_layer_,source_arg_set_id_storage_layer_,machine_id_storage_layer_},
      {{},{},{},parent_id_null_layer_,source_arg_set_id_null_layer_,machine_id_null_layer_});
  }
  ~TrackTable() override;

  static const char* Name() { return "track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
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
    parent_id_.ShrinkToFit();
    source_arg_set_id_.ShrinkToFit();
    machine_id_.ShrinkToFit();
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
    mutable_parent_id()->Append(row.parent_id);
    mutable_source_arg_set_id()->Append(row.source_arg_set_id);
    mutable_machine_id()->Append(row.machine_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<TrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<TrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<TrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::parent_id::stored_type> parent_id_;
  ColumnStorage<ColumnType::source_arg_set_id::stored_type> source_arg_set_id_;
  ColumnStorage<ColumnType::machine_id::stored_type> machine_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> parent_id_storage_layer_;
  RefPtr<column::StorageLayer> source_arg_set_id_storage_layer_;
  RefPtr<column::StorageLayer> machine_id_storage_layer_;

  RefPtr<column::OverlayLayer> parent_id_null_layer_;
  RefPtr<column::OverlayLayer> source_arg_set_id_null_layer_;
  RefPtr<column::OverlayLayer> machine_id_null_layer_;
};
  

class CounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 8;

  using Id = TrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
  };
  struct ColumnType {
    using id = IdColumn<CounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<CounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
  };
  struct Row : public TrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<CounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        std::nullptr_t = nullptr)
        : TrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id),
          unit(in_unit),
          description(in_description) {
      type_ = "counter_track";
    }
    StringPool::Id unit;
    StringPool::Id description;

    bool operator==(const CounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t unit = ColumnType::unit::default_flags();
    static constexpr uint32_t description = ColumnType::description::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      CounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    CounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const CounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const CounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }

   private:
    CounterTrackTable* mutable_table() const {
      return const_cast<CounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, CounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const CounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class CounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, CounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class CounterTrackTable;

     explicit Iterator(CounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     CounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      CounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "unit", &self->unit_, ColumnFlag::unit,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "description", &self->description_, ColumnFlag::description,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit CounterTrackTable(StringPool* pool, TrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), unit_(ColumnStorage<ColumnType::unit::stored_type>::Create<false>()),
        description_(ColumnStorage<ColumnType::description::stored_type>::Create<false>())
,
        unit_storage_layer_(
          new column::StringStorage(string_pool(), &unit_.vector())),
        description_storage_layer_(
          new column::StringStorage(string_pool(), &description_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::unit::stored_type>(
          ColumnFlag::unit),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::description::stored_type>(
          ColumnFlag::description),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],unit_storage_layer_,description_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{}});
  }
  ~CounterTrackTable() override;

  static const char* Name() { return "counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
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
    unit_.ShrinkToFit();
    description_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_unit()->Append(row.unit);
    mutable_description()->Append(row.description);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const TrackTable& parent,
      ColumnStorage<ColumnType::unit::stored_type> unit
, ColumnStorage<ColumnType::description::stored_type> description) {
    return std::unique_ptr<Table>(new CounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(unit), std::move(description)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const TrackTable& parent,
      std::vector<TrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::unit::stored_type> unit
, ColumnStorage<ColumnType::description::stored_type> description) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new CounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(unit), std::move(description)));
  }

  const IdColumn<CounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<CounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<CounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }

 private:
  CounterTrackTable(StringPool* pool,
            const TrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::unit::stored_type> unit
, ColumnStorage<ColumnType::description::stored_type> description)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        unit_storage_layer_(
          new column::StringStorage(string_pool(), &unit_.vector())),
        description_storage_layer_(
          new column::StringStorage(string_pool(), &description_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::unit::stored_type>(
          ColumnFlag::unit),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::description::stored_type>(
          ColumnFlag::description),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(unit.size() == parent_overlay.size());
    unit_ = std::move(unit);
    PERFETTO_DCHECK(description.size() == parent_overlay.size());
    description_ = std::move(description);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],unit_storage_layer_,description_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{}}, std::move(overlay_layers));
  }
  TrackTable* parent_ = nullptr;
  const TrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::unit::stored_type> unit_;
  ColumnStorage<ColumnType::description::stored_type> description_;

  RefPtr<column::StorageLayer> unit_storage_layer_;
  RefPtr<column::StorageLayer> description_storage_layer_;

  
};
  

class CpuCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  using Id = CounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t ucpu = 8;
  };
  struct ColumnType {
    using id = IdColumn<CpuCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<CpuCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using ucpu = TypedColumn<CpuTable::Id>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<CpuCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        CpuTable::Id in_ucpu = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          ucpu(in_ucpu) {
      type_ = "__intrinsic_cpu_counter_track";
    }
    CpuTable::Id ucpu;

    bool operator==(const CpuCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::ucpu::Equals(ucpu, other.ucpu);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ucpu = ColumnType::ucpu::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      CpuCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    CpuCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const CpuCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::ucpu::type ucpu() const {
      return table_->ucpu()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const CpuCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_ucpu(
        ColumnType::ucpu::non_optional_type v) {
      return mutable_table()->mutable_ucpu()->Set(row_number_, v);
    }

   private:
    CpuCounterTrackTable* mutable_table() const {
      return const_cast<CpuCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, CpuCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ucpu::type ucpu() const {
      const auto& col = table_->ucpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const CpuCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class CpuCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, CpuCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class CpuCounterTrackTable;

     explicit Iterator(CpuCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     CpuCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      CpuCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ucpu", &self->ucpu_, ColumnFlag::ucpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit CpuCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), ucpu_(ColumnStorage<ColumnType::ucpu::stored_type>::Create<false>())
,
        ucpu_storage_layer_(
        new column::NumericStorage<ColumnType::ucpu::non_optional_stored_type>(
          &ucpu_.vector(),
          ColumnTypeHelper<ColumnType::ucpu::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ucpu::stored_type>(
          ColumnFlag::ucpu),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],ucpu_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}});
  }
  ~CpuCounterTrackTable() override;

  static const char* Name() { return "__intrinsic_cpu_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ucpu", ColumnType::ucpu::SqlValueType(), false,
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
    ucpu_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_ucpu()->Append(row.ucpu);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::ucpu::stored_type> ucpu) {
    return std::unique_ptr<Table>(new CpuCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(ucpu)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::ucpu::stored_type> ucpu) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new CpuCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(ucpu)));
  }

  const IdColumn<CpuCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<CpuCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<CpuTable::Id>& ucpu() const {
    return static_cast<const ColumnType::ucpu&>(columns()[ColumnIndex::ucpu]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<CpuCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<CpuTable::Id>* mutable_ucpu() {
    return static_cast<ColumnType::ucpu*>(
        GetColumn(ColumnIndex::ucpu));
  }

 private:
  CpuCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::ucpu::stored_type> ucpu)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        ucpu_storage_layer_(
        new column::NumericStorage<ColumnType::ucpu::non_optional_stored_type>(
          &ucpu_.vector(),
          ColumnTypeHelper<ColumnType::ucpu::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ucpu::stored_type>(
          ColumnFlag::ucpu),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(ucpu.size() == parent_overlay.size());
    ucpu_ = std::move(ucpu);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],ucpu_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::ucpu::stored_type> ucpu_;

  RefPtr<column::StorageLayer> ucpu_storage_layer_;

  
};
  

class CpuTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 7;

  using Id = TrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t ucpu = 6;
  };
  struct ColumnType {
    using id = IdColumn<CpuTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<CpuTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using ucpu = TypedColumn<CpuTable::Id>;
  };
  struct Row : public TrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<CpuTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        CpuTable::Id in_ucpu = {},
        std::nullptr_t = nullptr)
        : TrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id),
          ucpu(in_ucpu) {
      type_ = "__intrinsic_cpu_track";
    }
    CpuTable::Id ucpu;

    bool operator==(const CpuTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::ucpu::Equals(ucpu, other.ucpu);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ucpu = ColumnType::ucpu::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      CpuTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    CpuTrackTable, RowNumber> {
   public:
    ConstRowReference(const CpuTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::ucpu::type ucpu() const {
      return table_->ucpu()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const CpuTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_ucpu(
        ColumnType::ucpu::non_optional_type v) {
      return mutable_table()->mutable_ucpu()->Set(row_number_, v);
    }

   private:
    CpuTrackTable* mutable_table() const {
      return const_cast<CpuTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, CpuTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ucpu::type ucpu() const {
      const auto& col = table_->ucpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const CpuTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class CpuTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, CpuTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class CpuTrackTable;

     explicit Iterator(CpuTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     CpuTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      CpuTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ucpu", &self->ucpu_, ColumnFlag::ucpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit CpuTrackTable(StringPool* pool, TrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), ucpu_(ColumnStorage<ColumnType::ucpu::stored_type>::Create<false>())
,
        ucpu_storage_layer_(
        new column::NumericStorage<ColumnType::ucpu::non_optional_stored_type>(
          &ucpu_.vector(),
          ColumnTypeHelper<ColumnType::ucpu::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ucpu::stored_type>(
          ColumnFlag::ucpu),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],ucpu_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{}});
  }
  ~CpuTrackTable() override;

  static const char* Name() { return "__intrinsic_cpu_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ucpu", ColumnType::ucpu::SqlValueType(), false,
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
    ucpu_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_ucpu()->Append(row.ucpu);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const TrackTable& parent,
      ColumnStorage<ColumnType::ucpu::stored_type> ucpu) {
    return std::unique_ptr<Table>(new CpuTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(ucpu)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const TrackTable& parent,
      std::vector<TrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::ucpu::stored_type> ucpu) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new CpuTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(ucpu)));
  }

  const IdColumn<CpuTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<CpuTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<CpuTable::Id>& ucpu() const {
    return static_cast<const ColumnType::ucpu&>(columns()[ColumnIndex::ucpu]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<CpuTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<CpuTable::Id>* mutable_ucpu() {
    return static_cast<ColumnType::ucpu*>(
        GetColumn(ColumnIndex::ucpu));
  }

 private:
  CpuTrackTable(StringPool* pool,
            const TrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::ucpu::stored_type> ucpu)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        ucpu_storage_layer_(
        new column::NumericStorage<ColumnType::ucpu::non_optional_stored_type>(
          &ucpu_.vector(),
          ColumnTypeHelper<ColumnType::ucpu::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ucpu::stored_type>(
          ColumnFlag::ucpu),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(ucpu.size() == parent_overlay.size());
    ucpu_ = std::move(ucpu);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],ucpu_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{}}, std::move(overlay_layers));
  }
  TrackTable* parent_ = nullptr;
  const TrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::ucpu::stored_type> ucpu_;

  RefPtr<column::StorageLayer> ucpu_storage_layer_;

  
};
  

class EnergyCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 11;

  using Id = CounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t consumer_id = 8;
    static constexpr uint32_t consumer_type = 9;
    static constexpr uint32_t ordinal = 10;
  };
  struct ColumnType {
    using id = IdColumn<EnergyCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<EnergyCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using consumer_id = TypedColumn<int32_t>;
    using consumer_type = TypedColumn<StringPool::Id>;
    using ordinal = TypedColumn<int32_t>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<EnergyCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        int32_t in_consumer_id = {},
        StringPool::Id in_consumer_type = {},
        int32_t in_ordinal = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          consumer_id(in_consumer_id),
          consumer_type(in_consumer_type),
          ordinal(in_ordinal) {
      type_ = "energy_counter_track";
    }
    int32_t consumer_id;
    StringPool::Id consumer_type;
    int32_t ordinal;

    bool operator==(const EnergyCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::consumer_id::Equals(consumer_id, other.consumer_id) &&
       ColumnType::consumer_type::Equals(consumer_type, other.consumer_type) &&
       ColumnType::ordinal::Equals(ordinal, other.ordinal);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t consumer_id = ColumnType::consumer_id::default_flags();
    static constexpr uint32_t consumer_type = ColumnType::consumer_type::default_flags();
    static constexpr uint32_t ordinal = ColumnType::ordinal::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      EnergyCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    EnergyCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const EnergyCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::consumer_id::type consumer_id() const {
      return table_->consumer_id()[row_number_];
    }
    ColumnType::consumer_type::type consumer_type() const {
      return table_->consumer_type()[row_number_];
    }
    ColumnType::ordinal::type ordinal() const {
      return table_->ordinal()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const EnergyCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_consumer_id(
        ColumnType::consumer_id::non_optional_type v) {
      return mutable_table()->mutable_consumer_id()->Set(row_number_, v);
    }
    void set_consumer_type(
        ColumnType::consumer_type::non_optional_type v) {
      return mutable_table()->mutable_consumer_type()->Set(row_number_, v);
    }
    void set_ordinal(
        ColumnType::ordinal::non_optional_type v) {
      return mutable_table()->mutable_ordinal()->Set(row_number_, v);
    }

   private:
    EnergyCounterTrackTable* mutable_table() const {
      return const_cast<EnergyCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, EnergyCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::consumer_id::type consumer_id() const {
      const auto& col = table_->consumer_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::consumer_type::type consumer_type() const {
      const auto& col = table_->consumer_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ordinal::type ordinal() const {
      const auto& col = table_->ordinal();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const EnergyCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class EnergyCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, EnergyCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class EnergyCounterTrackTable;

     explicit Iterator(EnergyCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     EnergyCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      EnergyCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "consumer_id", &self->consumer_id_, ColumnFlag::consumer_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "consumer_type", &self->consumer_type_, ColumnFlag::consumer_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ordinal", &self->ordinal_, ColumnFlag::ordinal,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit EnergyCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), consumer_id_(ColumnStorage<ColumnType::consumer_id::stored_type>::Create<false>()),
        consumer_type_(ColumnStorage<ColumnType::consumer_type::stored_type>::Create<false>()),
        ordinal_(ColumnStorage<ColumnType::ordinal::stored_type>::Create<false>())
,
        consumer_id_storage_layer_(
        new column::NumericStorage<ColumnType::consumer_id::non_optional_stored_type>(
          &consumer_id_.vector(),
          ColumnTypeHelper<ColumnType::consumer_id::stored_type>::ToColumnType(),
          false)),
        consumer_type_storage_layer_(
          new column::StringStorage(string_pool(), &consumer_type_.vector())),
        ordinal_storage_layer_(
        new column::NumericStorage<ColumnType::ordinal::non_optional_stored_type>(
          &ordinal_.vector(),
          ColumnTypeHelper<ColumnType::ordinal::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::consumer_id::stored_type>(
          ColumnFlag::consumer_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::consumer_type::stored_type>(
          ColumnFlag::consumer_type),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ordinal::stored_type>(
          ColumnFlag::ordinal),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],consumer_id_storage_layer_,consumer_type_storage_layer_,ordinal_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{},{},{}});
  }
  ~EnergyCounterTrackTable() override;

  static const char* Name() { return "energy_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "consumer_id", ColumnType::consumer_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "consumer_type", ColumnType::consumer_type::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ordinal", ColumnType::ordinal::SqlValueType(), false,
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
    consumer_id_.ShrinkToFit();
    consumer_type_.ShrinkToFit();
    ordinal_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_consumer_id()->Append(row.consumer_id);
    mutable_consumer_type()->Append(row.consumer_type);
    mutable_ordinal()->Append(row.ordinal);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::consumer_id::stored_type> consumer_id
, ColumnStorage<ColumnType::consumer_type::stored_type> consumer_type
, ColumnStorage<ColumnType::ordinal::stored_type> ordinal) {
    return std::unique_ptr<Table>(new EnergyCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(consumer_id), std::move(consumer_type), std::move(ordinal)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::consumer_id::stored_type> consumer_id
, ColumnStorage<ColumnType::consumer_type::stored_type> consumer_type
, ColumnStorage<ColumnType::ordinal::stored_type> ordinal) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new EnergyCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(consumer_id), std::move(consumer_type), std::move(ordinal)));
  }

  const IdColumn<EnergyCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<EnergyCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<int32_t>& consumer_id() const {
    return static_cast<const ColumnType::consumer_id&>(columns()[ColumnIndex::consumer_id]);
  }
  const TypedColumn<StringPool::Id>& consumer_type() const {
    return static_cast<const ColumnType::consumer_type&>(columns()[ColumnIndex::consumer_type]);
  }
  const TypedColumn<int32_t>& ordinal() const {
    return static_cast<const ColumnType::ordinal&>(columns()[ColumnIndex::ordinal]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<EnergyCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<int32_t>* mutable_consumer_id() {
    return static_cast<ColumnType::consumer_id*>(
        GetColumn(ColumnIndex::consumer_id));
  }
  TypedColumn<StringPool::Id>* mutable_consumer_type() {
    return static_cast<ColumnType::consumer_type*>(
        GetColumn(ColumnIndex::consumer_type));
  }
  TypedColumn<int32_t>* mutable_ordinal() {
    return static_cast<ColumnType::ordinal*>(
        GetColumn(ColumnIndex::ordinal));
  }

 private:
  EnergyCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::consumer_id::stored_type> consumer_id
, ColumnStorage<ColumnType::consumer_type::stored_type> consumer_type
, ColumnStorage<ColumnType::ordinal::stored_type> ordinal)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        consumer_id_storage_layer_(
        new column::NumericStorage<ColumnType::consumer_id::non_optional_stored_type>(
          &consumer_id_.vector(),
          ColumnTypeHelper<ColumnType::consumer_id::stored_type>::ToColumnType(),
          false)),
        consumer_type_storage_layer_(
          new column::StringStorage(string_pool(), &consumer_type_.vector())),
        ordinal_storage_layer_(
        new column::NumericStorage<ColumnType::ordinal::non_optional_stored_type>(
          &ordinal_.vector(),
          ColumnTypeHelper<ColumnType::ordinal::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::consumer_id::stored_type>(
          ColumnFlag::consumer_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::consumer_type::stored_type>(
          ColumnFlag::consumer_type),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ordinal::stored_type>(
          ColumnFlag::ordinal),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(consumer_id.size() == parent_overlay.size());
    consumer_id_ = std::move(consumer_id);
    PERFETTO_DCHECK(consumer_type.size() == parent_overlay.size());
    consumer_type_ = std::move(consumer_type);
    PERFETTO_DCHECK(ordinal.size() == parent_overlay.size());
    ordinal_ = std::move(ordinal);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],consumer_id_storage_layer_,consumer_type_storage_layer_,ordinal_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::consumer_id::stored_type> consumer_id_;
  ColumnStorage<ColumnType::consumer_type::stored_type> consumer_type_;
  ColumnStorage<ColumnType::ordinal::stored_type> ordinal_;

  RefPtr<column::StorageLayer> consumer_id_storage_layer_;
  RefPtr<column::StorageLayer> consumer_type_storage_layer_;
  RefPtr<column::StorageLayer> ordinal_storage_layer_;

  
};
  

class UidCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  using Id = CounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t uid = 8;
  };
  struct ColumnType {
    using id = IdColumn<UidCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<UidCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using uid = TypedColumn<int32_t>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<UidCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        int32_t in_uid = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          uid(in_uid) {
      type_ = "uid_counter_track";
    }
    int32_t uid;

    bool operator==(const UidCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::uid::Equals(uid, other.uid);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t uid = ColumnType::uid::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      UidCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    UidCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const UidCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::uid::type uid() const {
      return table_->uid()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const UidCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_uid(
        ColumnType::uid::non_optional_type v) {
      return mutable_table()->mutable_uid()->Set(row_number_, v);
    }

   private:
    UidCounterTrackTable* mutable_table() const {
      return const_cast<UidCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, UidCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::uid::type uid() const {
      const auto& col = table_->uid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const UidCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class UidCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, UidCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class UidCounterTrackTable;

     explicit Iterator(UidCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     UidCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      UidCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "uid", &self->uid_, ColumnFlag::uid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit UidCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), uid_(ColumnStorage<ColumnType::uid::stored_type>::Create<false>())
,
        uid_storage_layer_(
        new column::NumericStorage<ColumnType::uid::non_optional_stored_type>(
          &uid_.vector(),
          ColumnTypeHelper<ColumnType::uid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::uid::stored_type>(
          ColumnFlag::uid),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],uid_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}});
  }
  ~UidCounterTrackTable() override;

  static const char* Name() { return "uid_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "uid", ColumnType::uid::SqlValueType(), false,
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
    uid_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_uid()->Append(row.uid);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::uid::stored_type> uid) {
    return std::unique_ptr<Table>(new UidCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(uid)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::uid::stored_type> uid) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new UidCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(uid)));
  }

  const IdColumn<UidCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<UidCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<int32_t>& uid() const {
    return static_cast<const ColumnType::uid&>(columns()[ColumnIndex::uid]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<UidCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<int32_t>* mutable_uid() {
    return static_cast<ColumnType::uid*>(
        GetColumn(ColumnIndex::uid));
  }

 private:
  UidCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::uid::stored_type> uid)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        uid_storage_layer_(
        new column::NumericStorage<ColumnType::uid::non_optional_stored_type>(
          &uid_.vector(),
          ColumnTypeHelper<ColumnType::uid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::uid::stored_type>(
          ColumnFlag::uid),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(uid.size() == parent_overlay.size());
    uid_ = std::move(uid);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],uid_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::uid::stored_type> uid_;

  RefPtr<column::StorageLayer> uid_storage_layer_;

  
};
  

class EnergyPerUidCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 10;

  using Id = UidCounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t uid = 8;
    static constexpr uint32_t consumer_id = 9;
  };
  struct ColumnType {
    using id = IdColumn<EnergyPerUidCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<EnergyPerUidCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using uid = TypedColumn<int32_t>;
    using consumer_id = TypedColumn<int32_t>;
  };
  struct Row : public UidCounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<EnergyPerUidCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        int32_t in_uid = {},
        int32_t in_consumer_id = {},
        std::nullptr_t = nullptr)
        : UidCounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description, in_uid),
          consumer_id(in_consumer_id) {
      type_ = "energy_per_uid_counter_track";
    }
    int32_t consumer_id;

    bool operator==(const EnergyPerUidCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::uid::Equals(uid, other.uid) &&
       ColumnType::consumer_id::Equals(consumer_id, other.consumer_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t consumer_id = ColumnType::consumer_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      EnergyPerUidCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    EnergyPerUidCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const EnergyPerUidCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::uid::type uid() const {
      return table_->uid()[row_number_];
    }
    ColumnType::consumer_id::type consumer_id() const {
      return table_->consumer_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const EnergyPerUidCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_uid(
        ColumnType::uid::non_optional_type v) {
      return mutable_table()->mutable_uid()->Set(row_number_, v);
    }
    void set_consumer_id(
        ColumnType::consumer_id::non_optional_type v) {
      return mutable_table()->mutable_consumer_id()->Set(row_number_, v);
    }

   private:
    EnergyPerUidCounterTrackTable* mutable_table() const {
      return const_cast<EnergyPerUidCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, EnergyPerUidCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::uid::type uid() const {
      const auto& col = table_->uid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::consumer_id::type consumer_id() const {
      const auto& col = table_->consumer_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const EnergyPerUidCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class EnergyPerUidCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, EnergyPerUidCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class EnergyPerUidCounterTrackTable;

     explicit Iterator(EnergyPerUidCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     EnergyPerUidCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      EnergyPerUidCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "consumer_id", &self->consumer_id_, ColumnFlag::consumer_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit EnergyPerUidCounterTrackTable(StringPool* pool, UidCounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), consumer_id_(ColumnStorage<ColumnType::consumer_id::stored_type>::Create<false>())
,
        consumer_id_storage_layer_(
        new column::NumericStorage<ColumnType::consumer_id::non_optional_stored_type>(
          &consumer_id_.vector(),
          ColumnTypeHelper<ColumnType::consumer_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::consumer_id::stored_type>(
          ColumnFlag::consumer_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],const_parent_->storage_layers()[ColumnIndex::uid],consumer_id_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{},{}});
  }
  ~EnergyPerUidCounterTrackTable() override;

  static const char* Name() { return "energy_per_uid_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "uid", ColumnType::uid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "consumer_id", ColumnType::consumer_id::SqlValueType(), false,
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
    consumer_id_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_consumer_id()->Append(row.consumer_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const UidCounterTrackTable& parent,
      ColumnStorage<ColumnType::consumer_id::stored_type> consumer_id) {
    return std::unique_ptr<Table>(new EnergyPerUidCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(consumer_id)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const UidCounterTrackTable& parent,
      std::vector<UidCounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::consumer_id::stored_type> consumer_id) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new EnergyPerUidCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(consumer_id)));
  }

  const IdColumn<EnergyPerUidCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<EnergyPerUidCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<int32_t>& uid() const {
    return static_cast<const ColumnType::uid&>(columns()[ColumnIndex::uid]);
  }
  const TypedColumn<int32_t>& consumer_id() const {
    return static_cast<const ColumnType::consumer_id&>(columns()[ColumnIndex::consumer_id]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<EnergyPerUidCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<int32_t>* mutable_uid() {
    return static_cast<ColumnType::uid*>(
        GetColumn(ColumnIndex::uid));
  }
  TypedColumn<int32_t>* mutable_consumer_id() {
    return static_cast<ColumnType::consumer_id*>(
        GetColumn(ColumnIndex::consumer_id));
  }

 private:
  EnergyPerUidCounterTrackTable(StringPool* pool,
            const UidCounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::consumer_id::stored_type> consumer_id)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        consumer_id_storage_layer_(
        new column::NumericStorage<ColumnType::consumer_id::non_optional_stored_type>(
          &consumer_id_.vector(),
          ColumnTypeHelper<ColumnType::consumer_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::consumer_id::stored_type>(
          ColumnFlag::consumer_id),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(consumer_id.size() == parent_overlay.size());
    consumer_id_ = std::move(consumer_id);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],const_parent_->storage_layers()[ColumnIndex::uid],consumer_id_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{},{}}, std::move(overlay_layers));
  }
  UidCounterTrackTable* parent_ = nullptr;
  const UidCounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::consumer_id::stored_type> consumer_id_;

  RefPtr<column::StorageLayer> consumer_id_storage_layer_;

  
};
  

class GpuCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  using Id = CounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t gpu_id = 8;
  };
  struct ColumnType {
    using id = IdColumn<GpuCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<GpuCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using gpu_id = TypedColumn<uint32_t>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<GpuCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        uint32_t in_gpu_id = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          gpu_id(in_gpu_id) {
      type_ = "gpu_counter_track";
    }
    uint32_t gpu_id;

    bool operator==(const GpuCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::gpu_id::Equals(gpu_id, other.gpu_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t gpu_id = ColumnType::gpu_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      GpuCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    GpuCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const GpuCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::gpu_id::type gpu_id() const {
      return table_->gpu_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const GpuCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_gpu_id(
        ColumnType::gpu_id::non_optional_type v) {
      return mutable_table()->mutable_gpu_id()->Set(row_number_, v);
    }

   private:
    GpuCounterTrackTable* mutable_table() const {
      return const_cast<GpuCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, GpuCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::gpu_id::type gpu_id() const {
      const auto& col = table_->gpu_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const GpuCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class GpuCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, GpuCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class GpuCounterTrackTable;

     explicit Iterator(GpuCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     GpuCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      GpuCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "gpu_id", &self->gpu_id_, ColumnFlag::gpu_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit GpuCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), gpu_id_(ColumnStorage<ColumnType::gpu_id::stored_type>::Create<false>())
,
        gpu_id_storage_layer_(
        new column::NumericStorage<ColumnType::gpu_id::non_optional_stored_type>(
          &gpu_id_.vector(),
          ColumnTypeHelper<ColumnType::gpu_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::gpu_id::stored_type>(
          ColumnFlag::gpu_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],gpu_id_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}});
  }
  ~GpuCounterTrackTable() override;

  static const char* Name() { return "gpu_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "gpu_id", ColumnType::gpu_id::SqlValueType(), false,
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
    gpu_id_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_gpu_id()->Append(row.gpu_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::gpu_id::stored_type> gpu_id) {
    return std::unique_ptr<Table>(new GpuCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(gpu_id)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::gpu_id::stored_type> gpu_id) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new GpuCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(gpu_id)));
  }

  const IdColumn<GpuCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<GpuCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<uint32_t>& gpu_id() const {
    return static_cast<const ColumnType::gpu_id&>(columns()[ColumnIndex::gpu_id]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<GpuCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<uint32_t>* mutable_gpu_id() {
    return static_cast<ColumnType::gpu_id*>(
        GetColumn(ColumnIndex::gpu_id));
  }

 private:
  GpuCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::gpu_id::stored_type> gpu_id)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        gpu_id_storage_layer_(
        new column::NumericStorage<ColumnType::gpu_id::non_optional_stored_type>(
          &gpu_id_.vector(),
          ColumnTypeHelper<ColumnType::gpu_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::gpu_id::stored_type>(
          ColumnFlag::gpu_id),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(gpu_id.size() == parent_overlay.size());
    gpu_id_ = std::move(gpu_id);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],gpu_id_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::gpu_id::stored_type> gpu_id_;

  RefPtr<column::StorageLayer> gpu_id_storage_layer_;

  
};
  

class GpuTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  using Id = TrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t scope = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t context_id = 8;
  };
  struct ColumnType {
    using id = IdColumn<GpuTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<GpuTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using scope = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using context_id = TypedColumn<std::optional<int64_t>>;
  };
  struct Row : public TrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<GpuTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_scope = {},
        StringPool::Id in_description = {},
        std::optional<int64_t> in_context_id = {},
        std::nullptr_t = nullptr)
        : TrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id),
          scope(in_scope),
          description(in_description),
          context_id(in_context_id) {
      type_ = "gpu_track";
    }
    StringPool::Id scope;
    StringPool::Id description;
    std::optional<int64_t> context_id;

    bool operator==(const GpuTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::scope::Equals(scope, other.scope) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::context_id::Equals(context_id, other.context_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t scope = ColumnType::scope::default_flags();
    static constexpr uint32_t description = ColumnType::description::default_flags();
    static constexpr uint32_t context_id = ColumnType::context_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      GpuTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    GpuTrackTable, RowNumber> {
   public:
    ConstRowReference(const GpuTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::scope::type scope() const {
      return table_->scope()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::context_id::type context_id() const {
      return table_->context_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const GpuTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_scope(
        ColumnType::scope::non_optional_type v) {
      return mutable_table()->mutable_scope()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_context_id(
        ColumnType::context_id::non_optional_type v) {
      return mutable_table()->mutable_context_id()->Set(row_number_, v);
    }

   private:
    GpuTrackTable* mutable_table() const {
      return const_cast<GpuTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, GpuTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::scope::type scope() const {
      const auto& col = table_->scope();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::context_id::type context_id() const {
      const auto& col = table_->context_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const GpuTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class GpuTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, GpuTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class GpuTrackTable;

     explicit Iterator(GpuTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     GpuTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      GpuTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "scope", &self->scope_, ColumnFlag::scope,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "description", &self->description_, ColumnFlag::description,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "context_id", &self->context_id_, ColumnFlag::context_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit GpuTrackTable(StringPool* pool, TrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), scope_(ColumnStorage<ColumnType::scope::stored_type>::Create<false>()),
        description_(ColumnStorage<ColumnType::description::stored_type>::Create<false>()),
        context_id_(ColumnStorage<ColumnType::context_id::stored_type>::Create<false>())
,
        scope_storage_layer_(
          new column::StringStorage(string_pool(), &scope_.vector())),
        description_storage_layer_(
          new column::StringStorage(string_pool(), &description_.vector())),
        context_id_storage_layer_(
          new column::NumericStorage<ColumnType::context_id::non_optional_stored_type>(
            &context_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::context_id::stored_type>::ToColumnType(),
            false))
,
        context_id_null_layer_(new column::NullOverlay(context_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::scope::stored_type>(
          ColumnFlag::scope),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::description::stored_type>(
          ColumnFlag::description),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::context_id::stored_type>(
          ColumnFlag::context_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],scope_storage_layer_,description_storage_layer_,context_id_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},context_id_null_layer_});
  }
  ~GpuTrackTable() override;

  static const char* Name() { return "gpu_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "scope", ColumnType::scope::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "context_id", ColumnType::context_id::SqlValueType(), false,
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
    scope_.ShrinkToFit();
    description_.ShrinkToFit();
    context_id_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_scope()->Append(row.scope);
    mutable_description()->Append(row.description);
    mutable_context_id()->Append(row.context_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const TrackTable& parent,
      ColumnStorage<ColumnType::scope::stored_type> scope
, ColumnStorage<ColumnType::description::stored_type> description
, ColumnStorage<ColumnType::context_id::stored_type> context_id) {
    return std::unique_ptr<Table>(new GpuTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(scope), std::move(description), std::move(context_id)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const TrackTable& parent,
      std::vector<TrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::scope::stored_type> scope
, ColumnStorage<ColumnType::description::stored_type> description
, ColumnStorage<ColumnType::context_id::stored_type> context_id) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new GpuTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(scope), std::move(description), std::move(context_id)));
  }

  const IdColumn<GpuTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<GpuTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& scope() const {
    return static_cast<const ColumnType::scope&>(columns()[ColumnIndex::scope]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<std::optional<int64_t>>& context_id() const {
    return static_cast<const ColumnType::context_id&>(columns()[ColumnIndex::context_id]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<GpuTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_scope() {
    return static_cast<ColumnType::scope*>(
        GetColumn(ColumnIndex::scope));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<std::optional<int64_t>>* mutable_context_id() {
    return static_cast<ColumnType::context_id*>(
        GetColumn(ColumnIndex::context_id));
  }

 private:
  GpuTrackTable(StringPool* pool,
            const TrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::scope::stored_type> scope
, ColumnStorage<ColumnType::description::stored_type> description
, ColumnStorage<ColumnType::context_id::stored_type> context_id)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        scope_storage_layer_(
          new column::StringStorage(string_pool(), &scope_.vector())),
        description_storage_layer_(
          new column::StringStorage(string_pool(), &description_.vector())),
        context_id_storage_layer_(
          new column::NumericStorage<ColumnType::context_id::non_optional_stored_type>(
            &context_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::context_id::stored_type>::ToColumnType(),
            false))
,
        context_id_null_layer_(new column::NullOverlay(context_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::scope::stored_type>(
          ColumnFlag::scope),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::description::stored_type>(
          ColumnFlag::description),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::context_id::stored_type>(
          ColumnFlag::context_id),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(scope.size() == parent_overlay.size());
    scope_ = std::move(scope);
    PERFETTO_DCHECK(description.size() == parent_overlay.size());
    description_ = std::move(description);
    PERFETTO_DCHECK(context_id.size() == parent_overlay.size());
    context_id_ = std::move(context_id);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],scope_storage_layer_,description_storage_layer_,context_id_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},context_id_null_layer_}, std::move(overlay_layers));
  }
  TrackTable* parent_ = nullptr;
  const TrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::scope::stored_type> scope_;
  ColumnStorage<ColumnType::description::stored_type> description_;
  ColumnStorage<ColumnType::context_id::stored_type> context_id_;

  RefPtr<column::StorageLayer> scope_storage_layer_;
  RefPtr<column::StorageLayer> description_storage_layer_;
  RefPtr<column::StorageLayer> context_id_storage_layer_;

  RefPtr<column::OverlayLayer> context_id_null_layer_;
};
  

class UidTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 7;

  using Id = TrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t uid = 6;
  };
  struct ColumnType {
    using id = IdColumn<UidTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<UidTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using uid = TypedColumn<int32_t>;
  };
  struct Row : public TrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<UidTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        int32_t in_uid = {},
        std::nullptr_t = nullptr)
        : TrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id),
          uid(in_uid) {
      type_ = "uid_track";
    }
    int32_t uid;

    bool operator==(const UidTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::uid::Equals(uid, other.uid);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t uid = ColumnType::uid::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      UidTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    UidTrackTable, RowNumber> {
   public:
    ConstRowReference(const UidTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::uid::type uid() const {
      return table_->uid()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const UidTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_uid(
        ColumnType::uid::non_optional_type v) {
      return mutable_table()->mutable_uid()->Set(row_number_, v);
    }

   private:
    UidTrackTable* mutable_table() const {
      return const_cast<UidTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, UidTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::uid::type uid() const {
      const auto& col = table_->uid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const UidTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class UidTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, UidTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class UidTrackTable;

     explicit Iterator(UidTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     UidTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      UidTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "uid", &self->uid_, ColumnFlag::uid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit UidTrackTable(StringPool* pool, TrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), uid_(ColumnStorage<ColumnType::uid::stored_type>::Create<false>())
,
        uid_storage_layer_(
        new column::NumericStorage<ColumnType::uid::non_optional_stored_type>(
          &uid_.vector(),
          ColumnTypeHelper<ColumnType::uid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::uid::stored_type>(
          ColumnFlag::uid),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],uid_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{}});
  }
  ~UidTrackTable() override;

  static const char* Name() { return "uid_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "uid", ColumnType::uid::SqlValueType(), false,
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
    uid_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_uid()->Append(row.uid);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const TrackTable& parent,
      ColumnStorage<ColumnType::uid::stored_type> uid) {
    return std::unique_ptr<Table>(new UidTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(uid)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const TrackTable& parent,
      std::vector<TrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::uid::stored_type> uid) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new UidTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(uid)));
  }

  const IdColumn<UidTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<UidTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<int32_t>& uid() const {
    return static_cast<const ColumnType::uid&>(columns()[ColumnIndex::uid]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<UidTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<int32_t>* mutable_uid() {
    return static_cast<ColumnType::uid*>(
        GetColumn(ColumnIndex::uid));
  }

 private:
  UidTrackTable(StringPool* pool,
            const TrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::uid::stored_type> uid)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        uid_storage_layer_(
        new column::NumericStorage<ColumnType::uid::non_optional_stored_type>(
          &uid_.vector(),
          ColumnTypeHelper<ColumnType::uid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::uid::stored_type>(
          ColumnFlag::uid),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(uid.size() == parent_overlay.size());
    uid_ = std::move(uid);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],uid_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{}}, std::move(overlay_layers));
  }
  TrackTable* parent_ = nullptr;
  const TrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::uid::stored_type> uid_;

  RefPtr<column::StorageLayer> uid_storage_layer_;

  
};
  

class GpuWorkPeriodTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 8;

  using Id = UidTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t uid = 6;
    static constexpr uint32_t gpu_id = 7;
  };
  struct ColumnType {
    using id = IdColumn<GpuWorkPeriodTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<GpuWorkPeriodTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using uid = TypedColumn<int32_t>;
    using gpu_id = TypedColumn<uint32_t>;
  };
  struct Row : public UidTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<GpuWorkPeriodTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        int32_t in_uid = {},
        uint32_t in_gpu_id = {},
        std::nullptr_t = nullptr)
        : UidTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_uid),
          gpu_id(in_gpu_id) {
      type_ = "gpu_work_period_track";
    }
    uint32_t gpu_id;

    bool operator==(const GpuWorkPeriodTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::uid::Equals(uid, other.uid) &&
       ColumnType::gpu_id::Equals(gpu_id, other.gpu_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t gpu_id = ColumnType::gpu_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      GpuWorkPeriodTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    GpuWorkPeriodTrackTable, RowNumber> {
   public:
    ConstRowReference(const GpuWorkPeriodTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::uid::type uid() const {
      return table_->uid()[row_number_];
    }
    ColumnType::gpu_id::type gpu_id() const {
      return table_->gpu_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const GpuWorkPeriodTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_uid(
        ColumnType::uid::non_optional_type v) {
      return mutable_table()->mutable_uid()->Set(row_number_, v);
    }
    void set_gpu_id(
        ColumnType::gpu_id::non_optional_type v) {
      return mutable_table()->mutable_gpu_id()->Set(row_number_, v);
    }

   private:
    GpuWorkPeriodTrackTable* mutable_table() const {
      return const_cast<GpuWorkPeriodTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, GpuWorkPeriodTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::uid::type uid() const {
      const auto& col = table_->uid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::gpu_id::type gpu_id() const {
      const auto& col = table_->gpu_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const GpuWorkPeriodTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class GpuWorkPeriodTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, GpuWorkPeriodTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class GpuWorkPeriodTrackTable;

     explicit Iterator(GpuWorkPeriodTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     GpuWorkPeriodTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      GpuWorkPeriodTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "gpu_id", &self->gpu_id_, ColumnFlag::gpu_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit GpuWorkPeriodTrackTable(StringPool* pool, UidTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), gpu_id_(ColumnStorage<ColumnType::gpu_id::stored_type>::Create<false>())
,
        gpu_id_storage_layer_(
        new column::NumericStorage<ColumnType::gpu_id::non_optional_stored_type>(
          &gpu_id_.vector(),
          ColumnTypeHelper<ColumnType::gpu_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::gpu_id::stored_type>(
          ColumnFlag::gpu_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::uid],gpu_id_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{}});
  }
  ~GpuWorkPeriodTrackTable() override;

  static const char* Name() { return "gpu_work_period_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "uid", ColumnType::uid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "gpu_id", ColumnType::gpu_id::SqlValueType(), false,
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
    gpu_id_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_gpu_id()->Append(row.gpu_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const UidTrackTable& parent,
      ColumnStorage<ColumnType::gpu_id::stored_type> gpu_id) {
    return std::unique_ptr<Table>(new GpuWorkPeriodTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(gpu_id)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const UidTrackTable& parent,
      std::vector<UidTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::gpu_id::stored_type> gpu_id) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new GpuWorkPeriodTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(gpu_id)));
  }

  const IdColumn<GpuWorkPeriodTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<GpuWorkPeriodTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<int32_t>& uid() const {
    return static_cast<const ColumnType::uid&>(columns()[ColumnIndex::uid]);
  }
  const TypedColumn<uint32_t>& gpu_id() const {
    return static_cast<const ColumnType::gpu_id&>(columns()[ColumnIndex::gpu_id]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<GpuWorkPeriodTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<int32_t>* mutable_uid() {
    return static_cast<ColumnType::uid*>(
        GetColumn(ColumnIndex::uid));
  }
  TypedColumn<uint32_t>* mutable_gpu_id() {
    return static_cast<ColumnType::gpu_id*>(
        GetColumn(ColumnIndex::gpu_id));
  }

 private:
  GpuWorkPeriodTrackTable(StringPool* pool,
            const UidTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::gpu_id::stored_type> gpu_id)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        gpu_id_storage_layer_(
        new column::NumericStorage<ColumnType::gpu_id::non_optional_stored_type>(
          &gpu_id_.vector(),
          ColumnTypeHelper<ColumnType::gpu_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::gpu_id::stored_type>(
          ColumnFlag::gpu_id),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(gpu_id.size() == parent_overlay.size());
    gpu_id_ = std::move(gpu_id);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::uid],gpu_id_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{}}, std::move(overlay_layers));
  }
  UidTrackTable* parent_ = nullptr;
  const UidTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::gpu_id::stored_type> gpu_id_;

  RefPtr<column::StorageLayer> gpu_id_storage_layer_;

  
};
  

class IrqCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  using Id = CounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t irq = 8;
  };
  struct ColumnType {
    using id = IdColumn<IrqCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<IrqCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using irq = TypedColumn<int32_t>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<IrqCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        int32_t in_irq = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          irq(in_irq) {
      type_ = "irq_counter_track";
    }
    int32_t irq;

    bool operator==(const IrqCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::irq::Equals(irq, other.irq);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t irq = ColumnType::irq::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      IrqCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    IrqCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const IrqCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::irq::type irq() const {
      return table_->irq()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const IrqCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_irq(
        ColumnType::irq::non_optional_type v) {
      return mutable_table()->mutable_irq()->Set(row_number_, v);
    }

   private:
    IrqCounterTrackTable* mutable_table() const {
      return const_cast<IrqCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, IrqCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::irq::type irq() const {
      const auto& col = table_->irq();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const IrqCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class IrqCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, IrqCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class IrqCounterTrackTable;

     explicit Iterator(IrqCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     IrqCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      IrqCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "irq", &self->irq_, ColumnFlag::irq,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit IrqCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), irq_(ColumnStorage<ColumnType::irq::stored_type>::Create<false>())
,
        irq_storage_layer_(
        new column::NumericStorage<ColumnType::irq::non_optional_stored_type>(
          &irq_.vector(),
          ColumnTypeHelper<ColumnType::irq::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::irq::stored_type>(
          ColumnFlag::irq),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],irq_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}});
  }
  ~IrqCounterTrackTable() override;

  static const char* Name() { return "irq_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "irq", ColumnType::irq::SqlValueType(), false,
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
    irq_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_irq()->Append(row.irq);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::irq::stored_type> irq) {
    return std::unique_ptr<Table>(new IrqCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(irq)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::irq::stored_type> irq) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new IrqCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(irq)));
  }

  const IdColumn<IrqCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<IrqCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<int32_t>& irq() const {
    return static_cast<const ColumnType::irq&>(columns()[ColumnIndex::irq]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<IrqCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<int32_t>* mutable_irq() {
    return static_cast<ColumnType::irq*>(
        GetColumn(ColumnIndex::irq));
  }

 private:
  IrqCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::irq::stored_type> irq)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        irq_storage_layer_(
        new column::NumericStorage<ColumnType::irq::non_optional_stored_type>(
          &irq_.vector(),
          ColumnTypeHelper<ColumnType::irq::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::irq::stored_type>(
          ColumnFlag::irq),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(irq.size() == parent_overlay.size());
    irq_ = std::move(irq);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],irq_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::irq::stored_type> irq_;

  RefPtr<column::StorageLayer> irq_storage_layer_;

  
};
  

class LinuxDeviceTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 6;

  using Id = TrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
  };
  struct ColumnType {
    using id = IdColumn<LinuxDeviceTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<LinuxDeviceTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
  };
  struct Row : public TrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<LinuxDeviceTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        std::nullptr_t = nullptr)
        : TrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id)
           {
      type_ = "linux_device_track";
    }
    

    bool operator==(const LinuxDeviceTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id);
    }
  };
  struct ColumnFlag {
    
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      LinuxDeviceTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    LinuxDeviceTrackTable, RowNumber> {
   public:
    ConstRowReference(const LinuxDeviceTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const LinuxDeviceTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }

   private:
    LinuxDeviceTrackTable* mutable_table() const {
      return const_cast<LinuxDeviceTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, LinuxDeviceTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const LinuxDeviceTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class LinuxDeviceTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, LinuxDeviceTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class LinuxDeviceTrackTable;

     explicit Iterator(LinuxDeviceTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     LinuxDeviceTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      LinuxDeviceTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    
    
    return columns;
  }

  PERFETTO_NO_INLINE explicit LinuxDeviceTrackTable(StringPool* pool, TrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent)
        
         {
    
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id]},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id]});
  }
  ~LinuxDeviceTrackTable() override;

  static const char* Name() { return "linux_device_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const TrackTable& parent
      ) {
    return std::unique_ptr<Table>(new LinuxDeviceTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count())
        ));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const TrackTable& parent,
      std::vector<TrackTable::RowNumber> parent_overlay
      ) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new LinuxDeviceTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped))
        ));
  }

  const IdColumn<LinuxDeviceTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<LinuxDeviceTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<LinuxDeviceTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }

 private:
  LinuxDeviceTrackTable(StringPool* pool,
            const TrackTable& parent,
            const RowMap& parent_overlay
            )
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
        
         {
    
    

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id]}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id]}, std::move(overlay_layers));
  }
  TrackTable* parent_ = nullptr;
  const TrackTable* const_parent_ = nullptr;
  

  

  
};
  

class ProcessCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  using Id = CounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t upid = 8;
  };
  struct ColumnType {
    using id = IdColumn<ProcessCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<ProcessCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using upid = TypedColumn<uint32_t>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<ProcessCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        uint32_t in_upid = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          upid(in_upid) {
      type_ = "process_counter_track";
    }
    uint32_t upid;

    bool operator==(const ProcessCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::upid::Equals(upid, other.upid);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ProcessCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ProcessCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const ProcessCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ProcessCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }

   private:
    ProcessCounterTrackTable* mutable_table() const {
      return const_cast<ProcessCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ProcessCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ProcessCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ProcessCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ProcessCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ProcessCounterTrackTable;

     explicit Iterator(ProcessCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ProcessCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ProcessCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ProcessCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>())
,
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],upid_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}});
  }
  ~ProcessCounterTrackTable() override;

  static const char* Name() { return "process_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "upid", ColumnType::upid::SqlValueType(), false,
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
    upid_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_upid()->Append(row.upid);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::upid::stored_type> upid) {
    return std::unique_ptr<Table>(new ProcessCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(upid)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::upid::stored_type> upid) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new ProcessCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(upid)));
  }

  const IdColumn<ProcessCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<ProcessCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<uint32_t>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<ProcessCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<uint32_t>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }

 private:
  ProcessCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::upid::stored_type> upid)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(upid.size() == parent_overlay.size());
    upid_ = std::move(upid);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],upid_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::upid::stored_type> upid_;

  RefPtr<column::StorageLayer> upid_storage_layer_;

  
};
  

class ProcessTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 7;

  using Id = TrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t upid = 6;
  };
  struct ColumnType {
    using id = IdColumn<ProcessTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<ProcessTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using upid = TypedColumn<uint32_t>;
  };
  struct Row : public TrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<ProcessTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        uint32_t in_upid = {},
        std::nullptr_t = nullptr)
        : TrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id),
          upid(in_upid) {
      type_ = "process_track";
    }
    uint32_t upid;

    bool operator==(const ProcessTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::upid::Equals(upid, other.upid);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ProcessTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ProcessTrackTable, RowNumber> {
   public:
    ConstRowReference(const ProcessTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ProcessTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }

   private:
    ProcessTrackTable* mutable_table() const {
      return const_cast<ProcessTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ProcessTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ProcessTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ProcessTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ProcessTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ProcessTrackTable;

     explicit Iterator(ProcessTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ProcessTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ProcessTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ProcessTrackTable(StringPool* pool, TrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>())
,
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],upid_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{}});
  }
  ~ProcessTrackTable() override;

  static const char* Name() { return "process_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "upid", ColumnType::upid::SqlValueType(), false,
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
    upid_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_upid()->Append(row.upid);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const TrackTable& parent,
      ColumnStorage<ColumnType::upid::stored_type> upid) {
    return std::unique_ptr<Table>(new ProcessTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(upid)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const TrackTable& parent,
      std::vector<TrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::upid::stored_type> upid) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new ProcessTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(upid)));
  }

  const IdColumn<ProcessTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<ProcessTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<uint32_t>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<ProcessTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<uint32_t>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }

 private:
  ProcessTrackTable(StringPool* pool,
            const TrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::upid::stored_type> upid)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(upid.size() == parent_overlay.size());
    upid_ = std::move(upid);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],upid_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{}}, std::move(overlay_layers));
  }
  TrackTable* parent_ = nullptr;
  const TrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::upid::stored_type> upid_;

  RefPtr<column::StorageLayer> upid_storage_layer_;

  
};
  

class SoftirqCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  using Id = CounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t softirq = 8;
  };
  struct ColumnType {
    using id = IdColumn<SoftirqCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<SoftirqCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using softirq = TypedColumn<int32_t>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<SoftirqCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        int32_t in_softirq = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          softirq(in_softirq) {
      type_ = "softirq_counter_track";
    }
    int32_t softirq;

    bool operator==(const SoftirqCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::softirq::Equals(softirq, other.softirq);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t softirq = ColumnType::softirq::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      SoftirqCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    SoftirqCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const SoftirqCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::softirq::type softirq() const {
      return table_->softirq()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const SoftirqCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_softirq(
        ColumnType::softirq::non_optional_type v) {
      return mutable_table()->mutable_softirq()->Set(row_number_, v);
    }

   private:
    SoftirqCounterTrackTable* mutable_table() const {
      return const_cast<SoftirqCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, SoftirqCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::softirq::type softirq() const {
      const auto& col = table_->softirq();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const SoftirqCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class SoftirqCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, SoftirqCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class SoftirqCounterTrackTable;

     explicit Iterator(SoftirqCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     SoftirqCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      SoftirqCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "softirq", &self->softirq_, ColumnFlag::softirq,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit SoftirqCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), softirq_(ColumnStorage<ColumnType::softirq::stored_type>::Create<false>())
,
        softirq_storage_layer_(
        new column::NumericStorage<ColumnType::softirq::non_optional_stored_type>(
          &softirq_.vector(),
          ColumnTypeHelper<ColumnType::softirq::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::softirq::stored_type>(
          ColumnFlag::softirq),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],softirq_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}});
  }
  ~SoftirqCounterTrackTable() override;

  static const char* Name() { return "softirq_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "softirq", ColumnType::softirq::SqlValueType(), false,
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
    softirq_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_softirq()->Append(row.softirq);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::softirq::stored_type> softirq) {
    return std::unique_ptr<Table>(new SoftirqCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(softirq)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::softirq::stored_type> softirq) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new SoftirqCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(softirq)));
  }

  const IdColumn<SoftirqCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<SoftirqCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<int32_t>& softirq() const {
    return static_cast<const ColumnType::softirq&>(columns()[ColumnIndex::softirq]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<SoftirqCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<int32_t>* mutable_softirq() {
    return static_cast<ColumnType::softirq*>(
        GetColumn(ColumnIndex::softirq));
  }

 private:
  SoftirqCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::softirq::stored_type> softirq)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        softirq_storage_layer_(
        new column::NumericStorage<ColumnType::softirq::non_optional_stored_type>(
          &softirq_.vector(),
          ColumnTypeHelper<ColumnType::softirq::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::softirq::stored_type>(
          ColumnFlag::softirq),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(softirq.size() == parent_overlay.size());
    softirq_ = std::move(softirq);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],softirq_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::softirq::stored_type> softirq_;

  RefPtr<column::StorageLayer> softirq_storage_layer_;

  
};
  

class ThreadCounterTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 9;

  using Id = CounterTrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t unit = 6;
    static constexpr uint32_t description = 7;
    static constexpr uint32_t utid = 8;
  };
  struct ColumnType {
    using id = IdColumn<ThreadCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<ThreadCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using utid = TypedColumn<uint32_t>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<ThreadCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        uint32_t in_utid = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          utid(in_utid) {
      type_ = "thread_counter_track";
    }
    uint32_t utid;

    bool operator==(const ThreadCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::utid::Equals(utid, other.utid);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ThreadCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ThreadCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const ThreadCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::unit::type unit() const {
      return table_->unit()[row_number_];
    }
    ColumnType::description::type description() const {
      return table_->description()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ThreadCounterTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_unit(
        ColumnType::unit::non_optional_type v) {
      return mutable_table()->mutable_unit()->Set(row_number_, v);
    }
    void set_description(
        ColumnType::description::non_optional_type v) {
      return mutable_table()->mutable_description()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }

   private:
    ThreadCounterTrackTable* mutable_table() const {
      return const_cast<ThreadCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ThreadCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unit::type unit() const {
      const auto& col = table_->unit();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::description::type description() const {
      const auto& col = table_->description();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ThreadCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ThreadCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ThreadCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ThreadCounterTrackTable;

     explicit Iterator(ThreadCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ThreadCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ThreadCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ThreadCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>())
,
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],utid_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}});
  }
  ~ThreadCounterTrackTable() override;

  static const char* Name() { return "thread_counter_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unit", ColumnType::unit::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "description", ColumnType::description::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
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
    utid_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_utid()->Append(row.utid);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::utid::stored_type> utid) {
    return std::unique_ptr<Table>(new ThreadCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(utid)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::utid::stored_type> utid) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new ThreadCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(utid)));
  }

  const IdColumn<ThreadCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<ThreadCounterTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<StringPool::Id>& unit() const {
    return static_cast<const ColumnType::unit&>(columns()[ColumnIndex::unit]);
  }
  const TypedColumn<StringPool::Id>& description() const {
    return static_cast<const ColumnType::description&>(columns()[ColumnIndex::description]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<ThreadCounterTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<StringPool::Id>* mutable_unit() {
    return static_cast<ColumnType::unit*>(
        GetColumn(ColumnIndex::unit));
  }
  TypedColumn<StringPool::Id>* mutable_description() {
    return static_cast<ColumnType::description*>(
        GetColumn(ColumnIndex::description));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }

 private:
  ThreadCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::utid::stored_type> utid)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(utid.size() == parent_overlay.size());
    utid_ = std::move(utid);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],utid_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::utid::stored_type> utid_;

  RefPtr<column::StorageLayer> utid_storage_layer_;

  
};
  

class ThreadTrackTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 7;

  using Id = TrackTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t name = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t source_arg_set_id = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t utid = 6;
  };
  struct ColumnType {
    using id = IdColumn<ThreadTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<ThreadTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using utid = TypedColumn<uint32_t>;
  };
  struct Row : public TrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<ThreadTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        uint32_t in_utid = {},
        std::nullptr_t = nullptr)
        : TrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id),
          utid(in_utid) {
      type_ = "thread_track";
    }
    uint32_t utid;

    bool operator==(const ThreadTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::utid::Equals(utid, other.utid);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ThreadTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ThreadTrackTable, RowNumber> {
   public:
    ConstRowReference(const ThreadTrackTable* table, uint32_t row_number)
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
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      return table_->source_arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ThreadTrackTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_arg_set_id(
        ColumnType::source_arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_source_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }

   private:
    ThreadTrackTable* mutable_table() const {
      return const_cast<ThreadTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ThreadTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_arg_set_id::type source_arg_set_id() const {
      const auto& col = table_->source_arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ThreadTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ThreadTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ThreadTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ThreadTrackTable;

     explicit Iterator(ThreadTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ThreadTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ThreadTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ThreadTrackTable(StringPool* pool, TrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>())
,
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],utid_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{}});
  }
  ~ThreadTrackTable() override;

  static const char* Name() { return "thread_track"; }

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
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_arg_set_id", ColumnType::source_arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
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
    utid_.ShrinkToFit();
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
    Id id = Id{parent_->Insert(row).id};
    UpdateOverlaysAfterParentInsert();
    mutable_utid()->Append(row.utid);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const TrackTable& parent,
      ColumnStorage<ColumnType::utid::stored_type> utid) {
    return std::unique_ptr<Table>(new ThreadTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(utid)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const TrackTable& parent,
      std::vector<TrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::utid::stored_type> utid) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new ThreadTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(utid)));
  }

  const IdColumn<ThreadTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<ThreadTrackTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& source_arg_set_id() const {
    return static_cast<const ColumnType::source_arg_set_id&>(columns()[ColumnIndex::source_arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<ThreadTrackTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_source_arg_set_id() {
    return static_cast<ColumnType::source_arg_set_id*>(
        GetColumn(ColumnIndex::source_arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }

 private:
  ThreadTrackTable(StringPool* pool,
            const TrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::utid::stored_type> utid)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(utid.size() == parent_overlay.size());
    utid_ = std::move(utid);

    std::vector<RefPtr<column::OverlayLayer>> overlay_layers(OverlayCount(&parent) + 1);
    for (uint32_t i = 0; i < overlay_layers.size(); ++i) {
      if (overlays()[i].row_map().IsIndexVector()) {
        overlay_layers[i].reset(new column::ArrangementOverlay(
            overlays()[i].row_map().GetIfIndexVector(),
            column::DataLayerChain::Indices::State::kNonmonotonic));
      } else if (overlays()[i].row_map().IsBitVector()) {
        overlay_layers[i].reset(new column::SelectorOverlay(
            overlays()[i].row_map().GetIfBitVector()));
      } else if (overlays()[i].row_map().IsRange()) {
        overlay_layers[i].reset(new column::RangeOverlay(
            overlays()[i].row_map().GetIfIRange()));
      }
    }

    OnConstructionCompleted(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],utid_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{}}, std::move(overlay_layers));
  }
  TrackTable* parent_ = nullptr;
  const TrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::utid::stored_type> utid_;

  RefPtr<column::StorageLayer> utid_storage_layer_;

  
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_TRACK_TABLES_PY_H_
