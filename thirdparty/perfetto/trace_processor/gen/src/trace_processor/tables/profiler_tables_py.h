#ifndef SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_PY_H_

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

class StackProfileMappingTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t build_id = 2;
    static constexpr uint32_t exact_offset = 3;
    static constexpr uint32_t start_offset = 4;
    static constexpr uint32_t start = 5;
    static constexpr uint32_t end = 6;
    static constexpr uint32_t load_bias = 7;
    static constexpr uint32_t name = 8;
  };
  struct ColumnType {
    using id = IdColumn<StackProfileMappingTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using build_id = TypedColumn<StringPool::Id>;
    using exact_offset = TypedColumn<int64_t>;
    using start_offset = TypedColumn<int64_t>;
    using start = TypedColumn<int64_t>;
    using end = TypedColumn<int64_t>;
    using load_bias = TypedColumn<int64_t>;
    using name = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_build_id = {},
        int64_t in_exact_offset = {},
        int64_t in_start_offset = {},
        int64_t in_start = {},
        int64_t in_end = {},
        int64_t in_load_bias = {},
        StringPool::Id in_name = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          build_id(in_build_id),
          exact_offset(in_exact_offset),
          start_offset(in_start_offset),
          start(in_start),
          end(in_end),
          load_bias(in_load_bias),
          name(in_name) {
      type_ = "stack_profile_mapping";
    }
    StringPool::Id build_id;
    int64_t exact_offset;
    int64_t start_offset;
    int64_t start;
    int64_t end;
    int64_t load_bias;
    StringPool::Id name;

    bool operator==(const StackProfileMappingTable::Row& other) const {
      return type() == other.type() && ColumnType::build_id::Equals(build_id, other.build_id) &&
       ColumnType::exact_offset::Equals(exact_offset, other.exact_offset) &&
       ColumnType::start_offset::Equals(start_offset, other.start_offset) &&
       ColumnType::start::Equals(start, other.start) &&
       ColumnType::end::Equals(end, other.end) &&
       ColumnType::load_bias::Equals(load_bias, other.load_bias) &&
       ColumnType::name::Equals(name, other.name);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t build_id = ColumnType::build_id::default_flags();
    static constexpr uint32_t exact_offset = ColumnType::exact_offset::default_flags();
    static constexpr uint32_t start_offset = ColumnType::start_offset::default_flags();
    static constexpr uint32_t start = ColumnType::start::default_flags();
    static constexpr uint32_t end = ColumnType::end::default_flags();
    static constexpr uint32_t load_bias = ColumnType::load_bias::default_flags();
    static constexpr uint32_t name = ColumnType::name::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      StackProfileMappingTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    StackProfileMappingTable, RowNumber> {
   public:
    ConstRowReference(const StackProfileMappingTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::build_id::type build_id() const {
      return table_->build_id()[row_number_];
    }
    ColumnType::exact_offset::type exact_offset() const {
      return table_->exact_offset()[row_number_];
    }
    ColumnType::start_offset::type start_offset() const {
      return table_->start_offset()[row_number_];
    }
    ColumnType::start::type start() const {
      return table_->start()[row_number_];
    }
    ColumnType::end::type end() const {
      return table_->end()[row_number_];
    }
    ColumnType::load_bias::type load_bias() const {
      return table_->load_bias()[row_number_];
    }
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const StackProfileMappingTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_build_id(
        ColumnType::build_id::non_optional_type v) {
      return mutable_table()->mutable_build_id()->Set(row_number_, v);
    }
    void set_exact_offset(
        ColumnType::exact_offset::non_optional_type v) {
      return mutable_table()->mutable_exact_offset()->Set(row_number_, v);
    }
    void set_start_offset(
        ColumnType::start_offset::non_optional_type v) {
      return mutable_table()->mutable_start_offset()->Set(row_number_, v);
    }
    void set_start(
        ColumnType::start::non_optional_type v) {
      return mutable_table()->mutable_start()->Set(row_number_, v);
    }
    void set_end(
        ColumnType::end::non_optional_type v) {
      return mutable_table()->mutable_end()->Set(row_number_, v);
    }
    void set_load_bias(
        ColumnType::load_bias::non_optional_type v) {
      return mutable_table()->mutable_load_bias()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }

   private:
    StackProfileMappingTable* mutable_table() const {
      return const_cast<StackProfileMappingTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, StackProfileMappingTable, RowNumber, ConstRowReference> {
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
    ColumnType::build_id::type build_id() const {
      const auto& col = table_->build_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::exact_offset::type exact_offset() const {
      const auto& col = table_->exact_offset();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::start_offset::type start_offset() const {
      const auto& col = table_->start_offset();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::start::type start() const {
      const auto& col = table_->start();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::end::type end() const {
      const auto& col = table_->end();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::load_bias::type load_bias() const {
      const auto& col = table_->load_bias();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const StackProfileMappingTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class StackProfileMappingTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, StackProfileMappingTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class StackProfileMappingTable;

     explicit Iterator(StackProfileMappingTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     StackProfileMappingTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      StackProfileMappingTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "build_id", &self->build_id_, ColumnFlag::build_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "exact_offset", &self->exact_offset_, ColumnFlag::exact_offset,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "start_offset", &self->start_offset_, ColumnFlag::start_offset,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "start", &self->start_, ColumnFlag::start,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "end", &self->end_, ColumnFlag::end,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "load_bias", &self->load_bias_, ColumnFlag::load_bias,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit StackProfileMappingTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        build_id_(ColumnStorage<ColumnType::build_id::stored_type>::Create<false>()),
        exact_offset_(ColumnStorage<ColumnType::exact_offset::stored_type>::Create<false>()),
        start_offset_(ColumnStorage<ColumnType::start_offset::stored_type>::Create<false>()),
        start_(ColumnStorage<ColumnType::start::stored_type>::Create<false>()),
        end_(ColumnStorage<ColumnType::end::stored_type>::Create<false>()),
        load_bias_(ColumnStorage<ColumnType::load_bias::stored_type>::Create<false>()),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        build_id_storage_layer_(
          new column::StringStorage(string_pool(), &build_id_.vector())),
        exact_offset_storage_layer_(
        new column::NumericStorage<ColumnType::exact_offset::non_optional_stored_type>(
          &exact_offset_.vector(),
          ColumnTypeHelper<ColumnType::exact_offset::stored_type>::ToColumnType(),
          false)),
        start_offset_storage_layer_(
        new column::NumericStorage<ColumnType::start_offset::non_optional_stored_type>(
          &start_offset_.vector(),
          ColumnTypeHelper<ColumnType::start_offset::stored_type>::ToColumnType(),
          false)),
        start_storage_layer_(
        new column::NumericStorage<ColumnType::start::non_optional_stored_type>(
          &start_.vector(),
          ColumnTypeHelper<ColumnType::start::stored_type>::ToColumnType(),
          false)),
        end_storage_layer_(
        new column::NumericStorage<ColumnType::end::non_optional_stored_type>(
          &end_.vector(),
          ColumnTypeHelper<ColumnType::end::stored_type>::ToColumnType(),
          false)),
        load_bias_storage_layer_(
        new column::NumericStorage<ColumnType::load_bias::non_optional_stored_type>(
          &load_bias_.vector(),
          ColumnTypeHelper<ColumnType::load_bias::stored_type>::ToColumnType(),
          false)),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::build_id::stored_type>(
          ColumnFlag::build_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::exact_offset::stored_type>(
          ColumnFlag::exact_offset),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::start_offset::stored_type>(
          ColumnFlag::start_offset),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::start::stored_type>(
          ColumnFlag::start),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::end::stored_type>(
          ColumnFlag::end),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::load_bias::stored_type>(
          ColumnFlag::load_bias),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,build_id_storage_layer_,exact_offset_storage_layer_,start_offset_storage_layer_,start_storage_layer_,end_storage_layer_,load_bias_storage_layer_,name_storage_layer_},
      {{},{},{},{},{},{},{},{},{}});
  }
  ~StackProfileMappingTable() override;

  static const char* Name() { return "stack_profile_mapping"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "build_id", ColumnType::build_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "exact_offset", ColumnType::exact_offset::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "start_offset", ColumnType::start_offset::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "start", ColumnType::start::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "end", ColumnType::end::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "load_bias", ColumnType::load_bias::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "name", ColumnType::name::SqlValueType(), false,
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
    build_id_.ShrinkToFit();
    exact_offset_.ShrinkToFit();
    start_offset_.ShrinkToFit();
    start_.ShrinkToFit();
    end_.ShrinkToFit();
    load_bias_.ShrinkToFit();
    name_.ShrinkToFit();
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
    mutable_build_id()->Append(row.build_id);
    mutable_exact_offset()->Append(row.exact_offset);
    mutable_start_offset()->Append(row.start_offset);
    mutable_start()->Append(row.start);
    mutable_end()->Append(row.end);
    mutable_load_bias()->Append(row.load_bias);
    mutable_name()->Append(row.name);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<StackProfileMappingTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& build_id() const {
    return static_cast<const ColumnType::build_id&>(columns()[ColumnIndex::build_id]);
  }
  const TypedColumn<int64_t>& exact_offset() const {
    return static_cast<const ColumnType::exact_offset&>(columns()[ColumnIndex::exact_offset]);
  }
  const TypedColumn<int64_t>& start_offset() const {
    return static_cast<const ColumnType::start_offset&>(columns()[ColumnIndex::start_offset]);
  }
  const TypedColumn<int64_t>& start() const {
    return static_cast<const ColumnType::start&>(columns()[ColumnIndex::start]);
  }
  const TypedColumn<int64_t>& end() const {
    return static_cast<const ColumnType::end&>(columns()[ColumnIndex::end]);
  }
  const TypedColumn<int64_t>& load_bias() const {
    return static_cast<const ColumnType::load_bias&>(columns()[ColumnIndex::load_bias]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }

  TypedColumn<StringPool::Id>* mutable_build_id() {
    return static_cast<ColumnType::build_id*>(
        GetColumn(ColumnIndex::build_id));
  }
  TypedColumn<int64_t>* mutable_exact_offset() {
    return static_cast<ColumnType::exact_offset*>(
        GetColumn(ColumnIndex::exact_offset));
  }
  TypedColumn<int64_t>* mutable_start_offset() {
    return static_cast<ColumnType::start_offset*>(
        GetColumn(ColumnIndex::start_offset));
  }
  TypedColumn<int64_t>* mutable_start() {
    return static_cast<ColumnType::start*>(
        GetColumn(ColumnIndex::start));
  }
  TypedColumn<int64_t>* mutable_end() {
    return static_cast<ColumnType::end*>(
        GetColumn(ColumnIndex::end));
  }
  TypedColumn<int64_t>* mutable_load_bias() {
    return static_cast<ColumnType::load_bias*>(
        GetColumn(ColumnIndex::load_bias));
  }
  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }

 private:
  
  
  ColumnStorage<ColumnType::build_id::stored_type> build_id_;
  ColumnStorage<ColumnType::exact_offset::stored_type> exact_offset_;
  ColumnStorage<ColumnType::start_offset::stored_type> start_offset_;
  ColumnStorage<ColumnType::start::stored_type> start_;
  ColumnStorage<ColumnType::end::stored_type> end_;
  ColumnStorage<ColumnType::load_bias::stored_type> load_bias_;
  ColumnStorage<ColumnType::name::stored_type> name_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> build_id_storage_layer_;
  RefPtr<column::StorageLayer> exact_offset_storage_layer_;
  RefPtr<column::StorageLayer> start_offset_storage_layer_;
  RefPtr<column::StorageLayer> start_storage_layer_;
  RefPtr<column::StorageLayer> end_storage_layer_;
  RefPtr<column::StorageLayer> load_bias_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;

  
};
  

class StackProfileFrameTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t name = 2;
    static constexpr uint32_t mapping = 3;
    static constexpr uint32_t rel_pc = 4;
    static constexpr uint32_t symbol_set_id = 5;
    static constexpr uint32_t deobfuscated_name = 6;
  };
  struct ColumnType {
    using id = IdColumn<StackProfileFrameTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using mapping = TypedColumn<StackProfileMappingTable::Id>;
    using rel_pc = TypedColumn<int64_t>;
    using symbol_set_id = TypedColumn<std::optional<uint32_t>>;
    using deobfuscated_name = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_name = {},
        StackProfileMappingTable::Id in_mapping = {},
        int64_t in_rel_pc = {},
        std::optional<uint32_t> in_symbol_set_id = {},
        std::optional<StringPool::Id> in_deobfuscated_name = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          name(in_name),
          mapping(in_mapping),
          rel_pc(in_rel_pc),
          symbol_set_id(in_symbol_set_id),
          deobfuscated_name(in_deobfuscated_name) {
      type_ = "stack_profile_frame";
    }
    StringPool::Id name;
    StackProfileMappingTable::Id mapping;
    int64_t rel_pc;
    std::optional<uint32_t> symbol_set_id;
    std::optional<StringPool::Id> deobfuscated_name;

    bool operator==(const StackProfileFrameTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::mapping::Equals(mapping, other.mapping) &&
       ColumnType::rel_pc::Equals(rel_pc, other.rel_pc) &&
       ColumnType::symbol_set_id::Equals(symbol_set_id, other.symbol_set_id) &&
       ColumnType::deobfuscated_name::Equals(deobfuscated_name, other.deobfuscated_name);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t mapping = ColumnType::mapping::default_flags();
    static constexpr uint32_t rel_pc = ColumnType::rel_pc::default_flags();
    static constexpr uint32_t symbol_set_id = static_cast<uint32_t>(ColumnLegacy::Flag::kDense) | ColumnType::symbol_set_id::default_flags();
    static constexpr uint32_t deobfuscated_name = ColumnType::deobfuscated_name::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      StackProfileFrameTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    StackProfileFrameTable, RowNumber> {
   public:
    ConstRowReference(const StackProfileFrameTable* table, uint32_t row_number)
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
    ColumnType::mapping::type mapping() const {
      return table_->mapping()[row_number_];
    }
    ColumnType::rel_pc::type rel_pc() const {
      return table_->rel_pc()[row_number_];
    }
    ColumnType::symbol_set_id::type symbol_set_id() const {
      return table_->symbol_set_id()[row_number_];
    }
    ColumnType::deobfuscated_name::type deobfuscated_name() const {
      return table_->deobfuscated_name()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const StackProfileFrameTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_mapping(
        ColumnType::mapping::non_optional_type v) {
      return mutable_table()->mutable_mapping()->Set(row_number_, v);
    }
    void set_rel_pc(
        ColumnType::rel_pc::non_optional_type v) {
      return mutable_table()->mutable_rel_pc()->Set(row_number_, v);
    }
    void set_symbol_set_id(
        ColumnType::symbol_set_id::non_optional_type v) {
      return mutable_table()->mutable_symbol_set_id()->Set(row_number_, v);
    }
    void set_deobfuscated_name(
        ColumnType::deobfuscated_name::non_optional_type v) {
      return mutable_table()->mutable_deobfuscated_name()->Set(row_number_, v);
    }

   private:
    StackProfileFrameTable* mutable_table() const {
      return const_cast<StackProfileFrameTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, StackProfileFrameTable, RowNumber, ConstRowReference> {
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
    ColumnType::mapping::type mapping() const {
      const auto& col = table_->mapping();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::rel_pc::type rel_pc() const {
      const auto& col = table_->rel_pc();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::symbol_set_id::type symbol_set_id() const {
      const auto& col = table_->symbol_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::deobfuscated_name::type deobfuscated_name() const {
      const auto& col = table_->deobfuscated_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const StackProfileFrameTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class StackProfileFrameTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, StackProfileFrameTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class StackProfileFrameTable;

     explicit Iterator(StackProfileFrameTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     StackProfileFrameTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      StackProfileFrameTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "mapping", &self->mapping_, ColumnFlag::mapping,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "rel_pc", &self->rel_pc_, ColumnFlag::rel_pc,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "symbol_set_id", &self->symbol_set_id_, ColumnFlag::symbol_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "deobfuscated_name", &self->deobfuscated_name_, ColumnFlag::deobfuscated_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit StackProfileFrameTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        mapping_(ColumnStorage<ColumnType::mapping::stored_type>::Create<false>()),
        rel_pc_(ColumnStorage<ColumnType::rel_pc::stored_type>::Create<false>()),
        symbol_set_id_(ColumnStorage<ColumnType::symbol_set_id::stored_type>::Create<true>()),
        deobfuscated_name_(ColumnStorage<ColumnType::deobfuscated_name::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        mapping_storage_layer_(
        new column::NumericStorage<ColumnType::mapping::non_optional_stored_type>(
          &mapping_.vector(),
          ColumnTypeHelper<ColumnType::mapping::stored_type>::ToColumnType(),
          false)),
        rel_pc_storage_layer_(
        new column::NumericStorage<ColumnType::rel_pc::non_optional_stored_type>(
          &rel_pc_.vector(),
          ColumnTypeHelper<ColumnType::rel_pc::stored_type>::ToColumnType(),
          false)),
        symbol_set_id_storage_layer_(
          new column::NumericStorage<ColumnType::symbol_set_id::non_optional_stored_type>(
            &symbol_set_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::symbol_set_id::stored_type>::ToColumnType(),
            false)),
        deobfuscated_name_storage_layer_(
          new column::StringStorage(string_pool(), &deobfuscated_name_.vector()))
,
        symbol_set_id_null_layer_(new column::DenseNullOverlay(symbol_set_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::mapping::stored_type>(
          ColumnFlag::mapping),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::rel_pc::stored_type>(
          ColumnFlag::rel_pc),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::symbol_set_id::stored_type>(
          ColumnFlag::symbol_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::deobfuscated_name::stored_type>(
          ColumnFlag::deobfuscated_name),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,name_storage_layer_,mapping_storage_layer_,rel_pc_storage_layer_,symbol_set_id_storage_layer_,deobfuscated_name_storage_layer_},
      {{},{},{},{},{},symbol_set_id_null_layer_,{}});
  }
  ~StackProfileFrameTable() override;

  static const char* Name() { return "stack_profile_frame"; }

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
        "mapping", ColumnType::mapping::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "rel_pc", ColumnType::rel_pc::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "symbol_set_id", ColumnType::symbol_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "deobfuscated_name", ColumnType::deobfuscated_name::SqlValueType(), false,
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
    mapping_.ShrinkToFit();
    rel_pc_.ShrinkToFit();
    symbol_set_id_.ShrinkToFit();
    deobfuscated_name_.ShrinkToFit();
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
    mutable_mapping()->Append(row.mapping);
    mutable_rel_pc()->Append(row.rel_pc);
    mutable_symbol_set_id()->Append(row.symbol_set_id);
    mutable_deobfuscated_name()->Append(row.deobfuscated_name);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<StackProfileFrameTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<StackProfileMappingTable::Id>& mapping() const {
    return static_cast<const ColumnType::mapping&>(columns()[ColumnIndex::mapping]);
  }
  const TypedColumn<int64_t>& rel_pc() const {
    return static_cast<const ColumnType::rel_pc&>(columns()[ColumnIndex::rel_pc]);
  }
  const TypedColumn<std::optional<uint32_t>>& symbol_set_id() const {
    return static_cast<const ColumnType::symbol_set_id&>(columns()[ColumnIndex::symbol_set_id]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& deobfuscated_name() const {
    return static_cast<const ColumnType::deobfuscated_name&>(columns()[ColumnIndex::deobfuscated_name]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<StackProfileMappingTable::Id>* mutable_mapping() {
    return static_cast<ColumnType::mapping*>(
        GetColumn(ColumnIndex::mapping));
  }
  TypedColumn<int64_t>* mutable_rel_pc() {
    return static_cast<ColumnType::rel_pc*>(
        GetColumn(ColumnIndex::rel_pc));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_symbol_set_id() {
    return static_cast<ColumnType::symbol_set_id*>(
        GetColumn(ColumnIndex::symbol_set_id));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_deobfuscated_name() {
    return static_cast<ColumnType::deobfuscated_name*>(
        GetColumn(ColumnIndex::deobfuscated_name));
  }

 private:
  
  
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::mapping::stored_type> mapping_;
  ColumnStorage<ColumnType::rel_pc::stored_type> rel_pc_;
  ColumnStorage<ColumnType::symbol_set_id::stored_type> symbol_set_id_;
  ColumnStorage<ColumnType::deobfuscated_name::stored_type> deobfuscated_name_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> mapping_storage_layer_;
  RefPtr<column::StorageLayer> rel_pc_storage_layer_;
  RefPtr<column::StorageLayer> symbol_set_id_storage_layer_;
  RefPtr<column::StorageLayer> deobfuscated_name_storage_layer_;

  RefPtr<column::OverlayLayer> symbol_set_id_null_layer_;
};
  

class StackProfileCallsiteTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t depth = 2;
    static constexpr uint32_t parent_id = 3;
    static constexpr uint32_t frame_id = 4;
  };
  struct ColumnType {
    using id = IdColumn<StackProfileCallsiteTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using depth = TypedColumn<uint32_t>;
    using parent_id = TypedColumn<std::optional<StackProfileCallsiteTable::Id>>;
    using frame_id = TypedColumn<StackProfileFrameTable::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_depth = {},
        std::optional<StackProfileCallsiteTable::Id> in_parent_id = {},
        StackProfileFrameTable::Id in_frame_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          depth(in_depth),
          parent_id(in_parent_id),
          frame_id(in_frame_id) {
      type_ = "stack_profile_callsite";
    }
    uint32_t depth;
    std::optional<StackProfileCallsiteTable::Id> parent_id;
    StackProfileFrameTable::Id frame_id;

    bool operator==(const StackProfileCallsiteTable::Row& other) const {
      return type() == other.type() && ColumnType::depth::Equals(depth, other.depth) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::frame_id::Equals(frame_id, other.frame_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t depth = ColumnType::depth::default_flags();
    static constexpr uint32_t parent_id = ColumnType::parent_id::default_flags();
    static constexpr uint32_t frame_id = ColumnType::frame_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      StackProfileCallsiteTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    StackProfileCallsiteTable, RowNumber> {
   public:
    ConstRowReference(const StackProfileCallsiteTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::depth::type depth() const {
      return table_->depth()[row_number_];
    }
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::frame_id::type frame_id() const {
      return table_->frame_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const StackProfileCallsiteTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_depth(
        ColumnType::depth::non_optional_type v) {
      return mutable_table()->mutable_depth()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_frame_id(
        ColumnType::frame_id::non_optional_type v) {
      return mutable_table()->mutable_frame_id()->Set(row_number_, v);
    }

   private:
    StackProfileCallsiteTable* mutable_table() const {
      return const_cast<StackProfileCallsiteTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, StackProfileCallsiteTable, RowNumber, ConstRowReference> {
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
    ColumnType::depth::type depth() const {
      const auto& col = table_->depth();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::frame_id::type frame_id() const {
      const auto& col = table_->frame_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const StackProfileCallsiteTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class StackProfileCallsiteTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, StackProfileCallsiteTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class StackProfileCallsiteTable;

     explicit Iterator(StackProfileCallsiteTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     StackProfileCallsiteTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      StackProfileCallsiteTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "depth", &self->depth_, ColumnFlag::depth,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "parent_id", &self->parent_id_, ColumnFlag::parent_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "frame_id", &self->frame_id_, ColumnFlag::frame_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit StackProfileCallsiteTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        depth_(ColumnStorage<ColumnType::depth::stored_type>::Create<false>()),
        parent_id_(ColumnStorage<ColumnType::parent_id::stored_type>::Create<false>()),
        frame_id_(ColumnStorage<ColumnType::frame_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        depth_storage_layer_(
        new column::NumericStorage<ColumnType::depth::non_optional_stored_type>(
          &depth_.vector(),
          ColumnTypeHelper<ColumnType::depth::stored_type>::ToColumnType(),
          false)),
        parent_id_storage_layer_(
          new column::NumericStorage<ColumnType::parent_id::non_optional_stored_type>(
            &parent_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_id::stored_type>::ToColumnType(),
            false)),
        frame_id_storage_layer_(
        new column::NumericStorage<ColumnType::frame_id::non_optional_stored_type>(
          &frame_id_.vector(),
          ColumnTypeHelper<ColumnType::frame_id::stored_type>::ToColumnType(),
          false))
,
        parent_id_null_layer_(new column::NullOverlay(parent_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::depth::stored_type>(
          ColumnFlag::depth),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_id::stored_type>(
          ColumnFlag::parent_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::frame_id::stored_type>(
          ColumnFlag::frame_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,depth_storage_layer_,parent_id_storage_layer_,frame_id_storage_layer_},
      {{},{},{},parent_id_null_layer_,{}});
  }
  ~StackProfileCallsiteTable() override;

  static const char* Name() { return "stack_profile_callsite"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "depth", ColumnType::depth::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
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
    depth_.ShrinkToFit();
    parent_id_.ShrinkToFit();
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
    mutable_depth()->Append(row.depth);
    mutable_parent_id()->Append(row.parent_id);
    mutable_frame_id()->Append(row.frame_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<StackProfileCallsiteTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& depth() const {
    return static_cast<const ColumnType::depth&>(columns()[ColumnIndex::depth]);
  }
  const TypedColumn<std::optional<StackProfileCallsiteTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<StackProfileFrameTable::Id>& frame_id() const {
    return static_cast<const ColumnType::frame_id&>(columns()[ColumnIndex::frame_id]);
  }

  TypedColumn<uint32_t>* mutable_depth() {
    return static_cast<ColumnType::depth*>(
        GetColumn(ColumnIndex::depth));
  }
  TypedColumn<std::optional<StackProfileCallsiteTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<StackProfileFrameTable::Id>* mutable_frame_id() {
    return static_cast<ColumnType::frame_id*>(
        GetColumn(ColumnIndex::frame_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::depth::stored_type> depth_;
  ColumnStorage<ColumnType::parent_id::stored_type> parent_id_;
  ColumnStorage<ColumnType::frame_id::stored_type> frame_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> depth_storage_layer_;
  RefPtr<column::StorageLayer> parent_id_storage_layer_;
  RefPtr<column::StorageLayer> frame_id_storage_layer_;

  RefPtr<column::OverlayLayer> parent_id_null_layer_;
};
  

class StackSampleTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t ts = 2;
    static constexpr uint32_t callsite_id = 3;
  };
  struct ColumnType {
    using id = IdColumn<StackSampleTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using callsite_id = TypedColumn<StackProfileCallsiteTable::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        StackProfileCallsiteTable::Id in_callsite_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          callsite_id(in_callsite_id) {
      type_ = "stack_sample";
    }
    int64_t ts;
    StackProfileCallsiteTable::Id callsite_id;

    bool operator==(const StackSampleTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::callsite_id::Equals(callsite_id, other.callsite_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::ts::default_flags();
    static constexpr uint32_t callsite_id = ColumnType::callsite_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      StackSampleTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    StackSampleTable, RowNumber> {
   public:
    ConstRowReference(const StackSampleTable* table, uint32_t row_number)
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
    ColumnType::callsite_id::type callsite_id() const {
      return table_->callsite_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const StackSampleTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_callsite_id(
        ColumnType::callsite_id::non_optional_type v) {
      return mutable_table()->mutable_callsite_id()->Set(row_number_, v);
    }

   private:
    StackSampleTable* mutable_table() const {
      return const_cast<StackSampleTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, StackSampleTable, RowNumber, ConstRowReference> {
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
    ColumnType::callsite_id::type callsite_id() const {
      const auto& col = table_->callsite_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const StackSampleTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class StackSampleTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, StackSampleTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class StackSampleTable;

     explicit Iterator(StackSampleTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     StackSampleTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      StackSampleTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "callsite_id", &self->callsite_id_, ColumnFlag::callsite_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit StackSampleTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        callsite_id_(ColumnStorage<ColumnType::callsite_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          true)),
        callsite_id_storage_layer_(
        new column::NumericStorage<ColumnType::callsite_id::non_optional_stored_type>(
          &callsite_id_.vector(),
          ColumnTypeHelper<ColumnType::callsite_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::callsite_id::stored_type>(
          ColumnFlag::callsite_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,callsite_id_storage_layer_},
      {{},{},{},{}});
  }
  ~StackSampleTable() override;

  static const char* Name() { return "stack_sample"; }

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
        "callsite_id", ColumnType::callsite_id::SqlValueType(), false,
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
    callsite_id_.ShrinkToFit();
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
    mutable_callsite_id()->Append(row.callsite_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<StackSampleTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<StackProfileCallsiteTable::Id>& callsite_id() const {
    return static_cast<const ColumnType::callsite_id&>(columns()[ColumnIndex::callsite_id]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<StackProfileCallsiteTable::Id>* mutable_callsite_id() {
    return static_cast<ColumnType::callsite_id*>(
        GetColumn(ColumnIndex::callsite_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::callsite_id::stored_type> callsite_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> callsite_id_storage_layer_;

  
};
  

class CpuProfileStackSampleTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 6;

  using Id = StackSampleTable::Id;
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t ts = 2;
    static constexpr uint32_t callsite_id = 3;
    static constexpr uint32_t utid = 4;
    static constexpr uint32_t process_priority = 5;
  };
  struct ColumnType {
    using id = IdColumn<CpuProfileStackSampleTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using callsite_id = TypedColumn<StackProfileCallsiteTable::Id>;
    using utid = TypedColumn<uint32_t>;
    using process_priority = TypedColumn<int32_t>;
  };
  struct Row : public StackSampleTable::Row {
    Row(int64_t in_ts = {},
        StackProfileCallsiteTable::Id in_callsite_id = {},
        uint32_t in_utid = {},
        int32_t in_process_priority = {},
        std::nullptr_t = nullptr)
        : StackSampleTable::Row(in_ts, in_callsite_id),
          utid(in_utid),
          process_priority(in_process_priority) {
      type_ = "cpu_profile_stack_sample";
    }
    uint32_t utid;
    int32_t process_priority;

    bool operator==(const CpuProfileStackSampleTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::callsite_id::Equals(callsite_id, other.callsite_id) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::process_priority::Equals(process_priority, other.process_priority);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
    static constexpr uint32_t process_priority = ColumnType::process_priority::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      CpuProfileStackSampleTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    CpuProfileStackSampleTable, RowNumber> {
   public:
    ConstRowReference(const CpuProfileStackSampleTable* table, uint32_t row_number)
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
    ColumnType::callsite_id::type callsite_id() const {
      return table_->callsite_id()[row_number_];
    }
    ColumnType::utid::type utid() const {
      return table_->utid()[row_number_];
    }
    ColumnType::process_priority::type process_priority() const {
      return table_->process_priority()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const CpuProfileStackSampleTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_callsite_id(
        ColumnType::callsite_id::non_optional_type v) {
      return mutable_table()->mutable_callsite_id()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_process_priority(
        ColumnType::process_priority::non_optional_type v) {
      return mutable_table()->mutable_process_priority()->Set(row_number_, v);
    }

   private:
    CpuProfileStackSampleTable* mutable_table() const {
      return const_cast<CpuProfileStackSampleTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, CpuProfileStackSampleTable, RowNumber, ConstRowReference> {
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
    ColumnType::callsite_id::type callsite_id() const {
      const auto& col = table_->callsite_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::utid::type utid() const {
      const auto& col = table_->utid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::process_priority::type process_priority() const {
      const auto& col = table_->process_priority();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const CpuProfileStackSampleTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class CpuProfileStackSampleTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, CpuProfileStackSampleTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class CpuProfileStackSampleTable;

     explicit Iterator(CpuProfileStackSampleTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     CpuProfileStackSampleTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      CpuProfileStackSampleTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "process_priority", &self->process_priority_, ColumnFlag::process_priority,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit CpuProfileStackSampleTable(StringPool* pool, StackSampleTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>()),
        process_priority_(ColumnStorage<ColumnType::process_priority::stored_type>::Create<false>())
,
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false)),
        process_priority_storage_layer_(
        new column::NumericStorage<ColumnType::process_priority::non_optional_stored_type>(
          &process_priority_.vector(),
          ColumnTypeHelper<ColumnType::process_priority::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::process_priority::stored_type>(
          ColumnFlag::process_priority),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::ts],const_parent_->storage_layers()[ColumnIndex::callsite_id],utid_storage_layer_,process_priority_storage_layer_},
      {{},{},{},{},{},{}});
  }
  ~CpuProfileStackSampleTable() override;

  static const char* Name() { return "cpu_profile_stack_sample"; }

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
        "callsite_id", ColumnType::callsite_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "process_priority", ColumnType::process_priority::SqlValueType(), false,
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
    process_priority_.ShrinkToFit();
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
    mutable_process_priority()->Append(row.process_priority);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const StackSampleTable& parent,
      ColumnStorage<ColumnType::utid::stored_type> utid
, ColumnStorage<ColumnType::process_priority::stored_type> process_priority) {
    return std::unique_ptr<Table>(new CpuProfileStackSampleTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(utid), std::move(process_priority)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const StackSampleTable& parent,
      std::vector<StackSampleTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::utid::stored_type> utid
, ColumnStorage<ColumnType::process_priority::stored_type> process_priority) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new CpuProfileStackSampleTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(utid), std::move(process_priority)));
  }

  const IdColumn<CpuProfileStackSampleTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<StackProfileCallsiteTable::Id>& callsite_id() const {
    return static_cast<const ColumnType::callsite_id&>(columns()[ColumnIndex::callsite_id]);
  }
  const TypedColumn<uint32_t>& utid() const {
    return static_cast<const ColumnType::utid&>(columns()[ColumnIndex::utid]);
  }
  const TypedColumn<int32_t>& process_priority() const {
    return static_cast<const ColumnType::process_priority&>(columns()[ColumnIndex::process_priority]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<StackProfileCallsiteTable::Id>* mutable_callsite_id() {
    return static_cast<ColumnType::callsite_id*>(
        GetColumn(ColumnIndex::callsite_id));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<int32_t>* mutable_process_priority() {
    return static_cast<ColumnType::process_priority*>(
        GetColumn(ColumnIndex::process_priority));
  }

 private:
  CpuProfileStackSampleTable(StringPool* pool,
            const StackSampleTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::utid::stored_type> utid
, ColumnStorage<ColumnType::process_priority::stored_type> process_priority)
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
          false)),
        process_priority_storage_layer_(
        new column::NumericStorage<ColumnType::process_priority::non_optional_stored_type>(
          &process_priority_.vector(),
          ColumnTypeHelper<ColumnType::process_priority::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::process_priority::stored_type>(
          ColumnFlag::process_priority),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(utid.size() == parent_overlay.size());
    utid_ = std::move(utid);
    PERFETTO_DCHECK(process_priority.size() == parent_overlay.size());
    process_priority_ = std::move(process_priority);

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
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::ts],const_parent_->storage_layers()[ColumnIndex::callsite_id],utid_storage_layer_,process_priority_storage_layer_}, {{},{},{},{},{},{}}, std::move(overlay_layers));
  }
  StackSampleTable* parent_ = nullptr;
  const StackSampleTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::utid::stored_type> utid_;
  ColumnStorage<ColumnType::process_priority::stored_type> process_priority_;

  RefPtr<column::StorageLayer> utid_storage_layer_;
  RefPtr<column::StorageLayer> process_priority_storage_layer_;

  
};
  

class ExperimentalFlamegraphTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 23;

  struct Id : public BaseId {
    Id() = default;
    explicit constexpr Id(uint32_t v) : BaseId(v) {}
  };
  static_assert(std::is_trivially_destructible_v<Id>,
                "Inheritance used without trivial destruction");
    
  struct ColumnIndex {
    static constexpr uint32_t id = 0;
    static constexpr uint32_t type = 1;
    static constexpr uint32_t profile_type = 2;
    static constexpr uint32_t ts_in = 3;
    static constexpr uint32_t ts_constraint = 4;
    static constexpr uint32_t upid = 5;
    static constexpr uint32_t upid_group = 6;
    static constexpr uint32_t focus_str = 7;
    static constexpr uint32_t ts = 8;
    static constexpr uint32_t depth = 9;
    static constexpr uint32_t name = 10;
    static constexpr uint32_t map_name = 11;
    static constexpr uint32_t count = 12;
    static constexpr uint32_t cumulative_count = 13;
    static constexpr uint32_t size = 14;
    static constexpr uint32_t cumulative_size = 15;
    static constexpr uint32_t alloc_count = 16;
    static constexpr uint32_t cumulative_alloc_count = 17;
    static constexpr uint32_t alloc_size = 18;
    static constexpr uint32_t cumulative_alloc_size = 19;
    static constexpr uint32_t parent_id = 20;
    static constexpr uint32_t source_file = 21;
    static constexpr uint32_t line_number = 22;
  };
  struct ColumnType {
    using id = IdColumn<ExperimentalFlamegraphTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using profile_type = TypedColumn<StringPool::Id>;
    using ts_in = TypedColumn<std::optional<int64_t>>;
    using ts_constraint = TypedColumn<std::optional<StringPool::Id>>;
    using upid = TypedColumn<std::optional<uint32_t>>;
    using upid_group = TypedColumn<std::optional<StringPool::Id>>;
    using focus_str = TypedColumn<std::optional<StringPool::Id>>;
    using ts = TypedColumn<int64_t>;
    using depth = TypedColumn<uint32_t>;
    using name = TypedColumn<StringPool::Id>;
    using map_name = TypedColumn<StringPool::Id>;
    using count = TypedColumn<int64_t>;
    using cumulative_count = TypedColumn<int64_t>;
    using size = TypedColumn<int64_t>;
    using cumulative_size = TypedColumn<int64_t>;
    using alloc_count = TypedColumn<int64_t>;
    using cumulative_alloc_count = TypedColumn<int64_t>;
    using alloc_size = TypedColumn<int64_t>;
    using cumulative_alloc_size = TypedColumn<int64_t>;
    using parent_id = TypedColumn<std::optional<ExperimentalFlamegraphTable::Id>>;
    using source_file = TypedColumn<std::optional<StringPool::Id>>;
    using line_number = TypedColumn<std::optional<uint32_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_profile_type = {},
        std::optional<int64_t> in_ts_in = {},
        std::optional<StringPool::Id> in_ts_constraint = {},
        std::optional<uint32_t> in_upid = {},
        std::optional<StringPool::Id> in_upid_group = {},
        std::optional<StringPool::Id> in_focus_str = {},
        int64_t in_ts = {},
        uint32_t in_depth = {},
        StringPool::Id in_name = {},
        StringPool::Id in_map_name = {},
        int64_t in_count = {},
        int64_t in_cumulative_count = {},
        int64_t in_size = {},
        int64_t in_cumulative_size = {},
        int64_t in_alloc_count = {},
        int64_t in_cumulative_alloc_count = {},
        int64_t in_alloc_size = {},
        int64_t in_cumulative_alloc_size = {},
        std::optional<ExperimentalFlamegraphTable::Id> in_parent_id = {},
        std::optional<StringPool::Id> in_source_file = {},
        std::optional<uint32_t> in_line_number = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          profile_type(in_profile_type),
          ts_in(in_ts_in),
          ts_constraint(in_ts_constraint),
          upid(in_upid),
          upid_group(in_upid_group),
          focus_str(in_focus_str),
          ts(in_ts),
          depth(in_depth),
          name(in_name),
          map_name(in_map_name),
          count(in_count),
          cumulative_count(in_cumulative_count),
          size(in_size),
          cumulative_size(in_cumulative_size),
          alloc_count(in_alloc_count),
          cumulative_alloc_count(in_cumulative_alloc_count),
          alloc_size(in_alloc_size),
          cumulative_alloc_size(in_cumulative_alloc_size),
          parent_id(in_parent_id),
          source_file(in_source_file),
          line_number(in_line_number) {
      type_ = "experimental_flamegraph";
    }
    StringPool::Id profile_type;
    std::optional<int64_t> ts_in;
    std::optional<StringPool::Id> ts_constraint;
    std::optional<uint32_t> upid;
    std::optional<StringPool::Id> upid_group;
    std::optional<StringPool::Id> focus_str;
    int64_t ts;
    uint32_t depth;
    StringPool::Id name;
    StringPool::Id map_name;
    int64_t count;
    int64_t cumulative_count;
    int64_t size;
    int64_t cumulative_size;
    int64_t alloc_count;
    int64_t cumulative_alloc_count;
    int64_t alloc_size;
    int64_t cumulative_alloc_size;
    std::optional<ExperimentalFlamegraphTable::Id> parent_id;
    std::optional<StringPool::Id> source_file;
    std::optional<uint32_t> line_number;

    bool operator==(const ExperimentalFlamegraphTable::Row& other) const {
      return type() == other.type() && ColumnType::profile_type::Equals(profile_type, other.profile_type) &&
       ColumnType::ts_in::Equals(ts_in, other.ts_in) &&
       ColumnType::ts_constraint::Equals(ts_constraint, other.ts_constraint) &&
       ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::upid_group::Equals(upid_group, other.upid_group) &&
       ColumnType::focus_str::Equals(focus_str, other.focus_str) &&
       ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::depth::Equals(depth, other.depth) &&
       ColumnType::name::Equals(name, other.name) &&
       ColumnType::map_name::Equals(map_name, other.map_name) &&
       ColumnType::count::Equals(count, other.count) &&
       ColumnType::cumulative_count::Equals(cumulative_count, other.cumulative_count) &&
       ColumnType::size::Equals(size, other.size) &&
       ColumnType::cumulative_size::Equals(cumulative_size, other.cumulative_size) &&
       ColumnType::alloc_count::Equals(alloc_count, other.alloc_count) &&
       ColumnType::cumulative_alloc_count::Equals(cumulative_alloc_count, other.cumulative_alloc_count) &&
       ColumnType::alloc_size::Equals(alloc_size, other.alloc_size) &&
       ColumnType::cumulative_alloc_size::Equals(cumulative_alloc_size, other.cumulative_alloc_size) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_file::Equals(source_file, other.source_file) &&
       ColumnType::line_number::Equals(line_number, other.line_number);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t profile_type = static_cast<uint32_t>(ColumnLegacy::Flag::kHidden) | ColumnType::profile_type::default_flags();
    static constexpr uint32_t ts_in = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted | ColumnLegacy::Flag::kHidden) | ColumnType::ts_in::default_flags();
    static constexpr uint32_t ts_constraint = static_cast<uint32_t>(ColumnLegacy::Flag::kHidden) | ColumnType::ts_constraint::default_flags();
    static constexpr uint32_t upid = static_cast<uint32_t>(ColumnLegacy::Flag::kHidden) | ColumnType::upid::default_flags();
    static constexpr uint32_t upid_group = static_cast<uint32_t>(ColumnLegacy::Flag::kHidden) | ColumnType::upid_group::default_flags();
    static constexpr uint32_t focus_str = static_cast<uint32_t>(ColumnLegacy::Flag::kHidden) | ColumnType::focus_str::default_flags();
    static constexpr uint32_t ts = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::ts::default_flags();
    static constexpr uint32_t depth = ColumnType::depth::default_flags();
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t map_name = ColumnType::map_name::default_flags();
    static constexpr uint32_t count = ColumnType::count::default_flags();
    static constexpr uint32_t cumulative_count = ColumnType::cumulative_count::default_flags();
    static constexpr uint32_t size = ColumnType::size::default_flags();
    static constexpr uint32_t cumulative_size = ColumnType::cumulative_size::default_flags();
    static constexpr uint32_t alloc_count = ColumnType::alloc_count::default_flags();
    static constexpr uint32_t cumulative_alloc_count = ColumnType::cumulative_alloc_count::default_flags();
    static constexpr uint32_t alloc_size = ColumnType::alloc_size::default_flags();
    static constexpr uint32_t cumulative_alloc_size = ColumnType::cumulative_alloc_size::default_flags();
    static constexpr uint32_t parent_id = ColumnType::parent_id::default_flags();
    static constexpr uint32_t source_file = ColumnType::source_file::default_flags();
    static constexpr uint32_t line_number = ColumnType::line_number::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ExperimentalFlamegraphTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ExperimentalFlamegraphTable, RowNumber> {
   public:
    ConstRowReference(const ExperimentalFlamegraphTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::profile_type::type profile_type() const {
      return table_->profile_type()[row_number_];
    }
    ColumnType::ts_in::type ts_in() const {
      return table_->ts_in()[row_number_];
    }
    ColumnType::ts_constraint::type ts_constraint() const {
      return table_->ts_constraint()[row_number_];
    }
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
    ColumnType::upid_group::type upid_group() const {
      return table_->upid_group()[row_number_];
    }
    ColumnType::focus_str::type focus_str() const {
      return table_->focus_str()[row_number_];
    }
    ColumnType::ts::type ts() const {
      return table_->ts()[row_number_];
    }
    ColumnType::depth::type depth() const {
      return table_->depth()[row_number_];
    }
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::map_name::type map_name() const {
      return table_->map_name()[row_number_];
    }
    ColumnType::count::type count() const {
      return table_->count()[row_number_];
    }
    ColumnType::cumulative_count::type cumulative_count() const {
      return table_->cumulative_count()[row_number_];
    }
    ColumnType::size::type size() const {
      return table_->size()[row_number_];
    }
    ColumnType::cumulative_size::type cumulative_size() const {
      return table_->cumulative_size()[row_number_];
    }
    ColumnType::alloc_count::type alloc_count() const {
      return table_->alloc_count()[row_number_];
    }
    ColumnType::cumulative_alloc_count::type cumulative_alloc_count() const {
      return table_->cumulative_alloc_count()[row_number_];
    }
    ColumnType::alloc_size::type alloc_size() const {
      return table_->alloc_size()[row_number_];
    }
    ColumnType::cumulative_alloc_size::type cumulative_alloc_size() const {
      return table_->cumulative_alloc_size()[row_number_];
    }
    ColumnType::parent_id::type parent_id() const {
      return table_->parent_id()[row_number_];
    }
    ColumnType::source_file::type source_file() const {
      return table_->source_file()[row_number_];
    }
    ColumnType::line_number::type line_number() const {
      return table_->line_number()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ExperimentalFlamegraphTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_profile_type(
        ColumnType::profile_type::non_optional_type v) {
      return mutable_table()->mutable_profile_type()->Set(row_number_, v);
    }
    void set_ts_in(
        ColumnType::ts_in::non_optional_type v) {
      return mutable_table()->mutable_ts_in()->Set(row_number_, v);
    }
    void set_ts_constraint(
        ColumnType::ts_constraint::non_optional_type v) {
      return mutable_table()->mutable_ts_constraint()->Set(row_number_, v);
    }
    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_upid_group(
        ColumnType::upid_group::non_optional_type v) {
      return mutable_table()->mutable_upid_group()->Set(row_number_, v);
    }
    void set_focus_str(
        ColumnType::focus_str::non_optional_type v) {
      return mutable_table()->mutable_focus_str()->Set(row_number_, v);
    }
    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_depth(
        ColumnType::depth::non_optional_type v) {
      return mutable_table()->mutable_depth()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_map_name(
        ColumnType::map_name::non_optional_type v) {
      return mutable_table()->mutable_map_name()->Set(row_number_, v);
    }
    void set_count(
        ColumnType::count::non_optional_type v) {
      return mutable_table()->mutable_count()->Set(row_number_, v);
    }
    void set_cumulative_count(
        ColumnType::cumulative_count::non_optional_type v) {
      return mutable_table()->mutable_cumulative_count()->Set(row_number_, v);
    }
    void set_size(
        ColumnType::size::non_optional_type v) {
      return mutable_table()->mutable_size()->Set(row_number_, v);
    }
    void set_cumulative_size(
        ColumnType::cumulative_size::non_optional_type v) {
      return mutable_table()->mutable_cumulative_size()->Set(row_number_, v);
    }
    void set_alloc_count(
        ColumnType::alloc_count::non_optional_type v) {
      return mutable_table()->mutable_alloc_count()->Set(row_number_, v);
    }
    void set_cumulative_alloc_count(
        ColumnType::cumulative_alloc_count::non_optional_type v) {
      return mutable_table()->mutable_cumulative_alloc_count()->Set(row_number_, v);
    }
    void set_alloc_size(
        ColumnType::alloc_size::non_optional_type v) {
      return mutable_table()->mutable_alloc_size()->Set(row_number_, v);
    }
    void set_cumulative_alloc_size(
        ColumnType::cumulative_alloc_size::non_optional_type v) {
      return mutable_table()->mutable_cumulative_alloc_size()->Set(row_number_, v);
    }
    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_source_file(
        ColumnType::source_file::non_optional_type v) {
      return mutable_table()->mutable_source_file()->Set(row_number_, v);
    }
    void set_line_number(
        ColumnType::line_number::non_optional_type v) {
      return mutable_table()->mutable_line_number()->Set(row_number_, v);
    }

   private:
    ExperimentalFlamegraphTable* mutable_table() const {
      return const_cast<ExperimentalFlamegraphTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ExperimentalFlamegraphTable, RowNumber, ConstRowReference> {
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
    ColumnType::profile_type::type profile_type() const {
      const auto& col = table_->profile_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ts_in::type ts_in() const {
      const auto& col = table_->ts_in();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ts_constraint::type ts_constraint() const {
      const auto& col = table_->ts_constraint();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::upid_group::type upid_group() const {
      const auto& col = table_->upid_group();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::focus_str::type focus_str() const {
      const auto& col = table_->focus_str();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::ts::type ts() const {
      const auto& col = table_->ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::depth::type depth() const {
      const auto& col = table_->depth();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::map_name::type map_name() const {
      const auto& col = table_->map_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::count::type count() const {
      const auto& col = table_->count();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::cumulative_count::type cumulative_count() const {
      const auto& col = table_->cumulative_count();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::size::type size() const {
      const auto& col = table_->size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::cumulative_size::type cumulative_size() const {
      const auto& col = table_->cumulative_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::alloc_count::type alloc_count() const {
      const auto& col = table_->alloc_count();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::cumulative_alloc_count::type cumulative_alloc_count() const {
      const auto& col = table_->cumulative_alloc_count();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::alloc_size::type alloc_size() const {
      const auto& col = table_->alloc_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::cumulative_alloc_size::type cumulative_alloc_size() const {
      const auto& col = table_->cumulative_alloc_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::parent_id::type parent_id() const {
      const auto& col = table_->parent_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_file::type source_file() const {
      const auto& col = table_->source_file();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::line_number::type line_number() const {
      const auto& col = table_->line_number();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ExperimentalFlamegraphTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ExperimentalFlamegraphTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ExperimentalFlamegraphTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ExperimentalFlamegraphTable;

     explicit Iterator(ExperimentalFlamegraphTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ExperimentalFlamegraphTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ExperimentalFlamegraphTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "profile_type", &self->profile_type_, ColumnFlag::profile_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ts_in", &self->ts_in_, ColumnFlag::ts_in,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ts_constraint", &self->ts_constraint_, ColumnFlag::ts_constraint,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "upid_group", &self->upid_group_, ColumnFlag::upid_group,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "focus_str", &self->focus_str_, ColumnFlag::focus_str,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "depth", &self->depth_, ColumnFlag::depth,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "map_name", &self->map_name_, ColumnFlag::map_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "count", &self->count_, ColumnFlag::count,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cumulative_count", &self->cumulative_count_, ColumnFlag::cumulative_count,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "size", &self->size_, ColumnFlag::size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cumulative_size", &self->cumulative_size_, ColumnFlag::cumulative_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "alloc_count", &self->alloc_count_, ColumnFlag::alloc_count,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cumulative_alloc_count", &self->cumulative_alloc_count_, ColumnFlag::cumulative_alloc_count,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "alloc_size", &self->alloc_size_, ColumnFlag::alloc_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cumulative_alloc_size", &self->cumulative_alloc_size_, ColumnFlag::cumulative_alloc_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "parent_id", &self->parent_id_, ColumnFlag::parent_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "source_file", &self->source_file_, ColumnFlag::source_file,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "line_number", &self->line_number_, ColumnFlag::line_number,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ExperimentalFlamegraphTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        profile_type_(ColumnStorage<ColumnType::profile_type::stored_type>::Create<false>()),
        ts_in_(ColumnStorage<ColumnType::ts_in::stored_type>::Create<false>()),
        ts_constraint_(ColumnStorage<ColumnType::ts_constraint::stored_type>::Create<false>()),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        upid_group_(ColumnStorage<ColumnType::upid_group::stored_type>::Create<false>()),
        focus_str_(ColumnStorage<ColumnType::focus_str::stored_type>::Create<false>()),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        depth_(ColumnStorage<ColumnType::depth::stored_type>::Create<false>()),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        map_name_(ColumnStorage<ColumnType::map_name::stored_type>::Create<false>()),
        count_(ColumnStorage<ColumnType::count::stored_type>::Create<false>()),
        cumulative_count_(ColumnStorage<ColumnType::cumulative_count::stored_type>::Create<false>()),
        size_(ColumnStorage<ColumnType::size::stored_type>::Create<false>()),
        cumulative_size_(ColumnStorage<ColumnType::cumulative_size::stored_type>::Create<false>()),
        alloc_count_(ColumnStorage<ColumnType::alloc_count::stored_type>::Create<false>()),
        cumulative_alloc_count_(ColumnStorage<ColumnType::cumulative_alloc_count::stored_type>::Create<false>()),
        alloc_size_(ColumnStorage<ColumnType::alloc_size::stored_type>::Create<false>()),
        cumulative_alloc_size_(ColumnStorage<ColumnType::cumulative_alloc_size::stored_type>::Create<false>()),
        parent_id_(ColumnStorage<ColumnType::parent_id::stored_type>::Create<false>()),
        source_file_(ColumnStorage<ColumnType::source_file::stored_type>::Create<false>()),
        line_number_(ColumnStorage<ColumnType::line_number::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        profile_type_storage_layer_(
          new column::StringStorage(string_pool(), &profile_type_.vector())),
        ts_in_storage_layer_(
          new column::NumericStorage<ColumnType::ts_in::non_optional_stored_type>(
            &ts_in_.non_null_vector(),
            ColumnTypeHelper<ColumnType::ts_in::stored_type>::ToColumnType(),
            true)),
        ts_constraint_storage_layer_(
          new column::StringStorage(string_pool(), &ts_constraint_.vector())),
        upid_storage_layer_(
          new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
            &upid_.non_null_vector(),
            ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
            false)),
        upid_group_storage_layer_(
          new column::StringStorage(string_pool(), &upid_group_.vector())),
        focus_str_storage_layer_(
          new column::StringStorage(string_pool(), &focus_str_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          true)),
        depth_storage_layer_(
        new column::NumericStorage<ColumnType::depth::non_optional_stored_type>(
          &depth_.vector(),
          ColumnTypeHelper<ColumnType::depth::stored_type>::ToColumnType(),
          false)),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        map_name_storage_layer_(
          new column::StringStorage(string_pool(), &map_name_.vector())),
        count_storage_layer_(
        new column::NumericStorage<ColumnType::count::non_optional_stored_type>(
          &count_.vector(),
          ColumnTypeHelper<ColumnType::count::stored_type>::ToColumnType(),
          false)),
        cumulative_count_storage_layer_(
        new column::NumericStorage<ColumnType::cumulative_count::non_optional_stored_type>(
          &cumulative_count_.vector(),
          ColumnTypeHelper<ColumnType::cumulative_count::stored_type>::ToColumnType(),
          false)),
        size_storage_layer_(
        new column::NumericStorage<ColumnType::size::non_optional_stored_type>(
          &size_.vector(),
          ColumnTypeHelper<ColumnType::size::stored_type>::ToColumnType(),
          false)),
        cumulative_size_storage_layer_(
        new column::NumericStorage<ColumnType::cumulative_size::non_optional_stored_type>(
          &cumulative_size_.vector(),
          ColumnTypeHelper<ColumnType::cumulative_size::stored_type>::ToColumnType(),
          false)),
        alloc_count_storage_layer_(
        new column::NumericStorage<ColumnType::alloc_count::non_optional_stored_type>(
          &alloc_count_.vector(),
          ColumnTypeHelper<ColumnType::alloc_count::stored_type>::ToColumnType(),
          false)),
        cumulative_alloc_count_storage_layer_(
        new column::NumericStorage<ColumnType::cumulative_alloc_count::non_optional_stored_type>(
          &cumulative_alloc_count_.vector(),
          ColumnTypeHelper<ColumnType::cumulative_alloc_count::stored_type>::ToColumnType(),
          false)),
        alloc_size_storage_layer_(
        new column::NumericStorage<ColumnType::alloc_size::non_optional_stored_type>(
          &alloc_size_.vector(),
          ColumnTypeHelper<ColumnType::alloc_size::stored_type>::ToColumnType(),
          false)),
        cumulative_alloc_size_storage_layer_(
        new column::NumericStorage<ColumnType::cumulative_alloc_size::non_optional_stored_type>(
          &cumulative_alloc_size_.vector(),
          ColumnTypeHelper<ColumnType::cumulative_alloc_size::stored_type>::ToColumnType(),
          false)),
        parent_id_storage_layer_(
          new column::NumericStorage<ColumnType::parent_id::non_optional_stored_type>(
            &parent_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_id::stored_type>::ToColumnType(),
            false)),
        source_file_storage_layer_(
          new column::StringStorage(string_pool(), &source_file_.vector())),
        line_number_storage_layer_(
          new column::NumericStorage<ColumnType::line_number::non_optional_stored_type>(
            &line_number_.non_null_vector(),
            ColumnTypeHelper<ColumnType::line_number::stored_type>::ToColumnType(),
            false))
,
        ts_in_null_layer_(new column::NullOverlay(ts_in_.bv())),
        upid_null_layer_(new column::NullOverlay(upid_.bv())),
        parent_id_null_layer_(new column::NullOverlay(parent_id_.bv())),
        line_number_null_layer_(new column::NullOverlay(line_number_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::profile_type::stored_type>(
          ColumnFlag::profile_type),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts_in::stored_type>(
          ColumnFlag::ts_in),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts_constraint::stored_type>(
          ColumnFlag::ts_constraint),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid_group::stored_type>(
          ColumnFlag::upid_group),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::focus_str::stored_type>(
          ColumnFlag::focus_str),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::depth::stored_type>(
          ColumnFlag::depth),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::map_name::stored_type>(
          ColumnFlag::map_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::count::stored_type>(
          ColumnFlag::count),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cumulative_count::stored_type>(
          ColumnFlag::cumulative_count),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::size::stored_type>(
          ColumnFlag::size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cumulative_size::stored_type>(
          ColumnFlag::cumulative_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::alloc_count::stored_type>(
          ColumnFlag::alloc_count),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cumulative_alloc_count::stored_type>(
          ColumnFlag::cumulative_alloc_count),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::alloc_size::stored_type>(
          ColumnFlag::alloc_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cumulative_alloc_size::stored_type>(
          ColumnFlag::cumulative_alloc_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_id::stored_type>(
          ColumnFlag::parent_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::source_file::stored_type>(
          ColumnFlag::source_file),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::line_number::stored_type>(
          ColumnFlag::line_number),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,profile_type_storage_layer_,ts_in_storage_layer_,ts_constraint_storage_layer_,upid_storage_layer_,upid_group_storage_layer_,focus_str_storage_layer_,ts_storage_layer_,depth_storage_layer_,name_storage_layer_,map_name_storage_layer_,count_storage_layer_,cumulative_count_storage_layer_,size_storage_layer_,cumulative_size_storage_layer_,alloc_count_storage_layer_,cumulative_alloc_count_storage_layer_,alloc_size_storage_layer_,cumulative_alloc_size_storage_layer_,parent_id_storage_layer_,source_file_storage_layer_,line_number_storage_layer_},
      {{},{},{},ts_in_null_layer_,{},upid_null_layer_,{},{},{},{},{},{},{},{},{},{},{},{},{},{},parent_id_null_layer_,{},line_number_null_layer_});
  }
  ~ExperimentalFlamegraphTable() override;

  static const char* Name() { return "experimental_flamegraph"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "profile_type", ColumnType::profile_type::SqlValueType(), false,
        false,
        true,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts_in", ColumnType::ts_in::SqlValueType(), false,
        true,
        true,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts_constraint", ColumnType::ts_constraint::SqlValueType(), false,
        false,
        true,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "upid", ColumnType::upid::SqlValueType(), false,
        false,
        true,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "upid_group", ColumnType::upid_group::SqlValueType(), false,
        false,
        true,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "focus_str", ColumnType::focus_str::SqlValueType(), false,
        false,
        true,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "ts", ColumnType::ts::SqlValueType(), false,
        true,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "depth", ColumnType::depth::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "map_name", ColumnType::map_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "count", ColumnType::count::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cumulative_count", ColumnType::cumulative_count::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "size", ColumnType::size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cumulative_size", ColumnType::cumulative_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "alloc_count", ColumnType::alloc_count::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cumulative_alloc_count", ColumnType::cumulative_alloc_count::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "alloc_size", ColumnType::alloc_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cumulative_alloc_size", ColumnType::cumulative_alloc_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "parent_id", ColumnType::parent_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_file", ColumnType::source_file::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "line_number", ColumnType::line_number::SqlValueType(), false,
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
    profile_type_.ShrinkToFit();
    ts_in_.ShrinkToFit();
    ts_constraint_.ShrinkToFit();
    upid_.ShrinkToFit();
    upid_group_.ShrinkToFit();
    focus_str_.ShrinkToFit();
    ts_.ShrinkToFit();
    depth_.ShrinkToFit();
    name_.ShrinkToFit();
    map_name_.ShrinkToFit();
    count_.ShrinkToFit();
    cumulative_count_.ShrinkToFit();
    size_.ShrinkToFit();
    cumulative_size_.ShrinkToFit();
    alloc_count_.ShrinkToFit();
    cumulative_alloc_count_.ShrinkToFit();
    alloc_size_.ShrinkToFit();
    cumulative_alloc_size_.ShrinkToFit();
    parent_id_.ShrinkToFit();
    source_file_.ShrinkToFit();
    line_number_.ShrinkToFit();
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
    mutable_profile_type()->Append(row.profile_type);
    mutable_ts_in()->Append(row.ts_in);
    mutable_ts_constraint()->Append(row.ts_constraint);
    mutable_upid()->Append(row.upid);
    mutable_upid_group()->Append(row.upid_group);
    mutable_focus_str()->Append(row.focus_str);
    mutable_ts()->Append(row.ts);
    mutable_depth()->Append(row.depth);
    mutable_name()->Append(row.name);
    mutable_map_name()->Append(row.map_name);
    mutable_count()->Append(row.count);
    mutable_cumulative_count()->Append(row.cumulative_count);
    mutable_size()->Append(row.size);
    mutable_cumulative_size()->Append(row.cumulative_size);
    mutable_alloc_count()->Append(row.alloc_count);
    mutable_cumulative_alloc_count()->Append(row.cumulative_alloc_count);
    mutable_alloc_size()->Append(row.alloc_size);
    mutable_cumulative_alloc_size()->Append(row.cumulative_alloc_size);
    mutable_parent_id()->Append(row.parent_id);
    mutable_source_file()->Append(row.source_file);
    mutable_line_number()->Append(row.line_number);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ExperimentalFlamegraphTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& profile_type() const {
    return static_cast<const ColumnType::profile_type&>(columns()[ColumnIndex::profile_type]);
  }
  const TypedColumn<std::optional<int64_t>>& ts_in() const {
    return static_cast<const ColumnType::ts_in&>(columns()[ColumnIndex::ts_in]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& ts_constraint() const {
    return static_cast<const ColumnType::ts_constraint&>(columns()[ColumnIndex::ts_constraint]);
  }
  const TypedColumn<std::optional<uint32_t>>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& upid_group() const {
    return static_cast<const ColumnType::upid_group&>(columns()[ColumnIndex::upid_group]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& focus_str() const {
    return static_cast<const ColumnType::focus_str&>(columns()[ColumnIndex::focus_str]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<uint32_t>& depth() const {
    return static_cast<const ColumnType::depth&>(columns()[ColumnIndex::depth]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<StringPool::Id>& map_name() const {
    return static_cast<const ColumnType::map_name&>(columns()[ColumnIndex::map_name]);
  }
  const TypedColumn<int64_t>& count() const {
    return static_cast<const ColumnType::count&>(columns()[ColumnIndex::count]);
  }
  const TypedColumn<int64_t>& cumulative_count() const {
    return static_cast<const ColumnType::cumulative_count&>(columns()[ColumnIndex::cumulative_count]);
  }
  const TypedColumn<int64_t>& size() const {
    return static_cast<const ColumnType::size&>(columns()[ColumnIndex::size]);
  }
  const TypedColumn<int64_t>& cumulative_size() const {
    return static_cast<const ColumnType::cumulative_size&>(columns()[ColumnIndex::cumulative_size]);
  }
  const TypedColumn<int64_t>& alloc_count() const {
    return static_cast<const ColumnType::alloc_count&>(columns()[ColumnIndex::alloc_count]);
  }
  const TypedColumn<int64_t>& cumulative_alloc_count() const {
    return static_cast<const ColumnType::cumulative_alloc_count&>(columns()[ColumnIndex::cumulative_alloc_count]);
  }
  const TypedColumn<int64_t>& alloc_size() const {
    return static_cast<const ColumnType::alloc_size&>(columns()[ColumnIndex::alloc_size]);
  }
  const TypedColumn<int64_t>& cumulative_alloc_size() const {
    return static_cast<const ColumnType::cumulative_alloc_size&>(columns()[ColumnIndex::cumulative_alloc_size]);
  }
  const TypedColumn<std::optional<ExperimentalFlamegraphTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& source_file() const {
    return static_cast<const ColumnType::source_file&>(columns()[ColumnIndex::source_file]);
  }
  const TypedColumn<std::optional<uint32_t>>& line_number() const {
    return static_cast<const ColumnType::line_number&>(columns()[ColumnIndex::line_number]);
  }

  TypedColumn<StringPool::Id>* mutable_profile_type() {
    return static_cast<ColumnType::profile_type*>(
        GetColumn(ColumnIndex::profile_type));
  }
  TypedColumn<std::optional<int64_t>>* mutable_ts_in() {
    return static_cast<ColumnType::ts_in*>(
        GetColumn(ColumnIndex::ts_in));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_ts_constraint() {
    return static_cast<ColumnType::ts_constraint*>(
        GetColumn(ColumnIndex::ts_constraint));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_upid_group() {
    return static_cast<ColumnType::upid_group*>(
        GetColumn(ColumnIndex::upid_group));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_focus_str() {
    return static_cast<ColumnType::focus_str*>(
        GetColumn(ColumnIndex::focus_str));
  }
  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<uint32_t>* mutable_depth() {
    return static_cast<ColumnType::depth*>(
        GetColumn(ColumnIndex::depth));
  }
  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<StringPool::Id>* mutable_map_name() {
    return static_cast<ColumnType::map_name*>(
        GetColumn(ColumnIndex::map_name));
  }
  TypedColumn<int64_t>* mutable_count() {
    return static_cast<ColumnType::count*>(
        GetColumn(ColumnIndex::count));
  }
  TypedColumn<int64_t>* mutable_cumulative_count() {
    return static_cast<ColumnType::cumulative_count*>(
        GetColumn(ColumnIndex::cumulative_count));
  }
  TypedColumn<int64_t>* mutable_size() {
    return static_cast<ColumnType::size*>(
        GetColumn(ColumnIndex::size));
  }
  TypedColumn<int64_t>* mutable_cumulative_size() {
    return static_cast<ColumnType::cumulative_size*>(
        GetColumn(ColumnIndex::cumulative_size));
  }
  TypedColumn<int64_t>* mutable_alloc_count() {
    return static_cast<ColumnType::alloc_count*>(
        GetColumn(ColumnIndex::alloc_count));
  }
  TypedColumn<int64_t>* mutable_cumulative_alloc_count() {
    return static_cast<ColumnType::cumulative_alloc_count*>(
        GetColumn(ColumnIndex::cumulative_alloc_count));
  }
  TypedColumn<int64_t>* mutable_alloc_size() {
    return static_cast<ColumnType::alloc_size*>(
        GetColumn(ColumnIndex::alloc_size));
  }
  TypedColumn<int64_t>* mutable_cumulative_alloc_size() {
    return static_cast<ColumnType::cumulative_alloc_size*>(
        GetColumn(ColumnIndex::cumulative_alloc_size));
  }
  TypedColumn<std::optional<ExperimentalFlamegraphTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_source_file() {
    return static_cast<ColumnType::source_file*>(
        GetColumn(ColumnIndex::source_file));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_line_number() {
    return static_cast<ColumnType::line_number*>(
        GetColumn(ColumnIndex::line_number));
  }

 private:
  
  
  ColumnStorage<ColumnType::profile_type::stored_type> profile_type_;
  ColumnStorage<ColumnType::ts_in::stored_type> ts_in_;
  ColumnStorage<ColumnType::ts_constraint::stored_type> ts_constraint_;
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::upid_group::stored_type> upid_group_;
  ColumnStorage<ColumnType::focus_str::stored_type> focus_str_;
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::depth::stored_type> depth_;
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::map_name::stored_type> map_name_;
  ColumnStorage<ColumnType::count::stored_type> count_;
  ColumnStorage<ColumnType::cumulative_count::stored_type> cumulative_count_;
  ColumnStorage<ColumnType::size::stored_type> size_;
  ColumnStorage<ColumnType::cumulative_size::stored_type> cumulative_size_;
  ColumnStorage<ColumnType::alloc_count::stored_type> alloc_count_;
  ColumnStorage<ColumnType::cumulative_alloc_count::stored_type> cumulative_alloc_count_;
  ColumnStorage<ColumnType::alloc_size::stored_type> alloc_size_;
  ColumnStorage<ColumnType::cumulative_alloc_size::stored_type> cumulative_alloc_size_;
  ColumnStorage<ColumnType::parent_id::stored_type> parent_id_;
  ColumnStorage<ColumnType::source_file::stored_type> source_file_;
  ColumnStorage<ColumnType::line_number::stored_type> line_number_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> profile_type_storage_layer_;
  RefPtr<column::StorageLayer> ts_in_storage_layer_;
  RefPtr<column::StorageLayer> ts_constraint_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> upid_group_storage_layer_;
  RefPtr<column::StorageLayer> focus_str_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> depth_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> map_name_storage_layer_;
  RefPtr<column::StorageLayer> count_storage_layer_;
  RefPtr<column::StorageLayer> cumulative_count_storage_layer_;
  RefPtr<column::StorageLayer> size_storage_layer_;
  RefPtr<column::StorageLayer> cumulative_size_storage_layer_;
  RefPtr<column::StorageLayer> alloc_count_storage_layer_;
  RefPtr<column::StorageLayer> cumulative_alloc_count_storage_layer_;
  RefPtr<column::StorageLayer> alloc_size_storage_layer_;
  RefPtr<column::StorageLayer> cumulative_alloc_size_storage_layer_;
  RefPtr<column::StorageLayer> parent_id_storage_layer_;
  RefPtr<column::StorageLayer> source_file_storage_layer_;
  RefPtr<column::StorageLayer> line_number_storage_layer_;

  RefPtr<column::OverlayLayer> ts_in_null_layer_;
  RefPtr<column::OverlayLayer> upid_null_layer_;
  RefPtr<column::OverlayLayer> parent_id_null_layer_;
  RefPtr<column::OverlayLayer> line_number_null_layer_;
};
  

class GpuCounterGroupTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t group_id = 2;
    static constexpr uint32_t track_id = 3;
  };
  struct ColumnType {
    using id = IdColumn<GpuCounterGroupTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using group_id = TypedColumn<int32_t>;
    using track_id = TypedColumn<TrackTable::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int32_t in_group_id = {},
        TrackTable::Id in_track_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          group_id(in_group_id),
          track_id(in_track_id) {
      type_ = "gpu_counter_group";
    }
    int32_t group_id;
    TrackTable::Id track_id;

    bool operator==(const GpuCounterGroupTable::Row& other) const {
      return type() == other.type() && ColumnType::group_id::Equals(group_id, other.group_id) &&
       ColumnType::track_id::Equals(track_id, other.track_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t group_id = ColumnType::group_id::default_flags();
    static constexpr uint32_t track_id = ColumnType::track_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      GpuCounterGroupTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    GpuCounterGroupTable, RowNumber> {
   public:
    ConstRowReference(const GpuCounterGroupTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::group_id::type group_id() const {
      return table_->group_id()[row_number_];
    }
    ColumnType::track_id::type track_id() const {
      return table_->track_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const GpuCounterGroupTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_group_id(
        ColumnType::group_id::non_optional_type v) {
      return mutable_table()->mutable_group_id()->Set(row_number_, v);
    }
    void set_track_id(
        ColumnType::track_id::non_optional_type v) {
      return mutable_table()->mutable_track_id()->Set(row_number_, v);
    }

   private:
    GpuCounterGroupTable* mutable_table() const {
      return const_cast<GpuCounterGroupTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, GpuCounterGroupTable, RowNumber, ConstRowReference> {
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
    ColumnType::group_id::type group_id() const {
      const auto& col = table_->group_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::track_id::type track_id() const {
      const auto& col = table_->track_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const GpuCounterGroupTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class GpuCounterGroupTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, GpuCounterGroupTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class GpuCounterGroupTable;

     explicit Iterator(GpuCounterGroupTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     GpuCounterGroupTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      GpuCounterGroupTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "group_id", &self->group_id_, ColumnFlag::group_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "track_id", &self->track_id_, ColumnFlag::track_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit GpuCounterGroupTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        group_id_(ColumnStorage<ColumnType::group_id::stored_type>::Create<false>()),
        track_id_(ColumnStorage<ColumnType::track_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        group_id_storage_layer_(
        new column::NumericStorage<ColumnType::group_id::non_optional_stored_type>(
          &group_id_.vector(),
          ColumnTypeHelper<ColumnType::group_id::stored_type>::ToColumnType(),
          false)),
        track_id_storage_layer_(
        new column::NumericStorage<ColumnType::track_id::non_optional_stored_type>(
          &track_id_.vector(),
          ColumnTypeHelper<ColumnType::track_id::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::group_id::stored_type>(
          ColumnFlag::group_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::track_id::stored_type>(
          ColumnFlag::track_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,group_id_storage_layer_,track_id_storage_layer_},
      {{},{},{},{}});
  }
  ~GpuCounterGroupTable() override;

  static const char* Name() { return "gpu_counter_group"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "group_id", ColumnType::group_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "track_id", ColumnType::track_id::SqlValueType(), false,
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
    group_id_.ShrinkToFit();
    track_id_.ShrinkToFit();
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
    mutable_group_id()->Append(row.group_id);
    mutable_track_id()->Append(row.track_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<GpuCounterGroupTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int32_t>& group_id() const {
    return static_cast<const ColumnType::group_id&>(columns()[ColumnIndex::group_id]);
  }
  const TypedColumn<TrackTable::Id>& track_id() const {
    return static_cast<const ColumnType::track_id&>(columns()[ColumnIndex::track_id]);
  }

  TypedColumn<int32_t>* mutable_group_id() {
    return static_cast<ColumnType::group_id*>(
        GetColumn(ColumnIndex::group_id));
  }
  TypedColumn<TrackTable::Id>* mutable_track_id() {
    return static_cast<ColumnType::track_id*>(
        GetColumn(ColumnIndex::track_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::group_id::stored_type> group_id_;
  ColumnStorage<ColumnType::track_id::stored_type> track_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> group_id_storage_layer_;
  RefPtr<column::StorageLayer> track_id_storage_layer_;

  
};
  

class HeapGraphClassTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t deobfuscated_name = 3;
    static constexpr uint32_t location = 4;
    static constexpr uint32_t superclass_id = 5;
    static constexpr uint32_t classloader_id = 6;
    static constexpr uint32_t kind = 7;
  };
  struct ColumnType {
    using id = IdColumn<HeapGraphClassTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using deobfuscated_name = TypedColumn<std::optional<StringPool::Id>>;
    using location = TypedColumn<std::optional<StringPool::Id>>;
    using superclass_id = TypedColumn<std::optional<HeapGraphClassTable::Id>>;
    using classloader_id = TypedColumn<std::optional<uint32_t>>;
    using kind = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<StringPool::Id> in_deobfuscated_name = {},
        std::optional<StringPool::Id> in_location = {},
        std::optional<HeapGraphClassTable::Id> in_superclass_id = {},
        std::optional<uint32_t> in_classloader_id = {},
        StringPool::Id in_kind = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          name(in_name),
          deobfuscated_name(in_deobfuscated_name),
          location(in_location),
          superclass_id(in_superclass_id),
          classloader_id(in_classloader_id),
          kind(in_kind) {
      type_ = "heap_graph_class";
    }
    StringPool::Id name;
    std::optional<StringPool::Id> deobfuscated_name;
    std::optional<StringPool::Id> location;
    std::optional<HeapGraphClassTable::Id> superclass_id;
    std::optional<uint32_t> classloader_id;
    StringPool::Id kind;

    bool operator==(const HeapGraphClassTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::deobfuscated_name::Equals(deobfuscated_name, other.deobfuscated_name) &&
       ColumnType::location::Equals(location, other.location) &&
       ColumnType::superclass_id::Equals(superclass_id, other.superclass_id) &&
       ColumnType::classloader_id::Equals(classloader_id, other.classloader_id) &&
       ColumnType::kind::Equals(kind, other.kind);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t deobfuscated_name = ColumnType::deobfuscated_name::default_flags();
    static constexpr uint32_t location = ColumnType::location::default_flags();
    static constexpr uint32_t superclass_id = ColumnType::superclass_id::default_flags();
    static constexpr uint32_t classloader_id = ColumnType::classloader_id::default_flags();
    static constexpr uint32_t kind = ColumnType::kind::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      HeapGraphClassTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    HeapGraphClassTable, RowNumber> {
   public:
    ConstRowReference(const HeapGraphClassTable* table, uint32_t row_number)
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
    ColumnType::deobfuscated_name::type deobfuscated_name() const {
      return table_->deobfuscated_name()[row_number_];
    }
    ColumnType::location::type location() const {
      return table_->location()[row_number_];
    }
    ColumnType::superclass_id::type superclass_id() const {
      return table_->superclass_id()[row_number_];
    }
    ColumnType::classloader_id::type classloader_id() const {
      return table_->classloader_id()[row_number_];
    }
    ColumnType::kind::type kind() const {
      return table_->kind()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const HeapGraphClassTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_deobfuscated_name(
        ColumnType::deobfuscated_name::non_optional_type v) {
      return mutable_table()->mutable_deobfuscated_name()->Set(row_number_, v);
    }
    void set_location(
        ColumnType::location::non_optional_type v) {
      return mutable_table()->mutable_location()->Set(row_number_, v);
    }
    void set_superclass_id(
        ColumnType::superclass_id::non_optional_type v) {
      return mutable_table()->mutable_superclass_id()->Set(row_number_, v);
    }
    void set_classloader_id(
        ColumnType::classloader_id::non_optional_type v) {
      return mutable_table()->mutable_classloader_id()->Set(row_number_, v);
    }
    void set_kind(
        ColumnType::kind::non_optional_type v) {
      return mutable_table()->mutable_kind()->Set(row_number_, v);
    }

   private:
    HeapGraphClassTable* mutable_table() const {
      return const_cast<HeapGraphClassTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, HeapGraphClassTable, RowNumber, ConstRowReference> {
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
    ColumnType::deobfuscated_name::type deobfuscated_name() const {
      const auto& col = table_->deobfuscated_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::location::type location() const {
      const auto& col = table_->location();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::superclass_id::type superclass_id() const {
      const auto& col = table_->superclass_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::classloader_id::type classloader_id() const {
      const auto& col = table_->classloader_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::kind::type kind() const {
      const auto& col = table_->kind();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const HeapGraphClassTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class HeapGraphClassTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, HeapGraphClassTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class HeapGraphClassTable;

     explicit Iterator(HeapGraphClassTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     HeapGraphClassTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      HeapGraphClassTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "deobfuscated_name", &self->deobfuscated_name_, ColumnFlag::deobfuscated_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "location", &self->location_, ColumnFlag::location,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "superclass_id", &self->superclass_id_, ColumnFlag::superclass_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "classloader_id", &self->classloader_id_, ColumnFlag::classloader_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "kind", &self->kind_, ColumnFlag::kind,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit HeapGraphClassTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        deobfuscated_name_(ColumnStorage<ColumnType::deobfuscated_name::stored_type>::Create<false>()),
        location_(ColumnStorage<ColumnType::location::stored_type>::Create<false>()),
        superclass_id_(ColumnStorage<ColumnType::superclass_id::stored_type>::Create<false>()),
        classloader_id_(ColumnStorage<ColumnType::classloader_id::stored_type>::Create<false>()),
        kind_(ColumnStorage<ColumnType::kind::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        deobfuscated_name_storage_layer_(
          new column::StringStorage(string_pool(), &deobfuscated_name_.vector())),
        location_storage_layer_(
          new column::StringStorage(string_pool(), &location_.vector())),
        superclass_id_storage_layer_(
          new column::NumericStorage<ColumnType::superclass_id::non_optional_stored_type>(
            &superclass_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::superclass_id::stored_type>::ToColumnType(),
            false)),
        classloader_id_storage_layer_(
          new column::NumericStorage<ColumnType::classloader_id::non_optional_stored_type>(
            &classloader_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::classloader_id::stored_type>::ToColumnType(),
            false)),
        kind_storage_layer_(
          new column::StringStorage(string_pool(), &kind_.vector()))
,
        superclass_id_null_layer_(new column::NullOverlay(superclass_id_.bv())),
        classloader_id_null_layer_(new column::NullOverlay(classloader_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::deobfuscated_name::stored_type>(
          ColumnFlag::deobfuscated_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::location::stored_type>(
          ColumnFlag::location),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::superclass_id::stored_type>(
          ColumnFlag::superclass_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::classloader_id::stored_type>(
          ColumnFlag::classloader_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::kind::stored_type>(
          ColumnFlag::kind),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,name_storage_layer_,deobfuscated_name_storage_layer_,location_storage_layer_,superclass_id_storage_layer_,classloader_id_storage_layer_,kind_storage_layer_},
      {{},{},{},{},{},superclass_id_null_layer_,classloader_id_null_layer_,{}});
  }
  ~HeapGraphClassTable() override;

  static const char* Name() { return "heap_graph_class"; }

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
        "deobfuscated_name", ColumnType::deobfuscated_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "location", ColumnType::location::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "superclass_id", ColumnType::superclass_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "classloader_id", ColumnType::classloader_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "kind", ColumnType::kind::SqlValueType(), false,
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
    deobfuscated_name_.ShrinkToFit();
    location_.ShrinkToFit();
    superclass_id_.ShrinkToFit();
    classloader_id_.ShrinkToFit();
    kind_.ShrinkToFit();
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
    mutable_deobfuscated_name()->Append(row.deobfuscated_name);
    mutable_location()->Append(row.location);
    mutable_superclass_id()->Append(row.superclass_id);
    mutable_classloader_id()->Append(row.classloader_id);
    mutable_kind()->Append(row.kind);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<HeapGraphClassTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& deobfuscated_name() const {
    return static_cast<const ColumnType::deobfuscated_name&>(columns()[ColumnIndex::deobfuscated_name]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& location() const {
    return static_cast<const ColumnType::location&>(columns()[ColumnIndex::location]);
  }
  const TypedColumn<std::optional<HeapGraphClassTable::Id>>& superclass_id() const {
    return static_cast<const ColumnType::superclass_id&>(columns()[ColumnIndex::superclass_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& classloader_id() const {
    return static_cast<const ColumnType::classloader_id&>(columns()[ColumnIndex::classloader_id]);
  }
  const TypedColumn<StringPool::Id>& kind() const {
    return static_cast<const ColumnType::kind&>(columns()[ColumnIndex::kind]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_deobfuscated_name() {
    return static_cast<ColumnType::deobfuscated_name*>(
        GetColumn(ColumnIndex::deobfuscated_name));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_location() {
    return static_cast<ColumnType::location*>(
        GetColumn(ColumnIndex::location));
  }
  TypedColumn<std::optional<HeapGraphClassTable::Id>>* mutable_superclass_id() {
    return static_cast<ColumnType::superclass_id*>(
        GetColumn(ColumnIndex::superclass_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_classloader_id() {
    return static_cast<ColumnType::classloader_id*>(
        GetColumn(ColumnIndex::classloader_id));
  }
  TypedColumn<StringPool::Id>* mutable_kind() {
    return static_cast<ColumnType::kind*>(
        GetColumn(ColumnIndex::kind));
  }

 private:
  
  
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::deobfuscated_name::stored_type> deobfuscated_name_;
  ColumnStorage<ColumnType::location::stored_type> location_;
  ColumnStorage<ColumnType::superclass_id::stored_type> superclass_id_;
  ColumnStorage<ColumnType::classloader_id::stored_type> classloader_id_;
  ColumnStorage<ColumnType::kind::stored_type> kind_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> deobfuscated_name_storage_layer_;
  RefPtr<column::StorageLayer> location_storage_layer_;
  RefPtr<column::StorageLayer> superclass_id_storage_layer_;
  RefPtr<column::StorageLayer> classloader_id_storage_layer_;
  RefPtr<column::StorageLayer> kind_storage_layer_;

  RefPtr<column::OverlayLayer> superclass_id_null_layer_;
  RefPtr<column::OverlayLayer> classloader_id_null_layer_;
};
  

class HeapGraphObjectTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 11;

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
    static constexpr uint32_t graph_sample_ts = 3;
    static constexpr uint32_t self_size = 4;
    static constexpr uint32_t native_size = 5;
    static constexpr uint32_t reference_set_id = 6;
    static constexpr uint32_t reachable = 7;
    static constexpr uint32_t type_id = 8;
    static constexpr uint32_t root_type = 9;
    static constexpr uint32_t root_distance = 10;
  };
  struct ColumnType {
    using id = IdColumn<HeapGraphObjectTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using upid = TypedColumn<uint32_t>;
    using graph_sample_ts = TypedColumn<int64_t>;
    using self_size = TypedColumn<int64_t>;
    using native_size = TypedColumn<int64_t>;
    using reference_set_id = TypedColumn<std::optional<uint32_t>>;
    using reachable = TypedColumn<int32_t>;
    using type_id = TypedColumn<HeapGraphClassTable::Id>;
    using root_type = TypedColumn<std::optional<StringPool::Id>>;
    using root_distance = TypedColumn<int32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_upid = {},
        int64_t in_graph_sample_ts = {},
        int64_t in_self_size = {},
        int64_t in_native_size = {},
        std::optional<uint32_t> in_reference_set_id = {},
        int32_t in_reachable = {},
        HeapGraphClassTable::Id in_type_id = {},
        std::optional<StringPool::Id> in_root_type = {},
        int32_t in_root_distance = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          upid(in_upid),
          graph_sample_ts(in_graph_sample_ts),
          self_size(in_self_size),
          native_size(in_native_size),
          reference_set_id(in_reference_set_id),
          reachable(in_reachable),
          type_id(in_type_id),
          root_type(in_root_type),
          root_distance(in_root_distance) {
      type_ = "heap_graph_object";
    }
    uint32_t upid;
    int64_t graph_sample_ts;
    int64_t self_size;
    int64_t native_size;
    std::optional<uint32_t> reference_set_id;
    int32_t reachable;
    HeapGraphClassTable::Id type_id;
    std::optional<StringPool::Id> root_type;
    int32_t root_distance;

    bool operator==(const HeapGraphObjectTable::Row& other) const {
      return type() == other.type() && ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::graph_sample_ts::Equals(graph_sample_ts, other.graph_sample_ts) &&
       ColumnType::self_size::Equals(self_size, other.self_size) &&
       ColumnType::native_size::Equals(native_size, other.native_size) &&
       ColumnType::reference_set_id::Equals(reference_set_id, other.reference_set_id) &&
       ColumnType::reachable::Equals(reachable, other.reachable) &&
       ColumnType::type_id::Equals(type_id, other.type_id) &&
       ColumnType::root_type::Equals(root_type, other.root_type) &&
       ColumnType::root_distance::Equals(root_distance, other.root_distance);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
    static constexpr uint32_t graph_sample_ts = ColumnType::graph_sample_ts::default_flags();
    static constexpr uint32_t self_size = ColumnType::self_size::default_flags();
    static constexpr uint32_t native_size = ColumnType::native_size::default_flags();
    static constexpr uint32_t reference_set_id = static_cast<uint32_t>(ColumnLegacy::Flag::kDense) | ColumnType::reference_set_id::default_flags();
    static constexpr uint32_t reachable = ColumnType::reachable::default_flags();
    static constexpr uint32_t type_id = ColumnType::type_id::default_flags();
    static constexpr uint32_t root_type = ColumnType::root_type::default_flags();
    static constexpr uint32_t root_distance = static_cast<uint32_t>(ColumnLegacy::Flag::kHidden) | ColumnType::root_distance::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      HeapGraphObjectTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    HeapGraphObjectTable, RowNumber> {
   public:
    ConstRowReference(const HeapGraphObjectTable* table, uint32_t row_number)
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
    ColumnType::graph_sample_ts::type graph_sample_ts() const {
      return table_->graph_sample_ts()[row_number_];
    }
    ColumnType::self_size::type self_size() const {
      return table_->self_size()[row_number_];
    }
    ColumnType::native_size::type native_size() const {
      return table_->native_size()[row_number_];
    }
    ColumnType::reference_set_id::type reference_set_id() const {
      return table_->reference_set_id()[row_number_];
    }
    ColumnType::reachable::type reachable() const {
      return table_->reachable()[row_number_];
    }
    ColumnType::type_id::type type_id() const {
      return table_->type_id()[row_number_];
    }
    ColumnType::root_type::type root_type() const {
      return table_->root_type()[row_number_];
    }
    ColumnType::root_distance::type root_distance() const {
      return table_->root_distance()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const HeapGraphObjectTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_graph_sample_ts(
        ColumnType::graph_sample_ts::non_optional_type v) {
      return mutable_table()->mutable_graph_sample_ts()->Set(row_number_, v);
    }
    void set_self_size(
        ColumnType::self_size::non_optional_type v) {
      return mutable_table()->mutable_self_size()->Set(row_number_, v);
    }
    void set_native_size(
        ColumnType::native_size::non_optional_type v) {
      return mutable_table()->mutable_native_size()->Set(row_number_, v);
    }
    void set_reference_set_id(
        ColumnType::reference_set_id::non_optional_type v) {
      return mutable_table()->mutable_reference_set_id()->Set(row_number_, v);
    }
    void set_reachable(
        ColumnType::reachable::non_optional_type v) {
      return mutable_table()->mutable_reachable()->Set(row_number_, v);
    }
    void set_type_id(
        ColumnType::type_id::non_optional_type v) {
      return mutable_table()->mutable_type_id()->Set(row_number_, v);
    }
    void set_root_type(
        ColumnType::root_type::non_optional_type v) {
      return mutable_table()->mutable_root_type()->Set(row_number_, v);
    }
    void set_root_distance(
        ColumnType::root_distance::non_optional_type v) {
      return mutable_table()->mutable_root_distance()->Set(row_number_, v);
    }

   private:
    HeapGraphObjectTable* mutable_table() const {
      return const_cast<HeapGraphObjectTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, HeapGraphObjectTable, RowNumber, ConstRowReference> {
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
    ColumnType::graph_sample_ts::type graph_sample_ts() const {
      const auto& col = table_->graph_sample_ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::self_size::type self_size() const {
      const auto& col = table_->self_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::native_size::type native_size() const {
      const auto& col = table_->native_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::reference_set_id::type reference_set_id() const {
      const auto& col = table_->reference_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::reachable::type reachable() const {
      const auto& col = table_->reachable();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::type_id::type type_id() const {
      const auto& col = table_->type_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::root_type::type root_type() const {
      const auto& col = table_->root_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::root_distance::type root_distance() const {
      const auto& col = table_->root_distance();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const HeapGraphObjectTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class HeapGraphObjectTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, HeapGraphObjectTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class HeapGraphObjectTable;

     explicit Iterator(HeapGraphObjectTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     HeapGraphObjectTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      HeapGraphObjectTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "graph_sample_ts", &self->graph_sample_ts_, ColumnFlag::graph_sample_ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "self_size", &self->self_size_, ColumnFlag::self_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "native_size", &self->native_size_, ColumnFlag::native_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "reference_set_id", &self->reference_set_id_, ColumnFlag::reference_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "reachable", &self->reachable_, ColumnFlag::reachable,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "type_id", &self->type_id_, ColumnFlag::type_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "root_type", &self->root_type_, ColumnFlag::root_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "root_distance", &self->root_distance_, ColumnFlag::root_distance,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit HeapGraphObjectTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        graph_sample_ts_(ColumnStorage<ColumnType::graph_sample_ts::stored_type>::Create<false>()),
        self_size_(ColumnStorage<ColumnType::self_size::stored_type>::Create<false>()),
        native_size_(ColumnStorage<ColumnType::native_size::stored_type>::Create<false>()),
        reference_set_id_(ColumnStorage<ColumnType::reference_set_id::stored_type>::Create<true>()),
        reachable_(ColumnStorage<ColumnType::reachable::stored_type>::Create<false>()),
        type_id_(ColumnStorage<ColumnType::type_id::stored_type>::Create<false>()),
        root_type_(ColumnStorage<ColumnType::root_type::stored_type>::Create<false>()),
        root_distance_(ColumnStorage<ColumnType::root_distance::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false)),
        graph_sample_ts_storage_layer_(
        new column::NumericStorage<ColumnType::graph_sample_ts::non_optional_stored_type>(
          &graph_sample_ts_.vector(),
          ColumnTypeHelper<ColumnType::graph_sample_ts::stored_type>::ToColumnType(),
          false)),
        self_size_storage_layer_(
        new column::NumericStorage<ColumnType::self_size::non_optional_stored_type>(
          &self_size_.vector(),
          ColumnTypeHelper<ColumnType::self_size::stored_type>::ToColumnType(),
          false)),
        native_size_storage_layer_(
        new column::NumericStorage<ColumnType::native_size::non_optional_stored_type>(
          &native_size_.vector(),
          ColumnTypeHelper<ColumnType::native_size::stored_type>::ToColumnType(),
          false)),
        reference_set_id_storage_layer_(
          new column::NumericStorage<ColumnType::reference_set_id::non_optional_stored_type>(
            &reference_set_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::reference_set_id::stored_type>::ToColumnType(),
            false)),
        reachable_storage_layer_(
        new column::NumericStorage<ColumnType::reachable::non_optional_stored_type>(
          &reachable_.vector(),
          ColumnTypeHelper<ColumnType::reachable::stored_type>::ToColumnType(),
          false)),
        type_id_storage_layer_(
        new column::NumericStorage<ColumnType::type_id::non_optional_stored_type>(
          &type_id_.vector(),
          ColumnTypeHelper<ColumnType::type_id::stored_type>::ToColumnType(),
          false)),
        root_type_storage_layer_(
          new column::StringStorage(string_pool(), &root_type_.vector())),
        root_distance_storage_layer_(
        new column::NumericStorage<ColumnType::root_distance::non_optional_stored_type>(
          &root_distance_.vector(),
          ColumnTypeHelper<ColumnType::root_distance::stored_type>::ToColumnType(),
          false))
,
        reference_set_id_null_layer_(new column::DenseNullOverlay(reference_set_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::graph_sample_ts::stored_type>(
          ColumnFlag::graph_sample_ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::self_size::stored_type>(
          ColumnFlag::self_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::native_size::stored_type>(
          ColumnFlag::native_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::reference_set_id::stored_type>(
          ColumnFlag::reference_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::reachable::stored_type>(
          ColumnFlag::reachable),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::type_id::stored_type>(
          ColumnFlag::type_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::root_type::stored_type>(
          ColumnFlag::root_type),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::root_distance::stored_type>(
          ColumnFlag::root_distance),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,upid_storage_layer_,graph_sample_ts_storage_layer_,self_size_storage_layer_,native_size_storage_layer_,reference_set_id_storage_layer_,reachable_storage_layer_,type_id_storage_layer_,root_type_storage_layer_,root_distance_storage_layer_},
      {{},{},{},{},{},{},reference_set_id_null_layer_,{},{},{},{}});
  }
  ~HeapGraphObjectTable() override;

  static const char* Name() { return "heap_graph_object"; }

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
        "graph_sample_ts", ColumnType::graph_sample_ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "self_size", ColumnType::self_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "native_size", ColumnType::native_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "reference_set_id", ColumnType::reference_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "reachable", ColumnType::reachable::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type_id", ColumnType::type_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "root_type", ColumnType::root_type::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "root_distance", ColumnType::root_distance::SqlValueType(), false,
        false,
        true,
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
    graph_sample_ts_.ShrinkToFit();
    self_size_.ShrinkToFit();
    native_size_.ShrinkToFit();
    reference_set_id_.ShrinkToFit();
    reachable_.ShrinkToFit();
    type_id_.ShrinkToFit();
    root_type_.ShrinkToFit();
    root_distance_.ShrinkToFit();
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
    mutable_graph_sample_ts()->Append(row.graph_sample_ts);
    mutable_self_size()->Append(row.self_size);
    mutable_native_size()->Append(row.native_size);
    mutable_reference_set_id()->Append(row.reference_set_id);
    mutable_reachable()->Append(row.reachable);
    mutable_type_id()->Append(row.type_id);
    mutable_root_type()->Append(row.root_type);
    mutable_root_distance()->Append(row.root_distance);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<HeapGraphObjectTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<int64_t>& graph_sample_ts() const {
    return static_cast<const ColumnType::graph_sample_ts&>(columns()[ColumnIndex::graph_sample_ts]);
  }
  const TypedColumn<int64_t>& self_size() const {
    return static_cast<const ColumnType::self_size&>(columns()[ColumnIndex::self_size]);
  }
  const TypedColumn<int64_t>& native_size() const {
    return static_cast<const ColumnType::native_size&>(columns()[ColumnIndex::native_size]);
  }
  const TypedColumn<std::optional<uint32_t>>& reference_set_id() const {
    return static_cast<const ColumnType::reference_set_id&>(columns()[ColumnIndex::reference_set_id]);
  }
  const TypedColumn<int32_t>& reachable() const {
    return static_cast<const ColumnType::reachable&>(columns()[ColumnIndex::reachable]);
  }
  const TypedColumn<HeapGraphClassTable::Id>& type_id() const {
    return static_cast<const ColumnType::type_id&>(columns()[ColumnIndex::type_id]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& root_type() const {
    return static_cast<const ColumnType::root_type&>(columns()[ColumnIndex::root_type]);
  }
  const TypedColumn<int32_t>& root_distance() const {
    return static_cast<const ColumnType::root_distance&>(columns()[ColumnIndex::root_distance]);
  }

  TypedColumn<uint32_t>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<int64_t>* mutable_graph_sample_ts() {
    return static_cast<ColumnType::graph_sample_ts*>(
        GetColumn(ColumnIndex::graph_sample_ts));
  }
  TypedColumn<int64_t>* mutable_self_size() {
    return static_cast<ColumnType::self_size*>(
        GetColumn(ColumnIndex::self_size));
  }
  TypedColumn<int64_t>* mutable_native_size() {
    return static_cast<ColumnType::native_size*>(
        GetColumn(ColumnIndex::native_size));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_reference_set_id() {
    return static_cast<ColumnType::reference_set_id*>(
        GetColumn(ColumnIndex::reference_set_id));
  }
  TypedColumn<int32_t>* mutable_reachable() {
    return static_cast<ColumnType::reachable*>(
        GetColumn(ColumnIndex::reachable));
  }
  TypedColumn<HeapGraphClassTable::Id>* mutable_type_id() {
    return static_cast<ColumnType::type_id*>(
        GetColumn(ColumnIndex::type_id));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_root_type() {
    return static_cast<ColumnType::root_type*>(
        GetColumn(ColumnIndex::root_type));
  }
  TypedColumn<int32_t>* mutable_root_distance() {
    return static_cast<ColumnType::root_distance*>(
        GetColumn(ColumnIndex::root_distance));
  }

 private:
  
  
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::graph_sample_ts::stored_type> graph_sample_ts_;
  ColumnStorage<ColumnType::self_size::stored_type> self_size_;
  ColumnStorage<ColumnType::native_size::stored_type> native_size_;
  ColumnStorage<ColumnType::reference_set_id::stored_type> reference_set_id_;
  ColumnStorage<ColumnType::reachable::stored_type> reachable_;
  ColumnStorage<ColumnType::type_id::stored_type> type_id_;
  ColumnStorage<ColumnType::root_type::stored_type> root_type_;
  ColumnStorage<ColumnType::root_distance::stored_type> root_distance_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> graph_sample_ts_storage_layer_;
  RefPtr<column::StorageLayer> self_size_storage_layer_;
  RefPtr<column::StorageLayer> native_size_storage_layer_;
  RefPtr<column::StorageLayer> reference_set_id_storage_layer_;
  RefPtr<column::StorageLayer> reachable_storage_layer_;
  RefPtr<column::StorageLayer> type_id_storage_layer_;
  RefPtr<column::StorageLayer> root_type_storage_layer_;
  RefPtr<column::StorageLayer> root_distance_storage_layer_;

  RefPtr<column::OverlayLayer> reference_set_id_null_layer_;
};
  

class HeapGraphReferenceTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t reference_set_id = 2;
    static constexpr uint32_t owner_id = 3;
    static constexpr uint32_t owned_id = 4;
    static constexpr uint32_t field_name = 5;
    static constexpr uint32_t field_type_name = 6;
    static constexpr uint32_t deobfuscated_field_name = 7;
  };
  struct ColumnType {
    using id = IdColumn<HeapGraphReferenceTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using reference_set_id = TypedColumn<uint32_t>;
    using owner_id = TypedColumn<HeapGraphObjectTable::Id>;
    using owned_id = TypedColumn<std::optional<HeapGraphObjectTable::Id>>;
    using field_name = TypedColumn<StringPool::Id>;
    using field_type_name = TypedColumn<StringPool::Id>;
    using deobfuscated_field_name = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_reference_set_id = {},
        HeapGraphObjectTable::Id in_owner_id = {},
        std::optional<HeapGraphObjectTable::Id> in_owned_id = {},
        StringPool::Id in_field_name = {},
        StringPool::Id in_field_type_name = {},
        std::optional<StringPool::Id> in_deobfuscated_field_name = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          reference_set_id(in_reference_set_id),
          owner_id(in_owner_id),
          owned_id(in_owned_id),
          field_name(in_field_name),
          field_type_name(in_field_type_name),
          deobfuscated_field_name(in_deobfuscated_field_name) {
      type_ = "heap_graph_reference";
    }
    uint32_t reference_set_id;
    HeapGraphObjectTable::Id owner_id;
    std::optional<HeapGraphObjectTable::Id> owned_id;
    StringPool::Id field_name;
    StringPool::Id field_type_name;
    std::optional<StringPool::Id> deobfuscated_field_name;

    bool operator==(const HeapGraphReferenceTable::Row& other) const {
      return type() == other.type() && ColumnType::reference_set_id::Equals(reference_set_id, other.reference_set_id) &&
       ColumnType::owner_id::Equals(owner_id, other.owner_id) &&
       ColumnType::owned_id::Equals(owned_id, other.owned_id) &&
       ColumnType::field_name::Equals(field_name, other.field_name) &&
       ColumnType::field_type_name::Equals(field_type_name, other.field_type_name) &&
       ColumnType::deobfuscated_field_name::Equals(deobfuscated_field_name, other.deobfuscated_field_name);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t reference_set_id = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted | ColumnLegacy::Flag::kSetId) | ColumnType::reference_set_id::default_flags();
    static constexpr uint32_t owner_id = ColumnType::owner_id::default_flags();
    static constexpr uint32_t owned_id = ColumnType::owned_id::default_flags();
    static constexpr uint32_t field_name = ColumnType::field_name::default_flags();
    static constexpr uint32_t field_type_name = ColumnType::field_type_name::default_flags();
    static constexpr uint32_t deobfuscated_field_name = ColumnType::deobfuscated_field_name::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      HeapGraphReferenceTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    HeapGraphReferenceTable, RowNumber> {
   public:
    ConstRowReference(const HeapGraphReferenceTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::reference_set_id::type reference_set_id() const {
      return table_->reference_set_id()[row_number_];
    }
    ColumnType::owner_id::type owner_id() const {
      return table_->owner_id()[row_number_];
    }
    ColumnType::owned_id::type owned_id() const {
      return table_->owned_id()[row_number_];
    }
    ColumnType::field_name::type field_name() const {
      return table_->field_name()[row_number_];
    }
    ColumnType::field_type_name::type field_type_name() const {
      return table_->field_type_name()[row_number_];
    }
    ColumnType::deobfuscated_field_name::type deobfuscated_field_name() const {
      return table_->deobfuscated_field_name()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const HeapGraphReferenceTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_reference_set_id(
        ColumnType::reference_set_id::non_optional_type v) {
      return mutable_table()->mutable_reference_set_id()->Set(row_number_, v);
    }
    void set_owner_id(
        ColumnType::owner_id::non_optional_type v) {
      return mutable_table()->mutable_owner_id()->Set(row_number_, v);
    }
    void set_owned_id(
        ColumnType::owned_id::non_optional_type v) {
      return mutable_table()->mutable_owned_id()->Set(row_number_, v);
    }
    void set_field_name(
        ColumnType::field_name::non_optional_type v) {
      return mutable_table()->mutable_field_name()->Set(row_number_, v);
    }
    void set_field_type_name(
        ColumnType::field_type_name::non_optional_type v) {
      return mutable_table()->mutable_field_type_name()->Set(row_number_, v);
    }
    void set_deobfuscated_field_name(
        ColumnType::deobfuscated_field_name::non_optional_type v) {
      return mutable_table()->mutable_deobfuscated_field_name()->Set(row_number_, v);
    }

   private:
    HeapGraphReferenceTable* mutable_table() const {
      return const_cast<HeapGraphReferenceTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, HeapGraphReferenceTable, RowNumber, ConstRowReference> {
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
    ColumnType::reference_set_id::type reference_set_id() const {
      const auto& col = table_->reference_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::owner_id::type owner_id() const {
      const auto& col = table_->owner_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::owned_id::type owned_id() const {
      const auto& col = table_->owned_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::field_name::type field_name() const {
      const auto& col = table_->field_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::field_type_name::type field_type_name() const {
      const auto& col = table_->field_type_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::deobfuscated_field_name::type deobfuscated_field_name() const {
      const auto& col = table_->deobfuscated_field_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const HeapGraphReferenceTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class HeapGraphReferenceTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, HeapGraphReferenceTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class HeapGraphReferenceTable;

     explicit Iterator(HeapGraphReferenceTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     HeapGraphReferenceTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      HeapGraphReferenceTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "reference_set_id", &self->reference_set_id_, ColumnFlag::reference_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "owner_id", &self->owner_id_, ColumnFlag::owner_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "owned_id", &self->owned_id_, ColumnFlag::owned_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "field_name", &self->field_name_, ColumnFlag::field_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "field_type_name", &self->field_type_name_, ColumnFlag::field_type_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "deobfuscated_field_name", &self->deobfuscated_field_name_, ColumnFlag::deobfuscated_field_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit HeapGraphReferenceTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        reference_set_id_(ColumnStorage<ColumnType::reference_set_id::stored_type>::Create<false>()),
        owner_id_(ColumnStorage<ColumnType::owner_id::stored_type>::Create<false>()),
        owned_id_(ColumnStorage<ColumnType::owned_id::stored_type>::Create<false>()),
        field_name_(ColumnStorage<ColumnType::field_name::stored_type>::Create<false>()),
        field_type_name_(ColumnStorage<ColumnType::field_type_name::stored_type>::Create<false>()),
        deobfuscated_field_name_(ColumnStorage<ColumnType::deobfuscated_field_name::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        reference_set_id_storage_layer_(
          new column::SetIdStorage(&reference_set_id_.vector())),
        owner_id_storage_layer_(
        new column::NumericStorage<ColumnType::owner_id::non_optional_stored_type>(
          &owner_id_.vector(),
          ColumnTypeHelper<ColumnType::owner_id::stored_type>::ToColumnType(),
          false)),
        owned_id_storage_layer_(
          new column::NumericStorage<ColumnType::owned_id::non_optional_stored_type>(
            &owned_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::owned_id::stored_type>::ToColumnType(),
            false)),
        field_name_storage_layer_(
          new column::StringStorage(string_pool(), &field_name_.vector())),
        field_type_name_storage_layer_(
          new column::StringStorage(string_pool(), &field_type_name_.vector())),
        deobfuscated_field_name_storage_layer_(
          new column::StringStorage(string_pool(), &deobfuscated_field_name_.vector()))
,
        owned_id_null_layer_(new column::NullOverlay(owned_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::reference_set_id::stored_type>(
          ColumnFlag::reference_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::owner_id::stored_type>(
          ColumnFlag::owner_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::owned_id::stored_type>(
          ColumnFlag::owned_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::field_name::stored_type>(
          ColumnFlag::field_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::field_type_name::stored_type>(
          ColumnFlag::field_type_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::deobfuscated_field_name::stored_type>(
          ColumnFlag::deobfuscated_field_name),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,reference_set_id_storage_layer_,owner_id_storage_layer_,owned_id_storage_layer_,field_name_storage_layer_,field_type_name_storage_layer_,deobfuscated_field_name_storage_layer_},
      {{},{},{},{},owned_id_null_layer_,{},{},{}});
  }
  ~HeapGraphReferenceTable() override;

  static const char* Name() { return "heap_graph_reference"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "reference_set_id", ColumnType::reference_set_id::SqlValueType(), false,
        true,
        false,
        true});
    schema.columns.emplace_back(Table::Schema::Column{
        "owner_id", ColumnType::owner_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "owned_id", ColumnType::owned_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "field_name", ColumnType::field_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "field_type_name", ColumnType::field_type_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "deobfuscated_field_name", ColumnType::deobfuscated_field_name::SqlValueType(), false,
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
    reference_set_id_.ShrinkToFit();
    owner_id_.ShrinkToFit();
    owned_id_.ShrinkToFit();
    field_name_.ShrinkToFit();
    field_type_name_.ShrinkToFit();
    deobfuscated_field_name_.ShrinkToFit();
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
    mutable_reference_set_id()->Append(row.reference_set_id);
    mutable_owner_id()->Append(row.owner_id);
    mutable_owned_id()->Append(row.owned_id);
    mutable_field_name()->Append(row.field_name);
    mutable_field_type_name()->Append(row.field_type_name);
    mutable_deobfuscated_field_name()->Append(row.deobfuscated_field_name);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<HeapGraphReferenceTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& reference_set_id() const {
    return static_cast<const ColumnType::reference_set_id&>(columns()[ColumnIndex::reference_set_id]);
  }
  const TypedColumn<HeapGraphObjectTable::Id>& owner_id() const {
    return static_cast<const ColumnType::owner_id&>(columns()[ColumnIndex::owner_id]);
  }
  const TypedColumn<std::optional<HeapGraphObjectTable::Id>>& owned_id() const {
    return static_cast<const ColumnType::owned_id&>(columns()[ColumnIndex::owned_id]);
  }
  const TypedColumn<StringPool::Id>& field_name() const {
    return static_cast<const ColumnType::field_name&>(columns()[ColumnIndex::field_name]);
  }
  const TypedColumn<StringPool::Id>& field_type_name() const {
    return static_cast<const ColumnType::field_type_name&>(columns()[ColumnIndex::field_type_name]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& deobfuscated_field_name() const {
    return static_cast<const ColumnType::deobfuscated_field_name&>(columns()[ColumnIndex::deobfuscated_field_name]);
  }

  TypedColumn<uint32_t>* mutable_reference_set_id() {
    return static_cast<ColumnType::reference_set_id*>(
        GetColumn(ColumnIndex::reference_set_id));
  }
  TypedColumn<HeapGraphObjectTable::Id>* mutable_owner_id() {
    return static_cast<ColumnType::owner_id*>(
        GetColumn(ColumnIndex::owner_id));
  }
  TypedColumn<std::optional<HeapGraphObjectTable::Id>>* mutable_owned_id() {
    return static_cast<ColumnType::owned_id*>(
        GetColumn(ColumnIndex::owned_id));
  }
  TypedColumn<StringPool::Id>* mutable_field_name() {
    return static_cast<ColumnType::field_name*>(
        GetColumn(ColumnIndex::field_name));
  }
  TypedColumn<StringPool::Id>* mutable_field_type_name() {
    return static_cast<ColumnType::field_type_name*>(
        GetColumn(ColumnIndex::field_type_name));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_deobfuscated_field_name() {
    return static_cast<ColumnType::deobfuscated_field_name*>(
        GetColumn(ColumnIndex::deobfuscated_field_name));
  }

 private:
  
  
  ColumnStorage<ColumnType::reference_set_id::stored_type> reference_set_id_;
  ColumnStorage<ColumnType::owner_id::stored_type> owner_id_;
  ColumnStorage<ColumnType::owned_id::stored_type> owned_id_;
  ColumnStorage<ColumnType::field_name::stored_type> field_name_;
  ColumnStorage<ColumnType::field_type_name::stored_type> field_type_name_;
  ColumnStorage<ColumnType::deobfuscated_field_name::stored_type> deobfuscated_field_name_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> reference_set_id_storage_layer_;
  RefPtr<column::StorageLayer> owner_id_storage_layer_;
  RefPtr<column::StorageLayer> owned_id_storage_layer_;
  RefPtr<column::StorageLayer> field_name_storage_layer_;
  RefPtr<column::StorageLayer> field_type_name_storage_layer_;
  RefPtr<column::StorageLayer> deobfuscated_field_name_storage_layer_;

  RefPtr<column::OverlayLayer> owned_id_null_layer_;
};
  

class HeapProfileAllocationTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t upid = 3;
    static constexpr uint32_t heap_name = 4;
    static constexpr uint32_t callsite_id = 5;
    static constexpr uint32_t count = 6;
    static constexpr uint32_t size = 7;
  };
  struct ColumnType {
    using id = IdColumn<HeapProfileAllocationTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using upid = TypedColumn<uint32_t>;
    using heap_name = TypedColumn<StringPool::Id>;
    using callsite_id = TypedColumn<StackProfileCallsiteTable::Id>;
    using count = TypedColumn<int64_t>;
    using size = TypedColumn<int64_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        uint32_t in_upid = {},
        StringPool::Id in_heap_name = {},
        StackProfileCallsiteTable::Id in_callsite_id = {},
        int64_t in_count = {},
        int64_t in_size = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          upid(in_upid),
          heap_name(in_heap_name),
          callsite_id(in_callsite_id),
          count(in_count),
          size(in_size) {
      type_ = "heap_profile_allocation";
    }
    int64_t ts;
    uint32_t upid;
    StringPool::Id heap_name;
    StackProfileCallsiteTable::Id callsite_id;
    int64_t count;
    int64_t size;

    bool operator==(const HeapProfileAllocationTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::heap_name::Equals(heap_name, other.heap_name) &&
       ColumnType::callsite_id::Equals(callsite_id, other.callsite_id) &&
       ColumnType::count::Equals(count, other.count) &&
       ColumnType::size::Equals(size, other.size);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = ColumnType::ts::default_flags();
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
    static constexpr uint32_t heap_name = ColumnType::heap_name::default_flags();
    static constexpr uint32_t callsite_id = ColumnType::callsite_id::default_flags();
    static constexpr uint32_t count = ColumnType::count::default_flags();
    static constexpr uint32_t size = ColumnType::size::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      HeapProfileAllocationTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    HeapProfileAllocationTable, RowNumber> {
   public:
    ConstRowReference(const HeapProfileAllocationTable* table, uint32_t row_number)
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
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
    ColumnType::heap_name::type heap_name() const {
      return table_->heap_name()[row_number_];
    }
    ColumnType::callsite_id::type callsite_id() const {
      return table_->callsite_id()[row_number_];
    }
    ColumnType::count::type count() const {
      return table_->count()[row_number_];
    }
    ColumnType::size::type size() const {
      return table_->size()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const HeapProfileAllocationTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_heap_name(
        ColumnType::heap_name::non_optional_type v) {
      return mutable_table()->mutable_heap_name()->Set(row_number_, v);
    }
    void set_callsite_id(
        ColumnType::callsite_id::non_optional_type v) {
      return mutable_table()->mutable_callsite_id()->Set(row_number_, v);
    }
    void set_count(
        ColumnType::count::non_optional_type v) {
      return mutable_table()->mutable_count()->Set(row_number_, v);
    }
    void set_size(
        ColumnType::size::non_optional_type v) {
      return mutable_table()->mutable_size()->Set(row_number_, v);
    }

   private:
    HeapProfileAllocationTable* mutable_table() const {
      return const_cast<HeapProfileAllocationTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, HeapProfileAllocationTable, RowNumber, ConstRowReference> {
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
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::heap_name::type heap_name() const {
      const auto& col = table_->heap_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::callsite_id::type callsite_id() const {
      const auto& col = table_->callsite_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::count::type count() const {
      const auto& col = table_->count();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::size::type size() const {
      const auto& col = table_->size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const HeapProfileAllocationTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class HeapProfileAllocationTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, HeapProfileAllocationTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class HeapProfileAllocationTable;

     explicit Iterator(HeapProfileAllocationTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     HeapProfileAllocationTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      HeapProfileAllocationTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "heap_name", &self->heap_name_, ColumnFlag::heap_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "callsite_id", &self->callsite_id_, ColumnFlag::callsite_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "count", &self->count_, ColumnFlag::count,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "size", &self->size_, ColumnFlag::size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit HeapProfileAllocationTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        heap_name_(ColumnStorage<ColumnType::heap_name::stored_type>::Create<false>()),
        callsite_id_(ColumnStorage<ColumnType::callsite_id::stored_type>::Create<false>()),
        count_(ColumnStorage<ColumnType::count::stored_type>::Create<false>()),
        size_(ColumnStorage<ColumnType::size::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          false)),
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false)),
        heap_name_storage_layer_(
          new column::StringStorage(string_pool(), &heap_name_.vector())),
        callsite_id_storage_layer_(
        new column::NumericStorage<ColumnType::callsite_id::non_optional_stored_type>(
          &callsite_id_.vector(),
          ColumnTypeHelper<ColumnType::callsite_id::stored_type>::ToColumnType(),
          false)),
        count_storage_layer_(
        new column::NumericStorage<ColumnType::count::non_optional_stored_type>(
          &count_.vector(),
          ColumnTypeHelper<ColumnType::count::stored_type>::ToColumnType(),
          false)),
        size_storage_layer_(
        new column::NumericStorage<ColumnType::size::non_optional_stored_type>(
          &size_.vector(),
          ColumnTypeHelper<ColumnType::size::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::heap_name::stored_type>(
          ColumnFlag::heap_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::callsite_id::stored_type>(
          ColumnFlag::callsite_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::count::stored_type>(
          ColumnFlag::count),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::size::stored_type>(
          ColumnFlag::size),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,upid_storage_layer_,heap_name_storage_layer_,callsite_id_storage_layer_,count_storage_layer_,size_storage_layer_},
      {{},{},{},{},{},{},{},{}});
  }
  ~HeapProfileAllocationTable() override;

  static const char* Name() { return "heap_profile_allocation"; }

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
        "upid", ColumnType::upid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "heap_name", ColumnType::heap_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "callsite_id", ColumnType::callsite_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "count", ColumnType::count::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "size", ColumnType::size::SqlValueType(), false,
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
    upid_.ShrinkToFit();
    heap_name_.ShrinkToFit();
    callsite_id_.ShrinkToFit();
    count_.ShrinkToFit();
    size_.ShrinkToFit();
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
    mutable_upid()->Append(row.upid);
    mutable_heap_name()->Append(row.heap_name);
    mutable_callsite_id()->Append(row.callsite_id);
    mutable_count()->Append(row.count);
    mutable_size()->Append(row.size);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<HeapProfileAllocationTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<uint32_t>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<StringPool::Id>& heap_name() const {
    return static_cast<const ColumnType::heap_name&>(columns()[ColumnIndex::heap_name]);
  }
  const TypedColumn<StackProfileCallsiteTable::Id>& callsite_id() const {
    return static_cast<const ColumnType::callsite_id&>(columns()[ColumnIndex::callsite_id]);
  }
  const TypedColumn<int64_t>& count() const {
    return static_cast<const ColumnType::count&>(columns()[ColumnIndex::count]);
  }
  const TypedColumn<int64_t>& size() const {
    return static_cast<const ColumnType::size&>(columns()[ColumnIndex::size]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<uint32_t>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<StringPool::Id>* mutable_heap_name() {
    return static_cast<ColumnType::heap_name*>(
        GetColumn(ColumnIndex::heap_name));
  }
  TypedColumn<StackProfileCallsiteTable::Id>* mutable_callsite_id() {
    return static_cast<ColumnType::callsite_id*>(
        GetColumn(ColumnIndex::callsite_id));
  }
  TypedColumn<int64_t>* mutable_count() {
    return static_cast<ColumnType::count*>(
        GetColumn(ColumnIndex::count));
  }
  TypedColumn<int64_t>* mutable_size() {
    return static_cast<ColumnType::size*>(
        GetColumn(ColumnIndex::size));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::heap_name::stored_type> heap_name_;
  ColumnStorage<ColumnType::callsite_id::stored_type> callsite_id_;
  ColumnStorage<ColumnType::count::stored_type> count_;
  ColumnStorage<ColumnType::size::stored_type> size_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> heap_name_storage_layer_;
  RefPtr<column::StorageLayer> callsite_id_storage_layer_;
  RefPtr<column::StorageLayer> count_storage_layer_;
  RefPtr<column::StorageLayer> size_storage_layer_;

  
};
  

class PackageListTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t package_name = 2;
    static constexpr uint32_t uid = 3;
    static constexpr uint32_t debuggable = 4;
    static constexpr uint32_t profileable_from_shell = 5;
    static constexpr uint32_t version_code = 6;
  };
  struct ColumnType {
    using id = IdColumn<PackageListTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using package_name = TypedColumn<StringPool::Id>;
    using uid = TypedColumn<int64_t>;
    using debuggable = TypedColumn<int32_t>;
    using profileable_from_shell = TypedColumn<int32_t>;
    using version_code = TypedColumn<int64_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_package_name = {},
        int64_t in_uid = {},
        int32_t in_debuggable = {},
        int32_t in_profileable_from_shell = {},
        int64_t in_version_code = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          package_name(in_package_name),
          uid(in_uid),
          debuggable(in_debuggable),
          profileable_from_shell(in_profileable_from_shell),
          version_code(in_version_code) {
      type_ = "package_list";
    }
    StringPool::Id package_name;
    int64_t uid;
    int32_t debuggable;
    int32_t profileable_from_shell;
    int64_t version_code;

    bool operator==(const PackageListTable::Row& other) const {
      return type() == other.type() && ColumnType::package_name::Equals(package_name, other.package_name) &&
       ColumnType::uid::Equals(uid, other.uid) &&
       ColumnType::debuggable::Equals(debuggable, other.debuggable) &&
       ColumnType::profileable_from_shell::Equals(profileable_from_shell, other.profileable_from_shell) &&
       ColumnType::version_code::Equals(version_code, other.version_code);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t package_name = ColumnType::package_name::default_flags();
    static constexpr uint32_t uid = ColumnType::uid::default_flags();
    static constexpr uint32_t debuggable = ColumnType::debuggable::default_flags();
    static constexpr uint32_t profileable_from_shell = ColumnType::profileable_from_shell::default_flags();
    static constexpr uint32_t version_code = ColumnType::version_code::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      PackageListTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    PackageListTable, RowNumber> {
   public:
    ConstRowReference(const PackageListTable* table, uint32_t row_number)
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
    ColumnType::debuggable::type debuggable() const {
      return table_->debuggable()[row_number_];
    }
    ColumnType::profileable_from_shell::type profileable_from_shell() const {
      return table_->profileable_from_shell()[row_number_];
    }
    ColumnType::version_code::type version_code() const {
      return table_->version_code()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const PackageListTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_package_name(
        ColumnType::package_name::non_optional_type v) {
      return mutable_table()->mutable_package_name()->Set(row_number_, v);
    }
    void set_uid(
        ColumnType::uid::non_optional_type v) {
      return mutable_table()->mutable_uid()->Set(row_number_, v);
    }
    void set_debuggable(
        ColumnType::debuggable::non_optional_type v) {
      return mutable_table()->mutable_debuggable()->Set(row_number_, v);
    }
    void set_profileable_from_shell(
        ColumnType::profileable_from_shell::non_optional_type v) {
      return mutable_table()->mutable_profileable_from_shell()->Set(row_number_, v);
    }
    void set_version_code(
        ColumnType::version_code::non_optional_type v) {
      return mutable_table()->mutable_version_code()->Set(row_number_, v);
    }

   private:
    PackageListTable* mutable_table() const {
      return const_cast<PackageListTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, PackageListTable, RowNumber, ConstRowReference> {
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
    ColumnType::debuggable::type debuggable() const {
      const auto& col = table_->debuggable();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::profileable_from_shell::type profileable_from_shell() const {
      const auto& col = table_->profileable_from_shell();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::version_code::type version_code() const {
      const auto& col = table_->version_code();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const PackageListTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class PackageListTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, PackageListTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class PackageListTable;

     explicit Iterator(PackageListTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     PackageListTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      PackageListTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "package_name", &self->package_name_, ColumnFlag::package_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "uid", &self->uid_, ColumnFlag::uid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "debuggable", &self->debuggable_, ColumnFlag::debuggable,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "profileable_from_shell", &self->profileable_from_shell_, ColumnFlag::profileable_from_shell,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "version_code", &self->version_code_, ColumnFlag::version_code,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit PackageListTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        package_name_(ColumnStorage<ColumnType::package_name::stored_type>::Create<false>()),
        uid_(ColumnStorage<ColumnType::uid::stored_type>::Create<false>()),
        debuggable_(ColumnStorage<ColumnType::debuggable::stored_type>::Create<false>()),
        profileable_from_shell_(ColumnStorage<ColumnType::profileable_from_shell::stored_type>::Create<false>()),
        version_code_(ColumnStorage<ColumnType::version_code::stored_type>::Create<false>())
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
        debuggable_storage_layer_(
        new column::NumericStorage<ColumnType::debuggable::non_optional_stored_type>(
          &debuggable_.vector(),
          ColumnTypeHelper<ColumnType::debuggable::stored_type>::ToColumnType(),
          false)),
        profileable_from_shell_storage_layer_(
        new column::NumericStorage<ColumnType::profileable_from_shell::non_optional_stored_type>(
          &profileable_from_shell_.vector(),
          ColumnTypeHelper<ColumnType::profileable_from_shell::stored_type>::ToColumnType(),
          false)),
        version_code_storage_layer_(
        new column::NumericStorage<ColumnType::version_code::non_optional_stored_type>(
          &version_code_.vector(),
          ColumnTypeHelper<ColumnType::version_code::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::package_name::stored_type>(
          ColumnFlag::package_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::uid::stored_type>(
          ColumnFlag::uid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::debuggable::stored_type>(
          ColumnFlag::debuggable),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::profileable_from_shell::stored_type>(
          ColumnFlag::profileable_from_shell),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::version_code::stored_type>(
          ColumnFlag::version_code),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,package_name_storage_layer_,uid_storage_layer_,debuggable_storage_layer_,profileable_from_shell_storage_layer_,version_code_storage_layer_},
      {{},{},{},{},{},{},{}});
  }
  ~PackageListTable() override;

  static const char* Name() { return "package_list"; }

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
        "debuggable", ColumnType::debuggable::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "profileable_from_shell", ColumnType::profileable_from_shell::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "version_code", ColumnType::version_code::SqlValueType(), false,
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
    debuggable_.ShrinkToFit();
    profileable_from_shell_.ShrinkToFit();
    version_code_.ShrinkToFit();
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
    mutable_debuggable()->Append(row.debuggable);
    mutable_profileable_from_shell()->Append(row.profileable_from_shell);
    mutable_version_code()->Append(row.version_code);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<PackageListTable::Id>& id() const {
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
  const TypedColumn<int32_t>& debuggable() const {
    return static_cast<const ColumnType::debuggable&>(columns()[ColumnIndex::debuggable]);
  }
  const TypedColumn<int32_t>& profileable_from_shell() const {
    return static_cast<const ColumnType::profileable_from_shell&>(columns()[ColumnIndex::profileable_from_shell]);
  }
  const TypedColumn<int64_t>& version_code() const {
    return static_cast<const ColumnType::version_code&>(columns()[ColumnIndex::version_code]);
  }

  TypedColumn<StringPool::Id>* mutable_package_name() {
    return static_cast<ColumnType::package_name*>(
        GetColumn(ColumnIndex::package_name));
  }
  TypedColumn<int64_t>* mutable_uid() {
    return static_cast<ColumnType::uid*>(
        GetColumn(ColumnIndex::uid));
  }
  TypedColumn<int32_t>* mutable_debuggable() {
    return static_cast<ColumnType::debuggable*>(
        GetColumn(ColumnIndex::debuggable));
  }
  TypedColumn<int32_t>* mutable_profileable_from_shell() {
    return static_cast<ColumnType::profileable_from_shell*>(
        GetColumn(ColumnIndex::profileable_from_shell));
  }
  TypedColumn<int64_t>* mutable_version_code() {
    return static_cast<ColumnType::version_code*>(
        GetColumn(ColumnIndex::version_code));
  }

 private:
  
  
  ColumnStorage<ColumnType::package_name::stored_type> package_name_;
  ColumnStorage<ColumnType::uid::stored_type> uid_;
  ColumnStorage<ColumnType::debuggable::stored_type> debuggable_;
  ColumnStorage<ColumnType::profileable_from_shell::stored_type> profileable_from_shell_;
  ColumnStorage<ColumnType::version_code::stored_type> version_code_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> package_name_storage_layer_;
  RefPtr<column::StorageLayer> uid_storage_layer_;
  RefPtr<column::StorageLayer> debuggable_storage_layer_;
  RefPtr<column::StorageLayer> profileable_from_shell_storage_layer_;
  RefPtr<column::StorageLayer> version_code_storage_layer_;

  
};
  

class PerfSessionTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t cmdline = 2;
  };
  struct ColumnType {
    using id = IdColumn<PerfSessionTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using cmdline = TypedColumn<std::optional<StringPool::Id>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(std::optional<StringPool::Id> in_cmdline = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          cmdline(in_cmdline) {
      type_ = "__intrinsic_perf_session";
    }
    std::optional<StringPool::Id> cmdline;

    bool operator==(const PerfSessionTable::Row& other) const {
      return type() == other.type() && ColumnType::cmdline::Equals(cmdline, other.cmdline);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t cmdline = ColumnType::cmdline::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      PerfSessionTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    PerfSessionTable, RowNumber> {
   public:
    ConstRowReference(const PerfSessionTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::cmdline::type cmdline() const {
      return table_->cmdline()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const PerfSessionTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_cmdline(
        ColumnType::cmdline::non_optional_type v) {
      return mutable_table()->mutable_cmdline()->Set(row_number_, v);
    }

   private:
    PerfSessionTable* mutable_table() const {
      return const_cast<PerfSessionTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, PerfSessionTable, RowNumber, ConstRowReference> {
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
    ColumnType::cmdline::type cmdline() const {
      const auto& col = table_->cmdline();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const PerfSessionTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class PerfSessionTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, PerfSessionTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class PerfSessionTable;

     explicit Iterator(PerfSessionTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     PerfSessionTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      PerfSessionTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "cmdline", &self->cmdline_, ColumnFlag::cmdline,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit PerfSessionTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        cmdline_(ColumnStorage<ColumnType::cmdline::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        cmdline_storage_layer_(
          new column::StringStorage(string_pool(), &cmdline_.vector()))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cmdline::stored_type>(
          ColumnFlag::cmdline),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,cmdline_storage_layer_},
      {{},{},{}});
  }
  ~PerfSessionTable() override;

  static const char* Name() { return "__intrinsic_perf_session"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cmdline", ColumnType::cmdline::SqlValueType(), false,
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
    cmdline_.ShrinkToFit();
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
    mutable_cmdline()->Append(row.cmdline);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<PerfSessionTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& cmdline() const {
    return static_cast<const ColumnType::cmdline&>(columns()[ColumnIndex::cmdline]);
  }

  TypedColumn<std::optional<StringPool::Id>>* mutable_cmdline() {
    return static_cast<ColumnType::cmdline*>(
        GetColumn(ColumnIndex::cmdline));
  }

 private:
  
  
  ColumnStorage<ColumnType::cmdline::stored_type> cmdline_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> cmdline_storage_layer_;

  
};
  

class PerfSampleTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t ts = 2;
    static constexpr uint32_t utid = 3;
    static constexpr uint32_t cpu = 4;
    static constexpr uint32_t cpu_mode = 5;
    static constexpr uint32_t callsite_id = 6;
    static constexpr uint32_t unwind_error = 7;
    static constexpr uint32_t perf_session_id = 8;
  };
  struct ColumnType {
    using id = IdColumn<PerfSampleTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using ts = TypedColumn<int64_t>;
    using utid = TypedColumn<uint32_t>;
    using cpu = TypedColumn<std::optional<uint32_t>>;
    using cpu_mode = TypedColumn<StringPool::Id>;
    using callsite_id = TypedColumn<std::optional<StackProfileCallsiteTable::Id>>;
    using unwind_error = TypedColumn<std::optional<StringPool::Id>>;
    using perf_session_id = TypedColumn<PerfSessionTable::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(int64_t in_ts = {},
        uint32_t in_utid = {},
        std::optional<uint32_t> in_cpu = {},
        StringPool::Id in_cpu_mode = {},
        std::optional<StackProfileCallsiteTable::Id> in_callsite_id = {},
        std::optional<StringPool::Id> in_unwind_error = {},
        PerfSessionTable::Id in_perf_session_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          ts(in_ts),
          utid(in_utid),
          cpu(in_cpu),
          cpu_mode(in_cpu_mode),
          callsite_id(in_callsite_id),
          unwind_error(in_unwind_error),
          perf_session_id(in_perf_session_id) {
      type_ = "perf_sample";
    }
    int64_t ts;
    uint32_t utid;
    std::optional<uint32_t> cpu;
    StringPool::Id cpu_mode;
    std::optional<StackProfileCallsiteTable::Id> callsite_id;
    std::optional<StringPool::Id> unwind_error;
    PerfSessionTable::Id perf_session_id;

    bool operator==(const PerfSampleTable::Row& other) const {
      return type() == other.type() && ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::utid::Equals(utid, other.utid) &&
       ColumnType::cpu::Equals(cpu, other.cpu) &&
       ColumnType::cpu_mode::Equals(cpu_mode, other.cpu_mode) &&
       ColumnType::callsite_id::Equals(callsite_id, other.callsite_id) &&
       ColumnType::unwind_error::Equals(unwind_error, other.unwind_error) &&
       ColumnType::perf_session_id::Equals(perf_session_id, other.perf_session_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t ts = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted) | ColumnType::ts::default_flags();
    static constexpr uint32_t utid = ColumnType::utid::default_flags();
    static constexpr uint32_t cpu = ColumnType::cpu::default_flags();
    static constexpr uint32_t cpu_mode = ColumnType::cpu_mode::default_flags();
    static constexpr uint32_t callsite_id = ColumnType::callsite_id::default_flags();
    static constexpr uint32_t unwind_error = ColumnType::unwind_error::default_flags();
    static constexpr uint32_t perf_session_id = ColumnType::perf_session_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      PerfSampleTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    PerfSampleTable, RowNumber> {
   public:
    ConstRowReference(const PerfSampleTable* table, uint32_t row_number)
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
    ColumnType::cpu::type cpu() const {
      return table_->cpu()[row_number_];
    }
    ColumnType::cpu_mode::type cpu_mode() const {
      return table_->cpu_mode()[row_number_];
    }
    ColumnType::callsite_id::type callsite_id() const {
      return table_->callsite_id()[row_number_];
    }
    ColumnType::unwind_error::type unwind_error() const {
      return table_->unwind_error()[row_number_];
    }
    ColumnType::perf_session_id::type perf_session_id() const {
      return table_->perf_session_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const PerfSampleTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_utid(
        ColumnType::utid::non_optional_type v) {
      return mutable_table()->mutable_utid()->Set(row_number_, v);
    }
    void set_cpu(
        ColumnType::cpu::non_optional_type v) {
      return mutable_table()->mutable_cpu()->Set(row_number_, v);
    }
    void set_cpu_mode(
        ColumnType::cpu_mode::non_optional_type v) {
      return mutable_table()->mutable_cpu_mode()->Set(row_number_, v);
    }
    void set_callsite_id(
        ColumnType::callsite_id::non_optional_type v) {
      return mutable_table()->mutable_callsite_id()->Set(row_number_, v);
    }
    void set_unwind_error(
        ColumnType::unwind_error::non_optional_type v) {
      return mutable_table()->mutable_unwind_error()->Set(row_number_, v);
    }
    void set_perf_session_id(
        ColumnType::perf_session_id::non_optional_type v) {
      return mutable_table()->mutable_perf_session_id()->Set(row_number_, v);
    }

   private:
    PerfSampleTable* mutable_table() const {
      return const_cast<PerfSampleTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, PerfSampleTable, RowNumber, ConstRowReference> {
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
    ColumnType::cpu::type cpu() const {
      const auto& col = table_->cpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::cpu_mode::type cpu_mode() const {
      const auto& col = table_->cpu_mode();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::callsite_id::type callsite_id() const {
      const auto& col = table_->callsite_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::unwind_error::type unwind_error() const {
      const auto& col = table_->unwind_error();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::perf_session_id::type perf_session_id() const {
      const auto& col = table_->perf_session_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const PerfSampleTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class PerfSampleTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, PerfSampleTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class PerfSampleTable;

     explicit Iterator(PerfSampleTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     PerfSampleTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      PerfSampleTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "utid", &self->utid_, ColumnFlag::utid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cpu", &self->cpu_, ColumnFlag::cpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cpu_mode", &self->cpu_mode_, ColumnFlag::cpu_mode,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "callsite_id", &self->callsite_id_, ColumnFlag::callsite_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "unwind_error", &self->unwind_error_, ColumnFlag::unwind_error,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "perf_session_id", &self->perf_session_id_, ColumnFlag::perf_session_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit PerfSampleTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        utid_(ColumnStorage<ColumnType::utid::stored_type>::Create<false>()),
        cpu_(ColumnStorage<ColumnType::cpu::stored_type>::Create<false>()),
        cpu_mode_(ColumnStorage<ColumnType::cpu_mode::stored_type>::Create<false>()),
        callsite_id_(ColumnStorage<ColumnType::callsite_id::stored_type>::Create<false>()),
        unwind_error_(ColumnStorage<ColumnType::unwind_error::stored_type>::Create<false>()),
        perf_session_id_(ColumnStorage<ColumnType::perf_session_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          true)),
        utid_storage_layer_(
        new column::NumericStorage<ColumnType::utid::non_optional_stored_type>(
          &utid_.vector(),
          ColumnTypeHelper<ColumnType::utid::stored_type>::ToColumnType(),
          false)),
        cpu_storage_layer_(
          new column::NumericStorage<ColumnType::cpu::non_optional_stored_type>(
            &cpu_.non_null_vector(),
            ColumnTypeHelper<ColumnType::cpu::stored_type>::ToColumnType(),
            false)),
        cpu_mode_storage_layer_(
          new column::StringStorage(string_pool(), &cpu_mode_.vector())),
        callsite_id_storage_layer_(
          new column::NumericStorage<ColumnType::callsite_id::non_optional_stored_type>(
            &callsite_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::callsite_id::stored_type>::ToColumnType(),
            false)),
        unwind_error_storage_layer_(
          new column::StringStorage(string_pool(), &unwind_error_.vector())),
        perf_session_id_storage_layer_(
        new column::NumericStorage<ColumnType::perf_session_id::non_optional_stored_type>(
          &perf_session_id_.vector(),
          ColumnTypeHelper<ColumnType::perf_session_id::stored_type>::ToColumnType(),
          false))
,
        cpu_null_layer_(new column::NullOverlay(cpu_.bv())),
        callsite_id_null_layer_(new column::NullOverlay(callsite_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::utid::stored_type>(
          ColumnFlag::utid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cpu::stored_type>(
          ColumnFlag::cpu),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cpu_mode::stored_type>(
          ColumnFlag::cpu_mode),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::callsite_id::stored_type>(
          ColumnFlag::callsite_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::unwind_error::stored_type>(
          ColumnFlag::unwind_error),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::perf_session_id::stored_type>(
          ColumnFlag::perf_session_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,ts_storage_layer_,utid_storage_layer_,cpu_storage_layer_,cpu_mode_storage_layer_,callsite_id_storage_layer_,unwind_error_storage_layer_,perf_session_id_storage_layer_},
      {{},{},{},{},cpu_null_layer_,{},callsite_id_null_layer_,{},{}});
  }
  ~PerfSampleTable() override;

  static const char* Name() { return "perf_sample"; }

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
        "utid", ColumnType::utid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cpu", ColumnType::cpu::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cpu_mode", ColumnType::cpu_mode::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "callsite_id", ColumnType::callsite_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "unwind_error", ColumnType::unwind_error::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "perf_session_id", ColumnType::perf_session_id::SqlValueType(), false,
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
    cpu_.ShrinkToFit();
    cpu_mode_.ShrinkToFit();
    callsite_id_.ShrinkToFit();
    unwind_error_.ShrinkToFit();
    perf_session_id_.ShrinkToFit();
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
    mutable_cpu()->Append(row.cpu);
    mutable_cpu_mode()->Append(row.cpu_mode);
    mutable_callsite_id()->Append(row.callsite_id);
    mutable_unwind_error()->Append(row.unwind_error);
    mutable_perf_session_id()->Append(row.perf_session_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<PerfSampleTable::Id>& id() const {
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
  const TypedColumn<std::optional<uint32_t>>& cpu() const {
    return static_cast<const ColumnType::cpu&>(columns()[ColumnIndex::cpu]);
  }
  const TypedColumn<StringPool::Id>& cpu_mode() const {
    return static_cast<const ColumnType::cpu_mode&>(columns()[ColumnIndex::cpu_mode]);
  }
  const TypedColumn<std::optional<StackProfileCallsiteTable::Id>>& callsite_id() const {
    return static_cast<const ColumnType::callsite_id&>(columns()[ColumnIndex::callsite_id]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& unwind_error() const {
    return static_cast<const ColumnType::unwind_error&>(columns()[ColumnIndex::unwind_error]);
  }
  const TypedColumn<PerfSessionTable::Id>& perf_session_id() const {
    return static_cast<const ColumnType::perf_session_id&>(columns()[ColumnIndex::perf_session_id]);
  }

  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<uint32_t>* mutable_utid() {
    return static_cast<ColumnType::utid*>(
        GetColumn(ColumnIndex::utid));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_cpu() {
    return static_cast<ColumnType::cpu*>(
        GetColumn(ColumnIndex::cpu));
  }
  TypedColumn<StringPool::Id>* mutable_cpu_mode() {
    return static_cast<ColumnType::cpu_mode*>(
        GetColumn(ColumnIndex::cpu_mode));
  }
  TypedColumn<std::optional<StackProfileCallsiteTable::Id>>* mutable_callsite_id() {
    return static_cast<ColumnType::callsite_id*>(
        GetColumn(ColumnIndex::callsite_id));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_unwind_error() {
    return static_cast<ColumnType::unwind_error*>(
        GetColumn(ColumnIndex::unwind_error));
  }
  TypedColumn<PerfSessionTable::Id>* mutable_perf_session_id() {
    return static_cast<ColumnType::perf_session_id*>(
        GetColumn(ColumnIndex::perf_session_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::utid::stored_type> utid_;
  ColumnStorage<ColumnType::cpu::stored_type> cpu_;
  ColumnStorage<ColumnType::cpu_mode::stored_type> cpu_mode_;
  ColumnStorage<ColumnType::callsite_id::stored_type> callsite_id_;
  ColumnStorage<ColumnType::unwind_error::stored_type> unwind_error_;
  ColumnStorage<ColumnType::perf_session_id::stored_type> perf_session_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> utid_storage_layer_;
  RefPtr<column::StorageLayer> cpu_storage_layer_;
  RefPtr<column::StorageLayer> cpu_mode_storage_layer_;
  RefPtr<column::StorageLayer> callsite_id_storage_layer_;
  RefPtr<column::StorageLayer> unwind_error_storage_layer_;
  RefPtr<column::StorageLayer> perf_session_id_storage_layer_;

  RefPtr<column::OverlayLayer> cpu_null_layer_;
  RefPtr<column::OverlayLayer> callsite_id_null_layer_;
};
  

class ProfilerSmapsTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 19;

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
    static constexpr uint32_t ts = 3;
    static constexpr uint32_t path = 4;
    static constexpr uint32_t size_kb = 5;
    static constexpr uint32_t private_dirty_kb = 6;
    static constexpr uint32_t swap_kb = 7;
    static constexpr uint32_t file_name = 8;
    static constexpr uint32_t start_address = 9;
    static constexpr uint32_t module_timestamp = 10;
    static constexpr uint32_t module_debugid = 11;
    static constexpr uint32_t module_debug_path = 12;
    static constexpr uint32_t protection_flags = 13;
    static constexpr uint32_t private_clean_resident_kb = 14;
    static constexpr uint32_t shared_dirty_resident_kb = 15;
    static constexpr uint32_t shared_clean_resident_kb = 16;
    static constexpr uint32_t locked_kb = 17;
    static constexpr uint32_t proportional_resident_kb = 18;
  };
  struct ColumnType {
    using id = IdColumn<ProfilerSmapsTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using upid = TypedColumn<uint32_t>;
    using ts = TypedColumn<int64_t>;
    using path = TypedColumn<StringPool::Id>;
    using size_kb = TypedColumn<int64_t>;
    using private_dirty_kb = TypedColumn<int64_t>;
    using swap_kb = TypedColumn<int64_t>;
    using file_name = TypedColumn<StringPool::Id>;
    using start_address = TypedColumn<int64_t>;
    using module_timestamp = TypedColumn<int64_t>;
    using module_debugid = TypedColumn<StringPool::Id>;
    using module_debug_path = TypedColumn<StringPool::Id>;
    using protection_flags = TypedColumn<int64_t>;
    using private_clean_resident_kb = TypedColumn<int64_t>;
    using shared_dirty_resident_kb = TypedColumn<int64_t>;
    using shared_clean_resident_kb = TypedColumn<int64_t>;
    using locked_kb = TypedColumn<int64_t>;
    using proportional_resident_kb = TypedColumn<int64_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_upid = {},
        int64_t in_ts = {},
        StringPool::Id in_path = {},
        int64_t in_size_kb = {},
        int64_t in_private_dirty_kb = {},
        int64_t in_swap_kb = {},
        StringPool::Id in_file_name = {},
        int64_t in_start_address = {},
        int64_t in_module_timestamp = {},
        StringPool::Id in_module_debugid = {},
        StringPool::Id in_module_debug_path = {},
        int64_t in_protection_flags = {},
        int64_t in_private_clean_resident_kb = {},
        int64_t in_shared_dirty_resident_kb = {},
        int64_t in_shared_clean_resident_kb = {},
        int64_t in_locked_kb = {},
        int64_t in_proportional_resident_kb = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          upid(in_upid),
          ts(in_ts),
          path(in_path),
          size_kb(in_size_kb),
          private_dirty_kb(in_private_dirty_kb),
          swap_kb(in_swap_kb),
          file_name(in_file_name),
          start_address(in_start_address),
          module_timestamp(in_module_timestamp),
          module_debugid(in_module_debugid),
          module_debug_path(in_module_debug_path),
          protection_flags(in_protection_flags),
          private_clean_resident_kb(in_private_clean_resident_kb),
          shared_dirty_resident_kb(in_shared_dirty_resident_kb),
          shared_clean_resident_kb(in_shared_clean_resident_kb),
          locked_kb(in_locked_kb),
          proportional_resident_kb(in_proportional_resident_kb) {
      type_ = "profiler_smaps";
    }
    uint32_t upid;
    int64_t ts;
    StringPool::Id path;
    int64_t size_kb;
    int64_t private_dirty_kb;
    int64_t swap_kb;
    StringPool::Id file_name;
    int64_t start_address;
    int64_t module_timestamp;
    StringPool::Id module_debugid;
    StringPool::Id module_debug_path;
    int64_t protection_flags;
    int64_t private_clean_resident_kb;
    int64_t shared_dirty_resident_kb;
    int64_t shared_clean_resident_kb;
    int64_t locked_kb;
    int64_t proportional_resident_kb;

    bool operator==(const ProfilerSmapsTable::Row& other) const {
      return type() == other.type() && ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::ts::Equals(ts, other.ts) &&
       ColumnType::path::Equals(path, other.path) &&
       ColumnType::size_kb::Equals(size_kb, other.size_kb) &&
       ColumnType::private_dirty_kb::Equals(private_dirty_kb, other.private_dirty_kb) &&
       ColumnType::swap_kb::Equals(swap_kb, other.swap_kb) &&
       ColumnType::file_name::Equals(file_name, other.file_name) &&
       ColumnType::start_address::Equals(start_address, other.start_address) &&
       ColumnType::module_timestamp::Equals(module_timestamp, other.module_timestamp) &&
       ColumnType::module_debugid::Equals(module_debugid, other.module_debugid) &&
       ColumnType::module_debug_path::Equals(module_debug_path, other.module_debug_path) &&
       ColumnType::protection_flags::Equals(protection_flags, other.protection_flags) &&
       ColumnType::private_clean_resident_kb::Equals(private_clean_resident_kb, other.private_clean_resident_kb) &&
       ColumnType::shared_dirty_resident_kb::Equals(shared_dirty_resident_kb, other.shared_dirty_resident_kb) &&
       ColumnType::shared_clean_resident_kb::Equals(shared_clean_resident_kb, other.shared_clean_resident_kb) &&
       ColumnType::locked_kb::Equals(locked_kb, other.locked_kb) &&
       ColumnType::proportional_resident_kb::Equals(proportional_resident_kb, other.proportional_resident_kb);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
    static constexpr uint32_t ts = ColumnType::ts::default_flags();
    static constexpr uint32_t path = ColumnType::path::default_flags();
    static constexpr uint32_t size_kb = ColumnType::size_kb::default_flags();
    static constexpr uint32_t private_dirty_kb = ColumnType::private_dirty_kb::default_flags();
    static constexpr uint32_t swap_kb = ColumnType::swap_kb::default_flags();
    static constexpr uint32_t file_name = ColumnType::file_name::default_flags();
    static constexpr uint32_t start_address = ColumnType::start_address::default_flags();
    static constexpr uint32_t module_timestamp = ColumnType::module_timestamp::default_flags();
    static constexpr uint32_t module_debugid = ColumnType::module_debugid::default_flags();
    static constexpr uint32_t module_debug_path = ColumnType::module_debug_path::default_flags();
    static constexpr uint32_t protection_flags = ColumnType::protection_flags::default_flags();
    static constexpr uint32_t private_clean_resident_kb = ColumnType::private_clean_resident_kb::default_flags();
    static constexpr uint32_t shared_dirty_resident_kb = ColumnType::shared_dirty_resident_kb::default_flags();
    static constexpr uint32_t shared_clean_resident_kb = ColumnType::shared_clean_resident_kb::default_flags();
    static constexpr uint32_t locked_kb = ColumnType::locked_kb::default_flags();
    static constexpr uint32_t proportional_resident_kb = ColumnType::proportional_resident_kb::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ProfilerSmapsTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ProfilerSmapsTable, RowNumber> {
   public:
    ConstRowReference(const ProfilerSmapsTable* table, uint32_t row_number)
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
    ColumnType::ts::type ts() const {
      return table_->ts()[row_number_];
    }
    ColumnType::path::type path() const {
      return table_->path()[row_number_];
    }
    ColumnType::size_kb::type size_kb() const {
      return table_->size_kb()[row_number_];
    }
    ColumnType::private_dirty_kb::type private_dirty_kb() const {
      return table_->private_dirty_kb()[row_number_];
    }
    ColumnType::swap_kb::type swap_kb() const {
      return table_->swap_kb()[row_number_];
    }
    ColumnType::file_name::type file_name() const {
      return table_->file_name()[row_number_];
    }
    ColumnType::start_address::type start_address() const {
      return table_->start_address()[row_number_];
    }
    ColumnType::module_timestamp::type module_timestamp() const {
      return table_->module_timestamp()[row_number_];
    }
    ColumnType::module_debugid::type module_debugid() const {
      return table_->module_debugid()[row_number_];
    }
    ColumnType::module_debug_path::type module_debug_path() const {
      return table_->module_debug_path()[row_number_];
    }
    ColumnType::protection_flags::type protection_flags() const {
      return table_->protection_flags()[row_number_];
    }
    ColumnType::private_clean_resident_kb::type private_clean_resident_kb() const {
      return table_->private_clean_resident_kb()[row_number_];
    }
    ColumnType::shared_dirty_resident_kb::type shared_dirty_resident_kb() const {
      return table_->shared_dirty_resident_kb()[row_number_];
    }
    ColumnType::shared_clean_resident_kb::type shared_clean_resident_kb() const {
      return table_->shared_clean_resident_kb()[row_number_];
    }
    ColumnType::locked_kb::type locked_kb() const {
      return table_->locked_kb()[row_number_];
    }
    ColumnType::proportional_resident_kb::type proportional_resident_kb() const {
      return table_->proportional_resident_kb()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ProfilerSmapsTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_ts(
        ColumnType::ts::non_optional_type v) {
      return mutable_table()->mutable_ts()->Set(row_number_, v);
    }
    void set_path(
        ColumnType::path::non_optional_type v) {
      return mutable_table()->mutable_path()->Set(row_number_, v);
    }
    void set_size_kb(
        ColumnType::size_kb::non_optional_type v) {
      return mutable_table()->mutable_size_kb()->Set(row_number_, v);
    }
    void set_private_dirty_kb(
        ColumnType::private_dirty_kb::non_optional_type v) {
      return mutable_table()->mutable_private_dirty_kb()->Set(row_number_, v);
    }
    void set_swap_kb(
        ColumnType::swap_kb::non_optional_type v) {
      return mutable_table()->mutable_swap_kb()->Set(row_number_, v);
    }
    void set_file_name(
        ColumnType::file_name::non_optional_type v) {
      return mutable_table()->mutable_file_name()->Set(row_number_, v);
    }
    void set_start_address(
        ColumnType::start_address::non_optional_type v) {
      return mutable_table()->mutable_start_address()->Set(row_number_, v);
    }
    void set_module_timestamp(
        ColumnType::module_timestamp::non_optional_type v) {
      return mutable_table()->mutable_module_timestamp()->Set(row_number_, v);
    }
    void set_module_debugid(
        ColumnType::module_debugid::non_optional_type v) {
      return mutable_table()->mutable_module_debugid()->Set(row_number_, v);
    }
    void set_module_debug_path(
        ColumnType::module_debug_path::non_optional_type v) {
      return mutable_table()->mutable_module_debug_path()->Set(row_number_, v);
    }
    void set_protection_flags(
        ColumnType::protection_flags::non_optional_type v) {
      return mutable_table()->mutable_protection_flags()->Set(row_number_, v);
    }
    void set_private_clean_resident_kb(
        ColumnType::private_clean_resident_kb::non_optional_type v) {
      return mutable_table()->mutable_private_clean_resident_kb()->Set(row_number_, v);
    }
    void set_shared_dirty_resident_kb(
        ColumnType::shared_dirty_resident_kb::non_optional_type v) {
      return mutable_table()->mutable_shared_dirty_resident_kb()->Set(row_number_, v);
    }
    void set_shared_clean_resident_kb(
        ColumnType::shared_clean_resident_kb::non_optional_type v) {
      return mutable_table()->mutable_shared_clean_resident_kb()->Set(row_number_, v);
    }
    void set_locked_kb(
        ColumnType::locked_kb::non_optional_type v) {
      return mutable_table()->mutable_locked_kb()->Set(row_number_, v);
    }
    void set_proportional_resident_kb(
        ColumnType::proportional_resident_kb::non_optional_type v) {
      return mutable_table()->mutable_proportional_resident_kb()->Set(row_number_, v);
    }

   private:
    ProfilerSmapsTable* mutable_table() const {
      return const_cast<ProfilerSmapsTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ProfilerSmapsTable, RowNumber, ConstRowReference> {
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
    ColumnType::ts::type ts() const {
      const auto& col = table_->ts();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::path::type path() const {
      const auto& col = table_->path();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::size_kb::type size_kb() const {
      const auto& col = table_->size_kb();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::private_dirty_kb::type private_dirty_kb() const {
      const auto& col = table_->private_dirty_kb();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::swap_kb::type swap_kb() const {
      const auto& col = table_->swap_kb();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::file_name::type file_name() const {
      const auto& col = table_->file_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::start_address::type start_address() const {
      const auto& col = table_->start_address();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::module_timestamp::type module_timestamp() const {
      const auto& col = table_->module_timestamp();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::module_debugid::type module_debugid() const {
      const auto& col = table_->module_debugid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::module_debug_path::type module_debug_path() const {
      const auto& col = table_->module_debug_path();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::protection_flags::type protection_flags() const {
      const auto& col = table_->protection_flags();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::private_clean_resident_kb::type private_clean_resident_kb() const {
      const auto& col = table_->private_clean_resident_kb();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::shared_dirty_resident_kb::type shared_dirty_resident_kb() const {
      const auto& col = table_->shared_dirty_resident_kb();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::shared_clean_resident_kb::type shared_clean_resident_kb() const {
      const auto& col = table_->shared_clean_resident_kb();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::locked_kb::type locked_kb() const {
      const auto& col = table_->locked_kb();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::proportional_resident_kb::type proportional_resident_kb() const {
      const auto& col = table_->proportional_resident_kb();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ProfilerSmapsTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ProfilerSmapsTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ProfilerSmapsTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ProfilerSmapsTable;

     explicit Iterator(ProfilerSmapsTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ProfilerSmapsTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ProfilerSmapsTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "ts", &self->ts_, ColumnFlag::ts,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "path", &self->path_, ColumnFlag::path,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "size_kb", &self->size_kb_, ColumnFlag::size_kb,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "private_dirty_kb", &self->private_dirty_kb_, ColumnFlag::private_dirty_kb,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "swap_kb", &self->swap_kb_, ColumnFlag::swap_kb,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "file_name", &self->file_name_, ColumnFlag::file_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "start_address", &self->start_address_, ColumnFlag::start_address,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "module_timestamp", &self->module_timestamp_, ColumnFlag::module_timestamp,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "module_debugid", &self->module_debugid_, ColumnFlag::module_debugid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "module_debug_path", &self->module_debug_path_, ColumnFlag::module_debug_path,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "protection_flags", &self->protection_flags_, ColumnFlag::protection_flags,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "private_clean_resident_kb", &self->private_clean_resident_kb_, ColumnFlag::private_clean_resident_kb,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "shared_dirty_resident_kb", &self->shared_dirty_resident_kb_, ColumnFlag::shared_dirty_resident_kb,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "shared_clean_resident_kb", &self->shared_clean_resident_kb_, ColumnFlag::shared_clean_resident_kb,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "locked_kb", &self->locked_kb_, ColumnFlag::locked_kb,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "proportional_resident_kb", &self->proportional_resident_kb_, ColumnFlag::proportional_resident_kb,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ProfilerSmapsTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        ts_(ColumnStorage<ColumnType::ts::stored_type>::Create<false>()),
        path_(ColumnStorage<ColumnType::path::stored_type>::Create<false>()),
        size_kb_(ColumnStorage<ColumnType::size_kb::stored_type>::Create<false>()),
        private_dirty_kb_(ColumnStorage<ColumnType::private_dirty_kb::stored_type>::Create<false>()),
        swap_kb_(ColumnStorage<ColumnType::swap_kb::stored_type>::Create<false>()),
        file_name_(ColumnStorage<ColumnType::file_name::stored_type>::Create<false>()),
        start_address_(ColumnStorage<ColumnType::start_address::stored_type>::Create<false>()),
        module_timestamp_(ColumnStorage<ColumnType::module_timestamp::stored_type>::Create<false>()),
        module_debugid_(ColumnStorage<ColumnType::module_debugid::stored_type>::Create<false>()),
        module_debug_path_(ColumnStorage<ColumnType::module_debug_path::stored_type>::Create<false>()),
        protection_flags_(ColumnStorage<ColumnType::protection_flags::stored_type>::Create<false>()),
        private_clean_resident_kb_(ColumnStorage<ColumnType::private_clean_resident_kb::stored_type>::Create<false>()),
        shared_dirty_resident_kb_(ColumnStorage<ColumnType::shared_dirty_resident_kb::stored_type>::Create<false>()),
        shared_clean_resident_kb_(ColumnStorage<ColumnType::shared_clean_resident_kb::stored_type>::Create<false>()),
        locked_kb_(ColumnStorage<ColumnType::locked_kb::stored_type>::Create<false>()),
        proportional_resident_kb_(ColumnStorage<ColumnType::proportional_resident_kb::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        upid_storage_layer_(
        new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
          &upid_.vector(),
          ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
          false)),
        ts_storage_layer_(
        new column::NumericStorage<ColumnType::ts::non_optional_stored_type>(
          &ts_.vector(),
          ColumnTypeHelper<ColumnType::ts::stored_type>::ToColumnType(),
          false)),
        path_storage_layer_(
          new column::StringStorage(string_pool(), &path_.vector())),
        size_kb_storage_layer_(
        new column::NumericStorage<ColumnType::size_kb::non_optional_stored_type>(
          &size_kb_.vector(),
          ColumnTypeHelper<ColumnType::size_kb::stored_type>::ToColumnType(),
          false)),
        private_dirty_kb_storage_layer_(
        new column::NumericStorage<ColumnType::private_dirty_kb::non_optional_stored_type>(
          &private_dirty_kb_.vector(),
          ColumnTypeHelper<ColumnType::private_dirty_kb::stored_type>::ToColumnType(),
          false)),
        swap_kb_storage_layer_(
        new column::NumericStorage<ColumnType::swap_kb::non_optional_stored_type>(
          &swap_kb_.vector(),
          ColumnTypeHelper<ColumnType::swap_kb::stored_type>::ToColumnType(),
          false)),
        file_name_storage_layer_(
          new column::StringStorage(string_pool(), &file_name_.vector())),
        start_address_storage_layer_(
        new column::NumericStorage<ColumnType::start_address::non_optional_stored_type>(
          &start_address_.vector(),
          ColumnTypeHelper<ColumnType::start_address::stored_type>::ToColumnType(),
          false)),
        module_timestamp_storage_layer_(
        new column::NumericStorage<ColumnType::module_timestamp::non_optional_stored_type>(
          &module_timestamp_.vector(),
          ColumnTypeHelper<ColumnType::module_timestamp::stored_type>::ToColumnType(),
          false)),
        module_debugid_storage_layer_(
          new column::StringStorage(string_pool(), &module_debugid_.vector())),
        module_debug_path_storage_layer_(
          new column::StringStorage(string_pool(), &module_debug_path_.vector())),
        protection_flags_storage_layer_(
        new column::NumericStorage<ColumnType::protection_flags::non_optional_stored_type>(
          &protection_flags_.vector(),
          ColumnTypeHelper<ColumnType::protection_flags::stored_type>::ToColumnType(),
          false)),
        private_clean_resident_kb_storage_layer_(
        new column::NumericStorage<ColumnType::private_clean_resident_kb::non_optional_stored_type>(
          &private_clean_resident_kb_.vector(),
          ColumnTypeHelper<ColumnType::private_clean_resident_kb::stored_type>::ToColumnType(),
          false)),
        shared_dirty_resident_kb_storage_layer_(
        new column::NumericStorage<ColumnType::shared_dirty_resident_kb::non_optional_stored_type>(
          &shared_dirty_resident_kb_.vector(),
          ColumnTypeHelper<ColumnType::shared_dirty_resident_kb::stored_type>::ToColumnType(),
          false)),
        shared_clean_resident_kb_storage_layer_(
        new column::NumericStorage<ColumnType::shared_clean_resident_kb::non_optional_stored_type>(
          &shared_clean_resident_kb_.vector(),
          ColumnTypeHelper<ColumnType::shared_clean_resident_kb::stored_type>::ToColumnType(),
          false)),
        locked_kb_storage_layer_(
        new column::NumericStorage<ColumnType::locked_kb::non_optional_stored_type>(
          &locked_kb_.vector(),
          ColumnTypeHelper<ColumnType::locked_kb::stored_type>::ToColumnType(),
          false)),
        proportional_resident_kb_storage_layer_(
        new column::NumericStorage<ColumnType::proportional_resident_kb::non_optional_stored_type>(
          &proportional_resident_kb_.vector(),
          ColumnTypeHelper<ColumnType::proportional_resident_kb::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::ts::stored_type>(
          ColumnFlag::ts),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::path::stored_type>(
          ColumnFlag::path),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::size_kb::stored_type>(
          ColumnFlag::size_kb),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::private_dirty_kb::stored_type>(
          ColumnFlag::private_dirty_kb),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::swap_kb::stored_type>(
          ColumnFlag::swap_kb),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::file_name::stored_type>(
          ColumnFlag::file_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::start_address::stored_type>(
          ColumnFlag::start_address),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::module_timestamp::stored_type>(
          ColumnFlag::module_timestamp),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::module_debugid::stored_type>(
          ColumnFlag::module_debugid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::module_debug_path::stored_type>(
          ColumnFlag::module_debug_path),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::protection_flags::stored_type>(
          ColumnFlag::protection_flags),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::private_clean_resident_kb::stored_type>(
          ColumnFlag::private_clean_resident_kb),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::shared_dirty_resident_kb::stored_type>(
          ColumnFlag::shared_dirty_resident_kb),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::shared_clean_resident_kb::stored_type>(
          ColumnFlag::shared_clean_resident_kb),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::locked_kb::stored_type>(
          ColumnFlag::locked_kb),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::proportional_resident_kb::stored_type>(
          ColumnFlag::proportional_resident_kb),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,upid_storage_layer_,ts_storage_layer_,path_storage_layer_,size_kb_storage_layer_,private_dirty_kb_storage_layer_,swap_kb_storage_layer_,file_name_storage_layer_,start_address_storage_layer_,module_timestamp_storage_layer_,module_debugid_storage_layer_,module_debug_path_storage_layer_,protection_flags_storage_layer_,private_clean_resident_kb_storage_layer_,shared_dirty_resident_kb_storage_layer_,shared_clean_resident_kb_storage_layer_,locked_kb_storage_layer_,proportional_resident_kb_storage_layer_},
      {{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}});
  }
  ~ProfilerSmapsTable() override;

  static const char* Name() { return "profiler_smaps"; }

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
        "ts", ColumnType::ts::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "path", ColumnType::path::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "size_kb", ColumnType::size_kb::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "private_dirty_kb", ColumnType::private_dirty_kb::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "swap_kb", ColumnType::swap_kb::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "file_name", ColumnType::file_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "start_address", ColumnType::start_address::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "module_timestamp", ColumnType::module_timestamp::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "module_debugid", ColumnType::module_debugid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "module_debug_path", ColumnType::module_debug_path::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "protection_flags", ColumnType::protection_flags::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "private_clean_resident_kb", ColumnType::private_clean_resident_kb::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "shared_dirty_resident_kb", ColumnType::shared_dirty_resident_kb::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "shared_clean_resident_kb", ColumnType::shared_clean_resident_kb::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "locked_kb", ColumnType::locked_kb::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "proportional_resident_kb", ColumnType::proportional_resident_kb::SqlValueType(), false,
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
    ts_.ShrinkToFit();
    path_.ShrinkToFit();
    size_kb_.ShrinkToFit();
    private_dirty_kb_.ShrinkToFit();
    swap_kb_.ShrinkToFit();
    file_name_.ShrinkToFit();
    start_address_.ShrinkToFit();
    module_timestamp_.ShrinkToFit();
    module_debugid_.ShrinkToFit();
    module_debug_path_.ShrinkToFit();
    protection_flags_.ShrinkToFit();
    private_clean_resident_kb_.ShrinkToFit();
    shared_dirty_resident_kb_.ShrinkToFit();
    shared_clean_resident_kb_.ShrinkToFit();
    locked_kb_.ShrinkToFit();
    proportional_resident_kb_.ShrinkToFit();
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
    mutable_ts()->Append(row.ts);
    mutable_path()->Append(row.path);
    mutable_size_kb()->Append(row.size_kb);
    mutable_private_dirty_kb()->Append(row.private_dirty_kb);
    mutable_swap_kb()->Append(row.swap_kb);
    mutable_file_name()->Append(row.file_name);
    mutable_start_address()->Append(row.start_address);
    mutable_module_timestamp()->Append(row.module_timestamp);
    mutable_module_debugid()->Append(row.module_debugid);
    mutable_module_debug_path()->Append(row.module_debug_path);
    mutable_protection_flags()->Append(row.protection_flags);
    mutable_private_clean_resident_kb()->Append(row.private_clean_resident_kb);
    mutable_shared_dirty_resident_kb()->Append(row.shared_dirty_resident_kb);
    mutable_shared_clean_resident_kb()->Append(row.shared_clean_resident_kb);
    mutable_locked_kb()->Append(row.locked_kb);
    mutable_proportional_resident_kb()->Append(row.proportional_resident_kb);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ProfilerSmapsTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<int64_t>& ts() const {
    return static_cast<const ColumnType::ts&>(columns()[ColumnIndex::ts]);
  }
  const TypedColumn<StringPool::Id>& path() const {
    return static_cast<const ColumnType::path&>(columns()[ColumnIndex::path]);
  }
  const TypedColumn<int64_t>& size_kb() const {
    return static_cast<const ColumnType::size_kb&>(columns()[ColumnIndex::size_kb]);
  }
  const TypedColumn<int64_t>& private_dirty_kb() const {
    return static_cast<const ColumnType::private_dirty_kb&>(columns()[ColumnIndex::private_dirty_kb]);
  }
  const TypedColumn<int64_t>& swap_kb() const {
    return static_cast<const ColumnType::swap_kb&>(columns()[ColumnIndex::swap_kb]);
  }
  const TypedColumn<StringPool::Id>& file_name() const {
    return static_cast<const ColumnType::file_name&>(columns()[ColumnIndex::file_name]);
  }
  const TypedColumn<int64_t>& start_address() const {
    return static_cast<const ColumnType::start_address&>(columns()[ColumnIndex::start_address]);
  }
  const TypedColumn<int64_t>& module_timestamp() const {
    return static_cast<const ColumnType::module_timestamp&>(columns()[ColumnIndex::module_timestamp]);
  }
  const TypedColumn<StringPool::Id>& module_debugid() const {
    return static_cast<const ColumnType::module_debugid&>(columns()[ColumnIndex::module_debugid]);
  }
  const TypedColumn<StringPool::Id>& module_debug_path() const {
    return static_cast<const ColumnType::module_debug_path&>(columns()[ColumnIndex::module_debug_path]);
  }
  const TypedColumn<int64_t>& protection_flags() const {
    return static_cast<const ColumnType::protection_flags&>(columns()[ColumnIndex::protection_flags]);
  }
  const TypedColumn<int64_t>& private_clean_resident_kb() const {
    return static_cast<const ColumnType::private_clean_resident_kb&>(columns()[ColumnIndex::private_clean_resident_kb]);
  }
  const TypedColumn<int64_t>& shared_dirty_resident_kb() const {
    return static_cast<const ColumnType::shared_dirty_resident_kb&>(columns()[ColumnIndex::shared_dirty_resident_kb]);
  }
  const TypedColumn<int64_t>& shared_clean_resident_kb() const {
    return static_cast<const ColumnType::shared_clean_resident_kb&>(columns()[ColumnIndex::shared_clean_resident_kb]);
  }
  const TypedColumn<int64_t>& locked_kb() const {
    return static_cast<const ColumnType::locked_kb&>(columns()[ColumnIndex::locked_kb]);
  }
  const TypedColumn<int64_t>& proportional_resident_kb() const {
    return static_cast<const ColumnType::proportional_resident_kb&>(columns()[ColumnIndex::proportional_resident_kb]);
  }

  TypedColumn<uint32_t>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<int64_t>* mutable_ts() {
    return static_cast<ColumnType::ts*>(
        GetColumn(ColumnIndex::ts));
  }
  TypedColumn<StringPool::Id>* mutable_path() {
    return static_cast<ColumnType::path*>(
        GetColumn(ColumnIndex::path));
  }
  TypedColumn<int64_t>* mutable_size_kb() {
    return static_cast<ColumnType::size_kb*>(
        GetColumn(ColumnIndex::size_kb));
  }
  TypedColumn<int64_t>* mutable_private_dirty_kb() {
    return static_cast<ColumnType::private_dirty_kb*>(
        GetColumn(ColumnIndex::private_dirty_kb));
  }
  TypedColumn<int64_t>* mutable_swap_kb() {
    return static_cast<ColumnType::swap_kb*>(
        GetColumn(ColumnIndex::swap_kb));
  }
  TypedColumn<StringPool::Id>* mutable_file_name() {
    return static_cast<ColumnType::file_name*>(
        GetColumn(ColumnIndex::file_name));
  }
  TypedColumn<int64_t>* mutable_start_address() {
    return static_cast<ColumnType::start_address*>(
        GetColumn(ColumnIndex::start_address));
  }
  TypedColumn<int64_t>* mutable_module_timestamp() {
    return static_cast<ColumnType::module_timestamp*>(
        GetColumn(ColumnIndex::module_timestamp));
  }
  TypedColumn<StringPool::Id>* mutable_module_debugid() {
    return static_cast<ColumnType::module_debugid*>(
        GetColumn(ColumnIndex::module_debugid));
  }
  TypedColumn<StringPool::Id>* mutable_module_debug_path() {
    return static_cast<ColumnType::module_debug_path*>(
        GetColumn(ColumnIndex::module_debug_path));
  }
  TypedColumn<int64_t>* mutable_protection_flags() {
    return static_cast<ColumnType::protection_flags*>(
        GetColumn(ColumnIndex::protection_flags));
  }
  TypedColumn<int64_t>* mutable_private_clean_resident_kb() {
    return static_cast<ColumnType::private_clean_resident_kb*>(
        GetColumn(ColumnIndex::private_clean_resident_kb));
  }
  TypedColumn<int64_t>* mutable_shared_dirty_resident_kb() {
    return static_cast<ColumnType::shared_dirty_resident_kb*>(
        GetColumn(ColumnIndex::shared_dirty_resident_kb));
  }
  TypedColumn<int64_t>* mutable_shared_clean_resident_kb() {
    return static_cast<ColumnType::shared_clean_resident_kb*>(
        GetColumn(ColumnIndex::shared_clean_resident_kb));
  }
  TypedColumn<int64_t>* mutable_locked_kb() {
    return static_cast<ColumnType::locked_kb*>(
        GetColumn(ColumnIndex::locked_kb));
  }
  TypedColumn<int64_t>* mutable_proportional_resident_kb() {
    return static_cast<ColumnType::proportional_resident_kb*>(
        GetColumn(ColumnIndex::proportional_resident_kb));
  }

 private:
  
  
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::ts::stored_type> ts_;
  ColumnStorage<ColumnType::path::stored_type> path_;
  ColumnStorage<ColumnType::size_kb::stored_type> size_kb_;
  ColumnStorage<ColumnType::private_dirty_kb::stored_type> private_dirty_kb_;
  ColumnStorage<ColumnType::swap_kb::stored_type> swap_kb_;
  ColumnStorage<ColumnType::file_name::stored_type> file_name_;
  ColumnStorage<ColumnType::start_address::stored_type> start_address_;
  ColumnStorage<ColumnType::module_timestamp::stored_type> module_timestamp_;
  ColumnStorage<ColumnType::module_debugid::stored_type> module_debugid_;
  ColumnStorage<ColumnType::module_debug_path::stored_type> module_debug_path_;
  ColumnStorage<ColumnType::protection_flags::stored_type> protection_flags_;
  ColumnStorage<ColumnType::private_clean_resident_kb::stored_type> private_clean_resident_kb_;
  ColumnStorage<ColumnType::shared_dirty_resident_kb::stored_type> shared_dirty_resident_kb_;
  ColumnStorage<ColumnType::shared_clean_resident_kb::stored_type> shared_clean_resident_kb_;
  ColumnStorage<ColumnType::locked_kb::stored_type> locked_kb_;
  ColumnStorage<ColumnType::proportional_resident_kb::stored_type> proportional_resident_kb_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> ts_storage_layer_;
  RefPtr<column::StorageLayer> path_storage_layer_;
  RefPtr<column::StorageLayer> size_kb_storage_layer_;
  RefPtr<column::StorageLayer> private_dirty_kb_storage_layer_;
  RefPtr<column::StorageLayer> swap_kb_storage_layer_;
  RefPtr<column::StorageLayer> file_name_storage_layer_;
  RefPtr<column::StorageLayer> start_address_storage_layer_;
  RefPtr<column::StorageLayer> module_timestamp_storage_layer_;
  RefPtr<column::StorageLayer> module_debugid_storage_layer_;
  RefPtr<column::StorageLayer> module_debug_path_storage_layer_;
  RefPtr<column::StorageLayer> protection_flags_storage_layer_;
  RefPtr<column::StorageLayer> private_clean_resident_kb_storage_layer_;
  RefPtr<column::StorageLayer> shared_dirty_resident_kb_storage_layer_;
  RefPtr<column::StorageLayer> shared_clean_resident_kb_storage_layer_;
  RefPtr<column::StorageLayer> locked_kb_storage_layer_;
  RefPtr<column::StorageLayer> proportional_resident_kb_storage_layer_;

  
};
  

class SymbolTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t symbol_set_id = 2;
    static constexpr uint32_t name = 3;
    static constexpr uint32_t source_file = 4;
    static constexpr uint32_t line_number = 5;
  };
  struct ColumnType {
    using id = IdColumn<SymbolTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using symbol_set_id = TypedColumn<uint32_t>;
    using name = TypedColumn<StringPool::Id>;
    using source_file = TypedColumn<std::optional<StringPool::Id>>;
    using line_number = TypedColumn<std::optional<uint32_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_symbol_set_id = {},
        StringPool::Id in_name = {},
        std::optional<StringPool::Id> in_source_file = {},
        std::optional<uint32_t> in_line_number = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          symbol_set_id(in_symbol_set_id),
          name(in_name),
          source_file(in_source_file),
          line_number(in_line_number) {
      type_ = "stack_profile_symbol";
    }
    uint32_t symbol_set_id;
    StringPool::Id name;
    std::optional<StringPool::Id> source_file;
    std::optional<uint32_t> line_number;

    bool operator==(const SymbolTable::Row& other) const {
      return type() == other.type() && ColumnType::symbol_set_id::Equals(symbol_set_id, other.symbol_set_id) &&
       ColumnType::name::Equals(name, other.name) &&
       ColumnType::source_file::Equals(source_file, other.source_file) &&
       ColumnType::line_number::Equals(line_number, other.line_number);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t symbol_set_id = static_cast<uint32_t>(ColumnLegacy::Flag::kSorted | ColumnLegacy::Flag::kSetId) | ColumnType::symbol_set_id::default_flags();
    static constexpr uint32_t name = ColumnType::name::default_flags();
    static constexpr uint32_t source_file = ColumnType::source_file::default_flags();
    static constexpr uint32_t line_number = ColumnType::line_number::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      SymbolTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    SymbolTable, RowNumber> {
   public:
    ConstRowReference(const SymbolTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::symbol_set_id::type symbol_set_id() const {
      return table_->symbol_set_id()[row_number_];
    }
    ColumnType::name::type name() const {
      return table_->name()[row_number_];
    }
    ColumnType::source_file::type source_file() const {
      return table_->source_file()[row_number_];
    }
    ColumnType::line_number::type line_number() const {
      return table_->line_number()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const SymbolTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_symbol_set_id(
        ColumnType::symbol_set_id::non_optional_type v) {
      return mutable_table()->mutable_symbol_set_id()->Set(row_number_, v);
    }
    void set_name(
        ColumnType::name::non_optional_type v) {
      return mutable_table()->mutable_name()->Set(row_number_, v);
    }
    void set_source_file(
        ColumnType::source_file::non_optional_type v) {
      return mutable_table()->mutable_source_file()->Set(row_number_, v);
    }
    void set_line_number(
        ColumnType::line_number::non_optional_type v) {
      return mutable_table()->mutable_line_number()->Set(row_number_, v);
    }

   private:
    SymbolTable* mutable_table() const {
      return const_cast<SymbolTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, SymbolTable, RowNumber, ConstRowReference> {
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
    ColumnType::symbol_set_id::type symbol_set_id() const {
      const auto& col = table_->symbol_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::name::type name() const {
      const auto& col = table_->name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::source_file::type source_file() const {
      const auto& col = table_->source_file();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::line_number::type line_number() const {
      const auto& col = table_->line_number();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const SymbolTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class SymbolTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, SymbolTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class SymbolTable;

     explicit Iterator(SymbolTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     SymbolTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      SymbolTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "symbol_set_id", &self->symbol_set_id_, ColumnFlag::symbol_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "name", &self->name_, ColumnFlag::name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "source_file", &self->source_file_, ColumnFlag::source_file,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "line_number", &self->line_number_, ColumnFlag::line_number,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit SymbolTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        symbol_set_id_(ColumnStorage<ColumnType::symbol_set_id::stored_type>::Create<false>()),
        name_(ColumnStorage<ColumnType::name::stored_type>::Create<false>()),
        source_file_(ColumnStorage<ColumnType::source_file::stored_type>::Create<false>()),
        line_number_(ColumnStorage<ColumnType::line_number::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        symbol_set_id_storage_layer_(
          new column::SetIdStorage(&symbol_set_id_.vector())),
        name_storage_layer_(
          new column::StringStorage(string_pool(), &name_.vector())),
        source_file_storage_layer_(
          new column::StringStorage(string_pool(), &source_file_.vector())),
        line_number_storage_layer_(
          new column::NumericStorage<ColumnType::line_number::non_optional_stored_type>(
            &line_number_.non_null_vector(),
            ColumnTypeHelper<ColumnType::line_number::stored_type>::ToColumnType(),
            false))
,
        line_number_null_layer_(new column::NullOverlay(line_number_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::symbol_set_id::stored_type>(
          ColumnFlag::symbol_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::name::stored_type>(
          ColumnFlag::name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::source_file::stored_type>(
          ColumnFlag::source_file),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::line_number::stored_type>(
          ColumnFlag::line_number),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,symbol_set_id_storage_layer_,name_storage_layer_,source_file_storage_layer_,line_number_storage_layer_},
      {{},{},{},{},{},line_number_null_layer_});
  }
  ~SymbolTable() override;

  static const char* Name() { return "stack_profile_symbol"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "symbol_set_id", ColumnType::symbol_set_id::SqlValueType(), false,
        true,
        false,
        true});
    schema.columns.emplace_back(Table::Schema::Column{
        "name", ColumnType::name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source_file", ColumnType::source_file::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "line_number", ColumnType::line_number::SqlValueType(), false,
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
    symbol_set_id_.ShrinkToFit();
    name_.ShrinkToFit();
    source_file_.ShrinkToFit();
    line_number_.ShrinkToFit();
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
    mutable_symbol_set_id()->Append(row.symbol_set_id);
    mutable_name()->Append(row.name);
    mutable_source_file()->Append(row.source_file);
    mutable_line_number()->Append(row.line_number);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<SymbolTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& symbol_set_id() const {
    return static_cast<const ColumnType::symbol_set_id&>(columns()[ColumnIndex::symbol_set_id]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& source_file() const {
    return static_cast<const ColumnType::source_file&>(columns()[ColumnIndex::source_file]);
  }
  const TypedColumn<std::optional<uint32_t>>& line_number() const {
    return static_cast<const ColumnType::line_number&>(columns()[ColumnIndex::line_number]);
  }

  TypedColumn<uint32_t>* mutable_symbol_set_id() {
    return static_cast<ColumnType::symbol_set_id*>(
        GetColumn(ColumnIndex::symbol_set_id));
  }
  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_source_file() {
    return static_cast<ColumnType::source_file*>(
        GetColumn(ColumnIndex::source_file));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_line_number() {
    return static_cast<ColumnType::line_number*>(
        GetColumn(ColumnIndex::line_number));
  }

 private:
  
  
  ColumnStorage<ColumnType::symbol_set_id::stored_type> symbol_set_id_;
  ColumnStorage<ColumnType::name::stored_type> name_;
  ColumnStorage<ColumnType::source_file::stored_type> source_file_;
  ColumnStorage<ColumnType::line_number::stored_type> line_number_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> symbol_set_id_storage_layer_;
  RefPtr<column::StorageLayer> name_storage_layer_;
  RefPtr<column::StorageLayer> source_file_storage_layer_;
  RefPtr<column::StorageLayer> line_number_storage_layer_;

  RefPtr<column::OverlayLayer> line_number_null_layer_;
};
  

class VulkanMemoryAllocationsTable : public macros_internal::MacroTable {
 public:
  static constexpr uint32_t kColumnCount = 16;

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
    static constexpr uint32_t source = 3;
    static constexpr uint32_t operation = 4;
    static constexpr uint32_t timestamp = 5;
    static constexpr uint32_t upid = 6;
    static constexpr uint32_t device = 7;
    static constexpr uint32_t device_memory = 8;
    static constexpr uint32_t memory_type = 9;
    static constexpr uint32_t heap = 10;
    static constexpr uint32_t function_name = 11;
    static constexpr uint32_t object_handle = 12;
    static constexpr uint32_t memory_address = 13;
    static constexpr uint32_t memory_size = 14;
    static constexpr uint32_t scope = 15;
  };
  struct ColumnType {
    using id = IdColumn<VulkanMemoryAllocationsTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using source = TypedColumn<StringPool::Id>;
    using operation = TypedColumn<StringPool::Id>;
    using timestamp = TypedColumn<int64_t>;
    using upid = TypedColumn<std::optional<uint32_t>>;
    using device = TypedColumn<std::optional<int64_t>>;
    using device_memory = TypedColumn<std::optional<int64_t>>;
    using memory_type = TypedColumn<std::optional<uint32_t>>;
    using heap = TypedColumn<std::optional<uint32_t>>;
    using function_name = TypedColumn<std::optional<StringPool::Id>>;
    using object_handle = TypedColumn<std::optional<int64_t>>;
    using memory_address = TypedColumn<std::optional<int64_t>>;
    using memory_size = TypedColumn<std::optional<int64_t>>;
    using scope = TypedColumn<StringPool::Id>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(std::optional<uint32_t> in_arg_set_id = {},
        StringPool::Id in_source = {},
        StringPool::Id in_operation = {},
        int64_t in_timestamp = {},
        std::optional<uint32_t> in_upid = {},
        std::optional<int64_t> in_device = {},
        std::optional<int64_t> in_device_memory = {},
        std::optional<uint32_t> in_memory_type = {},
        std::optional<uint32_t> in_heap = {},
        std::optional<StringPool::Id> in_function_name = {},
        std::optional<int64_t> in_object_handle = {},
        std::optional<int64_t> in_memory_address = {},
        std::optional<int64_t> in_memory_size = {},
        StringPool::Id in_scope = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          arg_set_id(in_arg_set_id),
          source(in_source),
          operation(in_operation),
          timestamp(in_timestamp),
          upid(in_upid),
          device(in_device),
          device_memory(in_device_memory),
          memory_type(in_memory_type),
          heap(in_heap),
          function_name(in_function_name),
          object_handle(in_object_handle),
          memory_address(in_memory_address),
          memory_size(in_memory_size),
          scope(in_scope) {
      type_ = "vulkan_memory_allocations";
    }
    std::optional<uint32_t> arg_set_id;
    StringPool::Id source;
    StringPool::Id operation;
    int64_t timestamp;
    std::optional<uint32_t> upid;
    std::optional<int64_t> device;
    std::optional<int64_t> device_memory;
    std::optional<uint32_t> memory_type;
    std::optional<uint32_t> heap;
    std::optional<StringPool::Id> function_name;
    std::optional<int64_t> object_handle;
    std::optional<int64_t> memory_address;
    std::optional<int64_t> memory_size;
    StringPool::Id scope;

    bool operator==(const VulkanMemoryAllocationsTable::Row& other) const {
      return type() == other.type() && ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id) &&
       ColumnType::source::Equals(source, other.source) &&
       ColumnType::operation::Equals(operation, other.operation) &&
       ColumnType::timestamp::Equals(timestamp, other.timestamp) &&
       ColumnType::upid::Equals(upid, other.upid) &&
       ColumnType::device::Equals(device, other.device) &&
       ColumnType::device_memory::Equals(device_memory, other.device_memory) &&
       ColumnType::memory_type::Equals(memory_type, other.memory_type) &&
       ColumnType::heap::Equals(heap, other.heap) &&
       ColumnType::function_name::Equals(function_name, other.function_name) &&
       ColumnType::object_handle::Equals(object_handle, other.object_handle) &&
       ColumnType::memory_address::Equals(memory_address, other.memory_address) &&
       ColumnType::memory_size::Equals(memory_size, other.memory_size) &&
       ColumnType::scope::Equals(scope, other.scope);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
    static constexpr uint32_t source = ColumnType::source::default_flags();
    static constexpr uint32_t operation = ColumnType::operation::default_flags();
    static constexpr uint32_t timestamp = ColumnType::timestamp::default_flags();
    static constexpr uint32_t upid = ColumnType::upid::default_flags();
    static constexpr uint32_t device = ColumnType::device::default_flags();
    static constexpr uint32_t device_memory = ColumnType::device_memory::default_flags();
    static constexpr uint32_t memory_type = ColumnType::memory_type::default_flags();
    static constexpr uint32_t heap = ColumnType::heap::default_flags();
    static constexpr uint32_t function_name = ColumnType::function_name::default_flags();
    static constexpr uint32_t object_handle = ColumnType::object_handle::default_flags();
    static constexpr uint32_t memory_address = ColumnType::memory_address::default_flags();
    static constexpr uint32_t memory_size = ColumnType::memory_size::default_flags();
    static constexpr uint32_t scope = ColumnType::scope::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      VulkanMemoryAllocationsTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    VulkanMemoryAllocationsTable, RowNumber> {
   public:
    ConstRowReference(const VulkanMemoryAllocationsTable* table, uint32_t row_number)
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
    ColumnType::source::type source() const {
      return table_->source()[row_number_];
    }
    ColumnType::operation::type operation() const {
      return table_->operation()[row_number_];
    }
    ColumnType::timestamp::type timestamp() const {
      return table_->timestamp()[row_number_];
    }
    ColumnType::upid::type upid() const {
      return table_->upid()[row_number_];
    }
    ColumnType::device::type device() const {
      return table_->device()[row_number_];
    }
    ColumnType::device_memory::type device_memory() const {
      return table_->device_memory()[row_number_];
    }
    ColumnType::memory_type::type memory_type() const {
      return table_->memory_type()[row_number_];
    }
    ColumnType::heap::type heap() const {
      return table_->heap()[row_number_];
    }
    ColumnType::function_name::type function_name() const {
      return table_->function_name()[row_number_];
    }
    ColumnType::object_handle::type object_handle() const {
      return table_->object_handle()[row_number_];
    }
    ColumnType::memory_address::type memory_address() const {
      return table_->memory_address()[row_number_];
    }
    ColumnType::memory_size::type memory_size() const {
      return table_->memory_size()[row_number_];
    }
    ColumnType::scope::type scope() const {
      return table_->scope()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const VulkanMemoryAllocationsTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }
    void set_source(
        ColumnType::source::non_optional_type v) {
      return mutable_table()->mutable_source()->Set(row_number_, v);
    }
    void set_operation(
        ColumnType::operation::non_optional_type v) {
      return mutable_table()->mutable_operation()->Set(row_number_, v);
    }
    void set_timestamp(
        ColumnType::timestamp::non_optional_type v) {
      return mutable_table()->mutable_timestamp()->Set(row_number_, v);
    }
    void set_upid(
        ColumnType::upid::non_optional_type v) {
      return mutable_table()->mutable_upid()->Set(row_number_, v);
    }
    void set_device(
        ColumnType::device::non_optional_type v) {
      return mutable_table()->mutable_device()->Set(row_number_, v);
    }
    void set_device_memory(
        ColumnType::device_memory::non_optional_type v) {
      return mutable_table()->mutable_device_memory()->Set(row_number_, v);
    }
    void set_memory_type(
        ColumnType::memory_type::non_optional_type v) {
      return mutable_table()->mutable_memory_type()->Set(row_number_, v);
    }
    void set_heap(
        ColumnType::heap::non_optional_type v) {
      return mutable_table()->mutable_heap()->Set(row_number_, v);
    }
    void set_function_name(
        ColumnType::function_name::non_optional_type v) {
      return mutable_table()->mutable_function_name()->Set(row_number_, v);
    }
    void set_object_handle(
        ColumnType::object_handle::non_optional_type v) {
      return mutable_table()->mutable_object_handle()->Set(row_number_, v);
    }
    void set_memory_address(
        ColumnType::memory_address::non_optional_type v) {
      return mutable_table()->mutable_memory_address()->Set(row_number_, v);
    }
    void set_memory_size(
        ColumnType::memory_size::non_optional_type v) {
      return mutable_table()->mutable_memory_size()->Set(row_number_, v);
    }
    void set_scope(
        ColumnType::scope::non_optional_type v) {
      return mutable_table()->mutable_scope()->Set(row_number_, v);
    }

   private:
    VulkanMemoryAllocationsTable* mutable_table() const {
      return const_cast<VulkanMemoryAllocationsTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, VulkanMemoryAllocationsTable, RowNumber, ConstRowReference> {
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
    ColumnType::source::type source() const {
      const auto& col = table_->source();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::operation::type operation() const {
      const auto& col = table_->operation();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::timestamp::type timestamp() const {
      const auto& col = table_->timestamp();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::upid::type upid() const {
      const auto& col = table_->upid();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::device::type device() const {
      const auto& col = table_->device();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::device_memory::type device_memory() const {
      const auto& col = table_->device_memory();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::memory_type::type memory_type() const {
      const auto& col = table_->memory_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::heap::type heap() const {
      const auto& col = table_->heap();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::function_name::type function_name() const {
      const auto& col = table_->function_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::object_handle::type object_handle() const {
      const auto& col = table_->object_handle();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::memory_address::type memory_address() const {
      const auto& col = table_->memory_address();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::memory_size::type memory_size() const {
      const auto& col = table_->memory_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::scope::type scope() const {
      const auto& col = table_->scope();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const VulkanMemoryAllocationsTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class VulkanMemoryAllocationsTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, VulkanMemoryAllocationsTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class VulkanMemoryAllocationsTable;

     explicit Iterator(VulkanMemoryAllocationsTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     VulkanMemoryAllocationsTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      VulkanMemoryAllocationsTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "source", &self->source_, ColumnFlag::source,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "operation", &self->operation_, ColumnFlag::operation,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "timestamp", &self->timestamp_, ColumnFlag::timestamp,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "upid", &self->upid_, ColumnFlag::upid,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "device", &self->device_, ColumnFlag::device,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "device_memory", &self->device_memory_, ColumnFlag::device_memory,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "memory_type", &self->memory_type_, ColumnFlag::memory_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "heap", &self->heap_, ColumnFlag::heap,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "function_name", &self->function_name_, ColumnFlag::function_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "object_handle", &self->object_handle_, ColumnFlag::object_handle,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "memory_address", &self->memory_address_, ColumnFlag::memory_address,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "memory_size", &self->memory_size_, ColumnFlag::memory_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "scope", &self->scope_, ColumnFlag::scope,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit VulkanMemoryAllocationsTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>()),
        source_(ColumnStorage<ColumnType::source::stored_type>::Create<false>()),
        operation_(ColumnStorage<ColumnType::operation::stored_type>::Create<false>()),
        timestamp_(ColumnStorage<ColumnType::timestamp::stored_type>::Create<false>()),
        upid_(ColumnStorage<ColumnType::upid::stored_type>::Create<false>()),
        device_(ColumnStorage<ColumnType::device::stored_type>::Create<false>()),
        device_memory_(ColumnStorage<ColumnType::device_memory::stored_type>::Create<false>()),
        memory_type_(ColumnStorage<ColumnType::memory_type::stored_type>::Create<false>()),
        heap_(ColumnStorage<ColumnType::heap::stored_type>::Create<false>()),
        function_name_(ColumnStorage<ColumnType::function_name::stored_type>::Create<false>()),
        object_handle_(ColumnStorage<ColumnType::object_handle::stored_type>::Create<false>()),
        memory_address_(ColumnStorage<ColumnType::memory_address::stored_type>::Create<false>()),
        memory_size_(ColumnStorage<ColumnType::memory_size::stored_type>::Create<false>()),
        scope_(ColumnStorage<ColumnType::scope::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        arg_set_id_storage_layer_(
          new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
            &arg_set_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
            false)),
        source_storage_layer_(
          new column::StringStorage(string_pool(), &source_.vector())),
        operation_storage_layer_(
          new column::StringStorage(string_pool(), &operation_.vector())),
        timestamp_storage_layer_(
        new column::NumericStorage<ColumnType::timestamp::non_optional_stored_type>(
          &timestamp_.vector(),
          ColumnTypeHelper<ColumnType::timestamp::stored_type>::ToColumnType(),
          false)),
        upid_storage_layer_(
          new column::NumericStorage<ColumnType::upid::non_optional_stored_type>(
            &upid_.non_null_vector(),
            ColumnTypeHelper<ColumnType::upid::stored_type>::ToColumnType(),
            false)),
        device_storage_layer_(
          new column::NumericStorage<ColumnType::device::non_optional_stored_type>(
            &device_.non_null_vector(),
            ColumnTypeHelper<ColumnType::device::stored_type>::ToColumnType(),
            false)),
        device_memory_storage_layer_(
          new column::NumericStorage<ColumnType::device_memory::non_optional_stored_type>(
            &device_memory_.non_null_vector(),
            ColumnTypeHelper<ColumnType::device_memory::stored_type>::ToColumnType(),
            false)),
        memory_type_storage_layer_(
          new column::NumericStorage<ColumnType::memory_type::non_optional_stored_type>(
            &memory_type_.non_null_vector(),
            ColumnTypeHelper<ColumnType::memory_type::stored_type>::ToColumnType(),
            false)),
        heap_storage_layer_(
          new column::NumericStorage<ColumnType::heap::non_optional_stored_type>(
            &heap_.non_null_vector(),
            ColumnTypeHelper<ColumnType::heap::stored_type>::ToColumnType(),
            false)),
        function_name_storage_layer_(
          new column::StringStorage(string_pool(), &function_name_.vector())),
        object_handle_storage_layer_(
          new column::NumericStorage<ColumnType::object_handle::non_optional_stored_type>(
            &object_handle_.non_null_vector(),
            ColumnTypeHelper<ColumnType::object_handle::stored_type>::ToColumnType(),
            false)),
        memory_address_storage_layer_(
          new column::NumericStorage<ColumnType::memory_address::non_optional_stored_type>(
            &memory_address_.non_null_vector(),
            ColumnTypeHelper<ColumnType::memory_address::stored_type>::ToColumnType(),
            false)),
        memory_size_storage_layer_(
          new column::NumericStorage<ColumnType::memory_size::non_optional_stored_type>(
            &memory_size_.non_null_vector(),
            ColumnTypeHelper<ColumnType::memory_size::stored_type>::ToColumnType(),
            false)),
        scope_storage_layer_(
          new column::StringStorage(string_pool(), &scope_.vector()))
,
        arg_set_id_null_layer_(new column::NullOverlay(arg_set_id_.bv())),
        upid_null_layer_(new column::NullOverlay(upid_.bv())),
        device_null_layer_(new column::NullOverlay(device_.bv())),
        device_memory_null_layer_(new column::NullOverlay(device_memory_.bv())),
        memory_type_null_layer_(new column::NullOverlay(memory_type_.bv())),
        heap_null_layer_(new column::NullOverlay(heap_.bv())),
        object_handle_null_layer_(new column::NullOverlay(object_handle_.bv())),
        memory_address_null_layer_(new column::NullOverlay(memory_address_.bv())),
        memory_size_null_layer_(new column::NullOverlay(memory_size_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::source::stored_type>(
          ColumnFlag::source),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::operation::stored_type>(
          ColumnFlag::operation),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::timestamp::stored_type>(
          ColumnFlag::timestamp),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::upid::stored_type>(
          ColumnFlag::upid),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::device::stored_type>(
          ColumnFlag::device),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::device_memory::stored_type>(
          ColumnFlag::device_memory),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::memory_type::stored_type>(
          ColumnFlag::memory_type),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::heap::stored_type>(
          ColumnFlag::heap),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::function_name::stored_type>(
          ColumnFlag::function_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::object_handle::stored_type>(
          ColumnFlag::object_handle),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::memory_address::stored_type>(
          ColumnFlag::memory_address),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::memory_size::stored_type>(
          ColumnFlag::memory_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::scope::stored_type>(
          ColumnFlag::scope),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,arg_set_id_storage_layer_,source_storage_layer_,operation_storage_layer_,timestamp_storage_layer_,upid_storage_layer_,device_storage_layer_,device_memory_storage_layer_,memory_type_storage_layer_,heap_storage_layer_,function_name_storage_layer_,object_handle_storage_layer_,memory_address_storage_layer_,memory_size_storage_layer_,scope_storage_layer_},
      {{},{},arg_set_id_null_layer_,{},{},{},upid_null_layer_,device_null_layer_,device_memory_null_layer_,memory_type_null_layer_,heap_null_layer_,{},object_handle_null_layer_,memory_address_null_layer_,memory_size_null_layer_,{}});
  }
  ~VulkanMemoryAllocationsTable() override;

  static const char* Name() { return "vulkan_memory_allocations"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "arg_set_id", ColumnType::arg_set_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "source", ColumnType::source::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "operation", ColumnType::operation::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "timestamp", ColumnType::timestamp::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "upid", ColumnType::upid::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "device", ColumnType::device::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "device_memory", ColumnType::device_memory::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "memory_type", ColumnType::memory_type::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "heap", ColumnType::heap::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "function_name", ColumnType::function_name::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "object_handle", ColumnType::object_handle::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "memory_address", ColumnType::memory_address::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "memory_size", ColumnType::memory_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "scope", ColumnType::scope::SqlValueType(), false,
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
    source_.ShrinkToFit();
    operation_.ShrinkToFit();
    timestamp_.ShrinkToFit();
    upid_.ShrinkToFit();
    device_.ShrinkToFit();
    device_memory_.ShrinkToFit();
    memory_type_.ShrinkToFit();
    heap_.ShrinkToFit();
    function_name_.ShrinkToFit();
    object_handle_.ShrinkToFit();
    memory_address_.ShrinkToFit();
    memory_size_.ShrinkToFit();
    scope_.ShrinkToFit();
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
    mutable_source()->Append(row.source);
    mutable_operation()->Append(row.operation);
    mutable_timestamp()->Append(row.timestamp);
    mutable_upid()->Append(row.upid);
    mutable_device()->Append(row.device);
    mutable_device_memory()->Append(row.device_memory);
    mutable_memory_type()->Append(row.memory_type);
    mutable_heap()->Append(row.heap);
    mutable_function_name()->Append(row.function_name);
    mutable_object_handle()->Append(row.object_handle);
    mutable_memory_address()->Append(row.memory_address);
    mutable_memory_size()->Append(row.memory_size);
    mutable_scope()->Append(row.scope);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<VulkanMemoryAllocationsTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<std::optional<uint32_t>>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }
  const TypedColumn<StringPool::Id>& source() const {
    return static_cast<const ColumnType::source&>(columns()[ColumnIndex::source]);
  }
  const TypedColumn<StringPool::Id>& operation() const {
    return static_cast<const ColumnType::operation&>(columns()[ColumnIndex::operation]);
  }
  const TypedColumn<int64_t>& timestamp() const {
    return static_cast<const ColumnType::timestamp&>(columns()[ColumnIndex::timestamp]);
  }
  const TypedColumn<std::optional<uint32_t>>& upid() const {
    return static_cast<const ColumnType::upid&>(columns()[ColumnIndex::upid]);
  }
  const TypedColumn<std::optional<int64_t>>& device() const {
    return static_cast<const ColumnType::device&>(columns()[ColumnIndex::device]);
  }
  const TypedColumn<std::optional<int64_t>>& device_memory() const {
    return static_cast<const ColumnType::device_memory&>(columns()[ColumnIndex::device_memory]);
  }
  const TypedColumn<std::optional<uint32_t>>& memory_type() const {
    return static_cast<const ColumnType::memory_type&>(columns()[ColumnIndex::memory_type]);
  }
  const TypedColumn<std::optional<uint32_t>>& heap() const {
    return static_cast<const ColumnType::heap&>(columns()[ColumnIndex::heap]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& function_name() const {
    return static_cast<const ColumnType::function_name&>(columns()[ColumnIndex::function_name]);
  }
  const TypedColumn<std::optional<int64_t>>& object_handle() const {
    return static_cast<const ColumnType::object_handle&>(columns()[ColumnIndex::object_handle]);
  }
  const TypedColumn<std::optional<int64_t>>& memory_address() const {
    return static_cast<const ColumnType::memory_address&>(columns()[ColumnIndex::memory_address]);
  }
  const TypedColumn<std::optional<int64_t>>& memory_size() const {
    return static_cast<const ColumnType::memory_size&>(columns()[ColumnIndex::memory_size]);
  }
  const TypedColumn<StringPool::Id>& scope() const {
    return static_cast<const ColumnType::scope&>(columns()[ColumnIndex::scope]);
  }

  TypedColumn<std::optional<uint32_t>>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }
  TypedColumn<StringPool::Id>* mutable_source() {
    return static_cast<ColumnType::source*>(
        GetColumn(ColumnIndex::source));
  }
  TypedColumn<StringPool::Id>* mutable_operation() {
    return static_cast<ColumnType::operation*>(
        GetColumn(ColumnIndex::operation));
  }
  TypedColumn<int64_t>* mutable_timestamp() {
    return static_cast<ColumnType::timestamp*>(
        GetColumn(ColumnIndex::timestamp));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_upid() {
    return static_cast<ColumnType::upid*>(
        GetColumn(ColumnIndex::upid));
  }
  TypedColumn<std::optional<int64_t>>* mutable_device() {
    return static_cast<ColumnType::device*>(
        GetColumn(ColumnIndex::device));
  }
  TypedColumn<std::optional<int64_t>>* mutable_device_memory() {
    return static_cast<ColumnType::device_memory*>(
        GetColumn(ColumnIndex::device_memory));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_memory_type() {
    return static_cast<ColumnType::memory_type*>(
        GetColumn(ColumnIndex::memory_type));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_heap() {
    return static_cast<ColumnType::heap*>(
        GetColumn(ColumnIndex::heap));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_function_name() {
    return static_cast<ColumnType::function_name*>(
        GetColumn(ColumnIndex::function_name));
  }
  TypedColumn<std::optional<int64_t>>* mutable_object_handle() {
    return static_cast<ColumnType::object_handle*>(
        GetColumn(ColumnIndex::object_handle));
  }
  TypedColumn<std::optional<int64_t>>* mutable_memory_address() {
    return static_cast<ColumnType::memory_address*>(
        GetColumn(ColumnIndex::memory_address));
  }
  TypedColumn<std::optional<int64_t>>* mutable_memory_size() {
    return static_cast<ColumnType::memory_size*>(
        GetColumn(ColumnIndex::memory_size));
  }
  TypedColumn<StringPool::Id>* mutable_scope() {
    return static_cast<ColumnType::scope*>(
        GetColumn(ColumnIndex::scope));
  }

 private:
  
  
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;
  ColumnStorage<ColumnType::source::stored_type> source_;
  ColumnStorage<ColumnType::operation::stored_type> operation_;
  ColumnStorage<ColumnType::timestamp::stored_type> timestamp_;
  ColumnStorage<ColumnType::upid::stored_type> upid_;
  ColumnStorage<ColumnType::device::stored_type> device_;
  ColumnStorage<ColumnType::device_memory::stored_type> device_memory_;
  ColumnStorage<ColumnType::memory_type::stored_type> memory_type_;
  ColumnStorage<ColumnType::heap::stored_type> heap_;
  ColumnStorage<ColumnType::function_name::stored_type> function_name_;
  ColumnStorage<ColumnType::object_handle::stored_type> object_handle_;
  ColumnStorage<ColumnType::memory_address::stored_type> memory_address_;
  ColumnStorage<ColumnType::memory_size::stored_type> memory_size_;
  ColumnStorage<ColumnType::scope::stored_type> scope_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;
  RefPtr<column::StorageLayer> source_storage_layer_;
  RefPtr<column::StorageLayer> operation_storage_layer_;
  RefPtr<column::StorageLayer> timestamp_storage_layer_;
  RefPtr<column::StorageLayer> upid_storage_layer_;
  RefPtr<column::StorageLayer> device_storage_layer_;
  RefPtr<column::StorageLayer> device_memory_storage_layer_;
  RefPtr<column::StorageLayer> memory_type_storage_layer_;
  RefPtr<column::StorageLayer> heap_storage_layer_;
  RefPtr<column::StorageLayer> function_name_storage_layer_;
  RefPtr<column::StorageLayer> object_handle_storage_layer_;
  RefPtr<column::StorageLayer> memory_address_storage_layer_;
  RefPtr<column::StorageLayer> memory_size_storage_layer_;
  RefPtr<column::StorageLayer> scope_storage_layer_;

  RefPtr<column::OverlayLayer> arg_set_id_null_layer_;
  RefPtr<column::OverlayLayer> upid_null_layer_;
  RefPtr<column::OverlayLayer> device_null_layer_;
  RefPtr<column::OverlayLayer> device_memory_null_layer_;
  RefPtr<column::OverlayLayer> memory_type_null_layer_;
  RefPtr<column::OverlayLayer> heap_null_layer_;
  RefPtr<column::OverlayLayer> object_handle_null_layer_;
  RefPtr<column::OverlayLayer> memory_address_null_layer_;
  RefPtr<column::OverlayLayer> memory_size_null_layer_;
};
  

class PerfCounterTrackTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t perf_session_id = 8;
    static constexpr uint32_t cpu = 9;
    static constexpr uint32_t is_timebase = 10;
  };
  struct ColumnType {
    using id = IdColumn<PerfCounterTrackTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using name = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<PerfCounterTrackTable::Id>>;
    using source_arg_set_id = TypedColumn<std::optional<uint32_t>>;
    using machine_id = TypedColumn<std::optional<MachineTable::Id>>;
    using unit = TypedColumn<StringPool::Id>;
    using description = TypedColumn<StringPool::Id>;
    using perf_session_id = TypedColumn<PerfSessionTable::Id>;
    using cpu = TypedColumn<uint32_t>;
    using is_timebase = TypedColumn<uint32_t>;
  };
  struct Row : public CounterTrackTable::Row {
    Row(StringPool::Id in_name = {},
        std::optional<PerfCounterTrackTable::Id> in_parent_id = {},
        std::optional<uint32_t> in_source_arg_set_id = {},
        std::optional<MachineTable::Id> in_machine_id = {},
        StringPool::Id in_unit = {},
        StringPool::Id in_description = {},
        PerfSessionTable::Id in_perf_session_id = {},
        uint32_t in_cpu = {},
        uint32_t in_is_timebase = {},
        std::nullptr_t = nullptr)
        : CounterTrackTable::Row(in_name, in_parent_id, in_source_arg_set_id, in_machine_id, in_unit, in_description),
          perf_session_id(in_perf_session_id),
          cpu(in_cpu),
          is_timebase(in_is_timebase) {
      type_ = "perf_counter_track";
    }
    PerfSessionTable::Id perf_session_id;
    uint32_t cpu;
    uint32_t is_timebase;

    bool operator==(const PerfCounterTrackTable::Row& other) const {
      return type() == other.type() && ColumnType::name::Equals(name, other.name) &&
       ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::source_arg_set_id::Equals(source_arg_set_id, other.source_arg_set_id) &&
       ColumnType::machine_id::Equals(machine_id, other.machine_id) &&
       ColumnType::unit::Equals(unit, other.unit) &&
       ColumnType::description::Equals(description, other.description) &&
       ColumnType::perf_session_id::Equals(perf_session_id, other.perf_session_id) &&
       ColumnType::cpu::Equals(cpu, other.cpu) &&
       ColumnType::is_timebase::Equals(is_timebase, other.is_timebase);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t perf_session_id = ColumnType::perf_session_id::default_flags();
    static constexpr uint32_t cpu = ColumnType::cpu::default_flags();
    static constexpr uint32_t is_timebase = ColumnType::is_timebase::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      PerfCounterTrackTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    PerfCounterTrackTable, RowNumber> {
   public:
    ConstRowReference(const PerfCounterTrackTable* table, uint32_t row_number)
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
    ColumnType::perf_session_id::type perf_session_id() const {
      return table_->perf_session_id()[row_number_];
    }
    ColumnType::cpu::type cpu() const {
      return table_->cpu()[row_number_];
    }
    ColumnType::is_timebase::type is_timebase() const {
      return table_->is_timebase()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const PerfCounterTrackTable* table, uint32_t row_number)
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
    void set_perf_session_id(
        ColumnType::perf_session_id::non_optional_type v) {
      return mutable_table()->mutable_perf_session_id()->Set(row_number_, v);
    }
    void set_cpu(
        ColumnType::cpu::non_optional_type v) {
      return mutable_table()->mutable_cpu()->Set(row_number_, v);
    }
    void set_is_timebase(
        ColumnType::is_timebase::non_optional_type v) {
      return mutable_table()->mutable_is_timebase()->Set(row_number_, v);
    }

   private:
    PerfCounterTrackTable* mutable_table() const {
      return const_cast<PerfCounterTrackTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, PerfCounterTrackTable, RowNumber, ConstRowReference> {
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
    ColumnType::perf_session_id::type perf_session_id() const {
      const auto& col = table_->perf_session_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::cpu::type cpu() const {
      const auto& col = table_->cpu();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::is_timebase::type is_timebase() const {
      const auto& col = table_->is_timebase();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const PerfCounterTrackTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class PerfCounterTrackTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, PerfCounterTrackTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class PerfCounterTrackTable;

     explicit Iterator(PerfCounterTrackTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     PerfCounterTrackTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      PerfCounterTrackTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "perf_session_id", &self->perf_session_id_, ColumnFlag::perf_session_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "cpu", &self->cpu_, ColumnFlag::cpu,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "is_timebase", &self->is_timebase_, ColumnFlag::is_timebase,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit PerfCounterTrackTable(StringPool* pool, CounterTrackTable* parent)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, parent),
          parent),
        parent_(parent), const_parent_(parent), perf_session_id_(ColumnStorage<ColumnType::perf_session_id::stored_type>::Create<false>()),
        cpu_(ColumnStorage<ColumnType::cpu::stored_type>::Create<false>()),
        is_timebase_(ColumnStorage<ColumnType::is_timebase::stored_type>::Create<false>())
,
        perf_session_id_storage_layer_(
        new column::NumericStorage<ColumnType::perf_session_id::non_optional_stored_type>(
          &perf_session_id_.vector(),
          ColumnTypeHelper<ColumnType::perf_session_id::stored_type>::ToColumnType(),
          false)),
        cpu_storage_layer_(
        new column::NumericStorage<ColumnType::cpu::non_optional_stored_type>(
          &cpu_.vector(),
          ColumnTypeHelper<ColumnType::cpu::stored_type>::ToColumnType(),
          false)),
        is_timebase_storage_layer_(
        new column::NumericStorage<ColumnType::is_timebase::non_optional_stored_type>(
          &is_timebase_.vector(),
          ColumnTypeHelper<ColumnType::is_timebase::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::perf_session_id::stored_type>(
          ColumnFlag::perf_session_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cpu::stored_type>(
          ColumnFlag::cpu),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::is_timebase::stored_type>(
          ColumnFlag::is_timebase),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],perf_session_id_storage_layer_,cpu_storage_layer_,is_timebase_storage_layer_},
      {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{},{},{}});
  }
  ~PerfCounterTrackTable() override;

  static const char* Name() { return "perf_counter_track"; }

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
        "perf_session_id", ColumnType::perf_session_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "cpu", ColumnType::cpu::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "is_timebase", ColumnType::is_timebase::SqlValueType(), false,
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
    perf_session_id_.ShrinkToFit();
    cpu_.ShrinkToFit();
    is_timebase_.ShrinkToFit();
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
    mutable_perf_session_id()->Append(row.perf_session_id);
    mutable_cpu()->Append(row.cpu);
    mutable_is_timebase()->Append(row.is_timebase);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  static std::unique_ptr<Table> ExtendParent(
      const CounterTrackTable& parent,
      ColumnStorage<ColumnType::perf_session_id::stored_type> perf_session_id
, ColumnStorage<ColumnType::cpu::stored_type> cpu
, ColumnStorage<ColumnType::is_timebase::stored_type> is_timebase) {
    return std::unique_ptr<Table>(new PerfCounterTrackTable(
        parent.string_pool(), parent, RowMap(0, parent.row_count()),
        std::move(perf_session_id), std::move(cpu), std::move(is_timebase)));
  }

  static std::unique_ptr<Table> SelectAndExtendParent(
      const CounterTrackTable& parent,
      std::vector<CounterTrackTable::RowNumber> parent_overlay,
      ColumnStorage<ColumnType::perf_session_id::stored_type> perf_session_id
, ColumnStorage<ColumnType::cpu::stored_type> cpu
, ColumnStorage<ColumnType::is_timebase::stored_type> is_timebase) {
    std::vector<uint32_t> prs_untyped(parent_overlay.size());
    for (uint32_t i = 0; i < parent_overlay.size(); ++i) {
      prs_untyped[i] = parent_overlay[i].row_number();
    }
    return std::unique_ptr<Table>(new PerfCounterTrackTable(
        parent.string_pool(), parent, RowMap(std::move(prs_untyped)),
        std::move(perf_session_id), std::move(cpu), std::move(is_timebase)));
  }

  const IdColumn<PerfCounterTrackTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& name() const {
    return static_cast<const ColumnType::name&>(columns()[ColumnIndex::name]);
  }
  const TypedColumn<std::optional<PerfCounterTrackTable::Id>>& parent_id() const {
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
  const TypedColumn<PerfSessionTable::Id>& perf_session_id() const {
    return static_cast<const ColumnType::perf_session_id&>(columns()[ColumnIndex::perf_session_id]);
  }
  const TypedColumn<uint32_t>& cpu() const {
    return static_cast<const ColumnType::cpu&>(columns()[ColumnIndex::cpu]);
  }
  const TypedColumn<uint32_t>& is_timebase() const {
    return static_cast<const ColumnType::is_timebase&>(columns()[ColumnIndex::is_timebase]);
  }

  TypedColumn<StringPool::Id>* mutable_name() {
    return static_cast<ColumnType::name*>(
        GetColumn(ColumnIndex::name));
  }
  TypedColumn<std::optional<PerfCounterTrackTable::Id>>* mutable_parent_id() {
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
  TypedColumn<PerfSessionTable::Id>* mutable_perf_session_id() {
    return static_cast<ColumnType::perf_session_id*>(
        GetColumn(ColumnIndex::perf_session_id));
  }
  TypedColumn<uint32_t>* mutable_cpu() {
    return static_cast<ColumnType::cpu*>(
        GetColumn(ColumnIndex::cpu));
  }
  TypedColumn<uint32_t>* mutable_is_timebase() {
    return static_cast<ColumnType::is_timebase*>(
        GetColumn(ColumnIndex::is_timebase));
  }

 private:
  PerfCounterTrackTable(StringPool* pool,
            const CounterTrackTable& parent,
            const RowMap& parent_overlay,
            ColumnStorage<ColumnType::perf_session_id::stored_type> perf_session_id
, ColumnStorage<ColumnType::cpu::stored_type> cpu
, ColumnStorage<ColumnType::is_timebase::stored_type> is_timebase)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, &parent),
          parent,
          parent_overlay),
          const_parent_(&parent)
,
        perf_session_id_storage_layer_(
        new column::NumericStorage<ColumnType::perf_session_id::non_optional_stored_type>(
          &perf_session_id_.vector(),
          ColumnTypeHelper<ColumnType::perf_session_id::stored_type>::ToColumnType(),
          false)),
        cpu_storage_layer_(
        new column::NumericStorage<ColumnType::cpu::non_optional_stored_type>(
          &cpu_.vector(),
          ColumnTypeHelper<ColumnType::cpu::stored_type>::ToColumnType(),
          false)),
        is_timebase_storage_layer_(
        new column::NumericStorage<ColumnType::is_timebase::non_optional_stored_type>(
          &is_timebase_.vector(),
          ColumnTypeHelper<ColumnType::is_timebase::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::perf_session_id::stored_type>(
          ColumnFlag::perf_session_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::cpu::stored_type>(
          ColumnFlag::cpu),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::is_timebase::stored_type>(
          ColumnFlag::is_timebase),
        "Column type and flag combination is not valid");
    PERFETTO_DCHECK(perf_session_id.size() == parent_overlay.size());
    perf_session_id_ = std::move(perf_session_id);
    PERFETTO_DCHECK(cpu.size() == parent_overlay.size());
    cpu_ = std::move(cpu);
    PERFETTO_DCHECK(is_timebase.size() == parent_overlay.size());
    is_timebase_ = std::move(is_timebase);

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
      {const_parent_->storage_layers()[ColumnIndex::id],const_parent_->storage_layers()[ColumnIndex::type],const_parent_->storage_layers()[ColumnIndex::name],const_parent_->storage_layers()[ColumnIndex::parent_id],const_parent_->storage_layers()[ColumnIndex::source_arg_set_id],const_parent_->storage_layers()[ColumnIndex::machine_id],const_parent_->storage_layers()[ColumnIndex::unit],const_parent_->storage_layers()[ColumnIndex::description],perf_session_id_storage_layer_,cpu_storage_layer_,is_timebase_storage_layer_}, {{},{},{},const_parent_->null_layers()[ColumnIndex::parent_id],const_parent_->null_layers()[ColumnIndex::source_arg_set_id],const_parent_->null_layers()[ColumnIndex::machine_id],{},{},{},{},{}}, std::move(overlay_layers));
  }
  CounterTrackTable* parent_ = nullptr;
  const CounterTrackTable* const_parent_ = nullptr;
  ColumnStorage<ColumnType::perf_session_id::stored_type> perf_session_id_;
  ColumnStorage<ColumnType::cpu::stored_type> cpu_;
  ColumnStorage<ColumnType::is_timebase::stored_type> is_timebase_;

  RefPtr<column::StorageLayer> perf_session_id_storage_layer_;
  RefPtr<column::StorageLayer> cpu_storage_layer_;
  RefPtr<column::StorageLayer> is_timebase_storage_layer_;

  
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_PY_H_
