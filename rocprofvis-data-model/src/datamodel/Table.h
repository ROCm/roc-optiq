// MIT License
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef RPV_DATAMODEL_TABLE_H
#define RPV_DATAMODEL_TABLE_H


#include "../common/CommonTypes.h"
#include "TableRow.h"
#include <vector>



/* Base class for PMC and Event time slices
** Defines set of virtual methods to access an array of POD storage objects 
*/

class RpvDmTrace;

class RpvDmTable : public RpvObject {
    public:
        RpvDmTable(  RpvDmTrace* ctx, rocprofvis_dm_charptr_t description, rocprofvis_dm_charptr_t query) : 
                                                        m_ctx(ctx), m_query(query), m_description(description) {}; 

        rocprofvis_dm_result_t                  AddColumn(rocprofvis_dm_charptr_t column_name);
        rocprofvis_dm_table_row_t               AddRow();
        rocprofvis_dm_size_t                    GetNumberOfColumns() {return m_columns.size();};
        rocprofvis_dm_size_t                    GetNumberOfRows() {return m_rows.size();};
        rocprofvis_dm_size_t                    GetMemoryFootprint();

        // getters
        RpvDmTrace*                             Ctx() {return m_ctx;};
        rocprofvis_dm_charptr_t                 Query() {return m_query.c_str();}
        rocprofvis_dm_charptr_t                 Description() {return m_description.c_str();}


        rocprofvis_dm_result_t                  GetPropertyAsUint64(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            uint64_t* value) override;  
        rocprofvis_dm_result_t                  GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            char** value) override;
        rocprofvis_dm_result_t                  GetPropertyAsHandle(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            rocprofvis_dm_handle_t* value) override;


    private:

        RpvDmTrace*                                         m_ctx;
        std::vector<std::string>                            m_columns;
        std::vector<RpvDmTableRow>                          m_rows;
        std::string                                         m_query;
        std::string                                         m_description;

        // accessors
        rocprofvis_dm_result_t          GetColumnNameAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & column);
        rocprofvis_dm_result_t          GetRowHandleAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t & row);

};

#endif //RPV_DATAMODEL_TABLE_H