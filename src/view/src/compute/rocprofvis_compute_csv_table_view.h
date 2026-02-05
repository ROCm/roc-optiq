// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_tab_container.h"
#include "imgui.h"
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Dummy table row for CSV table view
struct CSVTableRow
{
    std::string metric_name;
    std::string avg_value;
    std::string unit;
    std::string peak_value;
    std::string pct_of_peak;
};

// Dummy table for CSV table view
class CSVDummyTable : public RocWidget
{
public:
    CSVDummyTable(const std::string& title, const std::vector<std::string>& columns);
    void Render() override;
    void Update() override {}

    void AddRow(const CSVTableRow& row);
    void Search(const std::string& term);

private:
    std::string m_title;
    std::string m_id;
    std::vector<std::string> m_columns;
    std::vector<CSVTableRow> m_rows;
    std::string m_search_term;
};

// Category containing multiple tables
class CSVTableCategory : public RocWidget
{
public:
    CSVTableCategory(const std::string& category_name);
    void Render() override;
    void Update() override;

    void AddTable(std::shared_ptr<CSVDummyTable> table);
    void Search(const std::string& term);

private:
    std::string m_category_name;
    std::vector<std::shared_ptr<CSVDummyTable>> m_tables;
};

// Main CSV table view with tabs
class ComputeCSVTableView : public RocWidget
{
public:
    ComputeCSVTableView();
    ~ComputeCSVTableView();

    void Render() override;
    void Update() override;

private:
    void RenderMenuBar();
    void CreateDummyTabs();

    char m_search_term[32];
    std::shared_ptr<TabContainer> m_tab_container;
    std::vector<std::shared_ptr<CSVTableCategory>> m_categories;
};

}  // namespace View
}  // namespace RocProfVis
