#ifndef SRC_TRACE_PROCESSOR_TABLES_ANDROID_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_ANDROID_TABLES_PY_H_

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

class AndroidLogTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t utid = 3;
    static constexpr uint32_t prio = 4;
    static constexpr uint32_t tag = 5;
    static constexpr uint32_t msg = 6;
  };
  struct ColumnType {
    using id = IdColumn<AndroidLogTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using utid = TypedColumn<uint32_t>;
    using prio = TypedColumn<uint32_t>;
    using tag = TypedColumn<std::optional<StringPool::Id>>;
    using msg = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        uint32_t in_utid = {},
        uint32_t in_prio = {},
        std::optional<StringPool::Id> in_tag = {},
        StringPool::Id in_msg = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          utid(in_utid),
          prio(in_prio),
          tag(in_tag),
          msg(in_msg) {
      type_ = "android_logs";
    }
    int64_t ts;
    uint32_t utid;
    uint32_t prio;
    std::optional<StringPool::Id> tag;
    StringPool::Id msg;

    bool operator==(const AndroidLogTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::prio::Equals(prio, other.prio) &&
       ColumnType::tag::Equals(tag, other.tag) &&
       ColumnType::msg::Equals(msg, other.msg);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = ColumnType::ts::default_flags();
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
    static constexpr uint32_t prio = ColumnType::prio::default_flags();
    static constexpr uint32_t tag = ColumnType::tag::default_flags();
    static constexpr uint32_t msg = ColumnType::msg::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      AndroidLogTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    AndroidLogTable, RowNumber> {
   public:
    ConstRowReference(const AndroidLogTable* table, uint32_t row_number)
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
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
    ColumnType::prio::type prio() const {
      return table_->prio()[row_number_];
    }
    ColumnType::tag::type tag() const {
      return table_->tag()[row_number_];
    }
    ColumnType::msg::type msg() const {
      return table_->msg()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const AndroidLogTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_prio(
        ColumnType::prio::non_optional_type v) {
      return mutable_table()->mutable_prio()->Set(row_number_, v);
    }
    void set_tag(
        ColumnType::tag::non_optional_type v) {
      return mutable_table()->mutable_tag()->Set(row_number_, v);
    }
    void set_msg(
        ColumnType::msg::non_optional_type v) {
      return mutable_table()->mutable_msg()->Set(row_number_, v);
    }

   private:
    AndroidLogTable* mutable_table() const {
      return const_cast<AndroidLogTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, AndroidLogTable, RowNumber, ConstRowReference> {
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
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::prio::type prio() const {
      const auto& col = table_->prio();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::tag::type tag() const {
      const auto& col = table_->tag();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::msg::type msg() const {
      const auto& col = table_->msg();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const AndroidLogTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class AndroidLogTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, AndroidLogTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class AndroidLogTable;

     explicit Iterator(AndroidLogTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     AndroidLogTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      AndroidLogTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "prio", &self->prio_, ColumnFlag::prio,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "tag", &self->tag_, ColumnFlag::tag,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "msg", &self->msg_, ColumnFlag::msg,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit AndroidLogTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>()),
        prio_(ColumnStorage<ColumnType::prio::stored_type>::Create<false>()),
        tag_(ColumnStorage<ColumnType::tag::stored_type>::Create<false>()),
        msg_(ColumnStorage<ColumnType::msg::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          false)),
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false)),
        prio_storage_layer_(
        new column::NumericStorage<ColumnType::prio::non_optional_stored_type>(
          &prio_.vector(),
          ColumnTypeHelper<ColumnType::prio::stored_type>::ToColumnType(),
          false)),
        tag_storage_layer_(
          new column::StringStorage(string_pool(), &tag_.vector())),
        msg_storage_layer_(
          new column::StringStorage(string_pool(), &msg_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::prio::stored_type>(
          ColumnFlag::prio),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::tag::stored_type>(
          ColumnFlag::tag),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::msg::stored_type>(
          ColumnFlag::msg),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,utid_storage_layer_,prio_storage_layer_,tag_storage_layer_,msg_storage_layer_},
      {{},{},{},{},{},{},{}});
  }
  ~AndroidLogTable() override;

  static const char* Name() { return "android_logs"; }

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
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "prio", ColumnType::prio::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "tag", ColumnType::tag::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "msg", ColumnType::msg::SqlValueType(), false,
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
    utid_.ShrinkToFit();
    prio_.ShrinkToFit();
    tag_.ShrinkToFit();
    msg_.ShrinkToFit();
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
    mutable_utid()->Append(row.utid);
    mutable_prio()->Append(row.prio);
    mutable_tag()->Append(row.tag);
    mutable_msg()->Append(row.msg);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<AndroidLogTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }
  const TypedColumn<uint32_t>& prio() const {
    return static_cast<const ColumnType::prio&>(columns()[ColumnIndex::prio]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& tag() const {
    return static_cast<const ColumnType::tag&>(columns()[ColumnIndex::tag]);
  }
  const TypedColumn<StringPool::Id>& msg() const {
    return static_cast<const ColumnType::msg&>(columns()[ColumnIndex::msg]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<uint32_t>* mutable_prio() {
    return static_cast<ColumnType::prio*>(
        GetColumn(ColumnIndex::prio));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_tag() {
    return static_cast<ColumnType::tag*>(
        GetColumn(ColumnIndex::tag));
  }
  TypedColumn<StringPool::Id>* mutable_msg() {
    return static_cast<ColumnType::msg*>(
        GetColumn(ColumnIndex::msg));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::utid::stored_type> utid_;
  ColumnStorage<ColumnType::prio::stored_type> prio_;
  ColumnStorage<ColumnType::tag::stored_type> tag_;
  ColumnStorage<ColumnType::msg::stored_type> msg_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> utid_storage_layer_;
  RefPtr<column::StorageLayer> prio_storage_layer_;
  RefPtr<column::StorageLayer> tag_storage_layer_;
  RefPtr<column::StorageLayer> msg_storage_layer_;

  
};
  

class AndroidDumpstateTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t section = 2;
    static constexpr uint32_t service = 3;
    static constexpr uint32_t line = 4;
  };
  struct ColumnType {
    using id = IdColumn<AndroidDumpstateTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using section = TypedColumn<std::optional<StringPool::Id>>;
    using service = TypedColumn<std::optional<StringPool::Id>>;
    using line = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(std::optional<StringPool::Id> in_section = {},
        std::optional<StringPool::Id> in_service = {},
        StringPool::Id in_line = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          section(in_section),
          service(in_service),
          line(in_line) {
      type_ = "android_dumpstate";
    }
    std::optional<StringPool::Id> section;
    std::optional<StringPool::Id> service;
    StringPool::Id line;

    bool operator==(const AndroidDumpstateTable::Row& other) const {
      return type() == other.type() && ColumnType::section::Equals(section, other.section) &&
       ColumnType::service::Equals(service, other.service) &&
       ColumnType::line::Equals(line, other.line);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t section = ColumnType::section::default_flags();
    static constexpr uint32_t service = ColumnType::service::default_flags();
    static constexpr uint32_t line = ColumnType::line::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      AndroidDumpstateTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    AndroidDumpstateTable, RowNumber> {
   public:
    ConstRowReference(const AndroidDumpstateTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::section::type section() const {
      return table_->section()[row_number_];
    }
    ColumnType::service::type service() const {
      return table_->service()[row_number_];
    }
    ColumnType::line::type line() const {
      return table_->line()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const AndroidDumpstateTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_section(
        ColumnType::section::non_optional_type v) {
      return mutable_table()->mutable_section()->Set(row_number_, v);
    }
    void set_service(
        ColumnType::service::non_optional_type v) {
      return mutable_table()->mutable_service()->Set(row_number_, v);
    }
    void set_line(
        ColumnType::line::non_optional_type v) {
      return mutable_table()->mutable_line()->Set(row_number_, v);
    }

   private:
    AndroidDumpstateTable* mutable_table() const {
      return const_cast<AndroidDumpstateTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, AndroidDumpstateTable, RowNumber, ConstRowReference> {
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
    ColumnType::section::type section() const {
      const auto& col = table_->section();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::service::type service() const {
      const auto& col = table_->service();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::line::type line() const {
      const auto& col = table_->line();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const AndroidDumpstateTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class AndroidDumpstateTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, AndroidDumpstateTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class AndroidDumpstateTable;

     explicit Iterator(AndroidDumpstateTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     AndroidDumpstateTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      AndroidDumpstateTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "section", &self->section_, ColumnFlag::section,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "service", &self->service_, ColumnFlag::service,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "line", &self->line_, ColumnFlag::line,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit AndroidDumpstateTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        section_(ColumnStorage<ColumnType::section::stored_type>::Create<false>()),
        service_(ColumnStorage<ColumnType::service::stored_type>::Create<false>()),
        line_(ColumnStorage<ColumnType::line::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        section_storage_layer_(
          new column::StringStorage(string_pool(), &section_.vector())),
        service_storage_layer_(
          new column::StringStorage(string_pool(), &service_.vector())),
        line_storage_layer_(
          new column::StringStorage(string_pool(), &line_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::section::stored_type>(
          ColumnFlag::section),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::service::stored_type>(
          ColumnFlag::service),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::line::stored_type>(
          ColumnFlag::line),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,section_storage_layer_,service_storage_layer_,line_storage_layer_},
      {{},{},{},{},{}});
  }
  ~AndroidDumpstateTable() override;

  static const char* Name() { return "android_dumpstate"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "section", ColumnType::section::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "service", ColumnType::service::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "line", ColumnType::line::SqlValueType(), false,
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
    section_.ShrinkToFit();
    service_.ShrinkToFit();
    line_.ShrinkToFit();
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
    mutable_section()->Append(row.section);
    mutable_service()->Append(row.service);
    mutable_line()->Append(row.line);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<AndroidDumpstateTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& section() const {
    return static_cast<const ColumnType::section&>(columns()[ColumnIndex::section]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& service() const {
    return static_cast<const ColumnType::service&>(columns()[ColumnIndex::service]);
  }
  const TypedColumn<StringPool::Id>& line() const {
    return static_cast<const ColumnType::line&>(columns()[ColumnIndex::line]);
  }

  TypedColumn<std::optional<StringPool::Id>>* mutable_section() {
    return static_cast<ColumnType::section*>(
        GetColumn(ColumnIndex::section));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_service() {
    return static_cast<ColumnType::service*>(
        GetColumn(ColumnIndex::service));
  }
  TypedColumn<StringPool::Id>* mutable_line() {
    return static_cast<ColumnType::line*>(
        GetColumn(ColumnIndex::line));
  }

 private:
  
  
  ColumnStorage<ColumnType::section::stored_type> section_;
  ColumnStorage<ColumnType::service::stored_type> service_;
  ColumnStorage<ColumnType::line::stored_type> line_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> section_storage_layer_;
  RefPtr<column::StorageLayer> service_storage_layer_;
  RefPtr<column::StorageLayer> line_storage_layer_;

  
};
  

class AndroidGameInterventionListTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 17;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t package_name = 2;
    static constexpr uint32_t uid = 3;
    static constexpr uint32_t current_mode = 4;
    static constexpr uint32_t standard_mode_supported = 5;
    static constexpr uint32_t standard_mode_downscale = 6;
    static constexpr uint32_t standard_mode_use_angle = 7;
    static constexpr uint32_t standard_mode_fps = 8;
    static constexpr uint32_t perf_mode_supported = 9;
    static constexpr uint32_t perf_mode_downscale = 10;
    static constexpr uint32_t perf_mode_use_angle = 11;
    static constexpr uint32_t perf_mode_fps = 12;
    static constexpr uint32_t battery_mode_supported = 13;
    static constexpr uint32_t battery_mode_downscale = 14;
    static constexpr uint32_t battery_mode_use_angle = 15;
    static constexpr uint32_t battery_mode_fps = 16;
  };
  struct ColumnType {
    using id = IdColumn<AndroidGameInterventionListTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using package_name = TypedColumn<StringPool::Id>;
    using uid = TypedColumn<int64_t>;
    using current_mode = TypedColumn<int32_t>;
    using standard_mode_supported = TypedColumn<int32_t>;
    using standard_mode_downscale = TypedColumn<std::optional<double>>;
    using standard_mode_use_angle = TypedColumn<std::optional<int32_t>>;
    using standard_mode_fps = TypedColumn<std::optional<double>>;
    using perf_mode_supported = TypedColumn<int32_t>;
    using perf_mode_downscale = TypedColumn<std::optional<double>>;
    using perf_mode_use_angle = TypedColumn<std::optional<int32_t>>;
    using perf_mode_fps = TypedColumn<std::optional<double>>;
    using battery_mode_supported = TypedColumn<int32_t>;
    using battery_mode_downscale = TypedColumn<std::optional<double>>;
    using battery_mode_use_angle = TypedColumn<std::optional<int32_t>>;
    using battery_mode_fps = TypedColumn<std::optional<double>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_package_name = {},
        int64_t in_uid = {},
        int32_t in_current_mode = {},
        int32_t in_standard_mode_supported = {},
        std::optional<double> in_standard_mode_downscale = {},
        std::optional<int32_t> in_standard_mode_use_angle = {},
        std::optional<double> in_standard_mode_fps = {},
        int32_t in_perf_mode_supported = {},
        std::optional<double> in_perf_mode_downscale = {},
        std::optional<int32_t> in_perf_mode_use_angle = {},
        std::optional<double> in_perf_mode_fps = {},
        int32_t in_battery_mode_supported = {},
        std::optional<double> in_battery_mode_downscale = {},
        std::optional<int32_t> in_battery_mode_use_angle = {},
        std::optional<double> in_battery_mode_fps = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          package_name(in_package_name),
          uid(in_uid),
          current_mode(in_current_mode),
          standard_mode_supported(in_standard_mode_supported),
          standard_mode_downscale(in_standard_mode_downscale),
          standard_mode_use_angle(in_standard_mode_use_angle),
          standard_mode_fps(in_standard_mode_fps),
          perf_mode_supported(in_perf_mode_supported),
          perf_mode_downscale(in_perf_mode_downscale),
          perf_mode_use_angle(in_perf_mode_use_angle),
          perf_mode_fps(in_perf_mode_fps),
          battery_mode_supported(in_battery_mode_supported),
          battery_mode_downscale(in_battery_mode_downscale),
          battery_mode_use_angle(in_battery_mode_use_angle),
          battery_mode_fps(in_battery_mode_fps) {
      type_ = "android_game_intervention_list";
    }
    StringPool::Id package_name;
    int64_t uid;
    int32_t current_mode;
    int32_t standard_mode_supported;
    std::optional<double> standard_mode_downscale;
    std::optional<int32_t> standard_mode_use_angle;
    std::optional<double> standard_mode_fps;
    int32_t perf_mode_supported;
    std::optional<double> perf_mode_downscale;
    std::optional<int32_t> perf_mode_use_angle;
    std::optional<double> perf_mode_fps;
    int32_t battery_mode_supported;
    std::optional<double> battery_mode_downscale;
    std::optional<int32_t> battery_mode_use_angle;
    std::optional<double> battery_mode_fps;

    bool operator==(const AndroidGameInterventionListTable::Row& other) const {
      return type() == other.type() && ColumnType::package_name::Equals(package_name, other.package_name) &&
       ColumnType::uid::Equals(uid, other.uid) &&
       ColumnType::current_mode::Equals(current_mode, other.current_mode) &&
       ColumnType::standard_mode_supported::Equals(standard_mode_supported, other.standard_mode_supported) &&
       ColumnType::standard_mode_downscale::Equals(standard_mode_downscale, other.standard_mode_downscale) &&
       ColumnType::standard_mode_use_angle::Equals(standard_mode_use_angle, other.standard_mode_use_angle) &&
       ColumnType::standard_mode_fps::Equals(standard_mode_fps, other.standard_mode_fps) &&
       ColumnType::perf_mode_supported::Equals(perf_mode_supported, other.perf_mode_supported) &&
       ColumnType::perf_mode_downscale::Equals(perf_mode_downscale, other.perf_mode_downscale) &&
       ColumnType::perf_mode_use_angle::Equals(perf_mode_use_angle, other.perf_mode_use_angle) &&
       ColumnType::perf_mode_fps::Equals(perf_mode_fps, other.perf_mode_fps) &&
       ColumnType::battery_mode_supported::Equals(battery_mode_supported, other.battery_mode_supported) &&
       ColumnType::battery_mode_downscale::Equals(battery_mode_downscale, other.battery_mode_downscale) &&
       ColumnType::battery_mode_use_angle::Equals(battery_mode_use_angle, other.battery_mode_use_angle) &&
       ColumnType::battery_mode_fps::Equals(battery_mode_fps, other.battery_mode_fps);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t package_name = ColumnType::package_name::default_flags();
    static constexpr uint32_t uid = ColumnType::uid::default_flags();
    static constexpr uint32_t current_mode = ColumnType::current_mode::default_flags();
    static constexpr uint32_t standard_mode_supported = ColumnType::standard_mode_supported::default_flags();
    static constexpr uint32_t standard_mode_downscale = ColumnType::standard_mode_downscale::default_flags();
    static constexpr uint32_t standard_mode_use_angle = ColumnType::standard_mode_use_angle::default_flags();
    static constexpr uint32_t standard_mode_fps = ColumnType::standard_mode_fps::default_flags();
    static constexpr uint32_t perf_mode_supported = ColumnType::perf_mode_supported::default_flags();
    static constexpr uint32_t perf_mode_downscale = ColumnType::perf_mode_downscale::default_flags();
    static constexpr uint32_t perf_mode_use_angle = ColumnType::perf_mode_use_angle::default_flags();
    static constexpr uint32_t perf_mode_fps = ColumnType::perf_mode_fps::default_flags();
    static constexpr uint32_t battery_mode_supported = ColumnType::battery_mode_supported::default_flags();
    static constexpr uint32_t battery_mode_downscale = ColumnType::battery_mode_downscale::default_flags();
    static constexpr uint32_t battery_mode_use_angle = ColumnType::battery_mode_use_angle::default_flags();
    static constexpr uint32_t battery_mode_fps = ColumnType::battery_mode_fps::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      AndroidGameInterventionListTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    AndroidGameInterventionListTable, RowNumber> {
   public:
    ConstRowReference(const AndroidGameInterventionListTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::package_name::type package_name() const {
      return table_->package_name()[row_number_];
    }
    ColumnType::uid::type uid() const {
      return table_->uid()[row_number_];
    }
    ColumnType::current_mode::type current_mode() const {
      return table_->current_mode()[row_number_];
    }
    ColumnType::standard_mode_supported::type standard_mode_supported() const {
      return table_->standard_mode_supported()[row_number_];
    }
    ColumnType::standard_mode_downscale::type standard_mode_downscale() const {
      return table_->standard_mode_downscale()[row_number_];
    }
    ColumnType::standard_mode_use_angle::type standard_mode_use_angle() const {
      return table_->standard_mode_use_angle()[row_number_];
    }
    ColumnType::standard_mode_fps::type standard_mode_fps() const {
      return table_->standard_mode_fps()[row_number_];
    }
    ColumnType::perf_mode_supported::type perf_mode_supported() const {
      return table_->perf_mode_supported()[row_number_];
    }
    ColumnType::perf_mode_downscale::type perf_mode_downscale() const {
      return table_->perf_mode_downscale()[row_number_];
    }
    ColumnType::perf_mode_use_angle::type perf_mode_use_angle() const {
      return table_->perf_mode_use_angle()[row_number_];
    }
    ColumnType::perf_mode_fps::type perf_mode_fps() const {
      return table_->perf_mode_fps()[row_number_];
    }
    ColumnType::battery_mode_supported::type battery_mode_supported() const {
      return table_->battery_mode_supported()[row_number_];
    }
    ColumnType::battery_mode_downscale::type battery_mode_downscale() const {
      return table_->battery_mode_downscale()[row_number_];
    }
    ColumnType::battery_mode_use_angle::type battery_mode_use_angle() const {
      return table_->battery_mode_use_angle()[row_number_];
    }
    ColumnType::battery_mode_fps::type battery_mode_fps() const {
      return table_->battery_mode_fps()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const AndroidGameInterventionListTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_package_name(
        ColumnType::package_name::non_optional_type v) {
      return mutable_table()->mutable_package_name()->Set(row_number_, v);
    }
    void set_uid(
        ColumnType::uid::non_optional_type v) {
      return mutable_table()->mutable_uid()->Set(row_number_, v);
    }
    void set_current_mode(
        ColumnType::current_mode::non_optional_type v) {
      return mutable_table()->mutable_current_mode()->Set(row_number_, v);
    }
    void set_standard_mode_supported(
        ColumnType::standard_mode_supported::non_optional_type v) {
      return mutable_table()->mutable_standard_mode_supported()->Set(row_number_, v);
    }
    void set_standard_mode_downscale(
        ColumnType::standard_mode_downscale::non_optional_type v) {
      return mutable_table()->mutable_standard_mode_downscale()->Set(row_number_, v);
    }
    void set_standard_mode_use_angle(
        ColumnType::standard_mode_use_angle::non_optional_type v) {
      return mutable_table()->mutable_standard_mode_use_angle()->Set(row_number_, v);
    }
    void set_standard_mode_fps(
        ColumnType::standard_mode_fps::non_optional_type v) {
      return mutable_table()->mutable_standard_mode_fps()->Set(row_number_, v);
    }
    void set_perf_mode_supported(
        ColumnType::perf_mode_supported::non_optional_type v) {
      return mutable_table()->mutable_perf_mode_supported()->Set(row_number_, v);
    }
    void set_perf_mode_downscale(
        ColumnType::perf_mode_downscale::non_optional_type v) {
      return mutable_table()->mutable_perf_mode_downscale()->Set(row_number_, v);
    }
    void set_perf_mode_use_angle(
        ColumnType::perf_mode_use_angle::non_optional_type v) {
      return mutable_table()->mutable_perf_mode_use_angle()->Set(row_number_, v);
    }
    void set_perf_mode_fps(
        ColumnType::perf_mode_fps::non_optional_type v) {
      return mutable_table()->mutable_perf_mode_fps()->Set(row_number_, v);
    }
    void set_battery_mode_supported(
        ColumnType::battery_mode_supported::non_optional_type v) {
      return mutable_table()->mutable_battery_mode_supported()->Set(row_number_, v);
    }
    void set_battery_mode_downscale(
        ColumnType::battery_mode_downscale::non_optional_type v) {
      return mutable_table()->mutable_battery_mode_downscale()->Set(row_number_, v);
    }
    void set_battery_mode_use_angle(
        ColumnType::battery_mode_use_angle::non_optional_type v) {
      return mutable_table()->mutable_battery_mode_use_angle()->Set(row_number_, v);
    }
    void set_battery_mode_fps(
        ColumnType::battery_mode_fps::non_optional_type v) {
      return mutable_table()->mutable_battery_mode_fps()->Set(row_number_, v);
    }

   private:
    AndroidGameInterventionListTable* mutable_table() const {
      return const_cast<AndroidGameInterventionListTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, AndroidGameInterventionListTable, RowNumber, ConstRowReference> {
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
    ColumnType::package_name::type package_name() const {
      const auto& col = table_->package_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::uid::type uid() const {
      const auto& col = table_->uid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::current_mode::type current_mode() const {
      const auto& col = table_->current_mode();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::standard_mode_supported::type standard_mode_supported() const {
      const auto& col = table_->standard_mode_supported();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::standard_mode_downscale::type standard_mode_downscale() const {
      const auto& col = table_->standard_mode_downscale();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::standard_mode_use_angle::type standard_mode_use_angle() const {
      const auto& col = table_->standard_mode_use_angle();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::standard_mode_fps::type standard_mode_fps() const {
      const auto& col = table_->standard_mode_fps();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::perf_mode_supported::type perf_mode_supported() const {
      const auto& col = table_->perf_mode_supported();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::perf_mode_downscale::type perf_mode_downscale() const {
      const auto& col = table_->perf_mode_downscale();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::perf_mode_use_angle::type perf_mode_use_angle() const {
      const auto& col = table_->perf_mode_use_angle();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::perf_mode_fps::type perf_mode_fps() const {
      const auto& col = table_->perf_mode_fps();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::battery_mode_supported::type battery_mode_supported() const {
      const auto& col = table_->battery_mode_supported();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::battery_mode_downscale::type battery_mode_downscale() const {
      const auto& col = table_->battery_mode_downscale();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::battery_mode_use_angle::type battery_mode_use_angle() const {
      const auto& col = table_->battery_mode_use_angle();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::battery_mode_fps::type battery_mode_fps() const {
      const auto& col = table_->battery_mode_fps();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const AndroidGameInterventionListTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class AndroidGameInterventionListTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, AndroidGameInterventionListTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class AndroidGameInterventionListTable;

     explicit Iterator(AndroidGameInterventionListTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     AndroidGameInterventionListTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      AndroidGameInterventionListTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "package_name", &self->package_name_, ColumnFlag::package_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "uid", &self->uid_, ColumnFlag::uid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "current_mode", &self->current_mode_, ColumnFlag::current_mode,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "standard_mode_supported", &self->standard_mode_supported_, ColumnFlag::standard_mode_supported,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "standard_mode_downscale", &self->standard_mode_downscale_, ColumnFlag::standard_mode_downscale,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "standard_mode_use_angle", &self->standard_mode_use_angle_, ColumnFlag::standard_mode_use_angle,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "standard_mode_fps", &self->standard_mode_fps_, ColumnFlag::standard_mode_fps,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "perf_mode_supported", &self->perf_mode_supported_, ColumnFlag::perf_mode_supported,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "perf_mode_downscale", &self->perf_mode_downscale_, ColumnFlag::perf_mode_downscale,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "perf_mode_use_angle", &self->perf_mode_use_angle_, ColumnFlag::perf_mode_use_angle,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "perf_mode_fps", &self->perf_mode_fps_, ColumnFlag::perf_mode_fps,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "battery_mode_supported", &self->battery_mode_supported_, ColumnFlag::battery_mode_supported,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "battery_mode_downscale", &self->battery_mode_downscale_, ColumnFlag::battery_mode_downscale,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "battery_mode_use_angle", &self->battery_mode_use_angle_, ColumnFlag::battery_mode_use_angle,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "battery_mode_fps", &self->battery_mode_fps_, ColumnFlag::battery_mode_fps,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit AndroidGameInterventionListTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        package_name_(ColumnStorage<ColumnType::package_name::stored_type>::Create<false>()),
        uid_(ColumnStorage<ColumnType::uid::stored_type>::Create<false>()),
        current_mode_(ColumnStorage<ColumnType::current_mode::stored_type>::Create<false>()),
        standard_mode_supported_(ColumnStorage<ColumnType::standard_mode_supported::stored_type>::Create<false>()),
        standard_mode_downscale_(ColumnStorage<ColumnType::standard_mode_downscale::stored_type>::Create<false>()),
        standard_mode_use_angle_(ColumnStorage<ColumnType::standard_mode_use_angle::stored_type>::Create<false>()),
        standard_mode_fps_(ColumnStorage<ColumnType::standard_mode_fps::stored_type>::Create<false>()),
        perf_mode_supported_(ColumnStorage<ColumnType::perf_mode_supported::stored_type>::Create<false>()),
        perf_mode_downscale_(ColumnStorage<ColumnType::perf_mode_downscale::stored_type>::Create<false>()),
        perf_mode_use_angle_(ColumnStorage<ColumnType::perf_mode_use_angle::stored_type>::Create<false>()),
        perf_mode_fps_(ColumnStorage<ColumnType::perf_mode_fps::stored_type>::Create<false>()),
        battery_mode_supported_(ColumnStorage<ColumnType::battery_mode_supported::stored_type>::Create<false>()),
        battery_mode_downscale_(ColumnStorage<ColumnType::battery_mode_downscale::stored_type>::Create<false>()),
        battery_mode_use_angle_(ColumnStorage<ColumnType::battery_mode_use_angle::stored_type>::Create<false>()),
        battery_mode_fps_(ColumnStorage<ColumnType::battery_mode_fps::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        package_name_storage_layer_(
          new column::StringStorage(string_pool(), &package_name_.vector())),
        uid_storage_layer_(
        new column::NumericStorage<ColumnType::uid::non_optional_stored_type>(
          &uid_.vector(),
          ColumnTypeHelper<ColumnType::uid::stored_type>::ToColumnType(),
          false)),
        current_mode_storage_layer_(
        new column::NumericStorage<ColumnType::current_mode::non_optional_stored_type>(
          &current_mode_.vector(),
          ColumnTypeHelper<ColumnType::current_mode::stored_type>::ToColumnType(),
          false)),
        standard_mode_supported_storage_layer_(
        new column::NumericStorage<ColumnType::standard_mode_supported::non_optional_stored_type>(
          &standard_mode_supported_.vector(),
          ColumnTypeHelper<ColumnType::standard_mode_supported::stored_type>::ToColumnType(),
          false)),
        standard_mode_downscale_storage_layer_(
          new column::NumericStorage<ColumnType::standard_mode_downscale::non_optional_stored_type>(
            &standard_mode_downscale_.non_null_vector(),
            ColumnTypeHelper<ColumnType::standard_mode_downscale::stored_type>::ToColumnType(),
            false)),
        standard_mode_use_angle_storage_layer_(
          new column::NumericStorage<ColumnType::standard_mode_use_angle::non_optional_stored_type>(
            &standard_mode_use_angle_.non_null_vector(),
            ColumnTypeHelper<ColumnType::standard_mode_use_angle::stored_type>::ToColumnType(),
            false)),
        standard_mode_fps_storage_layer_(
          new column::NumericStorage<ColumnType::standard_mode_fps::non_optional_stored_type>(
            &standard_mode_fps_.non_null_vector(),
            ColumnTypeHelper<ColumnType::standard_mode_fps::stored_type>::ToColumnType(),
            false)),
        perf_mode_supported_storage_layer_(
        new column::NumericStorage<ColumnType::perf_mode_supported::non_optional_stored_type>(
          &perf_mode_supported_.vector(),
          ColumnTypeHelper<ColumnType::perf_mode_supported::stored_type>::ToColumnType(),
          false)),
        perf_mode_downscale_storage_layer_(
          new column::NumericStorage<ColumnType::perf_mode_downscale::non_optional_stored_type>(
            &perf_mode_downscale_.non_null_vector(),
            ColumnTypeHelper<ColumnType::perf_mode_downscale::stored_type>::ToColumnType(),
            false)),
        perf_mode_use_angle_storage_layer_(
          new column::NumericStorage<ColumnType::perf_mode_use_angle::non_optional_stored_type>(
            &perf_mode_use_angle_.non_null_vector(),
            ColumnTypeHelper<ColumnType::perf_mode_use_angle::stored_type>::ToColumnType(),
            false)),
        perf_mode_fps_storage_layer_(
          new column::NumericStorage<ColumnType::perf_mode_fps::non_optional_stored_type>(
            &perf_mode_fps_.non_null_vector(),
            ColumnTypeHelper<ColumnType::perf_mode_fps::stored_type>::ToColumnType(),
            false)),
        battery_mode_supported_storage_layer_(
        new column::NumericStorage<ColumnType::battery_mode_supported::non_optional_stored_type>(
          &battery_mode_supported_.vector(),
          ColumnTypeHelper<ColumnType::battery_mode_supported::stored_type>::ToColumnType(),
          false)),
        battery_mode_downscale_storage_layer_(
          new column::NumericStorage<ColumnType::battery_mode_downscale::non_optional_stored_type>(
            &battery_mode_downscale_.non_null_vector(),
            ColumnTypeHelper<ColumnType::battery_mode_downscale::stored_type>::ToColumnType(),
            false)),
        battery_mode_use_angle_storage_layer_(
          new column::NumericStorage<ColumnType::battery_mode_use_angle::non_optional_stored_type>(
            &battery_mode_use_angle_.non_null_vector(),
            ColumnTypeHelper<ColumnType::battery_mode_use_angle::stored_type>::ToColumnType(),
            false)),
        battery_mode_fps_storage_layer_(
          new column::NumericStorage<ColumnType::battery_mode_fps::non_optional_stored_type>(
            &battery_mode_fps_.non_null_vector(),
            ColumnTypeHelper<ColumnType::battery_mode_fps::stored_type>::ToColumnType(),
            false))
,
        standard_mode_downscale_null_layer_(new column::NullOverlay(standard_mode_downscale_.bv())),
        standard_mode_use_angle_null_layer_(new column::NullOverlay(standard_mode_use_angle_.bv())),
        standard_mode_fps_null_layer_(new column::NullOverlay(standard_mode_fps_.bv())),
        perf_mode_downscale_null_layer_(new column::NullOverlay(perf_mode_downscale_.bv())),
        perf_mode_use_angle_null_layer_(new column::NullOverlay(perf_mode_use_angle_.bv())),
        perf_mode_fps_null_layer_(new column::NullOverlay(perf_mode_fps_.bv())),
        battery_mode_downscale_null_layer_(new column::NullOverlay(battery_mode_downscale_.bv())),
        battery_mode_use_angle_null_layer_(new column::NullOverlay(battery_mode_use_angle_.bv())),
        battery_mode_fps_null_layer_(new column::NullOverlay(battery_mode_fps_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::package_name::stored_type>(
          ColumnFlag::package_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::uid::stored_type>(
          ColumnFlag::uid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::current_mode::stored_type>(
          ColumnFlag::current_mode),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::standard_mode_supported::stored_type>(
          ColumnFlag::standard_mode_supported),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::standard_mode_downscale::stored_type>(
          ColumnFlag::standard_mode_downscale),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::standard_mode_use_angle::stored_type>(
          ColumnFlag::standard_mode_use_angle),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::standard_mode_fps::stored_type>(
          ColumnFlag::standard_mode_fps),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::perf_mode_supported::stored_type>(
          ColumnFlag::perf_mode_supported),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::perf_mode_downscale::stored_type>(
          ColumnFlag::perf_mode_downscale),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::perf_mode_use_angle::stored_type>(
          ColumnFlag::perf_mode_use_angle),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::perf_mode_fps::stored_type>(
          ColumnFlag::perf_mode_fps),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::battery_mode_supported::stored_type>(
          ColumnFlag::battery_mode_supported),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::battery_mode_downscale::stored_type>(
          ColumnFlag::battery_mode_downscale),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::battery_mode_use_angle::stored_type>(
          ColumnFlag::battery_mode_use_angle),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::battery_mode_fps::stored_type>(
          ColumnFlag::battery_mode_fps),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,package_name_storage_layer_,uid_storage_layer_,current_mode_storage_layer_,standard_mode_supported_storage_layer_,standard_mode_downscale_storage_layer_,standard_mode_use_angle_storage_layer_,standard_mode_fps_storage_layer_,perf_mode_supported_storage_layer_,perf_mode_downscale_storage_layer_,perf_mode_use_angle_storage_layer_,perf_mode_fps_storage_layer_,battery_mode_supported_storage_layer_,battery_mode_downscale_storage_layer_,battery_mode_use_angle_storage_layer_,battery_mode_fps_storage_layer_},
      {{},{},{},{},{},{},standard_mode_downscale_null_layer_,standard_mode_use_angle_null_layer_,standard_mode_fps_null_layer_,{},perf_mode_downscale_null_layer_,perf_mode_use_angle_null_layer_,perf_mode_fps_null_layer_,{},battery_mode_downscale_null_layer_,battery_mode_use_angle_null_layer_,battery_mode_fps_null_layer_});
  }
  ~AndroidGameInterventionListTable() override;

  static const char* Name() { return "android_game_intervention_list"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "package_name", ColumnType::package_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "uid", ColumnType::uid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "current_mode", ColumnType::current_mode::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "standard_mode_supported", ColumnType::standard_mode_supported::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "standard_mode_downscale", ColumnType::standard_mode_downscale::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "standard_mode_use_angle", ColumnType::standard_mode_use_angle::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "standard_mode_fps", ColumnType::standard_mode_fps::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "perf_mode_supported", ColumnType::perf_mode_supported::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "perf_mode_downscale", ColumnType::perf_mode_downscale::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "perf_mode_use_angle", ColumnType::perf_mode_use_angle::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "perf_mode_fps", ColumnType::perf_mode_fps::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "battery_mode_supported", ColumnType::battery_mode_supported::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "battery_mode_downscale", ColumnType::battery_mode_downscale::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "battery_mode_use_angle", ColumnType::battery_mode_use_angle::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "battery_mode_fps", ColumnType::battery_mode_fps::SqlValueType(), false,
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
    package_name_.ShrinkToFit();
    uid_.ShrinkToFit();
    current_mode_.ShrinkToFit();
    standard_mode_supported_.ShrinkToFit();
    standard_mode_downscale_.ShrinkToFit();
    standard_mode_use_angle_.ShrinkToFit();
    standard_mode_fps_.ShrinkToFit();
    perf_mode_supported_.ShrinkToFit();
    perf_mode_downscale_.ShrinkToFit();
    perf_mode_use_angle_.ShrinkToFit();
    perf_mode_fps_.ShrinkToFit();
    battery_mode_supported_.ShrinkToFit();
    battery_mode_downscale_.ShrinkToFit();
    battery_mode_use_angle_.ShrinkToFit();
    battery_mode_fps_.ShrinkToFit();
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
    mutable_package_name()->Append(row.package_name);
    mutable_uid()->Append(row.uid);
    mutable_current_mode()->Append(row.current_mode);
    mutable_standard_mode_supported()->Append(row.standard_mode_supported);
    mutable_standard_mode_downscale()->Append(row.standard_mode_downscale);
    mutable_standard_mode_use_angle()->Append(row.standard_mode_use_angle);
    mutable_standard_mode_fps()->Append(row.standard_mode_fps);
    mutable_perf_mode_supported()->Append(row.perf_mode_supported);
    mutable_perf_mode_downscale()->Append(row.perf_mode_downscale);
    mutable_perf_mode_use_angle()->Append(row.perf_mode_use_angle);
    mutable_perf_mode_fps()->Append(row.perf_mode_fps);
    mutable_battery_mode_supported()->Append(row.battery_mode_supported);
    mutable_battery_mode_downscale()->Append(row.battery_mode_downscale);
    mutable_battery_mode_use_angle()->Append(row.battery_mode_use_angle);
    mutable_battery_mode_fps()->Append(row.battery_mode_fps);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<AndroidGameInterventionListTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& package_name() const {
    return static_cast<const ColumnType::package_name&>(columns()[ColumnIndex::package_name]);
  }
  const TypedColumn<int64_t>& uid() const {
    return static_cast<const ColumnType::uid&>(columns()[ColumnIndex::uid]);
  }
  const TypedColumn<int32_t>& current_mode() const {
    return static_cast<const ColumnType::current_mode&>(columns()[ColumnIndex::current_mode]);
  }
  const TypedColumn<int32_t>& standard_mode_supported() const {
    return static_cast<const ColumnType::standard_mode_supported&>(columns()[ColumnIndex::standard_mode_supported]);
  }
  const TypedColumn<std::optional<double>>& standard_mode_downscale() const {
    return static_cast<const ColumnType::standard_mode_downscale&>(columns()[ColumnIndex::standard_mode_downscale]);
  }
  const TypedColumn<std::optional<int32_t>>& standard_mode_use_angle() const {
    return static_cast<const ColumnType::standard_mode_use_angle&>(columns()[ColumnIndex::standard_mode_use_angle]);
  }
  const TypedColumn<std::optional<double>>& standard_mode_fps() const {
    return static_cast<const ColumnType::standard_mode_fps&>(columns()[ColumnIndex::standard_mode_fps]);
  }
  const TypedColumn<int32_t>& perf_mode_supported() const {
    return static_cast<const ColumnType::perf_mode_supported&>(columns()[ColumnIndex::perf_mode_supported]);
  }
  const TypedColumn<std::optional<double>>& perf_mode_downscale() const {
    return static_cast<const ColumnType::perf_mode_downscale&>(columns()[ColumnIndex::perf_mode_downscale]);
  }
  const TypedColumn<std::optional<int32_t>>& perf_mode_use_angle() const {
    return static_cast<const ColumnType::perf_mode_use_angle&>(columns()[ColumnIndex::perf_mode_use_angle]);
  }
  const TypedColumn<std::optional<double>>& perf_mode_fps() const {
    return static_cast<const ColumnType::perf_mode_fps&>(columns()[ColumnIndex::perf_mode_fps]);
  }
  const TypedColumn<int32_t>& battery_mode_supported() const {
    return static_cast<const ColumnType::battery_mode_supported&>(columns()[ColumnIndex::battery_mode_supported]);
  }
  const TypedColumn<std::optional<double>>& battery_mode_downscale() const {
    return static_cast<const ColumnType::battery_mode_downscale&>(columns()[ColumnIndex::battery_mode_downscale]);
  }
  const TypedColumn<std::optional<int32_t>>& battery_mode_use_angle() const {
    return static_cast<const ColumnType::battery_mode_use_angle&>(columns()[ColumnIndex::battery_mode_use_angle]);
  }
  const TypedColumn<std::optional<double>>& battery_mode_fps() const {
    return static_cast<const ColumnType::battery_mode_fps&>(columns()[ColumnIndex::battery_mode_fps]);
  }

  TypedColumn<StringPool::Id>* mutable_package_name() {
    return static_cast<ColumnType::package_name*>(
        GetColumn(ColumnIndex::package_name));
  }
  TypedColumn<int64_t>* mutable_uid() {
    return static_cast<ColumnType::uid*>(
        GetColumn(ColumnIndex::uid));
  }
  TypedColumn<int32_t>* mutable_current_mode() {
    return static_cast<ColumnType::current_mode*>(
        GetColumn(ColumnIndex::current_mode));
  }
  TypedColumn<int32_t>* mutable_standard_mode_supported() {
    return static_cast<ColumnType::standard_mode_supported*>(
        GetColumn(ColumnIndex::standard_mode_supported));
  }
  TypedColumn<std::optional<double>>* mutable_standard_mode_downscale() {
    return static_cast<ColumnType::standard_mode_downscale*>(
        GetColumn(ColumnIndex::standard_mode_downscale));
  }
  TypedColumn<std::optional<int32_t>>* mutable_standard_mode_use_angle() {
    return static_cast<ColumnType::standard_mode_use_angle*>(
        GetColumn(ColumnIndex::standard_mode_use_angle));
  }
  TypedColumn<std::optional<double>>* mutable_standard_mode_fps() {
    return static_cast<ColumnType::standard_mode_fps*>(
        GetColumn(ColumnIndex::standard_mode_fps));
  }
  TypedColumn<int32_t>* mutable_perf_mode_supported() {
    return static_cast<ColumnType::perf_mode_supported*>(
        GetColumn(ColumnIndex::perf_mode_supported));
  }
  TypedColumn<std::optional<double>>* mutable_perf_mode_downscale() {
    return static_cast<ColumnType::perf_mode_downscale*>(
        GetColumn(ColumnIndex::perf_mode_downscale));
  }
  TypedColumn<std::optional<int32_t>>* mutable_perf_mode_use_angle() {
    return static_cast<ColumnType::perf_mode_use_angle*>(
        GetColumn(ColumnIndex::perf_mode_use_angle));
  }
  TypedColumn<std::optional<double>>* mutable_perf_mode_fps() {
    return static_cast<ColumnType::perf_mode_fps*>(
        GetColumn(ColumnIndex::perf_mode_fps));
  }
  TypedColumn<int32_t>* mutable_battery_mode_supported() {
    return static_cast<ColumnType::battery_mode_supported*>(
        GetColumn(ColumnIndex::battery_mode_supported));
  }
  TypedColumn<std::optional<double>>* mutable_battery_mode_downscale() {
    return static_cast<ColumnType::battery_mode_downscale*>(
        GetColumn(ColumnIndex::battery_mode_downscale));
  }
  TypedColumn<std::optional<int32_t>>* mutable_battery_mode_use_angle() {
    return static_cast<ColumnType::battery_mode_use_angle*>(
        GetColumn(ColumnIndex::battery_mode_use_angle));
  }
  TypedColumn<std::optional<double>>* mutable_battery_mode_fps() {
    return static_cast<ColumnType::battery_mode_fps*>(
        GetColumn(ColumnIndex::battery_mode_fps));
  }

 private:
  
  
  ColumnStorage<ColumnType::package_name::stored_type> package_name_;
  ColumnStorage<ColumnType::uid::stored_type> uid_;
  ColumnStorage<ColumnType::current_mode::stored_type> current_mode_;
  ColumnStorage<ColumnType::standard_mode_supported::stored_type> standard_mode_supported_;
  ColumnStorage<ColumnType::standard_mode_downscale::stored_type> standard_mode_downscale_;
  ColumnStorage<ColumnType::standard_mode_use_angle::stored_type> standard_mode_use_angle_;
  ColumnStorage<ColumnType::standard_mode_fps::stored_type> standard_mode_fps_;
  ColumnStorage<ColumnType::perf_mode_supported::stored_type> perf_mode_supported_;
  ColumnStorage<ColumnType::perf_mode_downscale::stored_type> perf_mode_downscale_;
  ColumnStorage<ColumnType::perf_mode_use_angle::stored_type> perf_mode_use_angle_;
  ColumnStorage<ColumnType::perf_mode_fps::stored_type> perf_mode_fps_;
  ColumnStorage<ColumnType::battery_mode_supported::stored_type> battery_mode_supported_;
  ColumnStorage<ColumnType::battery_mode_downscale::stored_type> battery_mode_downscale_;
  ColumnStorage<ColumnType::battery_mode_use_angle::stored_type> battery_mode_use_angle_;
  ColumnStorage<ColumnType::battery_mode_fps::stored_type> battery_mode_fps_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> package_name_storage_layer_;
  RefPtr<column::StorageLayer> uid_storage_layer_;
  RefPtr<column::StorageLayer> current_mode_storage_layer_;
  RefPtr<column::StorageLayer> standard_mode_supported_storage_layer_;
  RefPtr<column::StorageLayer> standard_mode_downscale_storage_layer_;
  RefPtr<column::StorageLayer> standard_mode_use_angle_storage_layer_;
  RefPtr<column::StorageLayer> standard_mode_fps_storage_layer_;
  RefPtr<column::StorageLayer> perf_mode_supported_storage_layer_;
  RefPtr<column::StorageLayer> perf_mode_downscale_storage_layer_;
  RefPtr<column::StorageLayer> perf_mode_use_angle_storage_layer_;
  RefPtr<column::StorageLayer> perf_mode_fps_storage_layer_;
  RefPtr<column::StorageLayer> battery_mode_supported_storage_layer_;
  RefPtr<column::StorageLayer> battery_mode_downscale_storage_layer_;
  RefPtr<column::StorageLayer> battery_mode_use_angle_storage_layer_;
  RefPtr<column::StorageLayer> battery_mode_fps_storage_layer_;

  RefPtr<column::OverlayLayer> standard_mode_downscale_null_layer_;
  RefPtr<column::OverlayLayer> standard_mode_use_angle_null_layer_;
  RefPtr<column::OverlayLayer> standard_mode_fps_null_layer_;
  RefPtr<column::OverlayLayer> perf_mode_downscale_null_layer_;
  RefPtr<column::OverlayLayer> perf_mode_use_angle_null_layer_;
  RefPtr<column::OverlayLayer> perf_mode_fps_null_layer_;
  RefPtr<column::OverlayLayer> battery_mode_downscale_null_layer_;
  RefPtr<column::OverlayLayer> battery_mode_use_angle_null_layer_;
  RefPtr<column::OverlayLayer> battery_mode_fps_null_layer_;
};
  

class AndroidKeyEventsTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t event_id = 2;
    static constexpr uint32_t ts = 3;
    static constexpr uint32_t arg_set_id = 4;
  };
  struct ColumnType {
    using id = IdColumn<AndroidKeyEventsTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using event_id = TypedColumn<uint32_t>;
    using ts = TypedColumn<int64_t>;
    using arg_set_id = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_event_id = {},
        int64_t in_ts = {},
        uint32_t in_arg_set_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          event_id(in_event_id),
          ts(in_ts),
          arg_set_id(in_arg_set_id) {
      type_ = "__intrinsic_android_key_events";
    }
    uint32_t event_id;
    int64_t ts;
    uint32_t arg_set_id;

    bool operator==(const AndroidKeyEventsTable::Row& other) const {
      return type() == other.type() && ColumnType::event_id::Equals(event_id, other.event_id) &&
       ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t event_id = ColumnType::event_id::default_flags();
    static constexpr uint32_t ts = ColumnType::ts::default_flags();
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      AndroidKeyEventsTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    AndroidKeyEventsTable, RowNumber> {
   public:
    ConstRowReference(const AndroidKeyEventsTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::event_id::type event_id() const {
      return table_->event_id()[row_number_];
    }
    ColumnType::ts::type ts() const {
      return table_->ts()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const AndroidKeyEventsTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_event_id(
        ColumnType::event_id::non_optional_type v) {
      return mutable_table()->mutable_event_id()->Set(row_number_, v);
    }
    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }

   private:
    AndroidKeyEventsTable* mutable_table() const {
      return const_cast<AndroidKeyEventsTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, AndroidKeyEventsTable, RowNumber, ConstRowReference> {
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
    ColumnType::event_id::type event_id() const {
      const auto& col = table_->event_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ts::type ts() const {
      const auto& col = table_->ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const AndroidKeyEventsTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class AndroidKeyEventsTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, AndroidKeyEventsTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class AndroidKeyEventsTable;

     explicit Iterator(AndroidKeyEventsTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     AndroidKeyEventsTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      AndroidKeyEventsTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "event_id", &self->event_id_, ColumnFlag::event_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit AndroidKeyEventsTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        event_id_(ColumnStorage<ColumnType::event_id::stored_type>::Create<false>()),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        event_id_storage_layer_(
        new column::NumericStorage<ColumnType::event_id::non_optional_stored_type>(
          &event_id_.vector(),
          ColumnTypeHelper<ColumnType::event_id::stored_type>::ToColumnType(),
          false)),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          false)),
        arg_set_id_storage_layer_(
        new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
          &arg_set_id_.vector(),
          ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::event_id::stored_type>(
          ColumnFlag::event_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,event_id_storage_layer_,ts_storage_layer_,arg_set_id_storage_layer_},
      {{},{},{},{},{}});
  }
  ~AndroidKeyEventsTable() override;

  static const char* Name() { return "__intrinsic_android_key_events"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "event_id", ColumnType::event_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts", ColumnType::ts::SqlValueType(), false,
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
    event_id_.ShrinkToFit();
    ts_.ShrinkToFit();
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
    mutable_event_id()->Append(row.event_id);
    mutable_ts()->Append(row.ts);
    mutable_arg_set_id()->Append(row.arg_set_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<AndroidKeyEventsTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& event_id() const {
    return static_cast<const ColumnType::event_id&>(columns()[ColumnIndex::event_id]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<uint32_t>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }

  TypedColumn<uint32_t>* mutable_event_id() {
    return static_cast<ColumnType::event_id*>(
        GetColumn(ColumnIndex::event_id));
  }
  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<uint32_t>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::event_id::stored_type> event_id_;
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> event_id_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;

  
};
  

class AndroidMotionEventsTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t event_id = 2;
    static constexpr uint32_t ts = 3;
    static constexpr uint32_t arg_set_id = 4;
  };
  struct ColumnType {
    using id = IdColumn<AndroidMotionEventsTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using event_id = TypedColumn<uint32_t>;
    using ts = TypedColumn<int64_t>;
    using arg_set_id = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_event_id = {},
        int64_t in_ts = {},
        uint32_t in_arg_set_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          event_id(in_event_id),
          ts(in_ts),
          arg_set_id(in_arg_set_id) {
      type_ = "__intrinsic_android_motion_events";
    }
    uint32_t event_id;
    int64_t ts;
    uint32_t arg_set_id;

    bool operator==(const AndroidMotionEventsTable::Row& other) const {
      return type() == other.type() && ColumnType::event_id::Equals(event_id, other.event_id) &&
       ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t event_id = ColumnType::event_id::default_flags();
    static constexpr uint32_t ts = ColumnType::ts::default_flags();
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      AndroidMotionEventsTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    AndroidMotionEventsTable, RowNumber> {
   public:
    ConstRowReference(const AndroidMotionEventsTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::event_id::type event_id() const {
      return table_->event_id()[row_number_];
    }
    ColumnType::ts::type ts() const {
      return table_->ts()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const AndroidMotionEventsTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_event_id(
        ColumnType::event_id::non_optional_type v) {
      return mutable_table()->mutable_event_id()->Set(row_number_, v);
    }
    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }

   private:
    AndroidMotionEventsTable* mutable_table() const {
      return const_cast<AndroidMotionEventsTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, AndroidMotionEventsTable, RowNumber, ConstRowReference> {
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
    ColumnType::event_id::type event_id() const {
      const auto& col = table_->event_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ts::type ts() const {
      const auto& col = table_->ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const AndroidMotionEventsTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class AndroidMotionEventsTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, AndroidMotionEventsTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class AndroidMotionEventsTable;

     explicit Iterator(AndroidMotionEventsTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     AndroidMotionEventsTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      AndroidMotionEventsTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "event_id", &self->event_id_, ColumnFlag::event_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit AndroidMotionEventsTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        event_id_(ColumnStorage<ColumnType::event_id::stored_type>::Create<false>()),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        event_id_storage_layer_(
        new column::NumericStorage<ColumnType::event_id::non_optional_stored_type>(
          &event_id_.vector(),
          ColumnTypeHelper<ColumnType::event_id::stored_type>::ToColumnType(),
          false)),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          false)),
        arg_set_id_storage_layer_(
        new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
          &arg_set_id_.vector(),
          ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::event_id::stored_type>(
          ColumnFlag::event_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,event_id_storage_layer_,ts_storage_layer_,arg_set_id_storage_layer_},
      {{},{},{},{},{}});
  }
  ~AndroidMotionEventsTable() override;

  static const char* Name() { return "__intrinsic_android_motion_events"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "event_id", ColumnType::event_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts", ColumnType::ts::SqlValueType(), false,
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
    event_id_.ShrinkToFit();
    ts_.ShrinkToFit();
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
    mutable_event_id()->Append(row.event_id);
    mutable_ts()->Append(row.ts);
    mutable_arg_set_id()->Append(row.arg_set_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<AndroidMotionEventsTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& event_id() const {
    return static_cast<const ColumnType::event_id&>(columns()[ColumnIndex::event_id]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<uint32_t>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }

  TypedColumn<uint32_t>* mutable_event_id() {
    return static_cast<ColumnType::event_id*>(
        GetColumn(ColumnIndex::event_id));
  }
  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<uint32_t>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::event_id::stored_type> event_id_;
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> event_id_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;

  
};
  

class AndroidInputEventDispatchTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t event_id = 2;
    static constexpr uint32_t arg_set_id = 3;
    static constexpr uint32_t vsync_id = 4;
    static constexpr uint32_t window_id = 5;
  };
  struct ColumnType {
    using id = IdColumn<AndroidInputEventDispatchTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using event_id = TypedColumn<uint32_t>;
    using arg_set_id = TypedColumn<uint32_t>;
    using vsync_id = TypedColumn<int64_t>;
    using window_id = TypedColumn<int32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_event_id = {},
        uint32_t in_arg_set_id = {},
        int64_t in_vsync_id = {},
        int32_t in_window_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          event_id(in_event_id),
          arg_set_id(in_arg_set_id),
          vsync_id(in_vsync_id),
          window_id(in_window_id) {
      type_ = "__intrinsic_android_input_event_dispatch";
    }
    uint32_t event_id;
    uint32_t arg_set_id;
    int64_t vsync_id;
    int32_t window_id;

    bool operator==(const AndroidInputEventDispatchTable::Row& other) const {
      return type() == other.type() && ColumnType::event_id::Equals(event_id, other.event_id) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id) &&
       ColumnType::vsync_id::Equals(vsync_id, other.vsync_id) &&
       ColumnType::window_id::Equals(window_id, other.window_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t event_id = ColumnType::event_id::default_flags();
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
    static constexpr uint32_t vsync_id = ColumnType::vsync_id::default_flags();
    static constexpr uint32_t window_id = ColumnType::window_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      AndroidInputEventDispatchTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    AndroidInputEventDispatchTable, RowNumber> {
   public:
    ConstRowReference(const AndroidInputEventDispatchTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::event_id::type event_id() const {
      return table_->event_id()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
    ColumnType::vsync_id::type vsync_id() const {
      return table_->vsync_id()[row_number_];
    }
    ColumnType::window_id::type window_id() const {
      return table_->window_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const AndroidInputEventDispatchTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_event_id(
        ColumnType::event_id::non_optional_type v) {
      return mutable_table()->mutable_event_id()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }
    void set_vsync_id(
        ColumnType::vsync_id::non_optional_type v) {
      return mutable_table()->mutable_vsync_id()->Set(row_number_, v);
    }
    void set_window_id(
        ColumnType::window_id::non_optional_type v) {
      return mutable_table()->mutable_window_id()->Set(row_number_, v);
    }

   private:
    AndroidInputEventDispatchTable* mutable_table() const {
      return const_cast<AndroidInputEventDispatchTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, AndroidInputEventDispatchTable, RowNumber, ConstRowReference> {
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
    ColumnType::event_id::type event_id() const {
      const auto& col = table_->event_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::vsync_id::type vsync_id() const {
      const auto& col = table_->vsync_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::window_id::type window_id() const {
      const auto& col = table_->window_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const AndroidInputEventDispatchTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class AndroidInputEventDispatchTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, AndroidInputEventDispatchTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class AndroidInputEventDispatchTable;

     explicit Iterator(AndroidInputEventDispatchTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     AndroidInputEventDispatchTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      AndroidInputEventDispatchTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "event_id", &self->event_id_, ColumnFlag::event_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "vsync_id", &self->vsync_id_, ColumnFlag::vsync_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "window_id", &self->window_id_, ColumnFlag::window_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit AndroidInputEventDispatchTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        event_id_(ColumnStorage<ColumnType::event_id::stored_type>::Create<false>()),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>()),
        vsync_id_(ColumnStorage<ColumnType::vsync_id::stored_type>::Create<false>()),
        window_id_(ColumnStorage<ColumnType::window_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        event_id_storage_layer_(
        new column::NumericStorage<ColumnType::event_id::non_optional_stored_type>(
          &event_id_.vector(),
          ColumnTypeHelper<ColumnType::event_id::stored_type>::ToColumnType(),
          false)),
        arg_set_id_storage_layer_(
        new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
          &arg_set_id_.vector(),
          ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
          false)),
        vsync_id_storage_layer_(
        new column::NumericStorage<ColumnType::vsync_id::non_optional_stored_type>(
          &vsync_id_.vector(),
          ColumnTypeHelper<ColumnType::vsync_id::stored_type>::ToColumnType(),
          false)),
        window_id_storage_layer_(
        new column::NumericStorage<ColumnType::window_id::non_optional_stored_type>(
          &window_id_.vector(),
          ColumnTypeHelper<ColumnType::window_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::event_id::stored_type>(
          ColumnFlag::event_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::vsync_id::stored_type>(
          ColumnFlag::vsync_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::window_id::stored_type>(
          ColumnFlag::window_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,event_id_storage_layer_,arg_set_id_storage_layer_,vsync_id_storage_layer_,window_id_storage_layer_},
      {{},{},{},{},{},{}});
  }
  ~AndroidInputEventDispatchTable() override;

  static const char* Name() { return "__intrinsic_android_input_event_dispatch"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "event_id", ColumnType::event_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "arg_set_id", ColumnType::arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "vsync_id", ColumnType::vsync_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "window_id", ColumnType::window_id::SqlValueType(), false,
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
    event_id_.ShrinkToFit();
    arg_set_id_.ShrinkToFit();
    vsync_id_.ShrinkToFit();
    window_id_.ShrinkToFit();
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
    mutable_event_id()->Append(row.event_id);
    mutable_arg_set_id()->Append(row.arg_set_id);
    mutable_vsync_id()->Append(row.vsync_id);
    mutable_window_id()->Append(row.window_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<AndroidInputEventDispatchTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& event_id() const {
    return static_cast<const ColumnType::event_id&>(columns()[ColumnIndex::event_id]);
  }
  const TypedColumn<uint32_t>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }
  const TypedColumn<int64_t>& vsync_id() const {
    return static_cast<const ColumnType::vsync_id&>(columns()[ColumnIndex::vsync_id]);
  }
  const TypedColumn<int32_t>& window_id() const {
    return static_cast<const ColumnType::window_id&>(columns()[ColumnIndex::window_id]);
  }

  TypedColumn<uint32_t>* mutable_event_id() {
    return static_cast<ColumnType::event_id*>(
        GetColumn(ColumnIndex::event_id));
  }
  TypedColumn<uint32_t>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }
  TypedColumn<int64_t>* mutable_vsync_id() {
    return static_cast<ColumnType::vsync_id*>(
        GetColumn(ColumnIndex::vsync_id));
  }
  TypedColumn<int32_t>* mutable_window_id() {
    return static_cast<ColumnType::window_id*>(
        GetColumn(ColumnIndex::window_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::event_id::stored_type> event_id_;
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;
  ColumnStorage<ColumnType::vsync_id::stored_type> vsync_id_;
  ColumnStorage<ColumnType::window_id::stored_type> window_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> event_id_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;
  RefPtr<column::StorageLayer> vsync_id_storage_layer_;
  RefPtr<column::StorageLayer> window_id_storage_layer_;

  
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_ANDROID_TABLES_PY_H_
