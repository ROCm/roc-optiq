#ifndef SRC_TRACE_PROCESSOR_TABLES_TRACE_PROTO_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_TABLES_TRACE_PROTO_TABLES_PY_H_

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

class ExperimentalProtoPathTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t field_type = 3;
    static constexpr uint32_t field_name = 4;
    static constexpr uint32_t arg_set_id = 5;
  };
  struct ColumnType {
    using id = IdColumn<ExperimentalProtoPathTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using parent_id = TypedColumn<std::optional<ExperimentalProtoPathTable::Id>>;
    using field_type = TypedColumn<StringPool::Id>;
    using field_name = TypedColumn<std::optional<StringPool::Id>>;
    using arg_set_id = TypedColumn<std::optional<uint32_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(std::optional<ExperimentalProtoPathTable::Id> in_parent_id = {},
        StringPool::Id in_field_type = {},
        std::optional<StringPool::Id> in_field_name = {},
        std::optional<uint32_t> in_arg_set_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          parent_id(in_parent_id),
          field_type(in_field_type),
          field_name(in_field_name),
          arg_set_id(in_arg_set_id) {
      type_ = "experimental_proto_path";
    }
    std::optional<ExperimentalProtoPathTable::Id> parent_id;
    StringPool::Id field_type;
    std::optional<StringPool::Id> field_name;
    std::optional<uint32_t> arg_set_id;

    bool operator==(const ExperimentalProtoPathTable::Row& other) const {
      return type() == other.type() && ColumnType::parent_id::Equals(parent_id, other.parent_id) &&
       ColumnType::field_type::Equals(field_type, other.field_type) &&
       ColumnType::field_name::Equals(field_name, other.field_name) &&
       ColumnType::arg_set_id::Equals(arg_set_id, other.arg_set_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t parent_id = ColumnType::parent_id::default_flags();
    static constexpr uint32_t field_type = ColumnType::field_type::default_flags();
    static constexpr uint32_t field_name = ColumnType::field_name::default_flags();
    static constexpr uint32_t arg_set_id = ColumnType::arg_set_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ExperimentalProtoPathTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ExperimentalProtoPathTable, RowNumber> {
   public:
    ConstRowReference(const ExperimentalProtoPathTable* table, uint32_t row_number)
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
    ColumnType::field_type::type field_type() const {
      return table_->field_type()[row_number_];
    }
    ColumnType::field_name::type field_name() const {
      return table_->field_name()[row_number_];
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      return table_->arg_set_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ExperimentalProtoPathTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_parent_id(
        ColumnType::parent_id::non_optional_type v) {
      return mutable_table()->mutable_parent_id()->Set(row_number_, v);
    }
    void set_field_type(
        ColumnType::field_type::non_optional_type v) {
      return mutable_table()->mutable_field_type()->Set(row_number_, v);
    }
    void set_field_name(
        ColumnType::field_name::non_optional_type v) {
      return mutable_table()->mutable_field_name()->Set(row_number_, v);
    }
    void set_arg_set_id(
        ColumnType::arg_set_id::non_optional_type v) {
      return mutable_table()->mutable_arg_set_id()->Set(row_number_, v);
    }

   private:
    ExperimentalProtoPathTable* mutable_table() const {
      return const_cast<ExperimentalProtoPathTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ExperimentalProtoPathTable, RowNumber, ConstRowReference> {
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
    ColumnType::field_type::type field_type() const {
      const auto& col = table_->field_type();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::field_name::type field_name() const {
      const auto& col = table_->field_name();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::arg_set_id::type arg_set_id() const {
      const auto& col = table_->arg_set_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ExperimentalProtoPathTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ExperimentalProtoPathTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ExperimentalProtoPathTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ExperimentalProtoPathTable;

     explicit Iterator(ExperimentalProtoPathTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ExperimentalProtoPathTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ExperimentalProtoPathTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "parent_id", &self->parent_id_, ColumnFlag::parent_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "field_type", &self->field_type_, ColumnFlag::field_type,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "field_name", &self->field_name_, ColumnFlag::field_name,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "arg_set_id", &self->arg_set_id_, ColumnFlag::arg_set_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ExperimentalProtoPathTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        parent_id_(ColumnStorage<ColumnType::parent_id::stored_type>::Create<false>()),
        field_type_(ColumnStorage<ColumnType::field_type::stored_type>::Create<false>()),
        field_name_(ColumnStorage<ColumnType::field_name::stored_type>::Create<false>()),
        arg_set_id_(ColumnStorage<ColumnType::arg_set_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        parent_id_storage_layer_(
          new column::NumericStorage<ColumnType::parent_id::non_optional_stored_type>(
            &parent_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_id::stored_type>::ToColumnType(),
            false)),
        field_type_storage_layer_(
          new column::StringStorage(string_pool(), &field_type_.vector())),
        field_name_storage_layer_(
          new column::StringStorage(string_pool(), &field_name_.vector())),
        arg_set_id_storage_layer_(
          new column::NumericStorage<ColumnType::arg_set_id::non_optional_stored_type>(
            &arg_set_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::arg_set_id::stored_type>::ToColumnType(),
            false))
,
        parent_id_null_layer_(new column::NullOverlay(parent_id_.bv())),
        arg_set_id_null_layer_(new column::NullOverlay(arg_set_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_id::stored_type>(
          ColumnFlag::parent_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::field_type::stored_type>(
          ColumnFlag::field_type),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::field_name::stored_type>(
          ColumnFlag::field_name),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::arg_set_id::stored_type>(
          ColumnFlag::arg_set_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,parent_id_storage_layer_,field_type_storage_layer_,field_name_storage_layer_,arg_set_id_storage_layer_},
      {{},{},parent_id_null_layer_,{},{},arg_set_id_null_layer_});
  }
  ~ExperimentalProtoPathTable() override;

  static const char* Name() { return "experimental_proto_path"; }

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
        "field_type", ColumnType::field_type::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "field_name", ColumnType::field_name::SqlValueType(), false,
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
    parent_id_.ShrinkToFit();
    field_type_.ShrinkToFit();
    field_name_.ShrinkToFit();
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
    mutable_parent_id()->Append(row.parent_id);
    mutable_field_type()->Append(row.field_type);
    mutable_field_name()->Append(row.field_name);
    mutable_arg_set_id()->Append(row.arg_set_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ExperimentalProtoPathTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<std::optional<ExperimentalProtoPathTable::Id>>& parent_id() const {
    return static_cast<const ColumnType::parent_id&>(columns()[ColumnIndex::parent_id]);
  }
  const TypedColumn<StringPool::Id>& field_type() const {
    return static_cast<const ColumnType::field_type&>(columns()[ColumnIndex::field_type]);
  }
  const TypedColumn<std::optional<StringPool::Id>>& field_name() const {
    return static_cast<const ColumnType::field_name&>(columns()[ColumnIndex::field_name]);
  }
  const TypedColumn<std::optional<uint32_t>>& arg_set_id() const {
    return static_cast<const ColumnType::arg_set_id&>(columns()[ColumnIndex::arg_set_id]);
  }

  TypedColumn<std::optional<ExperimentalProtoPathTable::Id>>* mutable_parent_id() {
    return static_cast<ColumnType::parent_id*>(
        GetColumn(ColumnIndex::parent_id));
  }
  TypedColumn<StringPool::Id>* mutable_field_type() {
    return static_cast<ColumnType::field_type*>(
        GetColumn(ColumnIndex::field_type));
  }
  TypedColumn<std::optional<StringPool::Id>>* mutable_field_name() {
    return static_cast<ColumnType::field_name*>(
        GetColumn(ColumnIndex::field_name));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_arg_set_id() {
    return static_cast<ColumnType::arg_set_id*>(
        GetColumn(ColumnIndex::arg_set_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::parent_id::stored_type> parent_id_;
  ColumnStorage<ColumnType::field_type::stored_type> field_type_;
  ColumnStorage<ColumnType::field_name::stored_type> field_name_;
  ColumnStorage<ColumnType::arg_set_id::stored_type> arg_set_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> parent_id_storage_layer_;
  RefPtr<column::StorageLayer> field_type_storage_layer_;
  RefPtr<column::StorageLayer> field_name_storage_layer_;
  RefPtr<column::StorageLayer> arg_set_id_storage_layer_;

  RefPtr<column::OverlayLayer> parent_id_null_layer_;
  RefPtr<column::OverlayLayer> arg_set_id_null_layer_;
};
  

class ExperimentalProtoContentTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t path = 2;
    static constexpr uint32_t path_id = 3;
    static constexpr uint32_t total_size = 4;
    static constexpr uint32_t size = 5;
    static constexpr uint32_t count = 6;
  };
  struct ColumnType {
    using id = IdColumn<ExperimentalProtoContentTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using path = TypedColumn<StringPool::Id>;
    using path_id = TypedColumn<ExperimentalProtoPathTable::Id>;
    using total_size = TypedColumn<int64_t>;
    using size = TypedColumn<int64_t>;
    using count = TypedColumn<int64_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(StringPool::Id in_path = {},
        ExperimentalProtoPathTable::Id in_path_id = {},
        int64_t in_total_size = {},
        int64_t in_size = {},
        int64_t in_count = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          path(in_path),
          path_id(in_path_id),
          total_size(in_total_size),
          size(in_size),
          count(in_count) {
      type_ = "experimental_proto_content";
    }
    StringPool::Id path;
    ExperimentalProtoPathTable::Id path_id;
    int64_t total_size;
    int64_t size;
    int64_t count;

    bool operator==(const ExperimentalProtoContentTable::Row& other) const {
      return type() == other.type() && ColumnType::path::Equals(path, other.path) &&
       ColumnType::path_id::Equals(path_id, other.path_id) &&
       ColumnType::total_size::Equals(total_size, other.total_size) &&
       ColumnType::size::Equals(size, other.size) &&
       ColumnType::count::Equals(count, other.count);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t path = ColumnType::path::default_flags();
    static constexpr uint32_t path_id = ColumnType::path_id::default_flags();
    static constexpr uint32_t total_size = ColumnType::total_size::default_flags();
    static constexpr uint32_t size = ColumnType::size::default_flags();
    static constexpr uint32_t count = ColumnType::count::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      ExperimentalProtoContentTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    ExperimentalProtoContentTable, RowNumber> {
   public:
    ConstRowReference(const ExperimentalProtoContentTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::path::type path() const {
      return table_->path()[row_number_];
    }
    ColumnType::path_id::type path_id() const {
      return table_->path_id()[row_number_];
    }
    ColumnType::total_size::type total_size() const {
      return table_->total_size()[row_number_];
    }
    ColumnType::size::type size() const {
      return table_->size()[row_number_];
    }
    ColumnType::count::type count() const {
      return table_->count()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const ExperimentalProtoContentTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_path(
        ColumnType::path::non_optional_type v) {
      return mutable_table()->mutable_path()->Set(row_number_, v);
    }
    void set_path_id(
        ColumnType::path_id::non_optional_type v) {
      return mutable_table()->mutable_path_id()->Set(row_number_, v);
    }
    void set_total_size(
        ColumnType::total_size::non_optional_type v) {
      return mutable_table()->mutable_total_size()->Set(row_number_, v);
    }
    void set_size(
        ColumnType::size::non_optional_type v) {
      return mutable_table()->mutable_size()->Set(row_number_, v);
    }
    void set_count(
        ColumnType::count::non_optional_type v) {
      return mutable_table()->mutable_count()->Set(row_number_, v);
    }

   private:
    ExperimentalProtoContentTable* mutable_table() const {
      return const_cast<ExperimentalProtoContentTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, ExperimentalProtoContentTable, RowNumber, ConstRowReference> {
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
    ColumnType::path::type path() const {
      const auto& col = table_->path();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::path_id::type path_id() const {
      const auto& col = table_->path_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::total_size::type total_size() const {
      const auto& col = table_->total_size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::size::type size() const {
      const auto& col = table_->size();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::count::type count() const {
      const auto& col = table_->count();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const ExperimentalProtoContentTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class ExperimentalProtoContentTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, ExperimentalProtoContentTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class ExperimentalProtoContentTable;

     explicit Iterator(ExperimentalProtoContentTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     ExperimentalProtoContentTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      ExperimentalProtoContentTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "path", &self->path_, ColumnFlag::path,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "path_id", &self->path_id_, ColumnFlag::path_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "total_size", &self->total_size_, ColumnFlag::total_size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "size", &self->size_, ColumnFlag::size,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "count", &self->count_, ColumnFlag::count,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit ExperimentalProtoContentTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        path_(ColumnStorage<ColumnType::path::stored_type>::Create<false>()),
        path_id_(ColumnStorage<ColumnType::path_id::stored_type>::Create<false>()),
        total_size_(ColumnStorage<ColumnType::total_size::stored_type>::Create<false>()),
        size_(ColumnStorage<ColumnType::size::stored_type>::Create<false>()),
        count_(ColumnStorage<ColumnType::count::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        path_storage_layer_(
          new column::StringStorage(string_pool(), &path_.vector())),
        path_id_storage_layer_(
        new column::NumericStorage<ColumnType::path_id::non_optional_stored_type>(
          &path_id_.vector(),
          ColumnTypeHelper<ColumnType::path_id::stored_type>::ToColumnType(),
          false)),
        total_size_storage_layer_(
        new column::NumericStorage<ColumnType::total_size::non_optional_stored_type>(
          &total_size_.vector(),
          ColumnTypeHelper<ColumnType::total_size::stored_type>::ToColumnType(),
          false)),
        size_storage_layer_(
        new column::NumericStorage<ColumnType::size::non_optional_stored_type>(
          &size_.vector(),
          ColumnTypeHelper<ColumnType::size::stored_type>::ToColumnType(),
          false)),
        count_storage_layer_(
        new column::NumericStorage<ColumnType::count::non_optional_stored_type>(
          &count_.vector(),
          ColumnTypeHelper<ColumnType::count::stored_type>::ToColumnType(),
          false))
         {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::path::stored_type>(
          ColumnFlag::path),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::path_id::stored_type>(
          ColumnFlag::path_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::total_size::stored_type>(
          ColumnFlag::total_size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::size::stored_type>(
          ColumnFlag::size),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::count::stored_type>(
          ColumnFlag::count),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,path_storage_layer_,path_id_storage_layer_,total_size_storage_layer_,size_storage_layer_,count_storage_layer_},
      {{},{},{},{},{},{},{}});
  }
  ~ExperimentalProtoContentTable() override;

  static const char* Name() { return "experimental_proto_content"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "path", ColumnType::path::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "path_id", ColumnType::path_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "total_size", ColumnType::total_size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "size", ColumnType::size::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "count", ColumnType::count::SqlValueType(), false,
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
    path_.ShrinkToFit();
    path_id_.ShrinkToFit();
    total_size_.ShrinkToFit();
    size_.ShrinkToFit();
    count_.ShrinkToFit();
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
    mutable_path()->Append(row.path);
    mutable_path_id()->Append(row.path_id);
    mutable_total_size()->Append(row.total_size);
    mutable_size()->Append(row.size);
    mutable_count()->Append(row.count);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<ExperimentalProtoContentTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<StringPool::Id>& path() const {
    return static_cast<const ColumnType::path&>(columns()[ColumnIndex::path]);
  }
  const TypedColumn<ExperimentalProtoPathTable::Id>& path_id() const {
    return static_cast<const ColumnType::path_id&>(columns()[ColumnIndex::path_id]);
  }
  const TypedColumn<int64_t>& total_size() const {
    return static_cast<const ColumnType::total_size&>(columns()[ColumnIndex::total_size]);
  }
  const TypedColumn<int64_t>& size() const {
    return static_cast<const ColumnType::size&>(columns()[ColumnIndex::size]);
  }
  const TypedColumn<int64_t>& count() const {
    return static_cast<const ColumnType::count&>(columns()[ColumnIndex::count]);
  }

  TypedColumn<StringPool::Id>* mutable_path() {
    return static_cast<ColumnType::path*>(
        GetColumn(ColumnIndex::path));
  }
  TypedColumn<ExperimentalProtoPathTable::Id>* mutable_path_id() {
    return static_cast<ColumnType::path_id*>(
        GetColumn(ColumnIndex::path_id));
  }
  TypedColumn<int64_t>* mutable_total_size() {
    return static_cast<ColumnType::total_size*>(
        GetColumn(ColumnIndex::total_size));
  }
  TypedColumn<int64_t>* mutable_size() {
    return static_cast<ColumnType::size*>(
        GetColumn(ColumnIndex::size));
  }
  TypedColumn<int64_t>* mutable_count() {
    return static_cast<ColumnType::count*>(
        GetColumn(ColumnIndex::count));
  }

 private:
  
  
  ColumnStorage<ColumnType::path::stored_type> path_;
  ColumnStorage<ColumnType::path_id::stored_type> path_id_;
  ColumnStorage<ColumnType::total_size::stored_type> total_size_;
  ColumnStorage<ColumnType::size::stored_type> size_;
  ColumnStorage<ColumnType::count::stored_type> count_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> path_storage_layer_;
  RefPtr<column::StorageLayer> path_id_storage_layer_;
  RefPtr<column::StorageLayer> total_size_storage_layer_;
  RefPtr<column::StorageLayer> size_storage_layer_;
  RefPtr<column::StorageLayer> count_storage_layer_;

  
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_TRACE_PROTO_TABLES_PY_H_
