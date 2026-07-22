#ifndef SRC_TRACE_PROCESSOR_TABLES_METADATA_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_METADATA_TABLES_PY_H_

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



namespace perfetto::trace_processor::tables {

class MachineTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 3;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t raw_id = 2;
  };
  struct ColumnType {
    using id = IdColumn<MachineTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using raw_id = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_raw_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          raw_id(in_raw_id) {
      type_ = "machine";
    }
    uint32_t raw_id;

    bool operator==(const MachineTable::Row& other) const {
      return type() == other.type() && ColumnType::raw_id::Equals(raw_id, other.raw_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t raw_id = ColumnType::raw_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      MachineTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    MachineTable, RowNumber> {
   public:
    ConstRowReference(const MachineTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::raw_id::type raw_id() const {
      return table_->raw_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const MachineTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_raw_id(
        ColumnType::raw_id::non_optional_type v) {
      return mutable_table()->mutable_raw_id()->Set(row_number_, v);
    }

   private:
    MachineTable* mutable_table() const {
      return const_cast<MachineTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, MachineTable, RowNumber, ConstRowReference> {
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
    ColumnType::raw_id::type raw_id() const {
      const auto& col = table_->raw_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const MachineTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class MachineTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, MachineTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class MachineTable;

     explicit Iterator(MachineTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     MachineTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      MachineTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "raw_id", &self->raw_id_, ColumnFlag::raw_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit MachineTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        raw_id_(ColumnStorage<ColumnType::raw_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        raw_id_storage_layer_(
        new column::NumericStorage<ColumnType::raw_id::non_optional_stored_type>(
          &raw_id_.vector(),
          ColumnTypeHelper<ColumnType::raw_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::raw_id::stored_type>(
          ColumnFlag::raw_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,raw_id_storage_layer_},
      {{},{},{}});
  }
  ~MachineTable() override;

  static const char* Name() { return "machine"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "raw_id", ColumnType::raw_id::SqlValueType(), false,
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
    raw_id_.ShrinkToFit();
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
    mutable_raw_id()->Append(row.raw_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<MachineTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& raw_id() const {
    return static_cast<const ColumnType::raw_id&>(columns()[ColumnIndex::raw_id]);
  }

  TypedColumn<uint32_t>* mutable_raw_id() {
    return static_cast<ColumnType::raw_id*>(
        GetColumn(ColumnIndex::raw_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::raw_id::stored_type> raw_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> raw_id_storage_layer_;

  
};
  

class ProcessTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t pid = 2;
    static constexpr uint32_t name = 3;
    static constexpr uint32_t start_ts = 4;
    static constexpr uint32_t end_ts = 5;
    static constexpr uint32_t parent_upid = 6;
    static constexpr uint32_t uid = 7;
    static constexpr uint32_t android_appid = 8;
    static constexpr uint32_t cmdline = 9;
    static constexpr uint32_t arg_set_id = 10;
    static constexpr uint32_t machine_id = 11;
  };
  struct ColumnType {
    using id = IdColumn<ProcessTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using pid = TypedColumn<uint32_t>;
    using name = TypedColumn<std::optional<StringPool::Id>>;
    using start_ts = TypedColumn<std::optional<int64_t>>;
    using end_ts = TypedColumn<std::optional<int64_t>>;
    using parent_upid = TypedColumn<std::optional<uint32_t>>;
    using uid = TypedColumn<std::optional<uint32_t>>;
    using android_appid = TypedColumn<std::optional<uint32_t>>;
    using cmdline = TypedColumn<std::optional<StringPool::Id>>;
    using arg_set_id = TypedColumn<uint32_t>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_pid = {},
        std::optional<StringPool::Id> in_name = {},
        std::optional<int64_t> in_start_ts = {},
        std::optional<int64_t> in_end_ts = {},
        std::optional<uint32_t> in_parent_upid = {},
        std::optional<uint32_t> in_uid = {},
        std::optional<uint32_t> in_android_appid = {},
        std::optional<StringPool::Id> in_cmdline = {},
        uint32_t in_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          pid(in_pid),
          name(in_name),
          start_ts(in_start_ts),
          end_ts(in_end_ts),
          parent_upid(in_parent_upid),
          uid(in_uid),
          android_appid(in_android_appid),
          cmdline(in_cmdline),
          arg_set_id(in_arg_set_id),
          machine_id(in_machine_id) {
      type_ = "__intrinsic_process";
    }
    uint32_t pid;
    std::optional<StringPool::Id> name;
    std::optional<int64_t> start_ts;
    std::optional<int64_t> end_ts;
    std::optional<uint32_t> parent_upid;
    std::optional<uint32_t> uid;
    std::optional<uint32_t> android_appid;
    std::optional<StringPool::Id> cmdline;
    uint32_t arg_set_id;
    std::optional<MachineTable::Id> machine_id;

    bool operator==(const ProcessTable::Row& other) const {
      return type() == other.type() && ColumnType::pid::Equals(pid, other.pid) &&
       ColumnType::name::Equals(name, other.name) &&
       ColumnType::start_ts::Equals(start_ts, other.start_ts) &&
       ColumnType::end_ts::Equals(end_ts, other.end_ts) &&
       ColumnType::parent_upid::Equals(parent_upid, other.parent_upid) &&
       ColumnType::uid::Equals(uid, other.uid) &&
       ColumnType::android_appid::Equals(android_appid, other.android_appid) &&
       ColumnType::cmdline::Equals(cmdline, other.cmdline) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t pid = ColumnType::pid::default_flags();
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t start_ts = ColumnType::start_ts::default_flags();
    static constexpr uint32_t end_ts = ColumnType::end_ts::default_flags();
    static constexpr uint32_t parent_upid = ColumnType::parent_upid::default_flags();
    static constexpr uint32_t uid = ColumnType::uid::default_flags();
    static constexpr uint32_t android_appid = ColumnType::android_appid::default_flags();
    static constexpr uint32_t cmdline = ColumnType::cmdline::default_flags();
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
    static constexpr uint32_t machine_id = ColumnType::machine_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ProcessTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ProcessTable, RowNumber> {
   public:
    ConstRowReference(const ProcessTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::pid::type pid() const {
      return table_->pid()[row_number_];
    }
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::start_ts::type start_ts() const {
      return table_->start_ts()[row_number_];
    }
    ColumnType::end_ts::type end_ts() const {
      return table_->end_ts()[row_number_];
    }
    ColumnType::parent_upid::type parent_upid() const {
      return table_->parent_upid()[row_number_];
    }
    ColumnType::uid::type uid() const {
      return table_->uid()[row_number_];
    }
    ColumnType::android_appid::type android_appid() const {
      return table_->android_appid()[row_number_];
    }
    ColumnType::cmdline::type cmdline() const {
      return table_->cmdline()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ProcessTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_pid(
        ColumnType::pid::non_optional_type v) {
      return mutable_table()->mutable_pid()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_start_ts(
        ColumnType::start_ts::non_optional_type v) {
      return mutable_table()->mutable_start_ts()->Set(row_number_, v);
    }
    void set_end_ts(
        ColumnType::end_ts::non_optional_type v) {
      return mutable_table()->mutable_end_ts()->Set(row_number_, v);
    }
    void set_parent_upid(
        ColumnType::parent_upid::non_optional_type v) {
      return mutable_table()->mutable_parent_upid()->Set(row_number_, v);
    }
    void set_uid(
        ColumnType::uid::non_optional_type v) {
      return mutable_table()->mutable_uid()->Set(row_number_, v);
    }
    void set_android_appid(
        ColumnType::android_appid::non_optional_type v) {
      return mutable_table()->mutable_android_appid()->Set(row_number_, v);
    }
    void set_cmdline(
        ColumnType::cmdline::non_optional_type v) {
      return mutable_table()->mutable_cmdline()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }

   private:
    ProcessTable* mutable_table() const {
      return const_cast<ProcessTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ProcessTable, RowNumber, ConstRowReference> {
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
    ColumnType::pid::type pid() const {
      const auto& col = table_->pid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::start_ts::type start_ts() const {
      const auto& col = table_->start_ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::end_ts::type end_ts() const {
      const auto& col = table_->end_ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::parent_upid::type parent_upid() const {
      const auto& col = table_->parent_upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::uid::type uid() const {
      const auto& col = table_->uid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::android_appid::type android_appid() const {
      const auto& col = table_->android_appid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::cmdline::type cmdline() const {
      const auto& col = table_->cmdline();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ProcessTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ProcessTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ProcessTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ProcessTable;

     explicit Iterator(ProcessTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ProcessTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ProcessTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "pid", &self->pid_, ColumnFlag::pid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "start_ts", &self->start_ts_, ColumnFlag::start_ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "end_ts", &self->end_ts_, ColumnFlag::end_ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "parent_upid", &self->parent_upid_, ColumnFlag::parent_upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "uid", &self->uid_, ColumnFlag::uid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "android_appid", &self->android_appid_, ColumnFlag::android_appid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cmdline", &self->cmdline_, ColumnFlag::cmdline,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "machine_id", &self->machine_id_, ColumnFlag::machine_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ProcessTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        pid_(ColumnStorage<ColumnType::pid::stored_type>::Create<false>()),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        start_ts_(ColumnStorage<ColumnType::start_ts::stored_type>::Create<false>()),
        end_ts_(ColumnStorage<ColumnType::end_ts::stored_type>::Create<false>()),
        parent_upid_(ColumnStorage<ColumnType::parent_upid::stored_type>::Create<false>()),
        uid_(ColumnStorage<ColumnType::uid::stored_type>::Create<false>()),
        android_appid_(ColumnStorage<ColumnType::android_appid::stored_type>::Create<false>()),
        cmdline_(ColumnStorage<ColumnType::cmdline::stored_type>::Create<false>()),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>()),
        machine_id_(ColumnStorage<ColumnType::machine_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        pid_storage_layer_(
        new column::NumericStorage<ColumnType::pid::non_optional_stored_type>(
          &pid_.vector(),
          ColumnTypeHelper<ColumnType::pid::stored_type>::ToColumnType(),
          false)),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        start_ts_storage_layer_(
          new column::NumericStorage<ColumnType::start_ts::non_optional_stored_type>(
            &start_ts_.non_null_vector(),
            ColumnTypeHelper<ColumnType::start_ts::stored_type>::ToColumnType(),
            false)),
        end_ts_storage_layer_(
          new column::NumericStorage<ColumnType::end_ts::non_optional_stored_type>(
            &end_ts_.non_null_vector(),
            ColumnTypeHelper<ColumnType::end_ts::stored_type>::ToColumnType(),
            false)),
        parent_upid_storage_layer_(
          new column::NumericStorage<ColumnType::parent_upid::non_optional_stored_type>(
            &parent_upid_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_upid::stored_type>::ToColumnType(),
            false)),
        uid_storage_layer_(
          new column::NumericStorage<ColumnType::uid::non_optional_stored_type>(
            &uid_.non_null_vector(),
            ColumnTypeHelper<ColumnType::uid::stored_type>::ToColumnType(),
            false)),
        android_appid_storage_layer_(
          new column::NumericStorage<ColumnType::android_appid::non_optional_stored_type>(
            &android_appid_.non_null_vector(),
            ColumnTypeHelper<ColumnType::android_appid::stored_type>::ToColumnType(),
            false)),
        cmdline_storage_layer_(
          new column::StringStorage(string_pool(), &cmdline_.vector())),
        arg_set_id_storage_layer_(
        new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
          &arg_set_id_.vector(),
          ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
          false)),
        machine_id_storage_layer_(
          new column::NumericStorage<ColumnType::machine_id::non_optional_stored_type>(
            &machine_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::machine_id::stored_type>::ToColumnType(),
            false))
,
        start_ts_null_layer_(new column::NullOverlay(start_ts_.bv())),
        end_ts_null_layer_(new column::NullOverlay(end_ts_.bv())),
        parent_upid_null_layer_(new column::NullOverlay(parent_upid_.bv())),
        uid_null_layer_(new column::NullOverlay(uid_.bv())),
        android_appid_null_layer_(new column::NullOverlay(android_appid_.bv())),
        machine_id_null_layer_(new column::NullOverlay(machine_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::pid::stored_type>(
          ColumnFlag::pid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::start_ts::stored_type>(
          ColumnFlag::start_ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::end_ts::stored_type>(
          ColumnFlag::end_ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_upid::stored_type>(
          ColumnFlag::parent_upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::uid::stored_type>(
          ColumnFlag::uid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::android_appid::stored_type>(
          ColumnFlag::android_appid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cmdline::stored_type>(
          ColumnFlag::cmdline),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::machine_id::stored_type>(
          ColumnFlag::machine_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,pid_storage_layer_,name_storage_layer_,start_ts_storage_layer_,end_ts_storage_layer_,parent_upid_storage_layer_,uid_storage_layer_,android_appid_storage_layer_,cmdline_storage_layer_,arg_set_id_storage_layer_,machine_id_storage_layer_},
      {{},{},{},{},start_ts_null_layer_,end_ts_null_layer_,parent_upid_null_layer_,uid_null_layer_,android_appid_null_layer_,{},{},machine_id_null_layer_});
  }
  ~ProcessTable() override;

  static const char* Name() { return "__intrinsic_process"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "pid", ColumnType::pid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "start_ts", ColumnType::start_ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "end_ts", ColumnType::end_ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "parent_upid", ColumnType::parent_upid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "uid", ColumnType::uid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "android_appid", ColumnType::android_appid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cmdline", ColumnType::cmdline::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "arg_set_id", ColumnType::arg_set_id::SqlValueType(), false,
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
    pid_.ShrinkToFit();
    name_.ShrinkToFit();
    start_ts_.ShrinkToFit();
    end_ts_.ShrinkToFit();
    parent_upid_.ShrinkToFit();
    uid_.ShrinkToFit();
    android_appid_.ShrinkToFit();
    cmdline_.ShrinkToFit();
    arg_set_id_.ShrinkToFit();
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
    mutable_pid()->Append(row.pid);
    mutable_name()->Append(row.name);
    mutable_start_ts()->Append(row.start_ts);
    mutable_end_ts()->Append(row.end_ts);
    mutable_parent_upid()->Append(row.parent_upid);
    mutable_uid()->Append(row.uid);
    mutable_android_appid()->Append(row.android_appid);
    mutable_cmdline()->Append(row.cmdline);
    mutable_arg_set_id()->Append(row.arg_set_id);
    mutable_machine_id()->Append(row.machine_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ProcessTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& pid() const {
    return static_cast<const ColumnType::pid&>(columns()[ColumnIndex::pid]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<int64_t>>& start_ts() const {
    return static_cast<const ColumnType::start_ts&>(columns()[ColumnIndex::start_ts]);
  }
  const TypedColumn<std::optional<int64_t>>& end_ts() const {
    return static_cast<const ColumnType::end_ts&>(columns()[ColumnIndex::end_ts]);
  }
  const TypedColumn<std::optional<uint32_t>>& parent_upid() const {
    return static_cast<const ColumnType::parent_upid&>(columns()[ColumnIndex::parent_upid]);
  }
  const TypedColumn<std::optional<uint32_t>>& uid() const {
    return static_cast<const ColumnType::uid&>(columns()[ColumnIndex::uid]);
  }
  const TypedColumn<std::optional<uint32_t>>& android_appid() const {
    return static_cast<const ColumnType::android_appid&>(columns()[ColumnIndex::android_appid]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& cmdline() const {
    return static_cast<const ColumnType::cmdline&>(columns()[ColumnIndex::cmdline]);
  }
  const TypedColumn<uint32_t>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }

  TypedColumn<uint32_t>* mutable_pid() {
    return static_cast<ColumnType::pid*>(
        GetColumn(ColumnIndex::pid));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<int64_t>>* mutable_start_ts() {
    return static_cast<ColumnType::start_ts*>(
        GetColumn(ColumnIndex::start_ts));
  }
  TypedColumn<std::optional<int64_t>>* mutable_end_ts() {
    return static_cast<ColumnType::end_ts*>(
        GetColumn(ColumnIndex::end_ts));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_parent_upid() {
    return static_cast<ColumnType::parent_upid*>(
        GetColumn(ColumnIndex::parent_upid));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_uid() {
    return static_cast<ColumnType::uid*>(
        GetColumn(ColumnIndex::uid));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_android_appid() {
    return static_cast<ColumnType::android_appid*>(
        GetColumn(ColumnIndex::android_appid));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_cmdline() {
    return static_cast<ColumnType::cmdline*>(
        GetColumn(ColumnIndex::cmdline));
  }
  TypedColumn<uint32_t>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::pid::stored_type> pid_;
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::start_ts::stored_type> start_ts_;
  ColumnStorage<ColumnType::end_ts::stored_type> end_ts_;
  ColumnStorage<ColumnType::parent_upid::stored_type> parent_upid_;
  ColumnStorage<ColumnType::uid::stored_type> uid_;
  ColumnStorage<ColumnType::android_appid::stored_type> android_appid_;
  ColumnStorage<ColumnType::cmdline::stored_type> cmdline_;
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;
  ColumnStorage<ColumnType::machine_id::stored_type> machine_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> pid_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> start_ts_storage_layer_;
  RefPtr<column::StorageLayer> end_ts_storage_layer_;
  RefPtr<column::StorageLayer> parent_upid_storage_layer_;
  RefPtr<column::StorageLayer> uid_storage_layer_;
  RefPtr<column::StorageLayer> android_appid_storage_layer_;
  RefPtr<column::StorageLayer> cmdline_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;
  RefPtr<column::StorageLayer> machine_id_storage_layer_;

  RefPtr<column::OverlayLayer> start_ts_null_layer_;
  RefPtr<column::OverlayLayer> end_ts_null_layer_;
  RefPtr<column::OverlayLayer> parent_upid_null_layer_;
  RefPtr<column::OverlayLayer> uid_null_layer_;
  RefPtr<column::OverlayLayer> android_appid_null_layer_;
  RefPtr<column::OverlayLayer> machine_id_null_layer_;
};
  

class ThreadTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t tid = 2;
    static constexpr uint32_t name = 3;
    static constexpr uint32_t start_ts = 4;
    static constexpr uint32_t end_ts = 5;
    static constexpr uint32_t upid = 6;
    static constexpr uint32_t is_main_thread = 7;
    static constexpr uint32_t machine_id = 8;
  };
  struct ColumnType {
    using id = IdColumn<ThreadTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using tid = TypedColumn<uint32_t>;
    using name = TypedColumn<std::optional<StringPool::Id>>;
    using start_ts = TypedColumn<std::optional<int64_t>>;
    using end_ts = TypedColumn<std::optional<int64_t>>;
    using upid = TypedColumn<std::optional<uint32_t>>;
    using is_main_thread = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_tid = {},
        std::optional<StringPool::Id> in_name = {},
        std::optional<int64_t> in_start_ts = {},
        std::optional<int64_t> in_end_ts = {},
        std::optional<uint32_t> in_upid = {},
        std::optional<uint32_t> in_is_main_thread = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          tid(in_tid),
          name(in_name),
          start_ts(in_start_ts),
          end_ts(in_end_ts),
          upid(in_upid),
          is_main_thread(in_is_main_thread),
          machine_id(in_machine_id) {
      type_ = "__intrinsic_thread";
    }
    uint32_t tid;
    std::optional<StringPool::Id> name;
    std::optional<int64_t> start_ts;
    std::optional<int64_t> end_ts;
    std::optional<uint32_t> upid;
    std::optional<uint32_t> is_main_thread;
    std::optional<MachineTable::Id> machine_id;

    bool operator==(const ThreadTable::Row& other) const {
      return type() == other.type() && ColumnType::tid::Equals(tid, other.tid) &&
       ColumnType::name::Equals(name, other.name) &&
       ColumnType::start_ts::Equals(start_ts, other.start_ts) &&
       ColumnType::end_ts::Equals(end_ts, other.end_ts) &&
       ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::is_main_thread::Equals(is_main_thread, other.is_main_thread) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t tid = ColumnType::tid::default_flags();
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t start_ts = ColumnType::start_ts::default_flags();
    static constexpr uint32_t end_ts = ColumnType::end_ts::default_flags();
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
    static constexpr uint32_t is_main_thread = ColumnType::is_main_thread::default_flags();
    static constexpr uint32_t machine_id = ColumnType::machine_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ThreadTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ThreadTable, RowNumber> {
   public:
    ConstRowReference(const ThreadTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::tid::type tid() const {
      return table_->tid()[row_number_];
    }
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::start_ts::type start_ts() const {
      return table_->start_ts()[row_number_];
    }
    ColumnType::end_ts::type end_ts() const {
      return table_->end_ts()[row_number_];
    }
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
    ColumnType::is_main_thread::type is_main_thread() const {
      return table_->is_main_thread()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ThreadTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_tid(
        ColumnType::tid::non_optional_type v) {
      return mutable_table()->mutable_tid()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_start_ts(
        ColumnType::start_ts::non_optional_type v) {
      return mutable_table()->mutable_start_ts()->Set(row_number_, v);
    }
    void set_end_ts(
        ColumnType::end_ts::non_optional_type v) {
      return mutable_table()->mutable_end_ts()->Set(row_number_, v);
    }
    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_is_main_thread(
        ColumnType::is_main_thread::non_optional_type v) {
      return mutable_table()->mutable_is_main_thread()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }

   private:
    ThreadTable* mutable_table() const {
      return const_cast<ThreadTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ThreadTable, RowNumber, ConstRowReference> {
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
    ColumnType::tid::type tid() const {
      const auto& col = table_->tid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::start_ts::type start_ts() const {
      const auto& col = table_->start_ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::end_ts::type end_ts() const {
      const auto& col = table_->end_ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::is_main_thread::type is_main_thread() const {
      const auto& col = table_->is_main_thread();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ThreadTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ThreadTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ThreadTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ThreadTable;

     explicit Iterator(ThreadTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ThreadTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ThreadTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "tid", &self->tid_, ColumnFlag::tid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "start_ts", &self->start_ts_, ColumnFlag::start_ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "end_ts", &self->end_ts_, ColumnFlag::end_ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "is_main_thread", &self->is_main_thread_, ColumnFlag::is_main_thread,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "machine_id", &self->machine_id_, ColumnFlag::machine_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ThreadTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        tid_(ColumnStorage<ColumnType::tid::stored_type>::Create<false>()),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        start_ts_(ColumnStorage<ColumnType::start_ts::stored_type>::Create<false>()),
        end_ts_(ColumnStorage<ColumnType::end_ts::stored_type>::Create<false>()),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        is_main_thread_(ColumnStorage<ColumnType::is_main_thread::stored_type>::Create<false>()),
        machine_id_(ColumnStorage<ColumnType::machine_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        tid_storage_layer_(
        new column::NumericStorage<ColumnType::tid::non_optional_stored_type>(
          &tid_.vector(),
          ColumnTypeHelper<ColumnType::tid::stored_type>::ToColumnType(),
          false)),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        start_ts_storage_layer_(
          new column::NumericStorage<ColumnType::start_ts::non_optional_stored_type>(
            &start_ts_.non_null_vector(),
            ColumnTypeHelper<ColumnType::start_ts::stored_type>::ToColumnType(),
            false)),
        end_ts_storage_layer_(
          new column::NumericStorage<ColumnType::end_ts::non_optional_stored_type>(
            &end_ts_.non_null_vector(),
            ColumnTypeHelper<ColumnType::end_ts::stored_type>::ToColumnType(),
            false)),
        upid_storage_layer_(
          new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
            &upid_.non_null_vector(),
            ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
            false)),
        is_main_thread_storage_layer_(
          new column::NumericStorage<ColumnType::is_main_thread::non_optional_stored_type>(
            &is_main_thread_.non_null_vector(),
            ColumnTypeHelper<ColumnType::is_main_thread::stored_type>::ToColumnType(),
            false)),
        machine_id_storage_layer_(
          new column::NumericStorage<ColumnType::machine_id::non_optional_stored_type>(
            &machine_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::machine_id::stored_type>::ToColumnType(),
            false))
,
        start_ts_null_layer_(new column::NullOverlay(start_ts_.bv())),
        end_ts_null_layer_(new column::NullOverlay(end_ts_.bv())),
        upid_null_layer_(new column::NullOverlay(upid_.bv())),
        is_main_thread_null_layer_(new column::NullOverlay(is_main_thread_.bv())),
        machine_id_null_layer_(new column::NullOverlay(machine_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::tid::stored_type>(
          ColumnFlag::tid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::start_ts::stored_type>(
          ColumnFlag::start_ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::end_ts::stored_type>(
          ColumnFlag::end_ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::is_main_thread::stored_type>(
          ColumnFlag::is_main_thread),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::machine_id::stored_type>(
          ColumnFlag::machine_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,tid_storage_layer_,name_storage_layer_,start_ts_storage_layer_,end_ts_storage_layer_,upid_storage_layer_,is_main_thread_storage_layer_,machine_id_storage_layer_},
      {{},{},{},{},start_ts_null_layer_,end_ts_null_layer_,upid_null_layer_,is_main_thread_null_layer_,machine_id_null_layer_});
  }
  ~ThreadTable() override;

  static const char* Name() { return "__intrinsic_thread"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "tid", ColumnType::tid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "start_ts", ColumnType::start_ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "end_ts", ColumnType::end_ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "upid", ColumnType::upid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "is_main_thread", ColumnType::is_main_thread::SqlValueType(), false,
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
    tid_.ShrinkToFit();
    name_.ShrinkToFit();
    start_ts_.ShrinkToFit();
    end_ts_.ShrinkToFit();
    upid_.ShrinkToFit();
    is_main_thread_.ShrinkToFit();
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
    mutable_tid()->Append(row.tid);
    mutable_name()->Append(row.name);
    mutable_start_ts()->Append(row.start_ts);
    mutable_end_ts()->Append(row.end_ts);
    mutable_upid()->Append(row.upid);
    mutable_is_main_thread()->Append(row.is_main_thread);
    mutable_machine_id()->Append(row.machine_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ThreadTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& tid() const {
    return static_cast<const ColumnType::tid&>(columns()[ColumnIndex::tid]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<int64_t>>& start_ts() const {
    return static_cast<const ColumnType::start_ts&>(columns()[ColumnIndex::start_ts]);
  }
  const TypedColumn<std::optional<int64_t>>& end_ts() const {
    return static_cast<const ColumnType::end_ts&>(columns()[ColumnIndex::end_ts]);
  }
  const TypedColumn<std::optional<uint32_t>>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<std::optional<uint32_t>>& is_main_thread() const {
    return static_cast<const ColumnType::is_main_thread&>(columns()[ColumnIndex::is_main_thread]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }

  TypedColumn<uint32_t>* mutable_tid() {
    return static_cast<ColumnType::tid*>(
        GetColumn(ColumnIndex::tid));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<int64_t>>* mutable_start_ts() {
    return static_cast<ColumnType::start_ts*>(
        GetColumn(ColumnIndex::start_ts));
  }
  TypedColumn<std::optional<int64_t>>* mutable_end_ts() {
    return static_cast<ColumnType::end_ts*>(
        GetColumn(ColumnIndex::end_ts));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_is_main_thread() {
    return static_cast<ColumnType::is_main_thread*>(
        GetColumn(ColumnIndex::is_main_thread));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::tid::stored_type> tid_;
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::start_ts::stored_type> start_ts_;
  ColumnStorage<ColumnType::end_ts::stored_type> end_ts_;
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::is_main_thread::stored_type> is_main_thread_;
  ColumnStorage<ColumnType::machine_id::stored_type> machine_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> tid_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> start_ts_storage_layer_;
  RefPtr<column::StorageLayer> end_ts_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> is_main_thread_storage_layer_;
  RefPtr<column::StorageLayer> machine_id_storage_layer_;

  RefPtr<column::OverlayLayer> start_ts_null_layer_;
  RefPtr<column::OverlayLayer> end_ts_null_layer_;
  RefPtr<column::OverlayLayer> upid_null_layer_;
  RefPtr<column::OverlayLayer> is_main_thread_null_layer_;
  RefPtr<column::OverlayLayer> machine_id_null_layer_;
};
  

class ArgTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t arg_set_id = 2;
    static constexpr uint32_t flat_key = 3;
    static constexpr uint32_t key = 4;
    static constexpr uint32_t int_value = 5;
    static constexpr uint32_t string_value = 6;
    static constexpr uint32_t real_value = 7;
    static constexpr uint32_t value_type = 8;
  };
  struct ColumnType {
    using id = IdColumn<ArgTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using arg_set_id = TypedColumn<uint32_t>;
    using flat_key = TypedColumn<StringPool::Id>;
    using key = TypedColumn<StringPool::Id>;
    using int_value = TypedColumn<std::optional<int64_t>>;
    using string_value = TypedColumn<std::optional<StringPool::Id>>;
    using real_value = TypedColumn<std::optional<double>>;
    using value_type = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_arg_set_id = {},
        StringPool::Id in_flat_key = {},
        StringPool::Id in_key = {},
        std::optional<int64_t> in_int_value = {},
        std::optional<StringPool::Id> in_string_value = {},
        std::optional<double> in_real_value = {},
        StringPool::Id in_value_type = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          arg_set_id(in_arg_set_id),
          flat_key(in_flat_key),
          key(in_key),
          int_value(in_int_value),
          string_value(in_string_value),
          real_value(in_real_value),
          value_type(in_value_type) {
      type_ = "__intrinsic_args";
    }
    uint32_t arg_set_id;
    StringPool::Id flat_key;
    StringPool::Id key;
    std::optional<int64_t> int_value;
    std::optional<StringPool::Id> string_value;
    std::optional<double> real_value;
    StringPool::Id value_type;

    bool operator==(const ArgTable::Row& other) const {
      return type() == other.type() && ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id) &&
       ColumnType::flat_key::Equals(flat_key, other.flat_key) &&
       ColumnType::key::Equals(key, other.key) &&
       ColumnType::int_value::Equals(int_value, other.int_value) &&
       ColumnType::string_value::Equals(string_value, other.string_value) &&
       ColumnType::real_value::Equals(real_value, other.real_value) &&
       ColumnType::value_type::Equals(value_type, other.value_type);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t arg_set_id = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::arg_set_id::default_flags();
    static constexpr uint32_t flat_key = ColumnType::flat_key::default_flags();
    static constexpr uint32_t key = ColumnType::key::default_flags();
    static constexpr uint32_t int_value = ColumnType::int_value::default_flags();
    static constexpr uint32_t string_value = ColumnType::string_value::default_flags();
    static constexpr uint32_t real_value = ColumnType::real_value::default_flags();
    static constexpr uint32_t value_type = ColumnType::value_type::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ArgTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ArgTable, RowNumber> {
   public:
    ConstRowReference(const ArgTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
    ColumnType::flat_key::type flat_key() const {
      return table_->flat_key()[row_number_];
    }
    ColumnType::key::type key() const {
      return table_->key()[row_number_];
    }
    ColumnType::int_value::type int_value() const {
      return table_->int_value()[row_number_];
    }
    ColumnType::string_value::type string_value() const {
      return table_->string_value()[row_number_];
    }
    ColumnType::real_value::type real_value() const {
      return table_->real_value()[row_number_];
    }
    ColumnType::value_type::type value_type() const {
      return table_->value_type()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ArgTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }
    void set_flat_key(
        ColumnType::flat_key::non_optional_type v) {
      return mutable_table()->mutable_flat_key()->Set(row_number_, v);
    }
    void set_key(
        ColumnType::key::non_optional_type v) {
      return mutable_table()->mutable_key()->Set(row_number_, v);
    }
    void set_int_value(
        ColumnType::int_value::non_optional_type v) {
      return mutable_table()->mutable_int_value()->Set(row_number_, v);
    }
    void set_string_value(
        ColumnType::string_value::non_optional_type v) {
      return mutable_table()->mutable_string_value()->Set(row_number_, v);
    }
    void set_real_value(
        ColumnType::real_value::non_optional_type v) {
      return mutable_table()->mutable_real_value()->Set(row_number_, v);
    }
    void set_value_type(
        ColumnType::value_type::non_optional_type v) {
      return mutable_table()->mutable_value_type()->Set(row_number_, v);
    }

   private:
    ArgTable* mutable_table() const {
      return const_cast<ArgTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ArgTable, RowNumber, ConstRowReference> {
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
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::flat_key::type flat_key() const {
      const auto& col = table_->flat_key();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::key::type key() const {
      const auto& col = table_->key();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::int_value::type int_value() const {
      const auto& col = table_->int_value();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::string_value::type string_value() const {
      const auto& col = table_->string_value();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::real_value::type real_value() const {
      const auto& col = table_->real_value();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::value_type::type value_type() const {
      const auto& col = table_->value_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ArgTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ArgTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ArgTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ArgTable;

     explicit Iterator(ArgTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ArgTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ArgTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "flat_key", &self->flat_key_, ColumnFlag::flat_key,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "key", &self->key_, ColumnFlag::key,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "int_value", &self->int_value_, ColumnFlag::int_value,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "string_value", &self->string_value_, ColumnFlag::string_value,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "real_value", &self->real_value_, ColumnFlag::real_value,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "value_type", &self->value_type_, ColumnFlag::value_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ArgTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>()),
        flat_key_(ColumnStorage<ColumnType::flat_key::stored_type>::Create<false>()),
        key_(ColumnStorage<ColumnType::key::stored_type>::Create<false>()),
        int_value_(ColumnStorage<ColumnType::int_value::stored_type>::Create<false>()),
        string_value_(ColumnStorage<ColumnType::string_value::stored_type>::Create<false>()),
        real_value_(ColumnStorage<ColumnType::real_value::stored_type>::Create<false>()),
        value_type_(ColumnStorage<ColumnType::value_type::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        arg_set_id_storage_layer_(
        new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
          &arg_set_id_.vector(),
          ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
          true)),
        flat_key_storage_layer_(
          new column::StringStorage(string_pool(), &flat_key_.vector())),
        key_storage_layer_(
          new column::StringStorage(string_pool(), &key_.vector())),
        int_value_storage_layer_(
          new column::NumericStorage<ColumnType::int_value::non_optional_stored_type>(
            &int_value_.non_null_vector(),
            ColumnTypeHelper<ColumnType::int_value::stored_type>::ToColumnType(),
            false)),
        string_value_storage_layer_(
          new column::StringStorage(string_pool(), &string_value_.vector())),
        real_value_storage_layer_(
          new column::NumericStorage<ColumnType::real_value::non_optional_stored_type>(
            &real_value_.non_null_vector(),
            ColumnTypeHelper<ColumnType::real_value::stored_type>::ToColumnType(),
            false)),
        value_type_storage_layer_(
          new column::StringStorage(string_pool(), &value_type_.vector()))
,
        int_value_null_layer_(new column::NullOverlay(int_value_.bv())),
        real_value_null_layer_(new column::NullOverlay(real_value_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::flat_key::stored_type>(
          ColumnFlag::flat_key),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::key::stored_type>(
          ColumnFlag::key),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::int_value::stored_type>(
          ColumnFlag::int_value),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::string_value::stored_type>(
          ColumnFlag::string_value),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::real_value::stored_type>(
          ColumnFlag::real_value),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::value_type::stored_type>(
          ColumnFlag::value_type),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,arg_set_id_storage_layer_,flat_key_storage_layer_,key_storage_layer_,int_value_storage_layer_,string_value_storage_layer_,real_value_storage_layer_,value_type_storage_layer_},
      {{},{},{},{},{},int_value_null_layer_,{},real_value_null_layer_,{}});
  }
  ~ArgTable() override;

  static const char* Name() { return "__intrinsic_args"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "arg_set_id", ColumnType::arg_set_id::SqlValueType(), false,
        true,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "flat_key", ColumnType::flat_key::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "key", ColumnType::key::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "int_value", ColumnType::int_value::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "string_value", ColumnType::string_value::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "real_value", ColumnType::real_value::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "value_type", ColumnType::value_type::SqlValueType(), false,
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
    arg_set_id_.ShrinkToFit();
    flat_key_.ShrinkToFit();
    key_.ShrinkToFit();
    int_value_.ShrinkToFit();
    string_value_.ShrinkToFit();
    real_value_.ShrinkToFit();
    value_type_.ShrinkToFit();
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
    mutable_arg_set_id()->Append(row.arg_set_id);
    mutable_flat_key()->Append(row.flat_key);
    mutable_key()->Append(row.key);
    mutable_int_value()->Append(row.int_value);
    mutable_string_value()->Append(row.string_value);
    mutable_real_value()->Append(row.real_value);
    mutable_value_type()->Append(row.value_type);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ArgTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }
  const TypedColumn<StringPool::Id>& flat_key() const {
    return static_cast<const ColumnType::flat_key&>(columns()[ColumnIndex::flat_key]);
  }
  const TypedColumn<StringPool::Id>& key() const {
    return static_cast<const ColumnType::key&>(columns()[ColumnIndex::key]);
  }
  const TypedColumn<std::optional<int64_t>>& int_value() const {
    return static_cast<const ColumnType::int_value&>(columns()[ColumnIndex::int_value]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& string_value() const {
    return static_cast<const ColumnType::string_value&>(columns()[ColumnIndex::string_value]);
  }
  const TypedColumn<std::optional<double>>& real_value() const {
    return static_cast<const ColumnType::real_value&>(columns()[ColumnIndex::real_value]);
  }
  const TypedColumn<StringPool::Id>& value_type() const {
    return static_cast<const ColumnType::value_type&>(columns()[ColumnIndex::value_type]);
  }

  TypedColumn<uint32_t>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }
  TypedColumn<StringPool::Id>* mutable_flat_key() {
    return static_cast<ColumnType::flat_key*>(
        GetColumn(ColumnIndex::flat_key));
  }
  TypedColumn<StringPool::Id>* mutable_key() {
    return static_cast<ColumnType::key*>(
        GetColumn(ColumnIndex::key));
  }
  TypedColumn<std::optional<int64_t>>* mutable_int_value() {
    return static_cast<ColumnType::int_value*>(
        GetColumn(ColumnIndex::int_value));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_string_value() {
    return static_cast<ColumnType::string_value*>(
        GetColumn(ColumnIndex::string_value));
  }
  TypedColumn<std::optional<double>>* mutable_real_value() {
    return static_cast<ColumnType::real_value*>(
        GetColumn(ColumnIndex::real_value));
  }
  TypedColumn<StringPool::Id>* mutable_value_type() {
    return static_cast<ColumnType::value_type*>(
        GetColumn(ColumnIndex::value_type));
  }

 private:
  
  
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;
  ColumnStorage<ColumnType::flat_key::stored_type> flat_key_;
  ColumnStorage<ColumnType::key::stored_type> key_;
  ColumnStorage<ColumnType::int_value::stored_type> int_value_;
  ColumnStorage<ColumnType::string_value::stored_type> string_value_;
  ColumnStorage<ColumnType::real_value::stored_type> real_value_;
  ColumnStorage<ColumnType::value_type::stored_type> value_type_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;
  RefPtr<column::StorageLayer> flat_key_storage_layer_;
  RefPtr<column::StorageLayer> key_storage_layer_;
  RefPtr<column::StorageLayer> int_value_storage_layer_;
  RefPtr<column::StorageLayer> string_value_storage_layer_;
  RefPtr<column::StorageLayer> real_value_storage_layer_;
  RefPtr<column::StorageLayer> value_type_storage_layer_;

  RefPtr<column::OverlayLayer> int_value_null_layer_;
  RefPtr<column::OverlayLayer> real_value_null_layer_;
};
  

class ClockSnapshotTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t clock_id = 3;
    static constexpr uint32_t clock_name = 4;
    static constexpr uint32_t clock_value = 5;
    static constexpr uint32_t snapshot_id = 6;
    static constexpr uint32_t machine_id = 7;
  };
  struct ColumnType {
    using id = IdColumn<ClockSnapshotTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using clock_id = TypedColumn<int64_t>;
    using clock_name = TypedColumn<std::optional<StringPool::Id>>;
    using clock_value = TypedColumn<int64_t>;
    using snapshot_id = TypedColumn<uint32_t>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        int64_t in_clock_id = {},
        std::optional<StringPool::Id> in_clock_name = {},
        int64_t in_clock_value = {},
        uint32_t in_snapshot_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          clock_id(in_clock_id),
          clock_name(in_clock_name),
          clock_value(in_clock_value),
          snapshot_id(in_snapshot_id),
          machine_id(in_machine_id) {
      type_ = "clock_snapshot";
    }
    int64_t ts;
    int64_t clock_id;
    std::optional<StringPool::Id> clock_name;
    int64_t clock_value;
    uint32_t snapshot_id;
    std::optional<MachineTable::Id> machine_id;

    bool operator==(const ClockSnapshotTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::clock_id::Equals(clock_id, other.clock_id) &&
       ColumnType::clock_name::Equals(clock_name, other.clock_name) &&
       ColumnType::clock_value::Equals(clock_value, other.clock_value) &&
       ColumnType::snapshot_id::Equals(snapshot_id, other.snapshot_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = ColumnType::ts::default_flags();
    static constexpr uint32_t clock_id = ColumnType::clock_id::default_flags();
    static constexpr uint32_t clock_name = ColumnType::clock_name::default_flags();
    static constexpr uint32_t clock_value = ColumnType::clock_value::default_flags();
    static constexpr uint32_t snapshot_id = ColumnType::snapshot_id::default_flags();
    static constexpr uint32_t machine_id = ColumnType::machine_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ClockSnapshotTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ClockSnapshotTable, RowNumber> {
   public:
    ConstRowReference(const ClockSnapshotTable* table, uint32_t row_number)
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
    ColumnType::clock_id::type clock_id() const {
      return table_->clock_id()[row_number_];
    }
    ColumnType::clock_name::type clock_name() const {
      return table_->clock_name()[row_number_];
    }
    ColumnType::clock_value::type clock_value() const {
      return table_->clock_value()[row_number_];
    }
    ColumnType::snapshot_id::type snapshot_id() const {
      return table_->snapshot_id()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ClockSnapshotTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_clock_id(
        ColumnType::clock_id::non_optional_type v) {
      return mutable_table()->mutable_clock_id()->Set(row_number_, v);
    }
    void set_clock_name(
        ColumnType::clock_name::non_optional_type v) {
      return mutable_table()->mutable_clock_name()->Set(row_number_, v);
    }
    void set_clock_value(
        ColumnType::clock_value::non_optional_type v) {
      return mutable_table()->mutable_clock_value()->Set(row_number_, v);
    }
    void set_snapshot_id(
        ColumnType::snapshot_id::non_optional_type v) {
      return mutable_table()->mutable_snapshot_id()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }

   private:
    ClockSnapshotTable* mutable_table() const {
      return const_cast<ClockSnapshotTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ClockSnapshotTable, RowNumber, ConstRowReference> {
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
    ColumnType::clock_id::type clock_id() const {
      const auto& col = table_->clock_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::clock_name::type clock_name() const {
      const auto& col = table_->clock_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::clock_value::type clock_value() const {
      const auto& col = table_->clock_value();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::snapshot_id::type snapshot_id() const {
      const auto& col = table_->snapshot_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ClockSnapshotTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ClockSnapshotTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ClockSnapshotTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ClockSnapshotTable;

     explicit Iterator(ClockSnapshotTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ClockSnapshotTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ClockSnapshotTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "clock_id", &self->clock_id_, ColumnFlag::clock_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "clock_name", &self->clock_name_, ColumnFlag::clock_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "clock_value", &self->clock_value_, ColumnFlag::clock_value,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "snapshot_id", &self->snapshot_id_, ColumnFlag::snapshot_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "machine_id", &self->machine_id_, ColumnFlag::machine_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ClockSnapshotTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        clock_id_(ColumnStorage<ColumnType::clock_id::stored_type>::Create<false>()),
        clock_name_(ColumnStorage<ColumnType::clock_name::stored_type>::Create<false>()),
        clock_value_(ColumnStorage<ColumnType::clock_value::stored_type>::Create<false>()),
        snapshot_id_(ColumnStorage<ColumnType::snapshot_id::stored_type>::Create<false>()),
        machine_id_(ColumnStorage<ColumnType::machine_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          false)),
        clock_id_storage_layer_(
        new column::NumericStorage<ColumnType::clock_id::non_optional_stored_type>(
          &clock_id_.vector(),
          ColumnTypeHelper<ColumnType::clock_id::stored_type>::ToColumnType(),
          false)),
        clock_name_storage_layer_(
          new column::StringStorage(string_pool(), &clock_name_.vector())),
        clock_value_storage_layer_(
        new column::NumericStorage<ColumnType::clock_value::non_optional_stored_type>(
          &clock_value_.vector(),
          ColumnTypeHelper<ColumnType::clock_value::stored_type>::ToColumnType(),
          false)),
        snapshot_id_storage_layer_(
        new column::NumericStorage<ColumnType::snapshot_id::non_optional_stored_type>(
          &snapshot_id_.vector(),
          ColumnTypeHelper<ColumnType::snapshot_id::stored_type>::ToColumnType(),
          false)),
        machine_id_storage_layer_(
          new column::NumericStorage<ColumnType::machine_id::non_optional_stored_type>(
            &machine_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::machine_id::stored_type>::ToColumnType(),
            false))
,
        machine_id_null_layer_(new column::NullOverlay(machine_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::clock_id::stored_type>(
          ColumnFlag::clock_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::clock_name::stored_type>(
          ColumnFlag::clock_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::clock_value::stored_type>(
          ColumnFlag::clock_value),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::snapshot_id::stored_type>(
          ColumnFlag::snapshot_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::machine_id::stored_type>(
          ColumnFlag::machine_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,clock_id_storage_layer_,clock_name_storage_layer_,clock_value_storage_layer_,snapshot_id_storage_layer_,machine_id_storage_layer_},
      {{},{},{},{},{},{},{},machine_id_null_layer_});
  }
  ~ClockSnapshotTable() override;

  static const char* Name() { return "clock_snapshot"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts", ColumnType::ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "clock_id", ColumnType::clock_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "clock_name", ColumnType::clock_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "clock_value", ColumnType::clock_value::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "snapshot_id", ColumnType::snapshot_id::SqlValueType(), false,
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
    ts_.ShrinkToFit();
    clock_id_.ShrinkToFit();
    clock_name_.ShrinkToFit();
    clock_value_.ShrinkToFit();
    snapshot_id_.ShrinkToFit();
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
    mutable_ts()->Append(row.ts);
    mutable_clock_id()->Append(row.clock_id);
    mutable_clock_name()->Append(row.clock_name);
    mutable_clock_value()->Append(row.clock_value);
    mutable_snapshot_id()->Append(row.snapshot_id);
    mutable_machine_id()->Append(row.machine_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ClockSnapshotTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<int64_t>& clock_id() const {
    return static_cast<const ColumnType::clock_id&>(columns()[ColumnIndex::clock_id]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& clock_name() const {
    return static_cast<const ColumnType::clock_name&>(columns()[ColumnIndex::clock_name]);
  }
  const TypedColumn<int64_t>& clock_value() const {
    return static_cast<const ColumnType::clock_value&>(columns()[ColumnIndex::clock_value]);
  }
  const TypedColumn<uint32_t>& snapshot_id() const {
    return static_cast<const ColumnType::snapshot_id&>(columns()[ColumnIndex::snapshot_id]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<int64_t>* mutable_clock_id() {
    return static_cast<ColumnType::clock_id*>(
        GetColumn(ColumnIndex::clock_id));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_clock_name() {
    return static_cast<ColumnType::clock_name*>(
        GetColumn(ColumnIndex::clock_name));
  }
  TypedColumn<int64_t>* mutable_clock_value() {
    return static_cast<ColumnType::clock_value*>(
        GetColumn(ColumnIndex::clock_value));
  }
  TypedColumn<uint32_t>* mutable_snapshot_id() {
    return static_cast<ColumnType::snapshot_id*>(
        GetColumn(ColumnIndex::snapshot_id));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::clock_id::stored_type> clock_id_;
  ColumnStorage<ColumnType::clock_name::stored_type> clock_name_;
  ColumnStorage<ColumnType::clock_value::stored_type> clock_value_;
  ColumnStorage<ColumnType::snapshot_id::stored_type> snapshot_id_;
  ColumnStorage<ColumnType::machine_id::stored_type> machine_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> clock_id_storage_layer_;
  RefPtr<column::StorageLayer> clock_name_storage_layer_;
  RefPtr<column::StorageLayer> clock_value_storage_layer_;
  RefPtr<column::StorageLayer> snapshot_id_storage_layer_;
  RefPtr<column::StorageLayer> machine_id_storage_layer_;

  RefPtr<column::OverlayLayer> machine_id_null_layer_;
};
  

class CpuTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t cpu = 2;
    static constexpr uint32_t cluster_id = 3;
    static constexpr uint32_t processor = 4;
    static constexpr uint32_t machine_id = 5;
    static constexpr uint32_t capacity = 6;
  };
  struct ColumnType {
    using id = IdColumn<CpuTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using cpu = TypedColumn<std::optional<uint32_t>>;
    using cluster_id = TypedColumn<uint32_t>;
    using processor = TypedColumn<StringPool::Id>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using capacity = TypedColumn<std::optional<uint32_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(std::optional<uint32_t> in_cpu = {},
        uint32_t in_cluster_id = {},
        StringPool::Id in_processor = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        std::optional<uint32_t> in_capacity = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          cpu(in_cpu),
          cluster_id(in_cluster_id),
          processor(in_processor),
          machine_id(in_machine_id),
          capacity(in_capacity) {
      type_ = "__intrinsic_cpu";
    }
    std::optional<uint32_t> cpu;
    uint32_t cluster_id;
    StringPool::Id processor;
    std::optional<MachineTable::Id> machine_id;
    std::optional<uint32_t> capacity;

    bool operator==(const CpuTable::Row& other) const {
      return type() == other.type() && ColumnType::cpu::Equals(cpu, other.cpu) &&
       ColumnType::cluster_id::Equals(cluster_id, other.cluster_id) &&
       ColumnType::processor::Equals(processor, other.processor) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::capacity::Equals(capacity, other.capacity);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t cpu = ColumnType::cpu::default_flags();
    static constexpr uint32_t cluster_id = ColumnType::cluster_id::default_flags();
    static constexpr uint32_t processor = ColumnType::processor::default_flags();
    static constexpr uint32_t machine_id = ColumnType::machine_id::default_flags();
    static constexpr uint32_t capacity = ColumnType::capacity::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      CpuTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    CpuTable, RowNumber> {
   public:
    ConstRowReference(const CpuTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::cpu::type cpu() const {
      return table_->cpu()[row_number_];
    }
    ColumnType::cluster_id::type cluster_id() const {
      return table_->cluster_id()[row_number_];
    }
    ColumnType::processor::type processor() const {
      return table_->processor()[row_number_];
    }
    ColumnType::machine_id::type machine_id() const {
      return table_->machine_id()[row_number_];
    }
    ColumnType::capacity::type capacity() const {
      return table_->capacity()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const CpuTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_cpu(
        ColumnType::cpu::non_optional_type v) {
      return mutable_table()->mutable_cpu()->Set(row_number_, v);
    }
    void set_cluster_id(
        ColumnType::cluster_id::non_optional_type v) {
      return mutable_table()->mutable_cluster_id()->Set(row_number_, v);
    }
    void set_processor(
        ColumnType::processor::non_optional_type v) {
      return mutable_table()->mutable_processor()->Set(row_number_, v);
    }
    void set_machine_id(
        ColumnType::machine_id::non_optional_type v) {
      return mutable_table()->mutable_machine_id()->Set(row_number_, v);
    }
    void set_capacity(
        ColumnType::capacity::non_optional_type v) {
      return mutable_table()->mutable_capacity()->Set(row_number_, v);
    }

   private:
    CpuTable* mutable_table() const {
      return const_cast<CpuTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, CpuTable, RowNumber, ConstRowReference> {
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
    ColumnType::cpu::type cpu() const {
      const auto& col = table_->cpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::cluster_id::type cluster_id() const {
      const auto& col = table_->cluster_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::processor::type processor() const {
      const auto& col = table_->processor();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::machine_id::type machine_id() const {
      const auto& col = table_->machine_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::capacity::type capacity() const {
      const auto& col = table_->capacity();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const CpuTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class CpuTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, CpuTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class CpuTable;

     explicit Iterator(CpuTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     CpuTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      CpuTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "cpu", &self->cpu_, ColumnFlag::cpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cluster_id", &self->cluster_id_, ColumnFlag::cluster_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "processor", &self->processor_, ColumnFlag::processor,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "machine_id", &self->machine_id_, ColumnFlag::machine_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "capacity", &self->capacity_, ColumnFlag::capacity,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit CpuTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        cpu_(ColumnStorage<ColumnType::cpu::stored_type>::Create<false>()),
        cluster_id_(ColumnStorage<ColumnType::cluster_id::stored_type>::Create<false>()),
        processor_(ColumnStorage<ColumnType::processor::stored_type>::Create<false>()),
        machine_id_(ColumnStorage<ColumnType::machine_id::stored_type>::Create<false>()),
        capacity_(ColumnStorage<ColumnType::capacity::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        cpu_storage_layer_(
          new column::NumericStorage<ColumnType::cpu::non_optional_stored_type>(
            &cpu_.non_null_vector(),
            ColumnTypeHelper<ColumnType::cpu::stored_type>::ToColumnType(),
            false)),
        cluster_id_storage_layer_(
        new column::NumericStorage<ColumnType::cluster_id::non_optional_stored_type>(
          &cluster_id_.vector(),
          ColumnTypeHelper<ColumnType::cluster_id::stored_type>::ToColumnType(),
          false)),
        processor_storage_layer_(
          new column::StringStorage(string_pool(), &processor_.vector())),
        machine_id_storage_layer_(
          new column::NumericStorage<ColumnType::machine_id::non_optional_stored_type>(
            &machine_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::machine_id::stored_type>::ToColumnType(),
            false)),
        capacity_storage_layer_(
          new column::NumericStorage<ColumnType::capacity::non_optional_stored_type>(
            &capacity_.non_null_vector(),
            ColumnTypeHelper<ColumnType::capacity::stored_type>::ToColumnType(),
            false))
,
        cpu_null_layer_(new column::NullOverlay(cpu_.bv())),
        machine_id_null_layer_(new column::NullOverlay(machine_id_.bv())),
        capacity_null_layer_(new column::NullOverlay(capacity_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cpu::stored_type>(
          ColumnFlag::cpu),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cluster_id::stored_type>(
          ColumnFlag::cluster_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::processor::stored_type>(
          ColumnFlag::processor),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::machine_id::stored_type>(
          ColumnFlag::machine_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::capacity::stored_type>(
          ColumnFlag::capacity),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,cpu_storage_layer_,cluster_id_storage_layer_,processor_storage_layer_,machine_id_storage_layer_,capacity_storage_layer_},
      {{},{},cpu_null_layer_,{},{},machine_id_null_layer_,capacity_null_layer_});
  }
  ~CpuTable() override;

  static const char* Name() { return "__intrinsic_cpu"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cpu", ColumnType::cpu::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cluster_id", ColumnType::cluster_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "processor", ColumnType::processor::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "machine_id", ColumnType::machine_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "capacity", ColumnType::capacity::SqlValueType(), false,
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
    cpu_.ShrinkToFit();
    cluster_id_.ShrinkToFit();
    processor_.ShrinkToFit();
    machine_id_.ShrinkToFit();
    capacity_.ShrinkToFit();
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
    mutable_cpu()->Append(row.cpu);
    mutable_cluster_id()->Append(row.cluster_id);
    mutable_processor()->Append(row.processor);
    mutable_machine_id()->Append(row.machine_id);
    mutable_capacity()->Append(row.capacity);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<CpuTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<std::optional<uint32_t>>& cpu() const {
    return static_cast<const ColumnType::cpu&>(columns()[ColumnIndex::cpu]);
  }
  const TypedColumn<uint32_t>& cluster_id() const {
    return static_cast<const ColumnType::cluster_id&>(columns()[ColumnIndex::cluster_id]);
  }
  const TypedColumn<StringPool::Id>& processor() const {
    return static_cast<const ColumnType::processor&>(columns()[ColumnIndex::processor]);
  }
  const TypedColumn<std::optional<MachineTable::Id>>& machine_id() const {
    return static_cast<const ColumnType::machine_id&>(columns()[ColumnIndex::machine_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& capacity() const {
    return static_cast<const ColumnType::capacity&>(columns()[ColumnIndex::capacity]);
  }

  TypedColumn<std::optional<uint32_t>>* mutable_cpu() {
    return static_cast<ColumnType::cpu*>(
        GetColumn(ColumnIndex::cpu));
  }
  TypedColumn<uint32_t>* mutable_cluster_id() {
    return static_cast<ColumnType::cluster_id*>(
        GetColumn(ColumnIndex::cluster_id));
  }
  TypedColumn<StringPool::Id>* mutable_processor() {
    return static_cast<ColumnType::processor*>(
        GetColumn(ColumnIndex::processor));
  }
  TypedColumn<std::optional<MachineTable::Id>>* mutable_machine_id() {
    return static_cast<ColumnType::machine_id*>(
        GetColumn(ColumnIndex::machine_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_capacity() {
    return static_cast<ColumnType::capacity*>(
        GetColumn(ColumnIndex::capacity));
  }

 private:
  
  
  ColumnStorage<ColumnType::cpu::stored_type> cpu_;
  ColumnStorage<ColumnType::cluster_id::stored_type> cluster_id_;
  ColumnStorage<ColumnType::processor::stored_type> processor_;
  ColumnStorage<ColumnType::machine_id::stored_type> machine_id_;
  ColumnStorage<ColumnType::capacity::stored_type> capacity_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> cpu_storage_layer_;
  RefPtr<column::StorageLayer> cluster_id_storage_layer_;
  RefPtr<column::StorageLayer> processor_storage_layer_;
  RefPtr<column::StorageLayer> machine_id_storage_layer_;
  RefPtr<column::StorageLayer> capacity_storage_layer_;

  RefPtr<column::OverlayLayer> cpu_null_layer_;
  RefPtr<column::OverlayLayer> machine_id_null_layer_;
  RefPtr<column::OverlayLayer> capacity_null_layer_;
};
  

class CpuFreqTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t ucpu = 2;
    static constexpr uint32_t freq = 3;
  };
  struct ColumnType {
    using id = IdColumn<CpuFreqTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ucpu = TypedColumn<CpuTable::Id>;
    using freq = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(CpuTable::Id in_ucpu = {},
        uint32_t in_freq = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ucpu(in_ucpu),
          freq(in_freq) {
      type_ = "__intrinsic_cpu_freq";
    }
    CpuTable::Id ucpu;
    uint32_t freq;

    bool operator==(const CpuFreqTable::Row& other) const {
      return type() == other.type() && ColumnType::ucpu::Equals(ucpu, other.ucpu) &&
       ColumnType::freq::Equals(freq, other.freq);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ucpu = ColumnType::ucpu::default_flags();
    static constexpr uint32_t freq = ColumnType::freq::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      CpuFreqTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    CpuFreqTable, RowNumber> {
   public:
    ConstRowReference(const CpuFreqTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::ucpu::type ucpu() const {
      return table_->ucpu()[row_number_];
    }
    ColumnType::freq::type freq() const {
      return table_->freq()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const CpuFreqTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ucpu(
        ColumnType::ucpu::non_optional_type v) {
      return mutable_table()->mutable_ucpu()->Set(row_number_, v);
    }
    void set_freq(
        ColumnType::freq::non_optional_type v) {
      return mutable_table()->mutable_freq()->Set(row_number_, v);
    }

   private:
    CpuFreqTable* mutable_table() const {
      return const_cast<CpuFreqTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, CpuFreqTable, RowNumber, ConstRowReference> {
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
    ColumnType::ucpu::type ucpu() const {
      const auto& col = table_->ucpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::freq::type freq() const {
      const auto& col = table_->freq();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const CpuFreqTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class CpuFreqTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, CpuFreqTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class CpuFreqTable;

     explicit Iterator(CpuFreqTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     CpuFreqTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      CpuFreqTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ucpu", &self->ucpu_, ColumnFlag::ucpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "freq", &self->freq_, ColumnFlag::freq,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit CpuFreqTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ucpu_(ColumnStorage<ColumnType::ucpu::stored_type>::Create<false>()),
        freq_(ColumnStorage<ColumnType::freq::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ucpu_storage_layer_(
        new column::NumericStorage<ColumnType::ucpu::non_optional_stored_type>(
          &ucpu_.vector(),
          ColumnTypeHelper<ColumnType::ucpu::stored_type>::ToColumnType(),
          false)),
        freq_storage_layer_(
        new column::NumericStorage<ColumnType::freq::non_optional_stored_type>(
          &freq_.vector(),
          ColumnTypeHelper<ColumnType::freq::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ucpu::stored_type>(
          ColumnFlag::ucpu),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::freq::stored_type>(
          ColumnFlag::freq),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ucpu_storage_layer_,freq_storage_layer_},
      {{},{},{},{}});
  }
  ~CpuFreqTable() override;

  static const char* Name() { return "__intrinsic_cpu_freq"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ucpu", ColumnType::ucpu::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "freq", ColumnType::freq::SqlValueType(), false,
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
    ucpu_.ShrinkToFit();
    freq_.ShrinkToFit();
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
    mutable_ucpu()->Append(row.ucpu);
    mutable_freq()->Append(row.freq);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<CpuFreqTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<CpuTable::Id>& ucpu() const {
    return static_cast<const ColumnType::ucpu&>(columns()[ColumnIndex::ucpu]);
  }
  const TypedColumn<uint32_t>& freq() const {
    return static_cast<const ColumnType::freq&>(columns()[ColumnIndex::freq]);
  }

  TypedColumn<CpuTable::Id>* mutable_ucpu() {
    return static_cast<ColumnType::ucpu*>(
        GetColumn(ColumnIndex::ucpu));
  }
  TypedColumn<uint32_t>* mutable_freq() {
    return static_cast<ColumnType::freq*>(
        GetColumn(ColumnIndex::freq));
  }

 private:
  
  
  ColumnStorage<ColumnType::ucpu::stored_type> ucpu_;
  ColumnStorage<ColumnType::freq::stored_type> freq_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ucpu_storage_layer_;
  RefPtr<column::StorageLayer> freq_storage_layer_;

  
};
  

class ExpMissingChromeProcTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t upid = 2;
    static constexpr uint32_t reliable_from = 3;
  };
  struct ColumnType {
    using id = IdColumn<ExpMissingChromeProcTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using upid = TypedColumn<uint32_t>;
    using reliable_from = TypedColumn<std::optional<int64_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_upid = {},
        std::optional<int64_t> in_reliable_from = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          upid(in_upid),
          reliable_from(in_reliable_from) {
      type_ = "experimental_missing_chrome_processes";
    }
    uint32_t upid;
    std::optional<int64_t> reliable_from;

    bool operator==(const ExpMissingChromeProcTable::Row& other) const {
      return type() == other.type() && ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::reliable_from::Equals(reliable_from, other.reliable_from);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
    static constexpr uint32_t reliable_from = ColumnType::reliable_from::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ExpMissingChromeProcTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ExpMissingChromeProcTable, RowNumber> {
   public:
    ConstRowReference(const ExpMissingChromeProcTable* table, uint32_t row_number)
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
    ColumnType::reliable_from::type reliable_from() const {
      return table_->reliable_from()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ExpMissingChromeProcTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_reliable_from(
        ColumnType::reliable_from::non_optional_type v) {
      return mutable_table()->mutable_reliable_from()->Set(row_number_, v);
    }

   private:
    ExpMissingChromeProcTable* mutable_table() const {
      return const_cast<ExpMissingChromeProcTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ExpMissingChromeProcTable, RowNumber, ConstRowReference> {
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
    ColumnType::reliable_from::type reliable_from() const {
      const auto& col = table_->reliable_from();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ExpMissingChromeProcTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ExpMissingChromeProcTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ExpMissingChromeProcTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ExpMissingChromeProcTable;

     explicit Iterator(ExpMissingChromeProcTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ExpMissingChromeProcTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ExpMissingChromeProcTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "reliable_from", &self->reliable_from_, ColumnFlag::reliable_from,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ExpMissingChromeProcTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        reliable_from_(ColumnStorage<ColumnType::reliable_from::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false)),
        reliable_from_storage_layer_(
          new column::NumericStorage<ColumnType::reliable_from::non_optional_stored_type>(
            &reliable_from_.non_null_vector(),
            ColumnTypeHelper<ColumnType::reliable_from::stored_type>::ToColumnType(),
            false))
,
        reliable_from_null_layer_(new column::NullOverlay(reliable_from_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::reliable_from::stored_type>(
          ColumnFlag::reliable_from),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,upid_storage_layer_,reliable_from_storage_layer_},
      {{},{},{},reliable_from_null_layer_});
  }
  ~ExpMissingChromeProcTable() override;

  static const char* Name() { return "experimental_missing_chrome_processes"; }

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
        "reliable_from", ColumnType::reliable_from::SqlValueType(), false,
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
    reliable_from_.ShrinkToFit();
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
    mutable_reliable_from()->Append(row.reliable_from);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ExpMissingChromeProcTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<std::optional<int64_t>>& reliable_from() const {
    return static_cast<const ColumnType::reliable_from&>(columns()[ColumnIndex::reliable_from]);
  }

  TypedColumn<uint32_t>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<std::optional<int64_t>>* mutable_reliable_from() {
    return static_cast<ColumnType::reliable_from*>(
        GetColumn(ColumnIndex::reliable_from));
  }

 private:
  
  
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::reliable_from::stored_type> reliable_from_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> reliable_from_storage_layer_;

  RefPtr<column::OverlayLayer> reliable_from_null_layer_;
};
  

class FiledescriptorTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t ufd = 2;
    static constexpr uint32_t fd = 3;
    static constexpr uint32_t ts = 4;
    static constexpr uint32_t upid = 5;
    static constexpr uint32_t path = 6;
  };
  struct ColumnType {
    using id = IdColumn<FiledescriptorTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ufd = TypedColumn<int64_t>;
    using fd = TypedColumn<int64_t>;
    using ts = TypedColumn<std::optional<int64_t>>;
    using upid = TypedColumn<std::optional<uint32_t>>;
    using path = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ufd = {},
        int64_t in_fd = {},
        std::optional<int64_t> in_ts = {},
        std::optional<uint32_t> in_upid = {},
        std::optional<StringPool::Id> in_path = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ufd(in_ufd),
          fd(in_fd),
          ts(in_ts),
          upid(in_upid),
          path(in_path) {
      type_ = "filedescriptor";
    }
    int64_t ufd;
    int64_t fd;
    std::optional<int64_t> ts;
    std::optional<uint32_t> upid;
    std::optional<StringPool::Id> path;

    bool operator==(const FiledescriptorTable::Row& other) const {
      return type() == other.type() && ColumnType::ufd::Equals(ufd, other.ufd) &&
       ColumnType::fd::Equals(fd, other.fd) &&
       ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::path::Equals(path, other.path);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ufd = ColumnType::ufd::default_flags();
    static constexpr uint32_t fd = ColumnType::fd::default_flags();
    static constexpr uint32_t ts = ColumnType::ts::default_flags();
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
    static constexpr uint32_t path = ColumnType::path::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      FiledescriptorTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    FiledescriptorTable, RowNumber> {
   public:
    ConstRowReference(const FiledescriptorTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::ufd::type ufd() const {
      return table_->ufd()[row_number_];
    }
    ColumnType::fd::type fd() const {
      return table_->fd()[row_number_];
    }
    ColumnType::ts::type ts() const {
      return table_->ts()[row_number_];
    }
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
    ColumnType::path::type path() const {
      return table_->path()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const FiledescriptorTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ufd(
        ColumnType::ufd::non_optional_type v) {
      return mutable_table()->mutable_ufd()->Set(row_number_, v);
    }
    void set_fd(
        ColumnType::fd::non_optional_type v) {
      return mutable_table()->mutable_fd()->Set(row_number_, v);
    }
    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_path(
        ColumnType::path::non_optional_type v) {
      return mutable_table()->mutable_path()->Set(row_number_, v);
    }

   private:
    FiledescriptorTable* mutable_table() const {
      return const_cast<FiledescriptorTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, FiledescriptorTable, RowNumber, ConstRowReference> {
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
    ColumnType::ufd::type ufd() const {
      const auto& col = table_->ufd();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::fd::type fd() const {
      const auto& col = table_->fd();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ts::type ts() const {
      const auto& col = table_->ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::path::type path() const {
      const auto& col = table_->path();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const FiledescriptorTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class FiledescriptorTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, FiledescriptorTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class FiledescriptorTable;

     explicit Iterator(FiledescriptorTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     FiledescriptorTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      FiledescriptorTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ufd", &self->ufd_, ColumnFlag::ufd,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "fd", &self->fd_, ColumnFlag::fd,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "path", &self->path_, ColumnFlag::path,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit FiledescriptorTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ufd_(ColumnStorage<ColumnType::ufd::stored_type>::Create<false>()),
        fd_(ColumnStorage<ColumnType::fd::stored_type>::Create<false>()),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        path_(ColumnStorage<ColumnType::path::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ufd_storage_layer_(
        new column::NumericStorage<ColumnType::ufd::non_optional_stored_type>(
          &ufd_.vector(),
          ColumnTypeHelper<ColumnType::ufd::stored_type>::ToColumnType(),
          false)),
        fd_storage_layer_(
        new column::NumericStorage<ColumnType::fd::non_optional_stored_type>(
          &fd_.vector(),
          ColumnTypeHelper<ColumnType::fd::stored_type>::ToColumnType(),
          false)),
        ts_storage_layer_(
          new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
            &ts_.non_null_vector(),
            ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
            false)),
        upid_storage_layer_(
          new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
            &upid_.non_null_vector(),
            ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
            false)),
        path_storage_layer_(
          new column::StringStorage(string_pool(), &path_.vector()))
,
        ts_null_layer_(new column::NullOverlay(ts_.bv())),
        upid_null_layer_(new column::NullOverlay(upid_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ufd::stored_type>(
          ColumnFlag::ufd),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::fd::stored_type>(
          ColumnFlag::fd),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::path::stored_type>(
          ColumnFlag::path),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ufd_storage_layer_,fd_storage_layer_,ts_storage_layer_,upid_storage_layer_,path_storage_layer_},
      {{},{},{},{},ts_null_layer_,upid_null_layer_,{}});
  }
  ~FiledescriptorTable() override;

  static const char* Name() { return "filedescriptor"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ufd", ColumnType::ufd::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "fd", ColumnType::fd::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts", ColumnType::ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "upid", ColumnType::upid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "path", ColumnType::path::SqlValueType(), false,
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
    ufd_.ShrinkToFit();
    fd_.ShrinkToFit();
    ts_.ShrinkToFit();
    upid_.ShrinkToFit();
    path_.ShrinkToFit();
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
    mutable_ufd()->Append(row.ufd);
    mutable_fd()->Append(row.fd);
    mutable_ts()->Append(row.ts);
    mutable_upid()->Append(row.upid);
    mutable_path()->Append(row.path);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<FiledescriptorTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ufd() const {
    return static_cast<const ColumnType::ufd&>(columns()[ColumnIndex::ufd]);
  }
  const TypedColumn<int64_t>& fd() const {
    return static_cast<const ColumnType::fd&>(columns()[ColumnIndex::fd]);
  }
  const TypedColumn<std::optional<int64_t>>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<std::optional<uint32_t>>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& path() const {
    return static_cast<const ColumnType::path&>(columns()[ColumnIndex::path]);
  }

  TypedColumn<int64_t>* mutable_ufd() {
    return static_cast<ColumnType::ufd*>(
        GetColumn(ColumnIndex::ufd));
  }
  TypedColumn<int64_t>* mutable_fd() {
    return static_cast<ColumnType::fd*>(
        GetColumn(ColumnIndex::fd));
  }
  TypedColumn<std::optional<int64_t>>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_path() {
    return static_cast<ColumnType::path*>(
        GetColumn(ColumnIndex::path));
  }

 private:
  
  
  ColumnStorage<ColumnType::ufd::stored_type> ufd_;
  ColumnStorage<ColumnType::fd::stored_type> fd_;
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::path::stored_type> path_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ufd_storage_layer_;
  RefPtr<column::StorageLayer> fd_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> path_storage_layer_;

  RefPtr<column::OverlayLayer> ts_null_layer_;
  RefPtr<column::OverlayLayer> upid_null_layer_;
};
  

class RawTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t name = 3;
    static constexpr uint32_t utid = 4;
    static constexpr uint32_t arg_set_id = 5;
    static constexpr uint32_t common_flags = 6;
    static constexpr uint32_t ucpu = 7;
  };
  struct ColumnType {
    using id = IdColumn<RawTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using name = TypedColumn<StringPool::Id>;
    using utid = TypedColumn<uint32_t>;
    using arg_set_id = TypedColumn<uint32_t>;
    using common_flags = TypedColumn<uint32_t>;
    using ucpu = TypedColumn<CpuTable::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        StringPool::Id in_name = {},
        uint32_t in_utid = {},
        uint32_t in_arg_set_id = {},
        uint32_t in_common_flags = {},
        CpuTable::Id in_ucpu = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          name(in_name),
          utid(in_utid),
          arg_set_id(in_arg_set_id),
          common_flags(in_common_flags),
          ucpu(in_ucpu) {
      type_ = "__intrinsic_raw";
    }
    int64_t ts;
    StringPool::Id name;
    uint32_t utid;
    uint32_t arg_set_id;
    uint32_t common_flags;
    CpuTable::Id ucpu;

    bool operator==(const RawTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::name::Equals(name, other.name) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id) &&
       ColumnType::common_flags::Equals(common_flags, other.common_flags) &&
       ColumnType::ucpu::Equals(ucpu, other.ucpu);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::ts::default_flags();
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
    static constexpr uint32_t common_flags = ColumnType::common_flags::default_flags();
    static constexpr uint32_t ucpu = ColumnType::ucpu::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      RawTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    RawTable, RowNumber> {
   public:
    ConstRowReference(const RawTable* table, uint32_t row_number)
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
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
    ColumnType::common_flags::type common_flags() const {
      return table_->common_flags()[row_number_];
    }
    ColumnType::ucpu::type ucpu() const {
      return table_->ucpu()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const RawTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }
    void set_common_flags(
        ColumnType::common_flags::non_optional_type v) {
      return mutable_table()->mutable_common_flags()->Set(row_number_, v);
    }
    void set_ucpu(
        ColumnType::ucpu::non_optional_type v) {
      return mutable_table()->mutable_ucpu()->Set(row_number_, v);
    }

   private:
    RawTable* mutable_table() const {
      return const_cast<RawTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, RawTable, RowNumber, ConstRowReference> {
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
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::common_flags::type common_flags() const {
      const auto& col = table_->common_flags();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ucpu::type ucpu() const {
      const auto& col = table_->ucpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const RawTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class RawTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, RawTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class RawTable;

     explicit Iterator(RawTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     RawTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      RawTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "common_flags", &self->common_flags_, ColumnFlag::common_flags,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ucpu", &self->ucpu_, ColumnFlag::ucpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit RawTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>()),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>()),
        common_flags_(ColumnStorage<ColumnType::common_flags::stored_type>::Create<false>()),
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
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false)),
        arg_set_id_storage_layer_(
        new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
          &arg_set_id_.vector(),
          ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
          false)),
        common_flags_storage_layer_(
        new column::NumericStorage<ColumnType::common_flags::non_optional_stored_type>(
          &common_flags_.vector(),
          ColumnTypeHelper<ColumnType::common_flags::stored_type>::ToColumnType(),
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
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::common_flags::stored_type>(
          ColumnFlag::common_flags),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ucpu::stored_type>(
          ColumnFlag::ucpu),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,name_storage_layer_,utid_storage_layer_,arg_set_id_storage_layer_,common_flags_storage_layer_,ucpu_storage_layer_},
      {{},{},{},{},{},{},{},{}});
  }
  ~RawTable() override;

  static const char* Name() { return "__intrinsic_raw"; }

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
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "arg_set_id", ColumnType::arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "common_flags", ColumnType::common_flags::SqlValueType(), false,
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
    name_.ShrinkToFit();
    utid_.ShrinkToFit();
    arg_set_id_.ShrinkToFit();
    common_flags_.ShrinkToFit();
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
    mutable_name()->Append(row.name);
    mutable_utid()->Append(row.utid);
    mutable_arg_set_id()->Append(row.arg_set_id);
    mutable_common_flags()->Append(row.common_flags);
    mutable_ucpu()->Append(row.ucpu);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<RawTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }
  const TypedColumn<uint32_t>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }
  const TypedColumn<uint32_t>& common_flags() const {
    return static_cast<const ColumnType::common_flags&>(columns()[ColumnIndex::common_flags]);
  }
  const TypedColumn<CpuTable::Id>& ucpu() const {
    return static_cast<const ColumnType::ucpu&>(columns()[ColumnIndex::ucpu]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<uint32_t>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }
  TypedColumn<uint32_t>* mutable_common_flags() {
    return static_cast<ColumnType::common_flags*>(
        GetColumn(ColumnIndex::common_flags));
  }
  TypedColumn<CpuTable::Id>* mutable_ucpu() {
    return static_cast<ColumnType::ucpu*>(
        GetColumn(ColumnIndex::ucpu));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::utid::stored_type> utid_;
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;
  ColumnStorage<ColumnType::common_flags::stored_type> common_flags_;
  ColumnStorage<ColumnType::ucpu::stored_type> ucpu_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> utid_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;
  RefPtr<column::StorageLayer> common_flags_storage_layer_;
  RefPtr<column::StorageLayer> ucpu_storage_layer_;

  
};
  

class FtraceEventTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 8;

  using Id = RawTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t ts = 2;
    static constexpr uint32_t name = 3;
    static constexpr uint32_t utid = 4;
    static constexpr uint32_t arg_set_id = 5;
    static constexpr uint32_t common_flags = 6;
    static constexpr uint32_t ucpu = 7;
  };
  struct ColumnType {
    using id = IdColumn<FtraceEventTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using name = TypedColumn<StringPool::Id>;
    using utid = TypedColumn<uint32_t>;
    using arg_set_id = TypedColumn<uint32_t>;
    using common_flags = TypedColumn<uint32_t>;
    using ucpu = TypedColumn<CpuTable::Id>;
  };
  struct Row : public RawTable::Row {
    Row(int64_t in_ts = {},
        StringPool::Id in_name = {},
        uint32_t in_utid = {},
        uint32_t in_arg_set_id = {},
        uint32_t in_common_flags = {},
        CpuTable::Id in_ucpu = {},
        std::nullptr_t = nullptr)
        : RawTable::Row(in_ts, in_name, in_utid, in_arg_set_id, in_common_flags, in_ucpu)
           {
      type_ = "__intrinsic_ftrace_event";
    }
    

    bool operator==(const FtraceEventTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::name::Equals(name, other.name) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id) &&
       ColumnType::common_flags::Equals(common_flags, other.common_flags) &&
       ColumnType::ucpu::Equals(ucpu, other.ucpu);
    }
  };
  struct ColumnFlag {
    
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      FtraceEventTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    FtraceEventTable, RowNumber> {
   public:
    ConstRowReference(const FtraceEventTable* table, uint32_t row_number)
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
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
    ColumnType::common_flags::type common_flags() const {
      return table_->common_flags()[row_number_];
    }
    ColumnType::ucpu::type ucpu() const {
      return table_->ucpu()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const FtraceEventTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }
    void set_common_flags(
        ColumnType::common_flags::non_optional_type v) {
      return mutable_table()->mutable_common_flags()->Set(row_number_, v);
    }
    void set_ucpu(
        ColumnType::ucpu::non_optional_type v) {
      return mutable_table()->mutable_ucpu()->Set(row_number_, v);
    }

   private:
    FtraceEventTable* mutable_table() const {
      return const_cast<FtraceEventTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, FtraceEventTable, RowNumber, ConstRowReference> {
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
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::common_flags::type common_flags() const {
      const auto& col = table_->common_flags();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ucpu::type ucpu() const {
      const auto& col = table_->ucpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const FtraceEventTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class FtraceEventTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, FtraceEventTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class FtraceEventTable;

     explicit Iterator(FtraceEventTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     FtraceEventTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      FtraceEventTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    
    
    return columns;
  }

  PERFETTO_NO_INLINE explicit FtraceEventTable(StringPool* pool, RawTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent)
        
         {
    
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::ts],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::utid],const_parent_->storage_layers()[ColumnIndex::arg_set_id],const_parent_->storage_layers()[ColumnIndex::common_flags],const_parent_->storage_layers()[ColumnIndex::ucpu]},
      {{},{},{},{},{},{},{},{}});
  }
  ~FtraceEventTable() override;

  static const char* Name() { return "__intrinsic_ftrace_event"; }

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
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "arg_set_id", ColumnType::arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "common_flags", ColumnType::common_flags::SqlValueType(), false,
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
      const RawTable& parent
      ) {
    return std::unique_ptr<Table>(new FtraceEventTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count())
        ));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const RawTable& parent,
      std::vector<RawTable::RowNumber> parent_overlay
      ) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new FtraceEventTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped))
        ));
  }

  const IdColumn<FtraceEventTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }
  const TypedColumn<uint32_t>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }
  const TypedColumn<uint32_t>& common_flags() const {
    return static_cast<const ColumnType::common_flags&>(columns()[ColumnIndex::common_flags]);
  }
  const TypedColumn<CpuTable::Id>& ucpu() const {
    return static_cast<const ColumnType::ucpu&>(columns()[ColumnIndex::ucpu]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<uint32_t>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }
  TypedColumn<uint32_t>* mutable_common_flags() {
    return static_cast<ColumnType::common_flags*>(
        GetColumn(ColumnIndex::common_flags));
  }
  TypedColumn<CpuTable::Id>* mutable_ucpu() {
    return static_cast<ColumnType::ucpu*>(
        GetColumn(ColumnIndex::ucpu));
  }

 private:
  FtraceEventTable(StringPool* pool,
            const RawTable& parent,
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
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::ts],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::utid],const_parent_->storage_layers()[ColumnIndex::arg_set_id],const_parent_->storage_layers()[ColumnIndex::common_flags],const_parent_->storage_layers()[ColumnIndex::ucpu]}, {{},{},{},{},{},{},{},{}}, std::move(overlay_layers));
  }
  RawTable* parent_ = nullptr;
  const RawTable* const_parent_ = nullptr;
  

  

  
};
  

class MetadataTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t key_type = 3;
    static constexpr uint32_t int_value = 4;
    static constexpr uint32_t str_value = 5;
  };
  struct ColumnType {
    using id = IdColumn<MetadataTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using key_type = TypedColumn<StringPool::Id>;
    using int_value = TypedColumn<std::optional<int64_t>>;
    using str_value = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_name = {},
        StringPool::Id in_key_type = {},
        std::optional<int64_t> in_int_value = {},
        std::optional<StringPool::Id> in_str_value = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          name(in_name),
          key_type(in_key_type),
          int_value(in_int_value),
          str_value(in_str_value) {
      type_ = "metadata";
    }
    StringPool::Id name;
    StringPool::Id key_type;
    std::optional<int64_t> int_value;
    std::optional<StringPool::Id> str_value;

    bool operator==(const MetadataTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::key_type::Equals(key_type, other.key_type) &&
       ColumnType::int_value::Equals(int_value, other.int_value) &&
       ColumnType::str_value::Equals(str_value, other.str_value);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t key_type = ColumnType::key_type::default_flags();
    static constexpr uint32_t int_value = ColumnType::int_value::default_flags();
    static constexpr uint32_t str_value = ColumnType::str_value::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      MetadataTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    MetadataTable, RowNumber> {
   public:
    ConstRowReference(const MetadataTable* table, uint32_t row_number)
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
    ColumnType::key_type::type key_type() const {
      return table_->key_type()[row_number_];
    }
    ColumnType::int_value::type int_value() const {
      return table_->int_value()[row_number_];
    }
    ColumnType::str_value::type str_value() const {
      return table_->str_value()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const MetadataTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_key_type(
        ColumnType::key_type::non_optional_type v) {
      return mutable_table()->mutable_key_type()->Set(row_number_, v);
    }
    void set_int_value(
        ColumnType::int_value::non_optional_type v) {
      return mutable_table()->mutable_int_value()->Set(row_number_, v);
    }
    void set_str_value(
        ColumnType::str_value::non_optional_type v) {
      return mutable_table()->mutable_str_value()->Set(row_number_, v);
    }

   private:
    MetadataTable* mutable_table() const {
      return const_cast<MetadataTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, MetadataTable, RowNumber, ConstRowReference> {
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
    ColumnType::key_type::type key_type() const {
      const auto& col = table_->key_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::int_value::type int_value() const {
      const auto& col = table_->int_value();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::str_value::type str_value() const {
      const auto& col = table_->str_value();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const MetadataTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class MetadataTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, MetadataTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class MetadataTable;

     explicit Iterator(MetadataTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     MetadataTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      MetadataTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "key_type", &self->key_type_, ColumnFlag::key_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "int_value", &self->int_value_, ColumnFlag::int_value,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "str_value", &self->str_value_, ColumnFlag::str_value,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit MetadataTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        key_type_(ColumnStorage<ColumnType::key_type::stored_type>::Create<false>()),
        int_value_(ColumnStorage<ColumnType::int_value::stored_type>::Create<false>()),
        str_value_(ColumnStorage<ColumnType::str_value::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        key_type_storage_layer_(
          new column::StringStorage(string_pool(), &key_type_.vector())),
        int_value_storage_layer_(
          new column::NumericStorage<ColumnType::int_value::non_optional_stored_type>(
            &int_value_.non_null_vector(),
            ColumnTypeHelper<ColumnType::int_value::stored_type>::ToColumnType(),
            false)),
        str_value_storage_layer_(
          new column::StringStorage(string_pool(), &str_value_.vector()))
,
        int_value_null_layer_(new column::NullOverlay(int_value_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::key_type::stored_type>(
          ColumnFlag::key_type),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::int_value::stored_type>(
          ColumnFlag::int_value),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::str_value::stored_type>(
          ColumnFlag::str_value),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,name_storage_layer_,key_type_storage_layer_,int_value_storage_layer_,str_value_storage_layer_},
      {{},{},{},{},int_value_null_layer_,{}});
  }
  ~MetadataTable() override;

  static const char* Name() { return "metadata"; }

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
        "key_type", ColumnType::key_type::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "int_value", ColumnType::int_value::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "str_value", ColumnType::str_value::SqlValueType(), false,
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
    key_type_.ShrinkToFit();
    int_value_.ShrinkToFit();
    str_value_.ShrinkToFit();
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
    mutable_key_type()->Append(row.key_type);
    mutable_int_value()->Append(row.int_value);
    mutable_str_value()->Append(row.str_value);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<MetadataTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<StringPool::Id>& key_type() const {
    return static_cast<const ColumnType::key_type&>(columns()[ColumnIndex::key_type]);
  }
  const TypedColumn<std::optional<int64_t>>& int_value() const {
    return static_cast<const ColumnType::int_value&>(columns()[ColumnIndex::int_value]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& str_value() const {
    return static_cast<const ColumnType::str_value&>(columns()[ColumnIndex::str_value]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<StringPool::Id>* mutable_key_type() {
    return static_cast<ColumnType::key_type*>(
        GetColumn(ColumnIndex::key_type));
  }
  TypedColumn<std::optional<int64_t>>* mutable_int_value() {
    return static_cast<ColumnType::int_value*>(
        GetColumn(ColumnIndex::int_value));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_str_value() {
    return static_cast<ColumnType::str_value*>(
        GetColumn(ColumnIndex::str_value));
  }

 private:
  
  
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::key_type::stored_type> key_type_;
  ColumnStorage<ColumnType::int_value::stored_type> int_value_;
  ColumnStorage<ColumnType::str_value::stored_type> str_value_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> key_type_storage_layer_;
  RefPtr<column::StorageLayer> int_value_storage_layer_;
  RefPtr<column::StorageLayer> str_value_storage_layer_;

  RefPtr<column::OverlayLayer> int_value_null_layer_;
};
  

class TraceFileTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t parent_id = 2;
    static constexpr uint32_t name = 3;
    static constexpr uint32_t size = 4;
    static constexpr uint32_t trace_type = 5;
  };
  struct ColumnType {
    using id = IdColumn<TraceFileTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<TraceFileTable::Id>>;
    using name = TypedColumn<std::optional<StringPool::Id>>;
    using size = TypedColumn<int64_t>;
    using trace_type = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(std::optional<TraceFileTable::Id> in_parent_id = {},
        std::optional<StringPool::Id> in_name = {},
        int64_t in_size = {},
        StringPool::Id in_trace_type = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          parent_id(in_parent_id),
          name(in_name),
          size(in_size),
          trace_type(in_trace_type) {
      type_ = "__intrinsic_trace_file";
    }
    std::optional<TraceFileTable::Id> parent_id;
    std::optional<StringPool::Id> name;
    int64_t size;
    StringPool::Id trace_type;

    bool operator==(const TraceFileTable::Row& other) const {
      return type() == other.type() && ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::name::Equals(name, other.name) &&
       ColumnType::size::Equals(size, other.size) &&
       ColumnType::trace_type::Equals(trace_type, other.trace_type);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t parent_id = ColumnType::parent_id::default_flags();
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t size = ColumnType::size::default_flags();
    static constexpr uint32_t trace_type = ColumnType::trace_type::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      TraceFileTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    TraceFileTable, RowNumber> {
   public:
    ConstRowReference(const TraceFileTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::size::type size() const {
      return table_->size()[row_number_];
    }
    ColumnType::trace_type::type trace_type() const {
      return table_->trace_type()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const TraceFileTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_size(
        ColumnType::size::non_optional_type v) {
      return mutable_table()->mutable_size()->Set(row_number_, v);
    }
    void set_trace_type(
        ColumnType::trace_type::non_optional_type v) {
      return mutable_table()->mutable_trace_type()->Set(row_number_, v);
    }

   private:
    TraceFileTable* mutable_table() const {
      return const_cast<TraceFileTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, TraceFileTable, RowNumber, ConstRowReference> {
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
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::size::type size() const {
      const auto& col = table_->size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::trace_type::type trace_type() const {
      const auto& col = table_->trace_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const TraceFileTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class TraceFileTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, TraceFileTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class TraceFileTable;

     explicit Iterator(TraceFileTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     TraceFileTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      TraceFileTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "parent_id", &self->parent_id_, ColumnFlag::parent_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "size", &self->size_, ColumnFlag::size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "trace_type", &self->trace_type_, ColumnFlag::trace_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit TraceFileTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        parent_id_(ColumnStorage<ColumnType::parent_id::stored_type>::Create<false>()),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        size_(ColumnStorage<ColumnType::size::stored_type>::Create<false>()),
        trace_type_(ColumnStorage<ColumnType::trace_type::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        parent_id_storage_layer_(
          new column::NumericStorage<ColumnType::parent_id::non_optional_stored_type>(
            &parent_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_id::stored_type>::ToColumnType(),
            false)),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        size_storage_layer_(
        new column::NumericStorage<ColumnType::size::non_optional_stored_type>(
          &size_.vector(),
          ColumnTypeHelper<ColumnType::size::stored_type>::ToColumnType(),
          false)),
        trace_type_storage_layer_(
          new column::StringStorage(string_pool(), &trace_type_.vector()))
,
        parent_id_null_layer_(new column::NullOverlay(parent_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_id::stored_type>(
          ColumnFlag::parent_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::size::stored_type>(
          ColumnFlag::size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::trace_type::stored_type>(
          ColumnFlag::trace_type),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,parent_id_storage_layer_,name_storage_layer_,size_storage_layer_,trace_type_storage_layer_},
      {{},{},parent_id_null_layer_,{},{},{}});
  }
  ~TraceFileTable() override;

  static const char* Name() { return "__intrinsic_trace_file"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "size", ColumnType::size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "trace_type", ColumnType::trace_type::SqlValueType(), false,
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
    parent_id_.ShrinkToFit();
    name_.ShrinkToFit();
    size_.ShrinkToFit();
    trace_type_.ShrinkToFit();
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
    mutable_parent_id()->Append(row.parent_id);
    mutable_name()->Append(row.name);
    mutable_size()->Append(row.size);
    mutable_trace_type()->Append(row.trace_type);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<TraceFileTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<std::optional<TraceFileTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<int64_t>& size() const {
    return static_cast<const ColumnType::size&>(columns()[ColumnIndex::size]);
  }
  const TypedColumn<StringPool::Id>& trace_type() const {
    return static_cast<const ColumnType::trace_type&>(columns()[ColumnIndex::trace_type]);
  }

  TypedColumn<std::optional<TraceFileTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<int64_t>* mutable_size() {
    return static_cast<ColumnType::size*>(
        GetColumn(ColumnIndex::size));
  }
  TypedColumn<StringPool::Id>* mutable_trace_type() {
    return static_cast<ColumnType::trace_type*>(
        GetColumn(ColumnIndex::trace_type));
  }

 private:
  
  
  ColumnStorage<ColumnType::parent_id::stored_type> parent_id_;
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::size::stored_type> size_;
  ColumnStorage<ColumnType::trace_type::stored_type> trace_type_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> parent_id_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> size_storage_layer_;
  RefPtr<column::StorageLayer> trace_type_storage_layer_;

  RefPtr<column::OverlayLayer> parent_id_null_layer_;
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_METADATA_TABLES_PY_H_
