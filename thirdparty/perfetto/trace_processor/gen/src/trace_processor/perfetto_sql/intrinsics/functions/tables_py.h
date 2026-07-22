#ifndef SRC_TRACE_PROCESSOR_PERFETTO_SQL_INTRINSICS_FUNCTIONS_TABLES_PY_H_
#define SRC_TRACE_PROCESSOR_PERFETTO_SQL_INTRINSICS_FUNCTIONS_TABLES_PY_H_

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

class DominatorTreeTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t node_id = 2;
    static constexpr uint32_t dominator_node_id = 3;
  };
  struct ColumnType {
    using id = IdColumn<DominatorTreeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using node_id = TypedColumn<uint32_t>;
    using dominator_node_id = TypedColumn<std::optional<uint32_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_node_id = {},
        std::optional<uint32_t> in_dominator_node_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          node_id(in_node_id),
          dominator_node_id(in_dominator_node_id) {
      type_ = "__intrinsic_dominator_tree";
    }
    uint32_t node_id;
    std::optional<uint32_t> dominator_node_id;

    bool operator==(const DominatorTreeTable::Row& other) const {
      return type() == other.type() && ColumnType::node_id::Equals(node_id, other.node_id) &&
       ColumnType::dominator_node_id::Equals(dominator_node_id, other.dominator_node_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t node_id = ColumnType::node_id::default_flags();
    static constexpr uint32_t dominator_node_id = ColumnType::dominator_node_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      DominatorTreeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    DominatorTreeTable, RowNumber> {
   public:
    ConstRowReference(const DominatorTreeTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::node_id::type node_id() const {
      return table_->node_id()[row_number_];
    }
    ColumnType::dominator_node_id::type dominator_node_id() const {
      return table_->dominator_node_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const DominatorTreeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_node_id(
        ColumnType::node_id::non_optional_type v) {
      return mutable_table()->mutable_node_id()->Set(row_number_, v);
    }
    void set_dominator_node_id(
        ColumnType::dominator_node_id::non_optional_type v) {
      return mutable_table()->mutable_dominator_node_id()->Set(row_number_, v);
    }

   private:
    DominatorTreeTable* mutable_table() const {
      return const_cast<DominatorTreeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, DominatorTreeTable, RowNumber, ConstRowReference> {
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
    ColumnType::node_id::type node_id() const {
      const auto& col = table_->node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::dominator_node_id::type dominator_node_id() const {
      const auto& col = table_->dominator_node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const DominatorTreeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class DominatorTreeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, DominatorTreeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class DominatorTreeTable;

     explicit Iterator(DominatorTreeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     DominatorTreeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      DominatorTreeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "node_id", &self->node_id_, ColumnFlag::node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "dominator_node_id", &self->dominator_node_id_, ColumnFlag::dominator_node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit DominatorTreeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        node_id_(ColumnStorage<ColumnType::node_id::stored_type>::Create<false>()),
        dominator_node_id_(ColumnStorage<ColumnType::dominator_node_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        node_id_storage_layer_(
        new column::NumericStorage<ColumnType::node_id::non_optional_stored_type>(
          &node_id_.vector(),
          ColumnTypeHelper<ColumnType::node_id::stored_type>::ToColumnType(),
          false)),
        dominator_node_id_storage_layer_(
          new column::NumericStorage<ColumnType::dominator_node_id::non_optional_stored_type>(
            &dominator_node_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::dominator_node_id::stored_type>::ToColumnType(),
            false))
,
        dominator_node_id_null_layer_(new column::NullOverlay(dominator_node_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::node_id::stored_type>(
          ColumnFlag::node_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::dominator_node_id::stored_type>(
          ColumnFlag::dominator_node_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,node_id_storage_layer_,dominator_node_id_storage_layer_},
      {{},{},{},dominator_node_id_null_layer_});
  }
  ~DominatorTreeTable() override;

  static const char* Name() { return "__intrinsic_dominator_tree"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "node_id", ColumnType::node_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "dominator_node_id", ColumnType::dominator_node_id::SqlValueType(), false,
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
    node_id_.ShrinkToFit();
    dominator_node_id_.ShrinkToFit();
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
    mutable_node_id()->Append(row.node_id);
    mutable_dominator_node_id()->Append(row.dominator_node_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<DominatorTreeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& node_id() const {
    return static_cast<const ColumnType::node_id&>(columns()[ColumnIndex::node_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& dominator_node_id() const {
    return static_cast<const ColumnType::dominator_node_id&>(columns()[ColumnIndex::dominator_node_id]);
  }

  TypedColumn<uint32_t>* mutable_node_id() {
    return static_cast<ColumnType::node_id*>(
        GetColumn(ColumnIndex::node_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_dominator_node_id() {
    return static_cast<ColumnType::dominator_node_id*>(
        GetColumn(ColumnIndex::dominator_node_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::node_id::stored_type> node_id_;
  ColumnStorage<ColumnType::dominator_node_id::stored_type> dominator_node_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> node_id_storage_layer_;
  RefPtr<column::StorageLayer> dominator_node_id_storage_layer_;

  RefPtr<column::OverlayLayer> dominator_node_id_null_layer_;
};
  

class StructuralTreePartitionTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t node_id = 2;
    static constexpr uint32_t parent_node_id = 3;
    static constexpr uint32_t group_key = 4;
  };
  struct ColumnType {
    using id = IdColumn<StructuralTreePartitionTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using node_id = TypedColumn<uint32_t>;
    using parent_node_id = TypedColumn<std::optional<uint32_t>>;
    using group_key = TypedColumn<uint32_t>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_node_id = {},
        std::optional<uint32_t> in_parent_node_id = {},
        uint32_t in_group_key = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          node_id(in_node_id),
          parent_node_id(in_parent_node_id),
          group_key(in_group_key) {
      type_ = "__intrinsic_structural_tree_partition";
    }
    uint32_t node_id;
    std::optional<uint32_t> parent_node_id;
    uint32_t group_key;

    bool operator==(const StructuralTreePartitionTable::Row& other) const {
      return type() == other.type() && ColumnType::node_id::Equals(node_id, other.node_id) &&
       ColumnType::parent_node_id::Equals(parent_node_id, other.parent_node_id) &&
       ColumnType::group_key::Equals(group_key, other.group_key);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t node_id = ColumnType::node_id::default_flags();
    static constexpr uint32_t parent_node_id = ColumnType::parent_node_id::default_flags();
    static constexpr uint32_t group_key = ColumnType::group_key::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      StructuralTreePartitionTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    StructuralTreePartitionTable, RowNumber> {
   public:
    ConstRowReference(const StructuralTreePartitionTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::node_id::type node_id() const {
      return table_->node_id()[row_number_];
    }
    ColumnType::parent_node_id::type parent_node_id() const {
      return table_->parent_node_id()[row_number_];
    }
    ColumnType::group_key::type group_key() const {
      return table_->group_key()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const StructuralTreePartitionTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_node_id(
        ColumnType::node_id::non_optional_type v) {
      return mutable_table()->mutable_node_id()->Set(row_number_, v);
    }
    void set_parent_node_id(
        ColumnType::parent_node_id::non_optional_type v) {
      return mutable_table()->mutable_parent_node_id()->Set(row_number_, v);
    }
    void set_group_key(
        ColumnType::group_key::non_optional_type v) {
      return mutable_table()->mutable_group_key()->Set(row_number_, v);
    }

   private:
    StructuralTreePartitionTable* mutable_table() const {
      return const_cast<StructuralTreePartitionTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, StructuralTreePartitionTable, RowNumber, ConstRowReference> {
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
    ColumnType::node_id::type node_id() const {
      const auto& col = table_->node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::parent_node_id::type parent_node_id() const {
      const auto& col = table_->parent_node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::group_key::type group_key() const {
      const auto& col = table_->group_key();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const StructuralTreePartitionTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class StructuralTreePartitionTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, StructuralTreePartitionTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class StructuralTreePartitionTable;

     explicit Iterator(StructuralTreePartitionTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     StructuralTreePartitionTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      StructuralTreePartitionTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "node_id", &self->node_id_, ColumnFlag::node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "parent_node_id", &self->parent_node_id_, ColumnFlag::parent_node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "group_key", &self->group_key_, ColumnFlag::group_key,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit StructuralTreePartitionTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        node_id_(ColumnStorage<ColumnType::node_id::stored_type>::Create<false>()),
        parent_node_id_(ColumnStorage<ColumnType::parent_node_id::stored_type>::Create<false>()),
        group_key_(ColumnStorage<ColumnType::group_key::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        node_id_storage_layer_(
        new column::NumericStorage<ColumnType::node_id::non_optional_stored_type>(
          &node_id_.vector(),
          ColumnTypeHelper<ColumnType::node_id::stored_type>::ToColumnType(),
          false)),
        parent_node_id_storage_layer_(
          new column::NumericStorage<ColumnType::parent_node_id::non_optional_stored_type>(
            &parent_node_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_node_id::stored_type>::ToColumnType(),
            false)),
        group_key_storage_layer_(
        new column::NumericStorage<ColumnType::group_key::non_optional_stored_type>(
          &group_key_.vector(),
          ColumnTypeHelper<ColumnType::group_key::stored_type>::ToColumnType(),
          false))
,
        parent_node_id_null_layer_(new column::NullOverlay(parent_node_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::node_id::stored_type>(
          ColumnFlag::node_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_node_id::stored_type>(
          ColumnFlag::parent_node_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::group_key::stored_type>(
          ColumnFlag::group_key),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,node_id_storage_layer_,parent_node_id_storage_layer_,group_key_storage_layer_},
      {{},{},{},parent_node_id_null_layer_,{}});
  }
  ~StructuralTreePartitionTable() override;

  static const char* Name() { return "__intrinsic_structural_tree_partition"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "node_id", ColumnType::node_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "parent_node_id", ColumnType::parent_node_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "group_key", ColumnType::group_key::SqlValueType(), false,
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
    node_id_.ShrinkToFit();
    parent_node_id_.ShrinkToFit();
    group_key_.ShrinkToFit();
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
    mutable_node_id()->Append(row.node_id);
    mutable_parent_node_id()->Append(row.parent_node_id);
    mutable_group_key()->Append(row.group_key);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<StructuralTreePartitionTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& node_id() const {
    return static_cast<const ColumnType::node_id&>(columns()[ColumnIndex::node_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& parent_node_id() const {
    return static_cast<const ColumnType::parent_node_id&>(columns()[ColumnIndex::parent_node_id]);
  }
  const TypedColumn<uint32_t>& group_key() const {
    return static_cast<const ColumnType::group_key&>(columns()[ColumnIndex::group_key]);
  }

  TypedColumn<uint32_t>* mutable_node_id() {
    return static_cast<ColumnType::node_id*>(
        GetColumn(ColumnIndex::node_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_parent_node_id() {
    return static_cast<ColumnType::parent_node_id*>(
        GetColumn(ColumnIndex::parent_node_id));
  }
  TypedColumn<uint32_t>* mutable_group_key() {
    return static_cast<ColumnType::group_key*>(
        GetColumn(ColumnIndex::group_key));
  }

 private:
  
  
  ColumnStorage<ColumnType::node_id::stored_type> node_id_;
  ColumnStorage<ColumnType::parent_node_id::stored_type> parent_node_id_;
  ColumnStorage<ColumnType::group_key::stored_type> group_key_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> node_id_storage_layer_;
  RefPtr<column::StorageLayer> parent_node_id_storage_layer_;
  RefPtr<column::StorageLayer> group_key_storage_layer_;

  RefPtr<column::OverlayLayer> parent_node_id_null_layer_;
};
  

class TreeTable : public macros_internal::MacroTable {
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
    static constexpr uint32_t node_id = 2;
    static constexpr uint32_t parent_node_id = 3;
  };
  struct ColumnType {
    using id = IdColumn<TreeTable::Id>;
    using type = TypedColumn<StringPool::Id>;
    using node_id = TypedColumn<uint32_t>;
    using parent_node_id = TypedColumn<std::optional<uint32_t>>;
  };
  struct Row : public macros_internal::RootParentTable::Row {
    Row(uint32_t in_node_id = {},
        std::optional<uint32_t> in_parent_node_id = {},
        std::nullptr_t = nullptr)
        : macros_internal::RootParentTable::Row(),
          node_id(in_node_id),
          parent_node_id(in_parent_node_id) {
      type_ = "__unused";
    }
    uint32_t node_id;
    std::optional<uint32_t> parent_node_id;

    bool operator==(const TreeTable::Row& other) const {
      return type() == other.type() && ColumnType::node_id::Equals(node_id, other.node_id) &&
       ColumnType::parent_node_id::Equals(parent_node_id, other.parent_node_id);
    }
  };
  struct ColumnFlag {
    static constexpr uint32_t node_id = ColumnType::node_id::default_flags();
    static constexpr uint32_t parent_node_id = ColumnType::parent_node_id::default_flags();
  };

  class RowNumber;
  class ConstRowReference;
  class RowReference;

  class RowNumber : public macros_internal::AbstractRowNumber<
      TreeTable, ConstRowReference, RowReference> {
   public:
    explicit RowNumber(uint32_t row_number)
        : AbstractRowNumber(row_number) {}
  };
  static_assert(std::is_trivially_destructible_v<RowNumber>,
                "Inheritance used without trivial destruction");

  class ConstRowReference : public macros_internal::AbstractConstRowReference<
    TreeTable, RowNumber> {
   public:
    ConstRowReference(const TreeTable* table, uint32_t row_number)
        : AbstractConstRowReference(table, row_number) {}

    ColumnType::id::type id() const {
      return table_->id()[row_number_];
    }
    ColumnType::type::type type() const {
      return table_->type()[row_number_];
    }
    ColumnType::node_id::type node_id() const {
      return table_->node_id()[row_number_];
    }
    ColumnType::parent_node_id::type parent_node_id() const {
      return table_->parent_node_id()[row_number_];
    }
  };
  static_assert(std::is_trivially_destructible_v<ConstRowReference>,
                "Inheritance used without trivial destruction");
  class RowReference : public ConstRowReference {
   public:
    RowReference(const TreeTable* table, uint32_t row_number)
        : ConstRowReference(table, row_number) {}

    void set_node_id(
        ColumnType::node_id::non_optional_type v) {
      return mutable_table()->mutable_node_id()->Set(row_number_, v);
    }
    void set_parent_node_id(
        ColumnType::parent_node_id::non_optional_type v) {
      return mutable_table()->mutable_parent_node_id()->Set(row_number_, v);
    }

   private:
    TreeTable* mutable_table() const {
      return const_cast<TreeTable*>(table_);
    }
  };
  static_assert(std::is_trivially_destructible_v<RowReference>,
                "Inheritance used without trivial destruction");

  class ConstIterator;
  class ConstIterator : public macros_internal::AbstractConstIterator<
    ConstIterator, TreeTable, RowNumber, ConstRowReference> {
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
    ColumnType::node_id::type node_id() const {
      const auto& col = table_->node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }
    ColumnType::parent_node_id::type parent_node_id() const {
      const auto& col = table_->parent_node_id();
      return col.GetAtIdx(
        iterator_.StorageIndexForColumn(col.index_in_table()));
    }

   protected:
    explicit ConstIterator(const TreeTable* table,
                           Table::Iterator iterator)
        : AbstractConstIterator(table, std::move(iterator)) {}

    uint32_t CurrentRowNumber() const {
      return iterator_.StorageIndexForLastOverlay();
    }

   private:
    friend class TreeTable;
    friend class macros_internal::AbstractConstIterator<
      ConstIterator, TreeTable, RowNumber, ConstRowReference>;
  };
  class Iterator : public ConstIterator {
    public:
     RowReference row_reference() const {
       return RowReference(mutable_table_, CurrentRowNumber());
     }

    private:
     friend class TreeTable;

     explicit Iterator(TreeTable* table, Table::Iterator iterator)
        : ConstIterator(table, std::move(iterator)),
          mutable_table_(table) {}

     TreeTable* mutable_table_ = nullptr;
  };

  struct IdAndRow {
    Id id;
    uint32_t row;
    RowReference row_reference;
    RowNumber row_number;
  };

  static std::vector<ColumnLegacy> GetColumns(
      TreeTable* self,
      const macros_internal::MacroTable* parent) {
    std::vector<ColumnLegacy> columns =
        CopyColumnsFromParentOrAddRootColumns(self, parent);
    uint32_t olay_idx = OverlayCount(parent);
    AddColumnToVector(columns, "node_id", &self->node_id_, ColumnFlag::node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    AddColumnToVector(columns, "parent_node_id", &self->parent_node_id_, ColumnFlag::parent_node_id,
                      static_cast<uint32_t>(columns.size()), olay_idx);
    return columns;
  }

  PERFETTO_NO_INLINE explicit TreeTable(StringPool* pool)
      : macros_internal::MacroTable(
          pool,
          GetColumns(this, nullptr),
          nullptr),
        node_id_(ColumnStorage<ColumnType::node_id::stored_type>::Create<false>()),
        parent_node_id_(ColumnStorage<ColumnType::parent_node_id::stored_type>::Create<false>())
,
        id_storage_layer_(new column::IdStorage()),
        type_storage_layer_(
          new column::StringStorage(string_pool(), &type_.vector())),
        node_id_storage_layer_(
        new column::NumericStorage<ColumnType::node_id::non_optional_stored_type>(
          &node_id_.vector(),
          ColumnTypeHelper<ColumnType::node_id::stored_type>::ToColumnType(),
          false)),
        parent_node_id_storage_layer_(
          new column::NumericStorage<ColumnType::parent_node_id::non_optional_stored_type>(
            &parent_node_id_.non_null_vector(),
            ColumnTypeHelper<ColumnType::parent_node_id::stored_type>::ToColumnType(),
            false))
,
        parent_node_id_null_layer_(new column::NullOverlay(parent_node_id_.bv())) {
    static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::node_id::stored_type>(
          ColumnFlag::node_id),
        "Column type and flag combination is not valid");
      static_assert(
        ColumnLegacy::IsFlagsAndTypeValid<ColumnType::parent_node_id::stored_type>(
          ColumnFlag::parent_node_id),
        "Column type and flag combination is not valid");
    OnConstructionCompletedRegularConstructor(
      {id_storage_layer_,type_storage_layer_,node_id_storage_layer_,parent_node_id_storage_layer_},
      {{},{},{},parent_node_id_null_layer_});
  }
  ~TreeTable() override;

  static const char* Name() { return "__unused"; }

  static Table::Schema ComputeStaticSchema() {
    Table::Schema schema;
    schema.columns.emplace_back(Table::Schema::Column{
        "id", SqlValue::Type::kLong, true, true, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "type", SqlValue::Type::kString, false, false, false, false});
    schema.columns.emplace_back(Table::Schema::Column{
        "node_id", ColumnType::node_id::SqlValueType(), false,
        false,
        false,
        false});
    schema.columns.emplace_back(Table::Schema::Column{
        "parent_node_id", ColumnType::parent_node_id::SqlValueType(), false,
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
    node_id_.ShrinkToFit();
    parent_node_id_.ShrinkToFit();
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
    mutable_node_id()->Append(row.node_id);
    mutable_parent_node_id()->Append(row.parent_node_id);
    UpdateSelfOverlayAfterInsert();
    return IdAndRow{id, row_number, RowReference(this, row_number),
                     RowNumber(row_number)};
  }

  

  const IdColumn<TreeTable::Id>& id() const {
    return static_cast<const ColumnType::id&>(columns()[ColumnIndex::id]);
  }
  const TypedColumn<StringPool::Id>& type() const {
    return static_cast<const ColumnType::type&>(columns()[ColumnIndex::type]);
  }
  const TypedColumn<uint32_t>& node_id() const {
    return static_cast<const ColumnType::node_id&>(columns()[ColumnIndex::node_id]);
  }
  const TypedColumn<std::optional<uint32_t>>& parent_node_id() const {
    return static_cast<const ColumnType::parent_node_id&>(columns()[ColumnIndex::parent_node_id]);
  }

  TypedColumn<uint32_t>* mutable_node_id() {
    return static_cast<ColumnType::node_id*>(
        GetColumn(ColumnIndex::node_id));
  }
  TypedColumn<std::optional<uint32_t>>* mutable_parent_node_id() {
    return static_cast<ColumnType::parent_node_id*>(
        GetColumn(ColumnIndex::parent_node_id));
  }

 private:
  
  
  ColumnStorage<ColumnType::node_id::stored_type> node_id_;
  ColumnStorage<ColumnType::parent_node_id::stored_type> parent_node_id_;

  RefPtr<column::StorageLayer> id_storage_layer_;
  RefPtr<column::StorageLayer> type_storage_layer_;
  RefPtr<column::StorageLayer> node_id_storage_layer_;
  RefPtr<column::StorageLayer> parent_node_id_storage_layer_;

  RefPtr<column::OverlayLayer> parent_node_id_null_layer_;
};

}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_PERFETTO_SQL_INTRINSICS_FUNCTIONS_TABLES_PY_H_
