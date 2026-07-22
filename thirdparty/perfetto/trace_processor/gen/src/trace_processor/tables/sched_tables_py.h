#ifndef SRC_TRACE_PROCESSOR_TABLES_SCHED_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_SCHED_TABLES_PY_H_

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

class SchedSliceTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t ts = 2;
    static constexpr uint32_t dur = 3;
    static constexpr uint32_t utid = 4;
    static constexpr uint32_t end_state = 5;
    static constexpr uint32_t priority = 6;
    static constexpr uint32_t ucpu = 7;
  };
  struct ColumnType {
    using id = IdColumn<SchedSliceTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using dur = TypedColumn<int64_t>;
    using utid = TypedColumn<uint32_t>;
    using end_state = TypedColumn<StringPool::Id>;
    using priority = TypedColumn<int32_t>;
    using ucpu = TypedColumn<CpuTable::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        int64_t in_dur = {},
        uint32_t in_utid = {},
        StringPool::Id in_end_state = {},
        int32_t in_priority = {},
        CpuTable::Id in_ucpu = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          dur(in_dur),
          utid(in_utid),
          end_state(in_end_state),
          priority(in_priority),
          ucpu(in_ucpu) {
      type_ = "__intrinsic_sched_slice";
    }
    int64_t ts;
    int64_t dur;
    uint32_t utid;
    StringPool::Id end_state;
    int32_t priority;
    CpuTable::Id ucpu;

    bool operator==(const SchedSliceTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::dur::Equals(dur, other.dur) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::end_state::Equals(end_state, other.end_state) &&
       ColumnType::priority::Equals(priority, other.priority) &&
       ColumnType::ucpu::Equals(ucpu, other.ucpu);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::ts::default_flags();
    static constexpr uint32_t dur = ColumnType::dur::default_flags();
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
    static constexpr uint32_t end_state = ColumnType::end_state::default_flags();
    static constexpr uint32_t priority = ColumnType::priority::default_flags();
    static constexpr uint32_t ucpu = ColumnType::ucpu::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      SchedSliceTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    SchedSliceTable, RowNumber> {
   public:
    ConstRowReference(const SchedSliceTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::ts::type ts() const {
      return table_->ts()[row_number_];
    }
    ColumnType::dur::type dur() const {
      return table_->dur()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
    ColumnType::end_state::type end_state() const {
      return table_->end_state()[row_number_];
    }
    ColumnType::priority::type priority() const {
      return table_->priority()[row_number_];
    }
    ColumnType::ucpu::type ucpu() const {
      return table_->ucpu()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const SchedSliceTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_dur(
        ColumnType::dur::non_optional_type v) {
      return mutable_table()->mutable_dur()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_end_state(
        ColumnType::end_state::non_optional_type v) {
      return mutable_table()->mutable_end_state()->Set(row_number_, v);
    }
    void set_priority(
        ColumnType::priority::non_optional_type v) {
      return mutable_table()->mutable_priority()->Set(row_number_, v);
    }
    void set_ucpu(
        ColumnType::ucpu::non_optional_type v) {
      return mutable_table()->mutable_ucpu()->Set(row_number_, v);
    }

   private:
    SchedSliceTable* mutable_table() const {
      return const_cast<SchedSliceTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, SchedSliceTable, RowNumber, ConstRowReference> {
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
    ColumnType::ts::type ts() const {
      const auto& col = table_->ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::dur::type dur() const {
      const auto& col = table_->dur();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::end_state::type end_state() const {
      const auto& col = table_->end_state();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::priority::type priority() const {
      const auto& col = table_->priority();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ucpu::type ucpu() const {
      const auto& col = table_->ucpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const SchedSliceTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class SchedSliceTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, SchedSliceTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class SchedSliceTable;

     explicit Iterator(SchedSliceTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     SchedSliceTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      SchedSliceTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "dur", &self->dur_, ColumnFlag::dur,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "end_state", &self->end_state_, ColumnFlag::end_state,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "priority", &self->priority_, ColumnFlag::priority,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ucpu", &self->ucpu_, ColumnFlag::ucpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit SchedSliceTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        dur_(ColumnStorage<ColumnType::dur::stored_type>::Create<false>()),
        utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>()),
        end_state_(ColumnStorage<ColumnType::end_state::stored_type>::Create<false>()),
        priority_(ColumnStorage<ColumnType::priority::stored_type>::Create<false>()),
        ucpu_(ColumnStorage<ColumnType::ucpu::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          true)),
        dur_storage_layer_(
        new column::NumericStorage<ColumnType::dur::non_optional_stored_type>(
          &dur_.vector(),
          ColumnTypeHelper<ColumnType::dur::stored_type>::ToColumnType(),
          false)),
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false)),
        end_state_storage_layer_(
          new column::StringStorage(string_pool(), &end_state_.vector())),
        priority_storage_layer_(
        new column::NumericStorage<ColumnType::priority::non_optional_stored_type>(
          &priority_.vector(),
          ColumnTypeHelper<ColumnType::priority::stored_type>::ToColumnType(),
          false)),
        ucpu_storage_layer_(
        new column::NumericStorage<ColumnType::ucpu::non_optional_stored_type>(
          &ucpu_.vector(),
          ColumnTypeHelper<ColumnType::ucpu::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::dur::stored_type>(
          ColumnFlag::dur),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::end_state::stored_type>(
          ColumnFlag::end_state),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::priority::stored_type>(
          ColumnFlag::priority),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ucpu::stored_type>(
          ColumnFlag::ucpu),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,dur_storage_layer_,utid_storage_layer_,end_state_storage_layer_,priority_storage_layer_,ucpu_storage_layer_},
      {{},{},{},{},{},{},{},{}});
  }
  ~SchedSliceTable() override;

  static const char* Name() { return "__intrinsic_sched_slice"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts", ColumnType::ts::SqlValueType(), false,
        true,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "dur", ColumnType::dur::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "end_state", ColumnType::end_state::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "priority", ColumnType::priority::SqlValueType(), false,
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
    type_.ShrinkToFit();
    ts_.ShrinkToFit();
    dur_.ShrinkToFit();
    utid_.ShrinkToFit();
    end_state_.ShrinkToFit();
    priority_.ShrinkToFit();
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
    Id id = Id{row_number};
    type_.Append(string_pool()->InternString(row.type()));
    mutable_ts()->Append(row.ts);
    mutable_dur()->Append(row.dur);
    mutable_utid()->Append(row.utid);
    mutable_end_state()->Append(row.end_state);
    mutable_priority()->Append(row.priority);
    mutable_ucpu()->Append(row.ucpu);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<SchedSliceTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<int64_t>& dur() const {
    return static_cast<const ColumnType::dur&>(columns()[ColumnIndex::dur]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }
  const TypedColumn<StringPool::Id>& end_state() const {
    return static_cast<const ColumnType::end_state&>(columns()[ColumnIndex::end_state]);
  }
  const TypedColumn<int32_t>& priority() const {
    return static_cast<const ColumnType::priority&>(columns()[ColumnIndex::priority]);
  }
  const TypedColumn<CpuTable::Id>& ucpu() const {
    return static_cast<const ColumnType::ucpu&>(columns()[ColumnIndex::ucpu]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<int64_t>* mutable_dur() {
    return static_cast<ColumnType::dur*>(
        GetColumn(ColumnIndex::dur));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<StringPool::Id>* mutable_end_state() {
    return static_cast<ColumnType::end_state*>(
        GetColumn(ColumnIndex::end_state));
  }
  TypedColumn<int32_t>* mutable_priority() {
    return static_cast<ColumnType::priority*>(
        GetColumn(ColumnIndex::priority));
  }
  TypedColumn<CpuTable::Id>* mutable_ucpu() {
    return static_cast<ColumnType::ucpu*>(
        GetColumn(ColumnIndex::ucpu));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::dur::stored_type> dur_;
  ColumnStorage<ColumnType::utid::stored_type> utid_;
  ColumnStorage<ColumnType::end_state::stored_type> end_state_;
  ColumnStorage<ColumnType::priority::stored_type> priority_;
  ColumnStorage<ColumnType::ucpu::stored_type> ucpu_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> dur_storage_layer_;
  RefPtr<column::StorageLayer> utid_storage_layer_;
  RefPtr<column::StorageLayer> end_state_storage_layer_;
  RefPtr<column::StorageLayer> priority_storage_layer_;
  RefPtr<column::StorageLayer> ucpu_storage_layer_;

  
};
  

class SpuriousSchedWakeupTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t ts = 2;
    static constexpr uint32_t thread_state_id = 3;
    static constexpr uint32_t irq_context = 4;
    static constexpr uint32_t utid = 5;
    static constexpr uint32_t waker_utid = 6;
  };
  struct ColumnType {
    using id = IdColumn<SpuriousSchedWakeupTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using thread_state_id = TypedColumn<int64_t>;
    using irq_context = TypedColumn<std::optional<uint32_t>>;
    using utid = TypedColumn<uint32_t>;
    using waker_utid = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        int64_t in_thread_state_id = {},
        std::optional<uint32_t> in_irq_context = {},
        uint32_t in_utid = {},
        uint32_t in_waker_utid = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          thread_state_id(in_thread_state_id),
          irq_context(in_irq_context),
          utid(in_utid),
          waker_utid(in_waker_utid) {
      type_ = "spurious_sched_wakeup";
    }
    int64_t ts;
    int64_t thread_state_id;
    std::optional<uint32_t> irq_context;
    uint32_t utid;
    uint32_t waker_utid;

    bool operator==(const SpuriousSchedWakeupTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::thread_state_id::Equals(thread_state_id, other.thread_state_id) &&
       ColumnType::irq_context::Equals(irq_context, other.irq_context) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::waker_utid::Equals(waker_utid, other.waker_utid);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::ts::default_flags();
    static constexpr uint32_t thread_state_id = ColumnType::thread_state_id::default_flags();
    static constexpr uint32_t irq_context = ColumnType::irq_context::default_flags();
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
    static constexpr uint32_t waker_utid = ColumnType::waker_utid::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      SpuriousSchedWakeupTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    SpuriousSchedWakeupTable, RowNumber> {
   public:
    ConstRowReference(const SpuriousSchedWakeupTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::ts::type ts() const {
      return table_->ts()[row_number_];
    }
    ColumnType::thread_state_id::type thread_state_id() const {
      return table_->thread_state_id()[row_number_];
    }
    ColumnType::irq_context::type irq_context() const {
      return table_->irq_context()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
    ColumnType::waker_utid::type waker_utid() const {
      return table_->waker_utid()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const SpuriousSchedWakeupTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_thread_state_id(
        ColumnType::thread_state_id::non_optional_type v) {
      return mutable_table()->mutable_thread_state_id()->Set(row_number_, v);
    }
    void set_irq_context(
        ColumnType::irq_context::non_optional_type v) {
      return mutable_table()->mutable_irq_context()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_waker_utid(
        ColumnType::waker_utid::non_optional_type v) {
      return mutable_table()->mutable_waker_utid()->Set(row_number_, v);
    }

   private:
    SpuriousSchedWakeupTable* mutable_table() const {
      return const_cast<SpuriousSchedWakeupTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, SpuriousSchedWakeupTable, RowNumber, ConstRowReference> {
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
    ColumnType::ts::type ts() const {
      const auto& col = table_->ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::thread_state_id::type thread_state_id() const {
      const auto& col = table_->thread_state_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::irq_context::type irq_context() const {
      const auto& col = table_->irq_context();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::waker_utid::type waker_utid() const {
      const auto& col = table_->waker_utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const SpuriousSchedWakeupTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class SpuriousSchedWakeupTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, SpuriousSchedWakeupTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class SpuriousSchedWakeupTable;

     explicit Iterator(SpuriousSchedWakeupTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     SpuriousSchedWakeupTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      SpuriousSchedWakeupTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "thread_state_id", &self->thread_state_id_, ColumnFlag::thread_state_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "irq_context", &self->irq_context_, ColumnFlag::irq_context,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "waker_utid", &self->waker_utid_, ColumnFlag::waker_utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit SpuriousSchedWakeupTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        thread_state_id_(ColumnStorage<ColumnType::thread_state_id::stored_type>::Create<false>()),
        irq_context_(ColumnStorage<ColumnType::irq_context::stored_type>::Create<false>()),
        utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>()),
        waker_utid_(ColumnStorage<ColumnType::waker_utid::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          true)),
        thread_state_id_storage_layer_(
        new column::NumericStorage<ColumnType::thread_state_id::non_optional_stored_type>(
          &thread_state_id_.vector(),
          ColumnTypeHelper<ColumnType::thread_state_id::stored_type>::ToColumnType(),
          false)),
        irq_context_storage_layer_(
          new column::NumericStorage<ColumnType::irq_context::non_optional_stored_type>(
            &irq_context_.non_null_vector(),
            ColumnTypeHelper<ColumnType::irq_context::stored_type>::ToColumnType(),
            false)),
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false)),
        waker_utid_storage_layer_(
        new column::NumericStorage<ColumnType::waker_utid::non_optional_stored_type>(
          &waker_utid_.vector(),
          ColumnTypeHelper<ColumnType::waker_utid::stored_type>::ToColumnType(),
          false))
,
        irq_context_null_layer_(new column::NullOverlay(irq_context_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::thread_state_id::stored_type>(
          ColumnFlag::thread_state_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::irq_context::stored_type>(
          ColumnFlag::irq_context),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::waker_utid::stored_type>(
          ColumnFlag::waker_utid),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,thread_state_id_storage_layer_,irq_context_storage_layer_,utid_storage_layer_,waker_utid_storage_layer_},
      {{},{},{},{},irq_context_null_layer_,{},{}});
  }
  ~SpuriousSchedWakeupTable() override;

  static const char* Name() { return "spurious_sched_wakeup"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts", ColumnType::ts::SqlValueType(), false,
        true,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "thread_state_id", ColumnType::thread_state_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "irq_context", ColumnType::irq_context::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "waker_utid", ColumnType::waker_utid::SqlValueType(), false,
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
    ts_.ShrinkToFit();
    thread_state_id_.ShrinkToFit();
    irq_context_.ShrinkToFit();
    utid_.ShrinkToFit();
    waker_utid_.ShrinkToFit();
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
    mutable_ts()->Append(row.ts);
    mutable_thread_state_id()->Append(row.thread_state_id);
    mutable_irq_context()->Append(row.irq_context);
    mutable_utid()->Append(row.utid);
    mutable_waker_utid()->Append(row.waker_utid);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<SpuriousSchedWakeupTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<int64_t>& thread_state_id() const {
    return static_cast<const ColumnType::thread_state_id&>(columns()[ColumnIndex::thread_state_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& irq_context() const {
    return static_cast<const ColumnType::irq_context&>(columns()[ColumnIndex::irq_context]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }
  const TypedColumn<uint32_t>& waker_utid() const {
    return static_cast<const ColumnType::waker_utid&>(columns()[ColumnIndex::waker_utid]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<int64_t>* mutable_thread_state_id() {
    return static_cast<ColumnType::thread_state_id*>(
        GetColumn(ColumnIndex::thread_state_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_irq_context() {
    return static_cast<ColumnType::irq_context*>(
        GetColumn(ColumnIndex::irq_context));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<uint32_t>* mutable_waker_utid() {
    return static_cast<ColumnType::waker_utid*>(
        GetColumn(ColumnIndex::waker_utid));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::thread_state_id::stored_type> thread_state_id_;
  ColumnStorage<ColumnType::irq_context::stored_type> irq_context_;
  ColumnStorage<ColumnType::utid::stored_type> utid_;
  ColumnStorage<ColumnType::waker_utid::stored_type> waker_utid_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> thread_state_id_storage_layer_;
  RefPtr<column::StorageLayer> irq_context_storage_layer_;
  RefPtr<column::StorageLayer> utid_storage_layer_;
  RefPtr<column::StorageLayer> waker_utid_storage_layer_;

  RefPtr<column::OverlayLayer> irq_context_null_layer_;
};
  

class ThreadStateTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 12;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t ts = 2;
    static constexpr uint32_t dur = 3;
    static constexpr uint32_t utid = 4;
    static constexpr uint32_t state = 5;
    static constexpr uint32_t io_wait = 6;
    static constexpr uint32_t blocked_function = 7;
    static constexpr uint32_t waker_utid = 8;
    static constexpr uint32_t waker_id = 9;
    static constexpr uint32_t irq_context = 10;
    static constexpr uint32_t ucpu = 11;
  };
  struct ColumnType {
    using id = IdColumn<ThreadStateTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using dur = TypedColumn<int64_t>;
    using utid = TypedColumn<uint32_t>;
    using state = TypedColumn<StringPool::Id>;
    using io_wait = TypedColumn<std::optional<uint32_t>>;
    using blocked_function = TypedColumn<std::optional<StringPool::Id>>;
    using waker_utid = TypedColumn<std::optional<uint32_t>>;
    using waker_id = TypedColumn<std::optional<ThreadStateTable::Id>>;
    using irq_context = TypedColumn<std::optional<uint32_t>>;
    using ucpu = TypedColumn<std::optional<CpuTable::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        int64_t in_dur = {},
        uint32_t in_utid = {},
        StringPool::Id in_state = {},
        std::optional<uint32_t> in_io_wait = {},
        std::optional<StringPool::Id> in_blocked_function = {},
        std::optional<uint32_t> in_waker_utid = {},
        std::optional<ThreadStateTable::Id> in_waker_id = {},
        std::optional<uint32_t> in_irq_context = {},
        std::optional<CpuTable::Id> in_ucpu = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          dur(in_dur),
          utid(in_utid),
          state(in_state),
          io_wait(in_io_wait),
          blocked_function(in_blocked_function),
          waker_utid(in_waker_utid),
          waker_id(in_waker_id),
          irq_context(in_irq_context),
          ucpu(in_ucpu) {
      type_ = "__intrinsic_thread_state";
    }
    int64_t ts;
    int64_t dur;
    uint32_t utid;
    StringPool::Id state;
    std::optional<uint32_t> io_wait;
    std::optional<StringPool::Id> blocked_function;
    std::optional<uint32_t> waker_utid;
    std::optional<ThreadStateTable::Id> waker_id;
    std::optional<uint32_t> irq_context;
    std::optional<CpuTable::Id> ucpu;

    bool operator==(const ThreadStateTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::dur::Equals(dur, other.dur) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::state::Equals(state, other.state) &&
       ColumnType::io_wait::Equals(io_wait, other.io_wait) &&
       ColumnType::blocked_function::Equals(blocked_function, other.blocked_function) &&
       ColumnType::waker_utid::Equals(waker_utid, other.waker_utid) &&
       ColumnType::waker_id::Equals(waker_id, other.waker_id) &&
       ColumnType::irq_context::Equals(irq_context, other.irq_context) &&
       ColumnType::ucpu::Equals(ucpu, other.ucpu);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::ts::default_flags();
    static constexpr uint32_t dur = ColumnType::dur::default_flags();
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
    static constexpr uint32_t state = ColumnType::state::default_flags();
    static constexpr uint32_t io_wait = ColumnType::io_wait::default_flags();
    static constexpr uint32_t blocked_function = ColumnType::blocked_function::default_flags();
    static constexpr uint32_t waker_utid = ColumnType::waker_utid::default_flags();
    static constexpr uint32_t waker_id = ColumnType::waker_id::default_flags();
    static constexpr uint32_t irq_context = ColumnType::irq_context::default_flags();
    static constexpr uint32_t ucpu = ColumnType::ucpu::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ThreadStateTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ThreadStateTable, RowNumber> {
   public:
    ConstRowReference(const ThreadStateTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::ts::type ts() const {
      return table_->ts()[row_number_];
    }
    ColumnType::dur::type dur() const {
      return table_->dur()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
    ColumnType::state::type state() const {
      return table_->state()[row_number_];
    }
    ColumnType::io_wait::type io_wait() const {
      return table_->io_wait()[row_number_];
    }
    ColumnType::blocked_function::type blocked_function() const {
      return table_->blocked_function()[row_number_];
    }
    ColumnType::waker_utid::type waker_utid() const {
      return table_->waker_utid()[row_number_];
    }
    ColumnType::waker_id::type waker_id() const {
      return table_->waker_id()[row_number_];
    }
    ColumnType::irq_context::type irq_context() const {
      return table_->irq_context()[row_number_];
    }
    ColumnType::ucpu::type ucpu() const {
      return table_->ucpu()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ThreadStateTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_dur(
        ColumnType::dur::non_optional_type v) {
      return mutable_table()->mutable_dur()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_state(
        ColumnType::state::non_optional_type v) {
      return mutable_table()->mutable_state()->Set(row_number_, v);
    }
    void set_io_wait(
        ColumnType::io_wait::non_optional_type v) {
      return mutable_table()->mutable_io_wait()->Set(row_number_, v);
    }
    void set_blocked_function(
        ColumnType::blocked_function::non_optional_type v) {
      return mutable_table()->mutable_blocked_function()->Set(row_number_, v);
    }
    void set_waker_utid(
        ColumnType::waker_utid::non_optional_type v) {
      return mutable_table()->mutable_waker_utid()->Set(row_number_, v);
    }
    void set_waker_id(
        ColumnType::waker_id::non_optional_type v) {
      return mutable_table()->mutable_waker_id()->Set(row_number_, v);
    }
    void set_irq_context(
        ColumnType::irq_context::non_optional_type v) {
      return mutable_table()->mutable_irq_context()->Set(row_number_, v);
    }
    void set_ucpu(
        ColumnType::ucpu::non_optional_type v) {
      return mutable_table()->mutable_ucpu()->Set(row_number_, v);
    }

   private:
    ThreadStateTable* mutable_table() const {
      return const_cast<ThreadStateTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ThreadStateTable, RowNumber, ConstRowReference> {
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
    ColumnType::ts::type ts() const {
      const auto& col = table_->ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::dur::type dur() const {
      const auto& col = table_->dur();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::state::type state() const {
      const auto& col = table_->state();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::io_wait::type io_wait() const {
      const auto& col = table_->io_wait();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::blocked_function::type blocked_function() const {
      const auto& col = table_->blocked_function();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::waker_utid::type waker_utid() const {
      const auto& col = table_->waker_utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::waker_id::type waker_id() const {
      const auto& col = table_->waker_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::irq_context::type irq_context() const {
      const auto& col = table_->irq_context();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ucpu::type ucpu() const {
      const auto& col = table_->ucpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ThreadStateTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ThreadStateTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ThreadStateTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ThreadStateTable;

     explicit Iterator(ThreadStateTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ThreadStateTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ThreadStateTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "dur", &self->dur_, ColumnFlag::dur,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "state", &self->state_, ColumnFlag::state,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "io_wait", &self->io_wait_, ColumnFlag::io_wait,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "blocked_function", &self->blocked_function_, ColumnFlag::blocked_function,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "waker_utid", &self->waker_utid_, ColumnFlag::waker_utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "waker_id", &self->waker_id_, ColumnFlag::waker_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "irq_context", &self->irq_context_, ColumnFlag::irq_context,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ucpu", &self->ucpu_, ColumnFlag::ucpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ThreadStateTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        dur_(ColumnStorage<ColumnType::dur::stored_type>::Create<false>()),
        utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>()),
        state_(ColumnStorage<ColumnType::state::stored_type>::Create<false>()),
        io_wait_(ColumnStorage<ColumnType::io_wait::stored_type>::Create<false>()),
        blocked_function_(ColumnStorage<ColumnType::blocked_function::stored_type>::Create<false>()),
        waker_utid_(ColumnStorage<ColumnType::waker_utid::stored_type>::Create<false>()),
        waker_id_(ColumnStorage<ColumnType::waker_id::stored_type>::Create<false>()),
        irq_context_(ColumnStorage<ColumnType::irq_context::stored_type>::Create<false>()),
        ucpu_(ColumnStorage<ColumnType::ucpu::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          true)),
        dur_storage_layer_(
        new column::NumericStorage<ColumnType::dur::non_optional_stored_type>(
          &dur_.vector(),
          ColumnTypeHelper<ColumnType::dur::stored_type>::ToColumnType(),
          false)),
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false)),
        state_storage_layer_(
          new column::StringStorage(string_pool(), &state_.vector())),
        io_wait_storage_layer_(
          new column::NumericStorage<ColumnType::io_wait::non_optional_stored_type>(
            &io_wait_.non_null_vector(),
            ColumnTypeHelper<ColumnType::io_wait::stored_type>::ToColumnType(),
            false)),
        blocked_function_storage_layer_(
          new column::StringStorage(string_pool(), &blocked_function_.vector())),
        waker_utid_storage_layer_(
          new column::NumericStorage<ColumnType::waker_utid::non_optional_stored_type>(
            &waker_utid_.non_null_vector(),
            ColumnTypeHelper<ColumnType::waker_utid::stored_type>::ToColumnType(),
            false)),
        waker_id_storage_layer_(
          new column::NumericStorage<ColumnType::waker_id::non_optional_stored_type>(
            &waker_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::waker_id::stored_type>::ToColumnType(),
            false)),
        irq_context_storage_layer_(
          new column::NumericStorage<ColumnType::irq_context::non_optional_stored_type>(
            &irq_context_.non_null_vector(),
            ColumnTypeHelper<ColumnType::irq_context::stored_type>::ToColumnType(),
            false)),
        ucpu_storage_layer_(
          new column::NumericStorage<ColumnType::ucpu::non_optional_stored_type>(
            &ucpu_.non_null_vector(),
            ColumnTypeHelper<ColumnType::ucpu::stored_type>::ToColumnType(),
            false))
,
        io_wait_null_layer_(new column::NullOverlay(io_wait_.bv())),
        waker_utid_null_layer_(new column::NullOverlay(waker_utid_.bv())),
        waker_id_null_layer_(new column::NullOverlay(waker_id_.bv())),
        irq_context_null_layer_(new column::NullOverlay(irq_context_.bv())),
        ucpu_null_layer_(new column::NullOverlay(ucpu_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::dur::stored_type>(
          ColumnFlag::dur),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::state::stored_type>(
          ColumnFlag::state),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::io_wait::stored_type>(
          ColumnFlag::io_wait),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::blocked_function::stored_type>(
          ColumnFlag::blocked_function),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::waker_utid::stored_type>(
          ColumnFlag::waker_utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::waker_id::stored_type>(
          ColumnFlag::waker_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::irq_context::stored_type>(
          ColumnFlag::irq_context),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ucpu::stored_type>(
          ColumnFlag::ucpu),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,dur_storage_layer_,utid_storage_layer_,state_storage_layer_,io_wait_storage_layer_,blocked_function_storage_layer_,waker_utid_storage_layer_,waker_id_storage_layer_,irq_context_storage_layer_,ucpu_storage_layer_},
      {{},{},{},{},{},{},io_wait_null_layer_,{},waker_utid_null_layer_,waker_id_null_layer_,irq_context_null_layer_,ucpu_null_layer_});
  }
  ~ThreadStateTable() override;

  static const char* Name() { return "__intrinsic_thread_state"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts", ColumnType::ts::SqlValueType(), false,
        true,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "dur", ColumnType::dur::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "state", ColumnType::state::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "io_wait", ColumnType::io_wait::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "blocked_function", ColumnType::blocked_function::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "waker_utid", ColumnType::waker_utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "waker_id", ColumnType::waker_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "irq_context", ColumnType::irq_context::SqlValueType(), false,
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
    type_.ShrinkToFit();
    ts_.ShrinkToFit();
    dur_.ShrinkToFit();
    utid_.ShrinkToFit();
    state_.ShrinkToFit();
    io_wait_.ShrinkToFit();
    blocked_function_.ShrinkToFit();
    waker_utid_.ShrinkToFit();
    waker_id_.ShrinkToFit();
    irq_context_.ShrinkToFit();
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
    Id id = Id{row_number};
    type_.Append(string_pool()->InternString(row.type()));
    mutable_ts()->Append(row.ts);
    mutable_dur()->Append(row.dur);
    mutable_utid()->Append(row.utid);
    mutable_state()->Append(row.state);
    mutable_io_wait()->Append(row.io_wait);
    mutable_blocked_function()->Append(row.blocked_function);
    mutable_waker_utid()->Append(row.waker_utid);
    mutable_waker_id()->Append(row.waker_id);
    mutable_irq_context()->Append(row.irq_context);
    mutable_ucpu()->Append(row.ucpu);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ThreadStateTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<int64_t>& dur() const {
    return static_cast<const ColumnType::dur&>(columns()[ColumnIndex::dur]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }
  const TypedColumn<StringPool::Id>& state() const {
    return static_cast<const ColumnType::state&>(columns()[ColumnIndex::state]);
  }
  const TypedColumn<std::optional<uint32_t>>& io_wait() const {
    return static_cast<const ColumnType::io_wait&>(columns()[ColumnIndex::io_wait]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& blocked_function() const {
    return static_cast<const ColumnType::blocked_function&>(columns()[ColumnIndex::blocked_function]);
  }
  const TypedColumn<std::optional<uint32_t>>& waker_utid() const {
    return static_cast<const ColumnType::waker_utid&>(columns()[ColumnIndex::waker_utid]);
  }
  const TypedColumn<std::optional<ThreadStateTable::Id>>& waker_id() const {
    return static_cast<const ColumnType::waker_id&>(columns()[ColumnIndex::waker_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& irq_context() const {
    return static_cast<const ColumnType::irq_context&>(columns()[ColumnIndex::irq_context]);
  }
  const TypedColumn<std::optional<CpuTable::Id>>& ucpu() const {
    return static_cast<const ColumnType::ucpu&>(columns()[ColumnIndex::ucpu]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<int64_t>* mutable_dur() {
    return static_cast<ColumnType::dur*>(
        GetColumn(ColumnIndex::dur));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<StringPool::Id>* mutable_state() {
    return static_cast<ColumnType::state*>(
        GetColumn(ColumnIndex::state));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_io_wait() {
    return static_cast<ColumnType::io_wait*>(
        GetColumn(ColumnIndex::io_wait));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_blocked_function() {
    return static_cast<ColumnType::blocked_function*>(
        GetColumn(ColumnIndex::blocked_function));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_waker_utid() {
    return static_cast<ColumnType::waker_utid*>(
        GetColumn(ColumnIndex::waker_utid));
  }
  TypedColumn<std::optional<ThreadStateTable::Id>>* mutable_waker_id() {
    return static_cast<ColumnType::waker_id*>(
        GetColumn(ColumnIndex::waker_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_irq_context() {
    return static_cast<ColumnType::irq_context*>(
        GetColumn(ColumnIndex::irq_context));
  }
  TypedColumn<std::optional<CpuTable::Id>>* mutable_ucpu() {
    return static_cast<ColumnType::ucpu*>(
        GetColumn(ColumnIndex::ucpu));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::dur::stored_type> dur_;
  ColumnStorage<ColumnType::utid::stored_type> utid_;
  ColumnStorage<ColumnType::state::stored_type> state_;
  ColumnStorage<ColumnType::io_wait::stored_type> io_wait_;
  ColumnStorage<ColumnType::blocked_function::stored_type> blocked_function_;
  ColumnStorage<ColumnType::waker_utid::stored_type> waker_utid_;
  ColumnStorage<ColumnType::waker_id::stored_type> waker_id_;
  ColumnStorage<ColumnType::irq_context::stored_type> irq_context_;
  ColumnStorage<ColumnType::ucpu::stored_type> ucpu_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> dur_storage_layer_;
  RefPtr<column::StorageLayer> utid_storage_layer_;
  RefPtr<column::StorageLayer> state_storage_layer_;
  RefPtr<column::StorageLayer> io_wait_storage_layer_;
  RefPtr<column::StorageLayer> blocked_function_storage_layer_;
  RefPtr<column::StorageLayer> waker_utid_storage_layer_;
  RefPtr<column::StorageLayer> waker_id_storage_layer_;
  RefPtr<column::StorageLayer> irq_context_storage_layer_;
  RefPtr<column::StorageLayer> ucpu_storage_layer_;

  RefPtr<column::OverlayLayer> io_wait_null_layer_;
  RefPtr<column::OverlayLayer> waker_utid_null_layer_;
  RefPtr<column::OverlayLayer> waker_id_null_layer_;
  RefPtr<column::OverlayLayer> irq_context_null_layer_;
  RefPtr<column::OverlayLayer> ucpu_null_layer_;
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_SCHED_TABLES_PY_H_
