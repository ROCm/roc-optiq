#ifndef SRC_TRACE_PROCESSOR_TABLES_MEMORY_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_MEMORY_TABLES_PY_H_

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

#include "src/trace_processor/tables/track_tables_py.h"

namespace perfetto::trace_processor::tables {

class MemorySnapshotTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t timestamp = 2;
    static constexpr uint32_t track_id = 3;
    static constexpr uint32_t detail_level = 4;
  };
  struct ColumnType {
    using id = IdColumn<MemorySnapshotTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using timestamp = TypedColumn<int64_t>;
    using track_id = TypedColumn<TrackTable::Id>;
    using detail_level = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_timestamp = {},
        TrackTable::Id in_track_id = {},
        StringPool::Id in_detail_level = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          timestamp(in_timestamp),
          track_id(in_track_id),
          detail_level(in_detail_level) {
      type_ = "memory_snapshot";
    }
    int64_t timestamp;
    TrackTable::Id track_id;
    StringPool::Id detail_level;

    bool operator==(const MemorySnapshotTable::Row& other) const {
      return type() == other.type() && ColumnType::timestamp::Equals(timestamp, other.timestamp) &&
       ColumnType::track_id::Equals(track_id, other.track_id) &&
       ColumnType::detail_level::Equals(detail_level, other.detail_level);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t timestamp = ColumnType::timestamp::default_flags();
    static constexpr uint32_t track_id = ColumnType::track_id::default_flags();
    static constexpr uint32_t detail_level = ColumnType::detail_level::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      MemorySnapshotTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    MemorySnapshotTable, RowNumber> {
   public:
    ConstRowReference(const MemorySnapshotTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::timestamp::type timestamp() const {
      return table_->timestamp()[row_number_];
    }
    ColumnType::track_id::type track_id() const {
      return table_->track_id()[row_number_];
    }
    ColumnType::detail_level::type detail_level() const {
      return table_->detail_level()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const MemorySnapshotTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_timestamp(
        ColumnType::timestamp::non_optional_type v) {
      return mutable_table()->mutable_timestamp()->Set(row_number_, v);
    }
    void set_track_id(
        ColumnType::track_id::non_optional_type v) {
      return mutable_table()->mutable_track_id()->Set(row_number_, v);
    }
    void set_detail_level(
        ColumnType::detail_level::non_optional_type v) {
      return mutable_table()->mutable_detail_level()->Set(row_number_, v);
    }

   private:
    MemorySnapshotTable* mutable_table() const {
      return const_cast<MemorySnapshotTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, MemorySnapshotTable, RowNumber, ConstRowReference> {
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
    ColumnType::timestamp::type timestamp() const {
      const auto& col = table_->timestamp();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::track_id::type track_id() const {
      const auto& col = table_->track_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::detail_level::type detail_level() const {
      const auto& col = table_->detail_level();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const MemorySnapshotTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class MemorySnapshotTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, MemorySnapshotTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class MemorySnapshotTable;

     explicit Iterator(MemorySnapshotTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     MemorySnapshotTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      MemorySnapshotTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "timestamp", &self->timestamp_, ColumnFlag::timestamp,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "track_id", &self->track_id_, ColumnFlag::track_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "detail_level", &self->detail_level_, ColumnFlag::detail_level,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit MemorySnapshotTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        timestamp_(ColumnStorage<ColumnType::timestamp::stored_type>::Create<false>()),
        track_id_(ColumnStorage<ColumnType::track_id::stored_type>::Create<false>()),
        detail_level_(ColumnStorage<ColumnType::detail_level::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        timestamp_storage_layer_(
        new column::NumericStorage<ColumnType::timestamp::non_optional_stored_type>(
          &timestamp_.vector(),
          ColumnTypeHelper<ColumnType::timestamp::stored_type>::ToColumnType(),
          false)),
        track_id_storage_layer_(
        new column::NumericStorage<ColumnType::track_id::non_optional_stored_type>(
          &track_id_.vector(),
          ColumnTypeHelper<ColumnType::track_id::stored_type>::ToColumnType(),
          false)),
        detail_level_storage_layer_(
          new column::StringStorage(string_pool(), &detail_level_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::timestamp::stored_type>(
          ColumnFlag::timestamp),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::track_id::stored_type>(
          ColumnFlag::track_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::detail_level::stored_type>(
          ColumnFlag::detail_level),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,timestamp_storage_layer_,track_id_storage_layer_,detail_level_storage_layer_},
      {{},{},{},{},{}});
  }
  ~MemorySnapshotTable() override;

  static const char* Name() { return "memory_snapshot"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "timestamp", ColumnType::timestamp::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "track_id", ColumnType::track_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "detail_level", ColumnType::detail_level::SqlValueType(), false,
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
    timestamp_.ShrinkToFit();
    track_id_.ShrinkToFit();
    detail_level_.ShrinkToFit();
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
    mutable_timestamp()->Append(row.timestamp);
    mutable_track_id()->Append(row.track_id);
    mutable_detail_level()->Append(row.detail_level);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<MemorySnapshotTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& timestamp() const {
    return static_cast<const ColumnType::timestamp&>(columns()[ColumnIndex::timestamp]);
  }
  const TypedColumn<TrackTable::Id>& track_id() const {
    return static_cast<const ColumnType::track_id&>(columns()[ColumnIndex::track_id]);
  }
  const TypedColumn<StringPool::Id>& detail_level() const {
    return static_cast<const ColumnType::detail_level&>(columns()[ColumnIndex::detail_level]);
  }

  TypedColumn<int64_t>* mutable_timestamp() {
    return static_cast<ColumnType::timestamp*>(
        GetColumn(ColumnIndex::timestamp));
  }
  TypedColumn<TrackTable::Id>* mutable_track_id() {
    return static_cast<ColumnType::track_id*>(
        GetColumn(ColumnIndex::track_id));
  }
  TypedColumn<StringPool::Id>* mutable_detail_level() {
    return static_cast<ColumnType::detail_level*>(
        GetColumn(ColumnIndex::detail_level));
  }

 private:
  
  
  ColumnStorage<ColumnType::timestamp::stored_type> timestamp_;
  ColumnStorage<ColumnType::track_id::stored_type> track_id_;
  ColumnStorage<ColumnType::detail_level::stored_type> detail_level_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> timestamp_storage_layer_;
  RefPtr<column::StorageLayer> track_id_storage_layer_;
  RefPtr<column::StorageLayer> detail_level_storage_layer_;

  
};
  

class ProcessMemorySnapshotTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t snapshot_id = 2;
    static constexpr uint32_t upid = 3;
  };
  struct ColumnType {
    using id = IdColumn<ProcessMemorySnapshotTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using snapshot_id = TypedColumn<MemorySnapshotTable::Id>;
    using upid = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(MemorySnapshotTable::Id in_snapshot_id = {},
        uint32_t in_upid = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          snapshot_id(in_snapshot_id),
          upid(in_upid) {
      type_ = "process_memory_snapshot";
    }
    MemorySnapshotTable::Id snapshot_id;
    uint32_t upid;

    bool operator==(const ProcessMemorySnapshotTable::Row& other) const {
      return type() == other.type() && ColumnType::snapshot_id::Equals(snapshot_id, other.snapshot_id) &&
       ColumnType::upid::Equals(upid, other.upid);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t snapshot_id = ColumnType::snapshot_id::default_flags();
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ProcessMemorySnapshotTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ProcessMemorySnapshotTable, RowNumber> {
   public:
    ConstRowReference(const ProcessMemorySnapshotTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::snapshot_id::type snapshot_id() const {
      return table_->snapshot_id()[row_number_];
    }
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ProcessMemorySnapshotTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_snapshot_id(
        ColumnType::snapshot_id::non_optional_type v) {
      return mutable_table()->mutable_snapshot_id()->Set(row_number_, v);
    }
    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }

   private:
    ProcessMemorySnapshotTable* mutable_table() const {
      return const_cast<ProcessMemorySnapshotTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ProcessMemorySnapshotTable, RowNumber, ConstRowReference> {
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
    ColumnType::snapshot_id::type snapshot_id() const {
      const auto& col = table_->snapshot_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ProcessMemorySnapshotTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ProcessMemorySnapshotTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ProcessMemorySnapshotTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ProcessMemorySnapshotTable;

     explicit Iterator(ProcessMemorySnapshotTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ProcessMemorySnapshotTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ProcessMemorySnapshotTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "snapshot_id", &self->snapshot_id_, ColumnFlag::snapshot_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ProcessMemorySnapshotTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        snapshot_id_(ColumnStorage<ColumnType::snapshot_id::stored_type>::Create<false>()),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        snapshot_id_storage_layer_(
        new column::NumericStorage<ColumnType::snapshot_id::non_optional_stored_type>(
          &snapshot_id_.vector(),
          ColumnTypeHelper<ColumnType::snapshot_id::stored_type>::ToColumnType(),
          false)),
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::snapshot_id::stored_type>(
          ColumnFlag::snapshot_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,snapshot_id_storage_layer_,upid_storage_layer_},
      {{},{},{},{}});
  }
  ~ProcessMemorySnapshotTable() override;

  static const char* Name() { return "process_memory_snapshot"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "snapshot_id", ColumnType::snapshot_id::SqlValueType(), false,
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
    type_.ShrinkToFit();
    snapshot_id_.ShrinkToFit();
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
    Id id = Id{row_number};
    type_.Append(string_pool()->InternString(row.type()));
    mutable_snapshot_id()->Append(row.snapshot_id);
    mutable_upid()->Append(row.upid);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ProcessMemorySnapshotTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<MemorySnapshotTable::Id>& snapshot_id() const {
    return static_cast<const ColumnType::snapshot_id&>(columns()[ColumnIndex::snapshot_id]);
  }
  const TypedColumn<uint32_t>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }

  TypedColumn<MemorySnapshotTable::Id>* mutable_snapshot_id() {
    return static_cast<ColumnType::snapshot_id*>(
        GetColumn(ColumnIndex::snapshot_id));
  }
  TypedColumn<uint32_t>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }

 private:
  
  
  ColumnStorage<ColumnType::snapshot_id::stored_type> snapshot_id_;
  ColumnStorage<ColumnType::upid::stored_type> upid_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> snapshot_id_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;

  
};
  

class MemorySnapshotNodeTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t process_snapshot_id = 2;
    static constexpr uint32_t parent_node_id = 3;
    static constexpr uint32_t path = 4;
    static constexpr uint32_t size = 5;
    static constexpr uint32_t effective_size = 6;
    static constexpr uint32_t arg_set_id = 7;
  };
  struct ColumnType {
    using id = IdColumn<MemorySnapshotNodeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using process_snapshot_id = TypedColumn<ProcessMemorySnapshotTable::Id>;
    using parent_node_id = TypedColumn<std::optional<MemorySnapshotNodeTable::Id>>;
    using path = TypedColumn<StringPool::Id>;
    using size = TypedColumn<int64_t>;
    using effective_size = TypedColumn<int64_t>;
    using arg_set_id = TypedColumn<std::optional<uint32_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(ProcessMemorySnapshotTable::Id in_process_snapshot_id = {},
        std::optional<MemorySnapshotNodeTable::Id> in_parent_node_id = {},
        StringPool::Id in_path = {},
        int64_t in_size = {},
        int64_t in_effective_size = {},
        std::optional<uint32_t> in_arg_set_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          process_snapshot_id(in_process_snapshot_id),
          parent_node_id(in_parent_node_id),
          path(in_path),
          size(in_size),
          effective_size(in_effective_size),
          arg_set_id(in_arg_set_id) {
      type_ = "memory_snapshot_node";
    }
    ProcessMemorySnapshotTable::Id process_snapshot_id;
    std::optional<MemorySnapshotNodeTable::Id> parent_node_id;
    StringPool::Id path;
    int64_t size;
    int64_t effective_size;
    std::optional<uint32_t> arg_set_id;

    bool operator==(const MemorySnapshotNodeTable::Row& other) const {
      return type() == other.type() && ColumnType::process_snapshot_id::Equals(process_snapshot_id, other.process_snapshot_id) &&
       ColumnType::parent_node_id::Equals(parent_node_id, other.parent_node_id) &&
       ColumnType::path::Equals(path, other.path) &&
       ColumnType::size::Equals(size, other.size) &&
       ColumnType::effective_size::Equals(effective_size, other.effective_size) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t process_snapshot_id = ColumnType::process_snapshot_id::default_flags();
    static constexpr uint32_t parent_node_id = ColumnType::parent_node_id::default_flags();
    static constexpr uint32_t path = ColumnType::path::default_flags();
    static constexpr uint32_t size = ColumnType::size::default_flags();
    static constexpr uint32_t effective_size = ColumnType::effective_size::default_flags();
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      MemorySnapshotNodeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    MemorySnapshotNodeTable, RowNumber> {
   public:
    ConstRowReference(const MemorySnapshotNodeTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::process_snapshot_id::type process_snapshot_id() const {
      return table_->process_snapshot_id()[row_number_];
    }
    ColumnType::parent_node_id::type parent_node_id() const {
      return table_->parent_node_id()[row_number_];
    }
    ColumnType::path::type path() const {
      return table_->path()[row_number_];
    }
    ColumnType::size::type size() const {
      return table_->size()[row_number_];
    }
    ColumnType::effective_size::type effective_size() const {
      return table_->effective_size()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const MemorySnapshotNodeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_process_snapshot_id(
        ColumnType::process_snapshot_id::non_optional_type v) {
      return mutable_table()->mutable_process_snapshot_id()->Set(row_number_, v);
    }
    void set_parent_node_id(
        ColumnType::parent_node_id::non_optional_type v) {
      return mutable_table()->mutable_parent_node_id()->Set(row_number_, v);
    }
    void set_path(
        ColumnType::path::non_optional_type v) {
      return mutable_table()->mutable_path()->Set(row_number_, v);
    }
    void set_size(
        ColumnType::size::non_optional_type v) {
      return mutable_table()->mutable_size()->Set(row_number_, v);
    }
    void set_effective_size(
        ColumnType::effective_size::non_optional_type v) {
      return mutable_table()->mutable_effective_size()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }

   private:
    MemorySnapshotNodeTable* mutable_table() const {
      return const_cast<MemorySnapshotNodeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, MemorySnapshotNodeTable, RowNumber, ConstRowReference> {
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
    ColumnType::process_snapshot_id::type process_snapshot_id() const {
      const auto& col = table_->process_snapshot_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::parent_node_id::type parent_node_id() const {
      const auto& col = table_->parent_node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::path::type path() const {
      const auto& col = table_->path();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::size::type size() const {
      const auto& col = table_->size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::effective_size::type effective_size() const {
      const auto& col = table_->effective_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const MemorySnapshotNodeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class MemorySnapshotNodeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, MemorySnapshotNodeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class MemorySnapshotNodeTable;

     explicit Iterator(MemorySnapshotNodeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     MemorySnapshotNodeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      MemorySnapshotNodeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "process_snapshot_id", &self->process_snapshot_id_, ColumnFlag::process_snapshot_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "parent_node_id", &self->parent_node_id_, ColumnFlag::parent_node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "path", &self->path_, ColumnFlag::path,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "size", &self->size_, ColumnFlag::size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "effective_size", &self->effective_size_, ColumnFlag::effective_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit MemorySnapshotNodeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        process_snapshot_id_(ColumnStorage<ColumnType::process_snapshot_id::stored_type>::Create<false>()),
        parent_node_id_(ColumnStorage<ColumnType::parent_node_id::stored_type>::Create<false>()),
        path_(ColumnStorage<ColumnType::path::stored_type>::Create<false>()),
        size_(ColumnStorage<ColumnType::size::stored_type>::Create<false>()),
        effective_size_(ColumnStorage<ColumnType::effective_size::stored_type>::Create<false>()),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        process_snapshot_id_storage_layer_(
        new column::NumericStorage<ColumnType::process_snapshot_id::non_optional_stored_type>(
          &process_snapshot_id_.vector(),
          ColumnTypeHelper<ColumnType::process_snapshot_id::stored_type>::ToColumnType(),
          false)),
        parent_node_id_storage_layer_(
          new column::NumericStorage<ColumnType::parent_node_id::non_optional_stored_type>(
            &parent_node_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_node_id::stored_type>::ToColumnType(),
            false)),
        path_storage_layer_(
          new column::StringStorage(string_pool(), &path_.vector())),
        size_storage_layer_(
        new column::NumericStorage<ColumnType::size::non_optional_stored_type>(
          &size_.vector(),
          ColumnTypeHelper<ColumnType::size::stored_type>::ToColumnType(),
          false)),
        effective_size_storage_layer_(
        new column::NumericStorage<ColumnType::effective_size::non_optional_stored_type>(
          &effective_size_.vector(),
          ColumnTypeHelper<ColumnType::effective_size::stored_type>::ToColumnType(),
          false)),
        arg_set_id_storage_layer_(
          new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
            &arg_set_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
            false))
,
        parent_node_id_null_layer_(new column::NullOverlay(parent_node_id_.bv())),
        arg_set_id_null_layer_(new column::NullOverlay(arg_set_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::process_snapshot_id::stored_type>(
          ColumnFlag::process_snapshot_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_node_id::stored_type>(
          ColumnFlag::parent_node_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::path::stored_type>(
          ColumnFlag::path),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::size::stored_type>(
          ColumnFlag::size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::effective_size::stored_type>(
          ColumnFlag::effective_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,process_snapshot_id_storage_layer_,parent_node_id_storage_layer_,path_storage_layer_,size_storage_layer_,effective_size_storage_layer_,arg_set_id_storage_layer_},
      {{},{},{},parent_node_id_null_layer_,{},{},{},arg_set_id_null_layer_});
  }
  ~MemorySnapshotNodeTable() override;

  static const char* Name() { return "memory_snapshot_node"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "process_snapshot_id", ColumnType::process_snapshot_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "parent_node_id", ColumnType::parent_node_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "path", ColumnType::path::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "size", ColumnType::size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "effective_size", ColumnType::effective_size::SqlValueType(), false,
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
    process_snapshot_id_.ShrinkToFit();
    parent_node_id_.ShrinkToFit();
    path_.ShrinkToFit();
    size_.ShrinkToFit();
    effective_size_.ShrinkToFit();
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
    mutable_process_snapshot_id()->Append(row.process_snapshot_id);
    mutable_parent_node_id()->Append(row.parent_node_id);
    mutable_path()->Append(row.path);
    mutable_size()->Append(row.size);
    mutable_effective_size()->Append(row.effective_size);
    mutable_arg_set_id()->Append(row.arg_set_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<MemorySnapshotNodeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<ProcessMemorySnapshotTable::Id>& process_snapshot_id() const {
    return static_cast<const ColumnType::process_snapshot_id&>(columns()[ColumnIndex::process_snapshot_id]);
  }
  const TypedColumn<std::optional<MemorySnapshotNodeTable::Id>>& parent_node_id() const {
    return static_cast<const ColumnType::parent_node_id&>(columns()[ColumnIndex::parent_node_id]);
  }
  const TypedColumn<StringPool::Id>& path() const {
    return static_cast<const ColumnType::path&>(columns()[ColumnIndex::path]);
  }
  const TypedColumn<int64_t>& size() const {
    return static_cast<const ColumnType::size&>(columns()[ColumnIndex::size]);
  }
  const TypedColumn<int64_t>& effective_size() const {
    return static_cast<const ColumnType::effective_size&>(columns()[ColumnIndex::effective_size]);
  }
  const TypedColumn<std::optional<uint32_t>>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }

  TypedColumn<ProcessMemorySnapshotTable::Id>* mutable_process_snapshot_id() {
    return static_cast<ColumnType::process_snapshot_id*>(
        GetColumn(ColumnIndex::process_snapshot_id));
  }
  TypedColumn<std::optional<MemorySnapshotNodeTable::Id>>* mutable_parent_node_id() {
    return static_cast<ColumnType::parent_node_id*>(
        GetColumn(ColumnIndex::parent_node_id));
  }
  TypedColumn<StringPool::Id>* mutable_path() {
    return static_cast<ColumnType::path*>(
        GetColumn(ColumnIndex::path));
  }
  TypedColumn<int64_t>* mutable_size() {
    return static_cast<ColumnType::size*>(
        GetColumn(ColumnIndex::size));
  }
  TypedColumn<int64_t>* mutable_effective_size() {
    return static_cast<ColumnType::effective_size*>(
        GetColumn(ColumnIndex::effective_size));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::process_snapshot_id::stored_type> process_snapshot_id_;
  ColumnStorage<ColumnType::parent_node_id::stored_type> parent_node_id_;
  ColumnStorage<ColumnType::path::stored_type> path_;
  ColumnStorage<ColumnType::size::stored_type> size_;
  ColumnStorage<ColumnType::effective_size::stored_type> effective_size_;
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> process_snapshot_id_storage_layer_;
  RefPtr<column::StorageLayer> parent_node_id_storage_layer_;
  RefPtr<column::StorageLayer> path_storage_layer_;
  RefPtr<column::StorageLayer> size_storage_layer_;
  RefPtr<column::StorageLayer> effective_size_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;

  RefPtr<column::OverlayLayer> parent_node_id_null_layer_;
  RefPtr<column::OverlayLayer> arg_set_id_null_layer_;
};
  

class MemorySnapshotEdgeTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t source_node_id = 2;
    static constexpr uint32_t target_node_id = 3;
    static constexpr uint32_t importance = 4;
  };
  struct ColumnType {
    using id = IdColumn<MemorySnapshotEdgeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using source_node_id = TypedColumn<MemorySnapshotNodeTable::Id>;
    using target_node_id = TypedColumn<MemorySnapshotNodeTable::Id>;
    using importance = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(MemorySnapshotNodeTable::Id in_source_node_id = {},
        MemorySnapshotNodeTable::Id in_target_node_id = {},
        uint32_t in_importance = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          source_node_id(in_source_node_id),
          target_node_id(in_target_node_id),
          importance(in_importance) {
      type_ = "memory_snapshot_edge";
    }
    MemorySnapshotNodeTable::Id source_node_id;
    MemorySnapshotNodeTable::Id target_node_id;
    uint32_t importance;

    bool operator==(const MemorySnapshotEdgeTable::Row& other) const {
      return type() == other.type() && ColumnType::source_node_id::Equals(source_node_id, other.source_node_id) &&
       ColumnType::target_node_id::Equals(target_node_id, other.target_node_id) &&
       ColumnType::importance::Equals(importance, other.importance);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t source_node_id = ColumnType::source_node_id::default_flags();
    static constexpr uint32_t target_node_id = ColumnType::target_node_id::default_flags();
    static constexpr uint32_t importance = ColumnType::importance::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      MemorySnapshotEdgeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    MemorySnapshotEdgeTable, RowNumber> {
   public:
    ConstRowReference(const MemorySnapshotEdgeTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::source_node_id::type source_node_id() const {
      return table_->source_node_id()[row_number_];
    }
    ColumnType::target_node_id::type target_node_id() const {
      return table_->target_node_id()[row_number_];
    }
    ColumnType::importance::type importance() const {
      return table_->importance()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const MemorySnapshotEdgeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_source_node_id(
        ColumnType::source_node_id::non_optional_type v) {
      return mutable_table()->mutable_source_node_id()->Set(row_number_, v);
    }
    void set_target_node_id(
        ColumnType::target_node_id::non_optional_type v) {
      return mutable_table()->mutable_target_node_id()->Set(row_number_, v);
    }
    void set_importance(
        ColumnType::importance::non_optional_type v) {
      return mutable_table()->mutable_importance()->Set(row_number_, v);
    }

   private:
    MemorySnapshotEdgeTable* mutable_table() const {
      return const_cast<MemorySnapshotEdgeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, MemorySnapshotEdgeTable, RowNumber, ConstRowReference> {
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
    ColumnType::source_node_id::type source_node_id() const {
      const auto& col = table_->source_node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::target_node_id::type target_node_id() const {
      const auto& col = table_->target_node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::importance::type importance() const {
      const auto& col = table_->importance();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const MemorySnapshotEdgeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class MemorySnapshotEdgeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, MemorySnapshotEdgeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class MemorySnapshotEdgeTable;

     explicit Iterator(MemorySnapshotEdgeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     MemorySnapshotEdgeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      MemorySnapshotEdgeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "source_node_id", &self->source_node_id_, ColumnFlag::source_node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "target_node_id", &self->target_node_id_, ColumnFlag::target_node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "importance", &self->importance_, ColumnFlag::importance,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit MemorySnapshotEdgeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        source_node_id_(ColumnStorage<ColumnType::source_node_id::stored_type>::Create<false>()),
        target_node_id_(ColumnStorage<ColumnType::target_node_id::stored_type>::Create<false>()),
        importance_(ColumnStorage<ColumnType::importance::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        source_node_id_storage_layer_(
        new column::NumericStorage<ColumnType::source_node_id::non_optional_stored_type>(
          &source_node_id_.vector(),
          ColumnTypeHelper<ColumnType::source_node_id::stored_type>::ToColumnType(),
          false)),
        target_node_id_storage_layer_(
        new column::NumericStorage<ColumnType::target_node_id::non_optional_stored_type>(
          &target_node_id_.vector(),
          ColumnTypeHelper<ColumnType::target_node_id::stored_type>::ToColumnType(),
          false)),
        importance_storage_layer_(
        new column::NumericStorage<ColumnType::importance::non_optional_stored_type>(
          &importance_.vector(),
          ColumnTypeHelper<ColumnType::importance::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::source_node_id::stored_type>(
          ColumnFlag::source_node_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::target_node_id::stored_type>(
          ColumnFlag::target_node_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::importance::stored_type>(
          ColumnFlag::importance),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,source_node_id_storage_layer_,target_node_id_storage_layer_,importance_storage_layer_},
      {{},{},{},{},{}});
  }
  ~MemorySnapshotEdgeTable() override;

  static const char* Name() { return "memory_snapshot_edge"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_node_id", ColumnType::source_node_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "target_node_id", ColumnType::target_node_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "importance", ColumnType::importance::SqlValueType(), false,
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
    source_node_id_.ShrinkToFit();
    target_node_id_.ShrinkToFit();
    importance_.ShrinkToFit();
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
    mutable_source_node_id()->Append(row.source_node_id);
    mutable_target_node_id()->Append(row.target_node_id);
    mutable_importance()->Append(row.importance);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<MemorySnapshotEdgeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<MemorySnapshotNodeTable::Id>& source_node_id() const {
    return static_cast<const ColumnType::source_node_id&>(columns()[ColumnIndex::source_node_id]);
  }
  const TypedColumn<MemorySnapshotNodeTable::Id>& target_node_id() const {
    return static_cast<const ColumnType::target_node_id&>(columns()[ColumnIndex::target_node_id]);
  }
  const TypedColumn<uint32_t>& importance() const {
    return static_cast<const ColumnType::importance&>(columns()[ColumnIndex::importance]);
  }

  TypedColumn<MemorySnapshotNodeTable::Id>* mutable_source_node_id() {
    return static_cast<ColumnType::source_node_id*>(
        GetColumn(ColumnIndex::source_node_id));
  }
  TypedColumn<MemorySnapshotNodeTable::Id>* mutable_target_node_id() {
    return static_cast<ColumnType::target_node_id*>(
        GetColumn(ColumnIndex::target_node_id));
  }
  TypedColumn<uint32_t>* mutable_importance() {
    return static_cast<ColumnType::importance*>(
        GetColumn(ColumnIndex::importance));
  }

 private:
  
  
  ColumnStorage<ColumnType::source_node_id::stored_type> source_node_id_;
  ColumnStorage<ColumnType::target_node_id::stored_type> target_node_id_;
  ColumnStorage<ColumnType::importance::stored_type> importance_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> source_node_id_storage_layer_;
  RefPtr<column::StorageLayer> target_node_id_storage_layer_;
  RefPtr<column::StorageLayer> importance_storage_layer_;

  
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_MEMORY_TABLES_PY_H_
